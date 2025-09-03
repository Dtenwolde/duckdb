#include "transformer/peg_transformer.hpp"
#include "matcher.hpp"
#include "chrono"
#include "duckdb/common/to_string.hpp"
#include "duckdb/common/enums/date_part_specifier.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformStatement(PEGTransformer &transformer,
                                                                   optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	return transformer.Transform<unique_ptr<SQLStatement>>(choice_pr.result);
}

unique_ptr<SQLStatement> PEGTransformerFactory::Transform(vector<MatcherToken> &tokens, const char *root_rule) {
	string token_stream;
	for (auto &token : tokens) {
		token_stream += token.text + " ";
	}

	vector<MatcherSuggestion> suggestions;
	ParseResultAllocator parse_result_allocator;
	MatchState state(tokens, suggestions, parse_result_allocator);
	MatcherAllocator allocator;
	auto &matcher = Matcher::RootMatcher(allocator);
	// --- TIMING START ---
	// auto start_time = std::chrono::high_resolution_clock::now();
	auto match_result = matcher.MatchParseResult(state);
	// auto end_time = std::chrono::high_resolution_clock::now();
	// --- TIMING END ---
	// auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

	if (match_result == nullptr || state.token_index < state.tokens.size()) {
		// TODO(dtenwolde) add error handling
		string token_list;
		for (idx_t i = 0; i < tokens.size(); i++) {
			if (!token_list.empty()) {
				token_list += "\n";
			}
			if (i < 10) {
				token_list += " ";
			}
			token_list += to_string(i) + ":" + tokens[i].text;
		}
		throw ParserException("Failed to parse query - did not consume all tokens (got to token %d - %s)\nTokens:\n%s",
		                      state.token_index, tokens[state.token_index].text, token_list);
	}

	Printer::Print(match_result->ToString());
	// Printer::PrintF("Parsing took: %lld µs\n", duration.count());

	match_result->name = "Statement";
	ArenaAllocator transformer_allocator(Allocator::DefaultAllocator());
	PEGTransformerState transformer_state(tokens);
	PEGTransformer transformer(transformer_allocator, transformer_state, sql_transform_functions, parser.rules,
	                           enum_mappings);
	return transformer.Transform<unique_ptr<SQLStatement>>(match_result);
}

#define REGISTER_TRANSFORM(FUNCTION) Register(string(#FUNCTION).substr(9), &FUNCTION)

PEGTransformerFactory::PEGTransformerFactory() {
	// Registering transform functions using the macro for brevity
	REGISTER_TRANSFORM(TransformStatement);
	REGISTER_TRANSFORM(TransformUseStatement);

	REGISTER_TRANSFORM(TransformSetStatement);
	REGISTER_TRANSFORM(TransformResetStatement);
	REGISTER_TRANSFORM(TransformDeleteStatement);
	REGISTER_TRANSFORM(TransformPragmaStatement);
	REGISTER_TRANSFORM(TransformPragmaAssign);
	REGISTER_TRANSFORM(TransformPragmaFunction);
	REGISTER_TRANSFORM(TransformPragmaParameters);
	REGISTER_TRANSFORM(TransformUseTarget);

	REGISTER_TRANSFORM(TransformCopyStatement);
	REGISTER_TRANSFORM(TransformCopyTable);
	REGISTER_TRANSFORM(TransformCopySelect);
	REGISTER_TRANSFORM(TransformFromOrTo);
	REGISTER_TRANSFORM(TransformCopyFileName);
	REGISTER_TRANSFORM(TransformIdentifierColId);
	REGISTER_TRANSFORM(TransformCopyOptions);

	REGISTER_TRANSFORM(TransformSelectStatement);
	REGISTER_TRANSFORM(TransformSelectOrParens);
	REGISTER_TRANSFORM(TransformSelectParens);
	REGISTER_TRANSFORM(TransformBaseSelect);
	REGISTER_TRANSFORM(TransformSelectStatementType);
	REGISTER_TRANSFORM(TransformOptionalParensSimpleSelect);
	REGISTER_TRANSFORM(TransformSimpleSelectParens);
	REGISTER_TRANSFORM(TransformSimpleSelect);
	REGISTER_TRANSFORM(TransformSelectFrom);
	REGISTER_TRANSFORM(TransformSelectFromClause);
	REGISTER_TRANSFORM(TransformFromSelectClause);

	REGISTER_TRANSFORM(TransformTableAlias);
	REGISTER_TRANSFORM(TransformColumnAliases);

	REGISTER_TRANSFORM(TransformFromClause);
	REGISTER_TRANSFORM(TransformTableRef);
	REGISTER_TRANSFORM(TransformInnerTableRef);
	REGISTER_TRANSFORM(TransformValuesRef);
	REGISTER_TRANSFORM(TransformValuesClause);
	REGISTER_TRANSFORM(TransformValuesExpressions);

	REGISTER_TRANSFORM(TransformSelectClause);
	REGISTER_TRANSFORM(TransformTargetList);
	REGISTER_TRANSFORM(TransformAliasedExpression);
	REGISTER_TRANSFORM(TransformExpressionAsCollabel);
	REGISTER_TRANSFORM(TransformColIdExpression);
	REGISTER_TRANSFORM(TransformExpressionOptIdentifier);

	REGISTER_TRANSFORM(TransformDetachStatement);
	REGISTER_TRANSFORM(TransformAttachStatement);
	REGISTER_TRANSFORM(TransformAttachAlias);
	REGISTER_TRANSFORM(TransformAttachOptions);
	REGISTER_TRANSFORM(TransformGenericCopyOptionList);
	REGISTER_TRANSFORM(TransformGenericCopyOption);
	REGISTER_TRANSFORM(TransformGenericCopyOptionListParens);
	REGISTER_TRANSFORM(TransformSpecializedOptionList);
	REGISTER_TRANSFORM(TransformSpecializedOption);
	REGISTER_TRANSFORM(TransformSingleOption);
	REGISTER_TRANSFORM(TransformEncodingOption);
	REGISTER_TRANSFORM(TransformForceQuoteOption);
	REGISTER_TRANSFORM(TransformCopyFromDatabase);
	REGISTER_TRANSFORM(TransformCopyDatabaseFlag);

	REGISTER_TRANSFORM(TransformInsertColumnList);
	REGISTER_TRANSFORM(TransformColumnList);

	REGISTER_TRANSFORM(TransformCommentStatement);
	REGISTER_TRANSFORM(TransformCommentOnType);
	REGISTER_TRANSFORM(TransformCommentValue);

	REGISTER_TRANSFORM(TransformCreateStatement);
	REGISTER_TRANSFORM(TransformCreateStatementVariation);
	REGISTER_TRANSFORM(TransformCreateTableStmt);
	REGISTER_TRANSFORM(TransformCreateColumnList);
	REGISTER_TRANSFORM(TransformCreateTableColumnList);
	REGISTER_TRANSFORM(TransformTemporary);

	REGISTER_TRANSFORM(TransformCreateViewStmt);

	REGISTER_TRANSFORM(TransformCreateTypeStmt);
	REGISTER_TRANSFORM(TransformCreateType);
	REGISTER_TRANSFORM(TransformEnumStringLiteralList);

	REGISTER_TRANSFORM(TransformCreateSecretStmt);
	REGISTER_TRANSFORM(TransformSecretStorageSpecifier);

	REGISTER_TRANSFORM(TransformCreateSchemaStmt);

	REGISTER_TRANSFORM(TransformCreateSequenceStmt);
	REGISTER_TRANSFORM(TransformSequenceOption);
	REGISTER_TRANSFORM(TransformSeqSetCycle);
	REGISTER_TRANSFORM(TransformSeqSetIncrement);
	REGISTER_TRANSFORM(TransformSeqSetMinMax);
	REGISTER_TRANSFORM(TransformNoMinMax);
	REGISTER_TRANSFORM(TransformSeqStartWith);
	REGISTER_TRANSFORM(TransformSeqOwnedBy);

	REGISTER_TRANSFORM(TransformColumnDefinition);
	REGISTER_TRANSFORM(TransformTypeOrGenerated);
	REGISTER_TRANSFORM(TransformType);
	REGISTER_TRANSFORM(TransformNumericType);
	REGISTER_TRANSFORM(TransformSimpleNumericType);
	REGISTER_TRANSFORM(TransformDecimalNumericType);
	REGISTER_TRANSFORM(TransformFloatType);
	REGISTER_TRANSFORM(TransformDecimalType);
	Register("DecType", &TransformDecimalType);
	Register("NumericModType", &TransformDecimalType);

	REGISTER_TRANSFORM(TransformArrayBounds);
	REGISTER_TRANSFORM(TransformArrayKeyword);
	REGISTER_TRANSFORM(TransformSquareBracketsArray);

	REGISTER_TRANSFORM(TransformTimeType);
	REGISTER_TRANSFORM(TransformTimeOrTimestamp);
	REGISTER_TRANSFORM(TransformTimeZone);
	REGISTER_TRANSFORM(TransformWithOrWithout);

	REGISTER_TRANSFORM(TransformSimpleType);
	REGISTER_TRANSFORM(TransformQualifiedTypeName);
	REGISTER_TRANSFORM(TransformCharacterType);
	REGISTER_TRANSFORM(TransformBitType);
	REGISTER_TRANSFORM(TransformRowType);
	REGISTER_TRANSFORM(TransformMapType);
	REGISTER_TRANSFORM(TransformUnionType);
	REGISTER_TRANSFORM(TransformIntervalType);
	REGISTER_TRANSFORM(TransformIntervalInterval);
	REGISTER_TRANSFORM(TransformInterval);
	REGISTER_TRANSFORM(TransformColIdTypeList);
	REGISTER_TRANSFORM(TransformColIdType);


	REGISTER_TRANSFORM(TransformTypeModifiers);

	REGISTER_TRANSFORM(TransformTopLevelConstraint);
	REGISTER_TRANSFORM(TransformTopLevelConstraintList);
	REGISTER_TRANSFORM(TransformTopPrimaryKeyConstraint);
	REGISTER_TRANSFORM(TransformTopUniqueConstraint);
	REGISTER_TRANSFORM(TransformCheckConstraint);
	REGISTER_TRANSFORM(TransformTopForeignKeyConstraint);

	REGISTER_TRANSFORM(TransformColumnIdList);

	REGISTER_TRANSFORM(TransformCheckpointStatement);
	REGISTER_TRANSFORM(TransformExportStatement);
	REGISTER_TRANSFORM(TransformImportStatement);
	REGISTER_TRANSFORM(TransformExportSource);

	REGISTER_TRANSFORM(TransformTransactionStatement);
	REGISTER_TRANSFORM(TransformBeginTransaction);
	REGISTER_TRANSFORM(TransformRollbackTransaction);

	REGISTER_TRANSFORM(TransformCommitTransaction);
	REGISTER_TRANSFORM(TransformReadOrWrite);

	REGISTER_TRANSFORM(TransformLoadStatement);
	REGISTER_TRANSFORM(TransformInstallStatement);
	REGISTER_TRANSFORM(TransformFromSource);
	REGISTER_TRANSFORM(TransformVersionNumber);

	REGISTER_TRANSFORM(TransformDeallocateStatement);

	REGISTER_TRANSFORM(TransformCallStatement);
	REGISTER_TRANSFORM(TransformTableFunctionArguments);
	REGISTER_TRANSFORM(TransformFunctionArgument);
	REGISTER_TRANSFORM(TransformNamedParameter);

	REGISTER_TRANSFORM(TransformTruncateStatement);
	REGISTER_TRANSFORM(TransformBaseTableName);
	REGISTER_TRANSFORM(TransformSchemaReservedTable);
	REGISTER_TRANSFORM(TransformCatalogReservedSchemaTable);
	REGISTER_TRANSFORM(TransformSchemaQualification);
	REGISTER_TRANSFORM(TransformCatalogQualification);
	REGISTER_TRANSFORM(TransformQualifiedName);
	REGISTER_TRANSFORM(TransformSchemaReservedIdentifierOrStringLiteral);
	REGISTER_TRANSFORM(TransformReservedIdentifierOrStringLiteral);
	REGISTER_TRANSFORM(TransformCatalogReservedSchemaIdentifierOrStringLiteral);
	REGISTER_TRANSFORM(TransformTableNameIdentifierOrStringLiteral);

	REGISTER_TRANSFORM(TransformStandardAssignment);
	REGISTER_TRANSFORM(TransformSetAssignment);
	REGISTER_TRANSFORM(TransformSettingOrVariable);
	REGISTER_TRANSFORM(TransformVariableList);

	REGISTER_TRANSFORM(TransformExpression);
	REGISTER_TRANSFORM(TransformBaseExpression);
	REGISTER_TRANSFORM(TransformRecursiveExpression);
	REGISTER_TRANSFORM(TransformSingleExpression);
	REGISTER_TRANSFORM(TransformParenthesisExpression);
	REGISTER_TRANSFORM(TransformLiteralExpression);
	REGISTER_TRANSFORM(TransformPrefixExpression);
	REGISTER_TRANSFORM(TransformPrefixOperator);
	REGISTER_TRANSFORM(TransformColumnReference);
	REGISTER_TRANSFORM(TransformColIdOrString);
	REGISTER_TRANSFORM(TransformIdentifierOrStringLiteral);
	REGISTER_TRANSFORM(TransformColId);
	REGISTER_TRANSFORM(TransformStringLiteral);
	REGISTER_TRANSFORM(TransformIdentifier);
	REGISTER_TRANSFORM(TransformSetSetting);
	REGISTER_TRANSFORM(TransformSetVariable);
	REGISTER_TRANSFORM(TransformDottedIdentifier);

	REGISTER_TRANSFORM(TransformOperator);

	// Manual registration for mismatched names or special cases
	Register("PragmaName", &TransformIdentifierOrKeyword);
	Register("TypeName", &TransformIdentifierOrKeyword);
	Register("ColLabel", &TransformIdentifierOrKeyword);
	Register("PlainIdentifier", &TransformIdentifierOrKeyword);
	Register("QuotedIdentifier", &TransformIdentifierOrKeyword);
	Register("ReservedKeyword", &TransformIdentifierOrKeyword);
	Register("UnreservedKeyword", &TransformIdentifierOrKeyword);
	Register("ColumnNameKeyword", &TransformIdentifierOrKeyword);
	Register("FuncNameKeyword", &TransformIdentifierOrKeyword);
	Register("TypeNameKeyword", &TransformIdentifierOrKeyword);
	Register("SettingName", &TransformIdentifierOrKeyword);

	Register("ReservedSchemaQualification", &TransformSchemaQualification);

	// Enum registration
	RegisterEnum<SetScope>("LocalScope", SetScope::LOCAL);
	RegisterEnum<SetScope>("GlobalScope", SetScope::GLOBAL);
	RegisterEnum<SetScope>("SessionScope", SetScope::SESSION);
	RegisterEnum<SetScope>("VariableScope", SetScope::VARIABLE);

	RegisterEnum<Value>("FalseLiteral", Value(false));
	RegisterEnum<Value>("TrueLiteral", Value(true));
	RegisterEnum<Value>("NullLiteral", Value());

	RegisterEnum<GenericCopyOption>("BinaryOption", GenericCopyOption("format", Value("binary")));
	RegisterEnum<GenericCopyOption>("FreezeOption", GenericCopyOption("freeze", Value()));
	RegisterEnum<GenericCopyOption>("OidsOption", GenericCopyOption("oids", Value()));
	RegisterEnum<GenericCopyOption>("CsvOption", GenericCopyOption("format", Value("csv")));
	RegisterEnum<GenericCopyOption>("HeaderOption", GenericCopyOption("header", Value()));

	RegisterEnum<TransactionModifierType>("ReadOnly", TransactionModifierType::TRANSACTION_READ_ONLY);
	RegisterEnum<TransactionModifierType>("ReadWrite", TransactionModifierType::TRANSACTION_READ_WRITE);

	RegisterEnum<CopyDatabaseType>("CopySchema", CopyDatabaseType::COPY_SCHEMA);
	RegisterEnum<CopyDatabaseType>("CopyData", CopyDatabaseType::COPY_DATA);

	RegisterEnum<LogicalType>("IntType", LogicalType(LogicalTypeId::INTEGER));
	RegisterEnum<LogicalType>("IntegerType", LogicalType(LogicalTypeId::INTEGER));
	RegisterEnum<LogicalType>("SmallintType", LogicalType(LogicalTypeId::SMALLINT));
	RegisterEnum<LogicalType>("BigintType", LogicalType(LogicalTypeId::BIGINT));
	RegisterEnum<LogicalType>("RealType", LogicalType(LogicalTypeId::FLOAT));
	RegisterEnum<LogicalType>("DoubleType", LogicalType(LogicalTypeId::DOUBLE));
	RegisterEnum<LogicalType>("BooleanType", LogicalType(LogicalTypeId::BOOLEAN));

	RegisterEnum<DatePartSpecifier>("YearKeyword", DatePartSpecifier::YEAR);
	RegisterEnum<DatePartSpecifier>("MonthKeyword", DatePartSpecifier::MONTH);

	RegisterEnum<LogicalTypeId>("TimeTypeId", LogicalTypeId::TIME);
	RegisterEnum<LogicalTypeId>("TimestampTypeId", LogicalTypeId::TIMESTAMP);
	RegisterEnum<bool>("WithRule", true);
	RegisterEnum<bool>("WithoutRule", false);

	RegisterEnum<PersistType>("TempPersistent", PersistType::TEMPORARY);
	RegisterEnum<PersistType>("TemporaryPersistent", PersistType::TEMPORARY);
	RegisterEnum<PersistType>("Persistent", PersistType::PERSISTENT);

	RegisterEnum<CatalogType>("CommentTable", CatalogType::TABLE_ENTRY);
	RegisterEnum<CatalogType>("CommentSequence", CatalogType::SEQUENCE_ENTRY);
	RegisterEnum<CatalogType>("CommentFunction", CatalogType::MACRO_ENTRY);
	RegisterEnum<CatalogType>("CommentMacroTable", CatalogType::TABLE_MACRO_ENTRY);
	RegisterEnum<CatalogType>("CommentMacro", CatalogType::MACRO_ENTRY);
	RegisterEnum<CatalogType>("CommentView", CatalogType::VIEW_ENTRY);
	RegisterEnum<CatalogType>("CommentDatabase", CatalogType::DATABASE_ENTRY);
	RegisterEnum<CatalogType>("CommentIndex", CatalogType::INDEX_ENTRY);
	RegisterEnum<CatalogType>("CommentSchema", CatalogType::SCHEMA_ENTRY);
	RegisterEnum<CatalogType>("CommentType", CatalogType::TYPE_ENTRY);
	RegisterEnum<CatalogType>("CommentColumn", CatalogType::INVALID);

	RegisterEnum<string>("MinValue", "minvalue");
	RegisterEnum<string>("MaxValue", "maxvalue");

	RegisterEnum<string>("NotPrefixOperator", "NOT");
	RegisterEnum<string>("MinusPrefixOperator", "-");
	RegisterEnum<string>("PlusPrefixOperator", "+");
	RegisterEnum<string>("TildePrefixOperator", "~");
	}

optional_ptr<ParseResult> PEGTransformerFactory::ExtractResultFromParens(optional_ptr<ParseResult> parse_result) {
	// Parens(D) <- '(' D ')'
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return list_pr.children[1];
}

vector<optional_ptr<ParseResult>>
PEGTransformerFactory::ExtractParseResultsFromList(optional_ptr<ParseResult> parse_result) {
	// List(D) <- D (',' D)* ','?
	vector<optional_ptr<ParseResult>> result;
	auto &list_pr = parse_result->Cast<ListParseResult>();
	result.push_back(list_pr.children[0]);
	auto opt_child = list_pr.Child<OptionalParseResult>(1);
	if (opt_child.HasResult()) {
		auto repeat_result = opt_child.optional_result->Cast<RepeatParseResult>();
		for (auto &child : repeat_result.children) {
			auto &list_child = child->Cast<ListParseResult>();
			result.push_back(list_child.children[1]);
		}
	}

	return result;
}

QualifiedName PEGTransformerFactory::StringToQualifiedName(vector<string> input) {
	QualifiedName result;
	if (input.empty()) {
		throw InternalException("QualifiedName cannot be made with an empty input.");
	}
	if (input.size() == 1) {
		result.catalog = INVALID_CATALOG;
		result.schema = INVALID_SCHEMA;
		result.name = input[0];
	} else if (input.size() == 2) {
		result.catalog = INVALID_CATALOG;
		result.schema = input[0];
		result.name = input[1];
	} else if (input.size() == 3) {
		result.catalog = input[0];
		result.schema = input[1];
		result.name = input[2];
	} else {
		throw ParserException("Too many dots found.");
	}
	return result;
}

} // namespace duckdb
