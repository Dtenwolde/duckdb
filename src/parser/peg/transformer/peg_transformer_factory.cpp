#include "duckdb/parser/peg/transformer/peg_transformer.hpp"
#include "duckdb/parser/peg/compiled_grammar.hpp"
#include "duckdb/parser/expression/star_expression.hpp"
#include "duckdb/common/enums/trigger_type.hpp"
#include "duckdb/common/query_location.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/peg/matcher.hpp"
#include "duckdb/common/to_string.hpp"
#include "duckdb/parser/sql_statement.hpp"
#include "duckdb/parser/token_iterator.hpp"
#include "duckdb/parser/tableref/showref.hpp"
#include "duckdb/common/enums/date_part_specifier.hpp"
#include "duckdb/common/enums/merge_action_type.hpp"
#include "duckdb/common/enums/subquery_type.hpp"
#include "duckdb/common/exception/conversion_exception.hpp"
#include "duckdb/parser/expression/cast_expression.hpp"
#include "duckdb/catalog/default/default_types.hpp"
#include "duckdb/parser/expression/type_expression.hpp"
#include "duckdb/parser/query_node/set_operation_node.hpp"
#include "duckdb/parser/statement/merge_into_statement.hpp"
#include "duckdb/parser/constraints/foreign_key_constraint.hpp"

namespace duckdb {

static unique_ptr<SQLStatement> ExtractAndTransformStatement(PEGTransformer &transformer,
                                                             const TokenIterator &token_iterator, ParseResult &stmt_pr,
                                                             optional_idx terminator_offset) {
	auto stmt = transformer.Transform<unique_ptr<SQLStatement>>(stmt_pr);

	if (!transformer.named_parameter_map.empty()) {
		stmt->named_param_map = transformer.named_parameter_map;
	}
	if (!transformer.pivot_entries.empty()) {
		stmt = transformer.CreatePivotStatement(std::move(stmt));
		// Unpacking discards the wrapper, so the parts keep their map; the wrapper carries it for introspection.
		stmt->named_param_map = transformer.named_parameter_map;
	}
	transformer.Clear();

	// Calculate location and length cleanly
	auto offset = stmt_pr.GetOffset();
	if (offset.IsValid()) {
		auto start = offset.GetIndex();
		idx_t end_index = terminator_offset.IsValid() ? terminator_offset.GetIndex() : token_iterator.EndOffset();
		stmt->stmt_location = QueryLocation(start, end_index - start);
	}

	return stmt;
}

PEGStatementResources::PEGStatementResources()
    : process_allocator(Allocator::DefaultAllocator(), 16384), packrat_allocator(Allocator::DefaultAllocator(), 16384),
      transformer_allocator(Allocator::DefaultAllocator(), 16384) {
}

PEGStatementResources::~PEGStatementResources() = default;

PEGTransformer &PEGStatementResources::GetTransformer(TokenIterator &token_iterator, ParserOptions &options,
                                                      const CompiledGrammar &grammar) {
	if (!transformer) {
		transformer =
		    make_uniq<PEGTransformer>(transformer_allocator, token_iterator, options, grammar, transform_stacks);
	}
	return *transformer;
}

void PEGStatementResources::ReleaseQuery() {
	transformer.reset();
	Reset();
}

void PEGStatementResources::Reset() {
	suggestions.clear();
	parse_results.Reset();
	process_allocator.Reset();
	packrat_allocator.Reset();
	transformer_allocator.Reset();
	if (transformer) {
		transformer->Clear();
		transformer->ResetProcesses();
	}
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformTopLevelStatement(TokenIterator &token_iterator,
                                                                           ParserOptions &options,
                                                                           const CompiledGrammar &grammar) {
	PEGStatementResources resources;
	return TransformTopLevelStatement(token_iterator, options, grammar, resources);
}

//! Matches one TopLevelStatement, returning the furthest token index seen, the furthest index a sequence skipped by
//! a tail check could have reached, and the position after the match
static MatcherResult MatchTopLevelStatement(TokenIterator &token_iterator, ParserOptions &options,
                                            const CompiledGrammar &grammar, PEGStatementResources &resources,
                                            bool sequence_checks, idx_t &max_token_index, idx_t &tail_rejected_until,
                                            idx_t &end_position) {
	resources.Reset();
	max_token_index = token_iterator.Position();
	// the packrat cache outlives the match processes, so they need separate arenas
	ParserPackratCache packrat_cache(resources.packrat_allocator, token_iterator.Position(), token_iterator.Size());
	MatchContext match_context(resources.suggestions, resources.parse_results, resources.process_allocator,
	                           max_token_index, MatchMode::BUILD_PARSE_RESULT, options.identifier_case_mode,
	                           &packrat_cache);
	if (sequence_checks) {
		match_context.sequence_checks = grammar.GetSequenceChecks();
	}
	MatchState state(token_iterator, match_context);
	auto match_result = grammar.TopLevelStatementMatcher().MatchParseResult(state, resources.match_stack);
	resources.process_allocator.Reset();
	tail_rejected_until = match_context.tail_rejected_until;
	end_position = state.token_iterator.Position();
	return match_result;
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformTopLevelStatement(TokenIterator &token_iterator,
                                                                           ParserOptions &options,
                                                                           const CompiledGrammar &grammar,
                                                                           PEGStatementResources &resources) {
	if (!token_iterator.Current()) {
		return nullptr;
	}
	idx_t max_token_index;
	idx_t tail_rejected_until;
	idx_t end_position;
	auto match_result = MatchTopLevelStatement(token_iterator, options, grammar, resources, true, max_token_index,
	                                           tail_rejected_until, end_position);
	if (!match_result.IsSuccess()) {
		if (tail_rejected_until > max_token_index) {
			// a skipped sequence could have come further than the match did: match again without tail checks for
			// the exact error position
			MatchTopLevelStatement(token_iterator, options, grammar, resources, false, max_token_index,
			                       tail_rejected_until, end_position);
		}
		// syntax error — surface as a parser exception in the same shape as Transform()
		auto token_stream = token_iterator.ToString();
		idx_t error_token_idx = max_token_index;
		if (error_token_idx >= token_iterator.Size()) {
			error_token_idx = token_iterator.Size() - 1;
		}
		// Walk back past the EOI sentinel so the error message names a real token.
		if (error_token_idx > 0 &&
		    (token_iterator.GetToken(error_token_idx).type == TokenType::END_OF_INPUT ||
		     token_iterator.GetToken(error_token_idx).type == TokenType::END_OF_INPUT_AUTOCOMPLETE)) {
			error_token_idx--;
		}
		auto &error_token = token_iterator.GetToken(error_token_idx);
		auto error_message = "syntax error at or near \"" + error_token.text + "\"";
		throw ParserException::SyntaxError(token_stream, error_message,
		                                   QueryLocation(error_token.offset, error_token.length));
	}
	D_ASSERT(match_result.HasParseResult());

	// Advance the caller's cursor past the consumed tokens.
	token_iterator.SetPosition(end_position);

	// TopLevelStatement <- Statement? (';'+ / EndOfInput)
	//   child 0: Optional<Statement>
	//   child 1: bracket-wrapper list around Choice<';'+ | EndOfInput>
	auto &tls = match_result.GetParseResult()->Cast<ListParseResult>();
	auto &stmt_opt = tls.Child<OptionalParseResult>(0);
	if (!stmt_opt.HasResult()) {
		// separator-only or EOI-only TopLevelStatement — no statement to yield
		return nullptr;
	}
	auto &term_wrapper = tls.Child<ListParseResult>(1);
	auto &term_inner = term_wrapper.Child<ChoiceParseResult>(0).GetResult();
	optional_idx terminator_offset;
	if (term_inner.type != ParseResultType::END_OF_INPUT) {
		auto semi_children = term_inner.Cast<RepeatParseResult>().GetChildren();
		if (!semi_children.empty()) {
			terminator_offset = semi_children[0].get().GetOffset();
		}
	}

	auto &transformer = resources.GetTransformer(token_iterator, options, grammar);
	return ExtractAndTransformStatement(transformer, token_iterator, stmt_opt.GetResult(), terminator_offset);
}

PEGTransformerFactory::PEGTransformerFactory(ParsedGrammar &grammar_p) : grammar(grammar_p) {
	case_insensitive_set_t collapsible_rules;
	//===--------------------------------------------------------------------===//
	// START GENERATED COLLAPSIBLE RULES
	//===--------------------------------------------------------------------===//
	collapsible_rules.insert("Expression");
	collapsible_rules.insert("ColumnDefaultExpr");
	collapsible_rules.insert("LambdaArrowExpression");
	collapsible_rules.insert("LogicalOrExpression");
	collapsible_rules.insert("ColDefOrExpr");
	collapsible_rules.insert("LogicalAndExpression");
	collapsible_rules.insert("ColDefAndExpr");
	collapsible_rules.insert("LogicalNotExpression");
	collapsible_rules.insert("IsExpression");
	collapsible_rules.insert("IsDistinctFromExpression");
	collapsible_rules.insert("ComparisonExpression");
	collapsible_rules.insert("BetweenInLikeExpression");
	collapsible_rules.insert("OtherOperatorExpression");
	collapsible_rules.insert("InfixOtherOperatorExpression");
	collapsible_rules.insert("BitwiseExpression");
	collapsible_rules.insert("TildeExpression");
	collapsible_rules.insert("AdditiveExpression");
	collapsible_rules.insert("MultiplicativeExpression");
	collapsible_rules.insert("ExponentiationExpression");
	collapsible_rules.insert("CollateExpression");
	collapsible_rules.insert("AtTimeZoneExpression");
	collapsible_rules.insert("PrefixExpression");
	collapsible_rules.insert("BaseExpression");
	collapsible_rules.insert("SelectSetOpChain");
	collapsible_rules.insert("IntersectChain");
	collapsible_rules.insert("TableRef");
	collapsible_rules.insert("AddOrDropDefault");
	collapsible_rules.insert("AfterMatchSkip");
	collapsible_rules.insert("AfterMatchSkipTarget");
	collapsible_rules.insert("AliasedExpression");
	collapsible_rules.insert("AllRowsEmptyMatches");
	collapsible_rules.insert("AlterColumnEntry");
	collapsible_rules.insert("AlterOptions");
	collapsible_rules.insert("AlterSchemaOptions");
	collapsible_rules.insert("AlterSequenceOptions");
	collapsible_rules.insert("AlterTableOptions");
	collapsible_rules.insert("AnyOrAll");
	collapsible_rules.insert("ApproxOrExact");
	collapsible_rules.insert("ArrayBounds");
	collapsible_rules.insert("BaseTableName");
	collapsible_rules.insert("BetweenLikeOpExpression");
	collapsible_rules.insert("ByNameOrPosition");
	collapsible_rules.insert("BySourceOrTarget");
	collapsible_rules.insert("CTEBody");
	collapsible_rules.insert("CastOrTryCast");
	collapsible_rules.insert("ColumnConstraint");
	collapsible_rules.insert("ColumnConstraintElement");
	collapsible_rules.insert("CommentOnType");
	collapsible_rules.insert("CommentTarget");
	collapsible_rules.insert("CommentValue");
	collapsible_rules.insert("ComparisonOperand");
	collapsible_rules.insert("ComparisonOperator");
	collapsible_rules.insert("ConstraintTiming");
	collapsible_rules.insert("CopyFileName");
	collapsible_rules.insert("CopyFileNameExpression");
	collapsible_rules.insert("CopyFileNameSuffix");
	collapsible_rules.insert("CopyFromDatabase");
	collapsible_rules.insert("CopyGenericOption");
	collapsible_rules.insert("CopyOptionList");
	collapsible_rules.insert("CopyVariations");
	collapsible_rules.insert("CreateStatementVariation");
	collapsible_rules.insert("CreateTableColumnElement");
	collapsible_rules.insert("CreateTableDefinition");
	collapsible_rules.insert("CreateType");
	collapsible_rules.insert("DecimalNumericType");
	collapsible_rules.insert("DefArg");
	collapsible_rules.insert("DescOrAsc");
	collapsible_rules.insert("DescribeOrSummarize");
	collapsible_rules.insert("DescribeRule");
	collapsible_rules.insert("DescribeTarget");
	collapsible_rules.insert("DistanceOrSimilarity");
	collapsible_rules.insert("DistinctClause");
	collapsible_rules.insert("DistinctOrAll");
	collapsible_rules.insert("DotOperator");
	collapsible_rules.insert("DropBehavior");
	collapsible_rules.insert("DropEntries");
	collapsible_rules.insert("ExcludeName");
	collapsible_rules.insert("ExcludeNames");
	collapsible_rules.insert("ExplainableStatements");
	collapsible_rules.insert("ExportOptions");
	collapsible_rules.insert("ExtensionRepositoryStatement");
	collapsible_rules.insert("ExternalResourceCreationOptions");
	collapsible_rules.insert("ExternalResourceSource");
	collapsible_rules.insert("ExternalResourceStatement");
	collapsible_rules.insert("ExtractArgument");
	collapsible_rules.insert("ExtractDatePart");
	collapsible_rules.insert("FetchClause");
	collapsible_rules.insert("FetchClauseWithValue");
	collapsible_rules.insert("ForEachClause");
	collapsible_rules.insert("FrameBound");
	collapsible_rules.insert("FrameExtent");
	collapsible_rules.insert("FromOrTo");
	collapsible_rules.insert("FromSource");
	collapsible_rules.insert("FunctionArgument");
	collapsible_rules.insert("FunctionIdentifier");
	collapsible_rules.insert("FunctionTypeMacro");
	collapsible_rules.insert("Generated");
	collapsible_rules.insert("GeneratedColumnType");
	collapsible_rules.insert("GenericCopyOptionValue");
	collapsible_rules.insert("GroupByExpression");
	collapsible_rules.insert("GroupByExpressions");
	collapsible_rules.insert("GroupingOrGroupingId");
	collapsible_rules.insert("IgnoreOrRespectNulls");
	collapsible_rules.insert("InExpression");
	collapsible_rules.insert("IncludeOrExcludeNulls");
	collapsible_rules.insert("Indirection");
	collapsible_rules.insert("InnerTableRef");
	collapsible_rules.insert("InsertByNameOrder");
	collapsible_rules.insert("InsertByPositionOrder");
	collapsible_rules.insert("InsertMatchInfo");
	collapsible_rules.insert("InsertValues");
	collapsible_rules.insert("Interval");
	collapsible_rules.insert("IntervalInterval");
	collapsible_rules.insert("IntervalParameter");
	collapsible_rules.insert("IntervalToInterval");
	collapsible_rules.insert("IntervalType");
	collapsible_rules.insert("IntervalWithSpecifier");
	collapsible_rules.insert("IsExpressionTail");
	collapsible_rules.insert("IsLiteralValue");
	collapsible_rules.insert("IsTest");
	collapsible_rules.insert("JoinClause");
	collapsible_rules.insert("JoinOrPivot");
	collapsible_rules.insert("JoinPrefix");
	collapsible_rules.insert("JoinQualifier");
	collapsible_rules.insert("JoinType");
	collapsible_rules.insert("KeyActions");
	collapsible_rules.insert("LimitOffset");
	collapsible_rules.insert("LimitValue");
	collapsible_rules.insert("ListExpression");
	collapsible_rules.insert("MacroDefinitionBody");
	collapsible_rules.insert("MacroOrFunction");
	collapsible_rules.insert("MacroParameter");
	collapsible_rules.insert("MatchRecognizeClause");
	collapsible_rules.insert("MatchedClauseAction");
	collapsible_rules.insert("MeasureSemantics");
	collapsible_rules.insert("MergeMatch");
	collapsible_rules.insert("NearestBareTableRef");
	collapsible_rules.insert("NearestJoinClause");
	collapsible_rules.insert("NotNull");
	collapsible_rules.insert("NullsFirstOrLast");
	collapsible_rules.insert("NumericType");
	collapsible_rules.insert("OnConflictAction");
	collapsible_rules.insert("OnConflictTarget");
	collapsible_rules.insert("OptionalParensNameList");
	collapsible_rules.insert("OptionalParensSimpleSelect");
	collapsible_rules.insert("OrAction");
	collapsible_rules.insert("OrderByExpressions");
	collapsible_rules.insert("OtherOperator");
	collapsible_rules.insert("OverlayArguments");
	collapsible_rules.insert("Parameter");
	collapsible_rules.insert("PartitionByColumnList");
	collapsible_rules.insert("PartitionSortedOptions");
	collapsible_rules.insert("PivotColumnEntry");
	collapsible_rules.insert("PivotValueTarget");
	collapsible_rules.insert("PragmaAssignOrFunction");
	collapsible_rules.insert("PrecedingOrFollowing");
	collapsible_rules.insert("PreserveOrDelete");
	collapsible_rules.insert("QualifiedIndexName");
	collapsible_rules.insert("QualifiedName");
	collapsible_rules.insert("QualifiedTableName");
	collapsible_rules.insert("QualifiedTypeName");
	collapsible_rules.insert("QuantifierBounds");
	collapsible_rules.insert("QuantifierRange");
	collapsible_rules.insert("ReadOnlyOrReadWrite");
	collapsible_rules.insert("ReferencingItem");
	collapsible_rules.insert("RelOptionOrOids");
	collapsible_rules.insert("RenameEntries");
	collapsible_rules.insert("ReplaceEntries");
	collapsible_rules.insert("ResetTarget");
	collapsible_rules.insert("RowPatternAlternative");
	collapsible_rules.insert("RowPatternAnchor");
	collapsible_rules.insert("RowPatternPrimary");
	collapsible_rules.insert("RowPatternQuantifierKind");
	collapsible_rules.insert("RowsPerMatch");
	collapsible_rules.insert("SampleEntry");
	collapsible_rules.insert("SampleUnit");
	collapsible_rules.insert("SampleValue");
	collapsible_rules.insert("SchemaOrData");
	collapsible_rules.insert("SelectAtom");
	collapsible_rules.insert("SelectFrom");
	collapsible_rules.insert("SelectStatementType");
	collapsible_rules.insert("SeqSetCycle");
	collapsible_rules.insert("SequenceOption");
	collapsible_rules.insert("SessionTarget");
	collapsible_rules.insert("SetAssignmentOrTimeZone");
	collapsible_rules.insert("SetVariableOrSetting");
	collapsible_rules.insert("SetopType");
	collapsible_rules.insert("SettingScope");
	collapsible_rules.insert("ShowOrDescribe");
	collapsible_rules.insert("SimpleType");
	collapsible_rules.insert("SingleExpression");
	collapsible_rules.insert("SingleOption");
	collapsible_rules.insert("SliceUpperBound");
	collapsible_rules.insert("SpecialFunctionExpression");
	collapsible_rules.insert("SpecializedOption");
	collapsible_rules.insert("SubstringArguments");
	collapsible_rules.insert("SubstringFromFor");
	collapsible_rules.insert("TableAlias");
	collapsible_rules.insert("TableFunction");
	collapsible_rules.insert("TableOrView");
	collapsible_rules.insert("Temporary");
	collapsible_rules.insert("TimeOrTimestamp");
	collapsible_rules.insert("TopLevelConstraintList");
	collapsible_rules.insert("TransactionStatement");
	collapsible_rules.insert("TriggerBody");
	collapsible_rules.insert("TriggerEvent");
	collapsible_rules.insert("TriggerTiming");
	collapsible_rules.insert("TruncateOptions");
	collapsible_rules.insert("TypeVariations");
	collapsible_rules.insert("UnaryExpression");
	collapsible_rules.insert("UnpivotHeader");
	collapsible_rules.insert("UpdateMatchInfo");
	collapsible_rules.insert("UpdateMatchSetClause");
	collapsible_rules.insert("UpdateSetClause");
	collapsible_rules.insert("UpdateTarget");
	collapsible_rules.insert("UseTarget");
	collapsible_rules.insert("VacuumOptions");
	collapsible_rules.insert("WindowExcludeElement");
	collapsible_rules.insert("WindowFrame");
	collapsible_rules.insert("WindowFrameDefinition");
	collapsible_rules.insert("WithData");
	collapsible_rules.insert("WithOrWithout");
	collapsible_rules.insert("WithOrWithoutOids");
	collapsible_rules.insert("ZoneValue");
	//===--------------------------------------------------------------------===//
	// END GENERATED COLLAPSIBLE RULES
	//===--------------------------------------------------------------------===//

	for (auto &entry : GeneratedTransformFrameOps()) {
		auto process_info = entry.second;
		grammar.SetTransformProcess(entry.first, GeneratedTransformFactory {*process_info},
		                            collapsible_rules.count(entry.first) > 0);
	}
	//===--------------------------------------------------------------------===//
	// START GENERATED EXPRESSION DEPTH RULES
	//===--------------------------------------------------------------------===//
	grammar.SetExpressionDepth("SingleExpression", ExpressionDepthKind::NESTING);
	grammar.SetExpressionDepth("Type", ExpressionDepthKind::NESTING);
	grammar.SetExpressionDepth("NestedType", ExpressionDepthKind::NESTING);
	grammar.SetExpressionDepth("SubqueryReference", ExpressionDepthKind::NESTING);
	grammar.SetExpressionDepth("CustomPrefixExpression", ExpressionDepthKind::NESTING);
	grammar.SetExpressionDepth("LambdaArrowExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("NotExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("IsExpressionContinuation", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("IsDistinctFromExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("ComparisonExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("BetweenInLikeExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("InfixOtherOperatorExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("BitwiseExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("TildeExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("AdditiveExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("MultiplicativeExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("ExponentiationExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("CollateExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("AtTimeZoneExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("PrefixExpression", ExpressionDepthKind::CHAIN);
	grammar.SetExpressionDepth("IndirectionList", ExpressionDepthKind::CHAIN);
	//===--------------------------------------------------------------------===//
	// END GENERATED EXPRESSION DEPTH RULES
	//===--------------------------------------------------------------------===//
}

void PEGTransformerFactory::RegisterDefaultTransforms(ParsedGrammar &grammar) {
	PEGTransformerFactory factory(grammar);
}

template <class FUNC>
static idx_t VisitParseResultsFromList(ParseResult &parse_result, FUNC &&func) {
	// List(D) <- D (',' D)* ','?; validate every child before returning, including when only counting.
	auto &list_pr = parse_result.Cast<ListParseResult>();
	func(list_pr.GetChild(0));
	idx_t count = 1;
	auto &opt_child = list_pr.Child<OptionalParseResult>(1);
	if (opt_child.HasResult()) {
		auto &repeat_result = opt_child.GetResult().Cast<RepeatParseResult>();
		for (auto &child : repeat_result.GetChildren()) {
			auto &list_child = child.get().Cast<ListParseResult>();
			func(list_child.GetChild(1));
			count++;
		}
	}
	return count;
}

ParseResultListItems PEGTransformerFactory::ExtractParseResultsFromList(ParseResult &parse_result) {
	return ParseResultListItems(parse_result);
}

idx_t PEGTransformerFactory::CountParseResultsFromList(ParseResult &parse_result) {
	return VisitParseResultsFromList(parse_result, [](ParseResult &) {});
}

ParseResult &PEGTransformerFactory::ExtractResultFromParens(ParseResult &parse_result) {
	// Parens(D) <- '(' D ')'
	auto &list_pr = parse_result.Cast<ListParseResult>();
	return list_pr.GetChild(1);
}

bool PEGTransformerFactory::ExpressionIsEmptyStar(const ParsedExpression &expr) {
	if (expr.GetExpressionClass() != ExpressionClass::STAR) {
		return false;
	}
	auto &star = expr.Cast<StarExpression>();
	if (star.IsColumns()) {
		return false;
	}
	if (!star.ExcludeList().empty()) {
		return false;
	}
	if (!star.ReplaceList().empty()) {
		return false;
	}
	if (!star.RenameList().empty()) {
		return false;
	}
	return true;
}

QualifiedName PEGTransformerFactory::StringToQualifiedName(vector<string> input) {
	if (input.empty()) {
		throw InternalException("QualifiedName cannot be made with an empty input.");
	}
	if (input.size() == 1) {
		return QualifiedName(Identifier(input[0]));
	} else if (input.size() == 2) {
		return QualifiedName({Identifier(input[0])}, Identifier(input[1]));
	} else if (input.size() == 3) {
		return QualifiedName(Identifier(input[0]), Identifier(input[1]), Identifier(input[2]));
	} else {
		throw ParserException("Too many qualifications found - expected [catalog.schema.name] or [schema.name]");
	}
}

QualifiedColumnName PEGTransformerFactory::StringToQualifiedColumnName(const vector<string> &input) {
	if (input.empty()) {
		throw InternalException("QualifiedColumnName cannot be made with an empty input.");
	}
	auto identifiers = StringsToIdentifiers(input);
	if (identifiers.size() == 1) {
		return QualifiedColumnName(std::move(identifiers[0]));
	} else if (identifiers.size() == 2) {
		return QualifiedColumnName(std::move(identifiers[0]), std::move(identifiers[1]));
	} else if (identifiers.size() == 3) {
		QualifiedColumnName result;
		result.schema = std::move(identifiers[0]);
		result.table = std::move(identifiers[1]);
		result.column = std::move(identifiers[2]);
		return result;
	} else if (identifiers.size() == 4) {
		QualifiedColumnName result;
		result.catalog = std::move(identifiers[0]);
		result.schema = std::move(identifiers[1]);
		result.table = std::move(identifiers[2]);
		result.column = std::move(identifiers[3]);
		return result;
	}
	throw ParserException("Expected at most 4 entries (catalog.schema.table.column), but found %zu entries (input: %s)",
	                      input.size(), StringUtil::Join(input, "."));
}

LogicalType PEGTransformerFactory::GetIntervalTargetType(DatePartSpecifier date_part) {
	switch (date_part) {
	case DatePartSpecifier::YEAR:
	case DatePartSpecifier::MONTH:
	case DatePartSpecifier::DAY:
	case DatePartSpecifier::WEEK:
	case DatePartSpecifier::QUARTER:
	case DatePartSpecifier::DECADE:
	case DatePartSpecifier::CENTURY:
	case DatePartSpecifier::MILLENNIUM:
		return LogicalType::INTEGER;
	case DatePartSpecifier::HOUR:
	case DatePartSpecifier::MINUTE:
	case DatePartSpecifier::MICROSECONDS:
		return LogicalType::BIGINT;
	case DatePartSpecifier::MILLISECONDS:
	case DatePartSpecifier::SECOND:
		return LogicalType::DOUBLE;
	default:
		throw InternalException("Unsupported interval post-fix");
	}
}

LogicalType PEGTransformerFactory::ApplyColumnCollation(const optional<LogicalType> &type,
                                                        unique_ptr<ParsedExpression> collation) {
	if (!type) {
		throw ParserException("Specify the VARCHAR type to set a collation");
	}
	if (!type->IsUnbound()) {
		throw InternalException("Expected only unbound types here");
	}
	auto &expr = UnboundType::GetTypeExpression(*type);
	if (expr->GetExpressionClass() != ExpressionClass::TYPE) {
		throw InternalException("Expected a type expression");
	}
	auto &type_expr = expr->Cast<TypeExpression>();
	if (DefaultTypeGenerator::GetDefaultType(type_expr.GetTypeName()) != LogicalTypeId::VARCHAR) {
		throw ParserException("Only VARCHAR columns can have collations!");
	}
	vector<unique_ptr<ParsedExpression>> type_children;
	type_children.push_back(std::move(collation));
	return LogicalType::UNBOUND(make_uniq<TypeExpression>(Identifier("VARCHAR"), std::move(type_children)));
}

} // namespace duckdb
