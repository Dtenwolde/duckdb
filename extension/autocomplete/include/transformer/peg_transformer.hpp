#pragma once

#include "tokenizer.hpp"
#include "parse_result.hpp"
#include "transform_enum_result.hpp"
#include "transform_result.hpp"
#include "ast/column_element.hpp"
#include "ast/create_table_as.hpp"
#include "ast/extension_repository_info.hpp"
#include "ast/generic_copy_option.hpp"
#include "ast/insert_values.hpp"
#include "ast/join_prefix.hpp"
#include "ast/join_qualifier.hpp"
#include "ast/limit_percent_result.hpp"
#include "ast/on_conflict_expression_target.hpp"
#include "ast/prepared_parameter.hpp"
#include "ast/sequence_option.hpp"
#include "ast/set_info.hpp"
#include "ast/table_alias.hpp"
#include "ast/update_set_element.hpp"
#include "duckdb/function/macro_function.hpp"
#include "duckdb/parser/expression/case_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/window_expression.hpp"
#include "duckdb/parser/parsed_data/create_type_info.hpp"
#include "duckdb/parser/parsed_data/transaction_info.hpp"
#include "duckdb/parser/statement/copy_database_statement.hpp"
#include "duckdb/parser/statement/set_statement.hpp"
#include "duckdb/parser/statement/create_statement.hpp"
#include "duckdb/parser/tableref/basetableref.hpp"
#include "parser/peg_parser.hpp"
#include "duckdb/storage/arena_allocator.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/statement/drop_statement.hpp"
#include "duckdb/parser/statement/insert_statement.hpp"

namespace duckdb {

// Forward declare
struct QualifiedName;

struct PEGTransformerState {
	explicit PEGTransformerState(const vector<MatcherToken> &tokens_p) : tokens(tokens_p), token_index(0) {
	}

	const vector<MatcherToken> &tokens;
	idx_t token_index;
};

class PEGTransformer {
public:
	using AnyTransformFunction =
	    std::function<unique_ptr<TransformResultValue>(PEGTransformer &, optional_ptr<ParseResult>)>;

	PEGTransformer(ArenaAllocator &allocator, PEGTransformerState &state,
	               const case_insensitive_map_t<AnyTransformFunction> &transform_functions,
	               const case_insensitive_map_t<PEGRule> &grammar_rules,
	               const case_insensitive_map_t<unique_ptr<TransformEnumValue>> &enum_mappings)
	    : allocator(allocator), state(state), grammar_rules(grammar_rules), transform_functions(transform_functions),
	      enum_mappings(enum_mappings) {
	}

public:
	template <typename T>
	T Transform(optional_ptr<ParseResult> parse_result);

	template <typename T>
	T TransformEnum(optional_ptr<ParseResult> parse_result);

	template <typename T>
	void TransformOptional(ListParseResult &list_pr, idx_t child_idx, T &target);

	// Make overloads return raw pointers, as ownership is handled by the ArenaAllocator.
	template <class T, typename... Args>
	T *Make(Args &&...args) {
		return allocator.Make<T>(std::forward<Args>(args)...);
	}

	void ClearParameters();
	static void ParamTypeCheck(PreparedParamType last_type, PreparedParamType new_type);
	void SetParam(const string &name, idx_t index, PreparedParamType type);
	bool GetParam(const string &name, idx_t &index, PreparedParamType type);

public:
	ArenaAllocator &allocator;
	PEGTransformerState &state;
	const case_insensitive_map_t<PEGRule> &grammar_rules;
	const case_insensitive_map_t<AnyTransformFunction> &transform_functions;
	const case_insensitive_map_t<unique_ptr<TransformEnumValue>> &enum_mappings;
	case_insensitive_map_t<idx_t> named_parameter_map;
	idx_t prepared_statement_parameter_index = 0;
	PreparedParamType last_param_type = PreparedParamType::INVALID;

};

class PEGTransformerFactory {
public:
	explicit PEGTransformerFactory();
	unique_ptr<SQLStatement> Transform(vector<MatcherToken> &tokens, const char *root_rule = "Statement");

private:
	template <typename T>
	void RegisterEnum(const string &rule_name, T value) {
		auto existing_rule = enum_mappings.find(rule_name);
		if (existing_rule != enum_mappings.end()) {
			throw InternalException("EnumRule %s already exists", rule_name);
		}
		enum_mappings[rule_name] = make_uniq<TypedTransformEnumResult<T>>(value);
	}

	template <class FUNC>
	void Register(const string &rule_name, FUNC function) {
		auto existing_rule = sql_transform_functions.find(rule_name);
		if (existing_rule != sql_transform_functions.end()) {
			throw InternalException("Rule %s already exists", rule_name);
		}
		sql_transform_functions[rule_name] =
		    [function](PEGTransformer &transformer,
		               optional_ptr<ParseResult> parse_result) -> unique_ptr<TransformResultValue> {
			auto result_value = function(transformer, parse_result);
			return make_uniq<TypedTransformResult<decltype(result_value)>>(std::move(result_value));
		};
	}

	// Register for functions taking one ParseResult child
	template <class RETURN_TYPE, class A, idx_t INDEX_A, class FUNC>
	void Register(const string &rule_name, FUNC function) {
		sql_transform_functions[rule_name] =
		    [function](PEGTransformer &transformer,
		               optional_ptr<ParseResult> parse_result) -> unique_ptr<TransformResultValue> {
			auto &list_pr = parse_result->Cast<ListParseResult>();
			auto &child_a = list_pr.Child<A>(INDEX_A);
			RETURN_TYPE result_value = function(transformer, child_a);
			return make_uniq<TypedTransformResult<RETURN_TYPE>>(std::move(result_value));
		};
	}

	// Register for functions taking two ParseResult children
	template <class RETURN_TYPE, class A, idx_t INDEX_A, class B, idx_t INDEX_B, class FUNC>
	void Register(const string &rule_name, FUNC function) {
		sql_transform_functions[rule_name] =
		    [function](PEGTransformer &transformer,
		               optional_ptr<ParseResult> parse_result) -> unique_ptr<TransformResultValue> {
			auto &list_pr = parse_result->Cast<ListParseResult>();
			auto &child_a = list_pr.Child<A>(INDEX_A);
			auto &child_b = list_pr.Child<B>(INDEX_B);
			RETURN_TYPE result_value = function(transformer, child_a, child_b);
			return make_uniq<TypedTransformResult<RETURN_TYPE>>(std::move(result_value));
		};
	}
	static unique_ptr<SQLStatement> TransformStatement(PEGTransformer &, optional_ptr<ParseResult> list);

	static unique_ptr<SQLStatement> TransformUseStatement(PEGTransformer &, optional_ptr<ParseResult> identifier_pr);
	static unique_ptr<SQLStatement> TransformSetStatement(PEGTransformer &, optional_ptr<ParseResult> choice_pr);
	static unique_ptr<SQLStatement> TransformResetStatement(PEGTransformer &, optional_ptr<ParseResult> choice_pr);
	static vector<string> TransformDottedIdentifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static QualifiedName TransformUseTarget(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformPragmaStatement(PEGTransformer &transformer,
	                                                         optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformPragmaAssign(PEGTransformer &transformer,
	                                                      optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformPragmaFunction(PEGTransformer &transformer,
	                                                        optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformPragmaParameters(PEGTransformer &transformer,
	                                                                      optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformDetachStatement(PEGTransformer &transformer,
	                                                         optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformAttachStatement(PEGTransformer &transformer,
	                                                         optional_ptr<ParseResult> parse_result);
	static string TransformAttachAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unordered_map<string, Value> TransformAttachOptions(PEGTransformer &transformer,
	                                                           optional_ptr<ParseResult> parse_result);
	static unordered_map<string, vector<Value>> TransformGenericCopyOptionList(PEGTransformer &transformer,
	                                                                           optional_ptr<ParseResult> parse_result);
	static GenericCopyOption TransformGenericCopyOption(PEGTransformer &transformer,
	                                                    optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformCheckpointStatement(PEGTransformer &transformer,
	                                                             optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformDeallocateStatement(PEGTransformer &transformer,
	                                                             optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformCallStatement(PEGTransformer &transformer,
	                                                       optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformTableFunctionArguments(PEGTransformer &transformer,
	                                                                            optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformFunctionArgument(PEGTransformer &transformer,
	                                                              optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformNamedParameter(PEGTransformer &transformer,
	                                                            optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformTransactionStatement(PEGTransformer &transformer,
	                                                              optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformBeginTransaction(PEGTransformer &transformer,
	                                                          optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformRollbackTransaction(PEGTransformer &transformer,
	                                                             optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformCommitTransaction(PEGTransformer &transformer,
	                                                           optional_ptr<ParseResult> parse_result);
	static TransactionModifierType TransformReadOrWrite(PEGTransformer &transformer,
	                                                    optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformExportStatement(PEGTransformer &transformer,
	                                                         optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformImportStatement(PEGTransformer &transformer,
	                                                         optional_ptr<ParseResult> parse_result);
	static string TransformExportSource(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static vector<string> TransformInsertColumnList(PEGTransformer &transformer,
	                                                optional_ptr<ParseResult> parse_result);
	static vector<string> TransformColumnList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformLoadStatement(PEGTransformer &transformer,
	                                                       optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformInstallStatement(PEGTransformer &transformer,
	                                                          optional_ptr<ParseResult> parse_result);
	static ExtensionRepositoryInfo TransformFromSource(PEGTransformer &transformer,
	                                                   optional_ptr<ParseResult> parse_result);
	static string TransformVersionNumber(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformTruncateStatement(PEGTransformer &transformer,
	                                                           optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformCommentStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static CatalogType TransformCommentOnType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static Value TransformCommentValue(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformCreateStatement(PEGTransformer &transformer,
	                                                         optional_ptr<ParseResult> parse_result);
	static unique_ptr<CreateStatement> TransformCreateStatementVariation(PEGTransformer &transformer,
	                                                                  optional_ptr<ParseResult> parse_result);
	static unique_ptr<CreateStatement> TransformCreateTableStmt(PEGTransformer &transformer,
	                                                         optional_ptr<ParseResult> parse_result);
	static CreateTableAs TransformCreateTableAs(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static ColumnList TransformIdentifierList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static ColumnElements TransformCreateColumnList(PEGTransformer &transformer,
	                                                optional_ptr<ParseResult> parse_result);
	static ColumnElements TransformCreateTableColumnList(PEGTransformer &transformer,
	                                                     optional_ptr<ParseResult> parse_result);
	static SecretPersistType TransformTemporary(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<CreateStatement> TransformCreateSchemaStmt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<CreateStatement> TransformCreateViewStmt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<CreateStatement> TransformCreateTypeStmt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<CreateTypeInfo> TransformCreateType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformEnumSelectType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static LogicalType TransformEnumStringLiteralList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<CreateStatement> TransformCreateSequenceStmt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSequenceOption(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSeqSetCycle(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSeqSetIncrement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSeqSetMinMax(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformSeqMinOrMax(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static pair<string, unique_ptr<SequenceOption>> TransformNoMinMax(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSeqStartWith(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSeqOwnedBy(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<CreateStatement> TransformCreateMacroStmt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<MacroFunction> TransformMacroDefinition(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<MacroFunction> TransformTableMacroDefinition(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<MacroFunction> TransformScalarMacroDefinition(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformMacroParameters(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformMacroParameter(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformSimpleParameter(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformTypeFuncName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);


	static ColumnDefinition TransformColumnDefinition(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformTypeOrGenerated(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<Constraint> TransformTopLevelConstraint(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<Constraint> TransformTopLevelConstraintList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<Constraint> TransformTopPrimaryKeyConstraint(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<Constraint> TransformTopUniqueConstraint(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<Constraint> TransformCheckConstraint(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<Constraint> TransformTopForeignKeyConstraint(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static int64_t TransformArrayBounds(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static int64_t TransformArrayKeyword(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static int64_t TransformSquareBracketsArray(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static LogicalType TransformTimeType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalTypeId TransformTimeOrTimestamp(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static bool TransformTimeZone(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static bool TransformWithOrWithout(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static LogicalType TransformBitType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformMapType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformRowType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformUnionType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformIntervalType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformIntervalInterval(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformInterval(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static child_list_t<LogicalType> TransformColIdTypeList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static std::pair<std::string, LogicalType> TransformColIdType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<string> TransformColumnIdList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformSelectStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SelectStatement> TransformSelectStatementInternal(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SelectStatement> TransformSelectOrParens(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SelectStatement> TransformSelectParens(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SelectStatement> TransformBaseSelect(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SelectStatement> TransformSelectStatementType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SelectStatement> TransformOptionalParensSimpleSelect(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SelectStatement> TransformSimpleSelectParens(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SelectStatement> TransformSimpleSelect(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<TableRef> TransformFromClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<TableRef> TransformTableRef(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<TableRef> TransformInnerTableRef(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<TableRef> TransformTableFunction(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<TableRef> TransformTableFunctionLateralOpt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<TableRef> TransformTableFunctionAliasColon(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformTableAliasColon(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static QualifiedName TransformQualifiedTableFunction(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<TableRef> TransformTableSubquery(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<TableRef> TransformSubqueryReference(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<ParsedExpression> TransformWhereClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static GroupByNode TransformGroupByClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static GroupByNode TransformGroupByExpressions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static GroupByNode TransformGroupByAll(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static GroupByNode TransformGroupByList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformGroupByExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformEmptyGroupingItem(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformCubeOrRollupClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformGroupingSetsClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<ParsedExpression> TransformParameter(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static PreparedParameter TransformQuestionMarkParameter(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static PreparedParameter TransformNumberedParameter(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static PreparedParameter TransformColLabelParameter(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<ParsedExpression> TransformDefaultExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);


	static unique_ptr<ParsedExpression> TransformHavingClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformQualifyClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static vector<unique_ptr<ResultModifier>> TransformResultModifiers(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ResultModifier> TransformLimitOffsetClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LimitPercentResult TransformLimitClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LimitPercentResult TransformLimitValue(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LimitPercentResult TransformLimitAll(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LimitPercentResult TransformLimitLiteralPercent(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LimitPercentResult TransformLimitExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LimitPercentResult TransformOffsetClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);



	static unique_ptr<TableRef> TransformBaseTableRef(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<TableRef> TransformValuesRef(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SelectStatement> TransformValuesClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformValuesExpressions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SelectStatement> TransformTableStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static TableAlias TransformTableAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<string> TransformColumnAliases(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<AtClause> TransformAtClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<AtClause> TransformAtSpecifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformAtUnit(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<TableRef> TransformJoinOrPivot(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<TableRef> TransformJoinClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<TableRef> TransformRegularJoinClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<TableRef> TransformJoinWithoutOnClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static JoinQualifier TransformJoinQualifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static JoinQualifier TransformOnClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static JoinQualifier TransformUsingClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static JoinType TransformJoinType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static JoinPrefix TransformJoinPrefix(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static JoinPrefix TransformCrossJoinPrefix(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static JoinPrefix TransformNaturalJoinPrefix(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static JoinPrefix TransformPositionalJoinPrefix(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);




	static unique_ptr<SelectNode> TransformSelectFrom(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SelectNode> TransformSelectFromClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SelectNode> TransformFromSelectClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformSelectClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformTargetList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformAliasedExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformExpressionAsCollabel(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformColIdExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformExpressionOptIdentifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<CreateStatement> TransformCreateSecretStmt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformSecretStorageSpecifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformSecretName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);


	static unique_ptr<CreateStatement> TransformCreateIndexStmt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformIndexType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformIndexElement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static case_insensitive_map_t<Value> TransformWithList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformIndexName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);



	static unique_ptr<SQLStatement> TransformCopyStatement(PEGTransformer &transformer,
	                                                       optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformCopySelect(PEGTransformer &transformer,
	                                                    optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformCopyFromDatabase(PEGTransformer &transformer,
	                                                          optional_ptr<ParseResult> parse_result);
	static CopyDatabaseType TransformCopyDatabaseFlag(PEGTransformer &transformer,
	                                                  optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformCopyTable(PEGTransformer &transformer,
	                                                   optional_ptr<ParseResult> parse_result);
	static bool TransformFromOrTo(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformCopyFileName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformIdentifierColId(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static case_insensitive_map_t<vector<Value>> TransformCopyOptions(PEGTransformer &transformer,
	                                                                  optional_ptr<ParseResult> parse_result);
	static case_insensitive_map_t<vector<Value>>
	TransformGenericCopyOptionListParens(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static case_insensitive_map_t<vector<Value>> TransformSpecializedOptionList(PEGTransformer &transformer,
	                                                                            optional_ptr<ParseResult> parse_result);

	static LogicalType TransformType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformNumericType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformSimpleNumericType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformDecimalNumericType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformFloatType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformDecimalType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformTypeModifiers(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformSimpleType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformQualifiedTypeName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static LogicalType TransformCharacterType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static GenericCopyOption TransformSpecializedOption(PEGTransformer &transformer,
	                                                    optional_ptr<ParseResult> parse_result);
	static GenericCopyOption TransformSingleOption(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static GenericCopyOption TransformEncodingOption(PEGTransformer &transformer,
	                                                 optional_ptr<ParseResult> parse_result);
	static GenericCopyOption TransformForceQuoteOption(PEGTransformer &transformer,
	                                                   optional_ptr<ParseResult> parse_result);

	static unique_ptr<BaseTableRef> TransformBaseTableName(PEGTransformer &transformer,
	                                                       optional_ptr<ParseResult> parse_result);
	static unique_ptr<BaseTableRef> TransformSchemaReservedTable(PEGTransformer &transformer,
	                                                             optional_ptr<ParseResult> parse_result);
	static unique_ptr<BaseTableRef> TransformCatalogReservedSchemaTable(PEGTransformer &transformer,
	                                                                    optional_ptr<ParseResult> parse_result);
	static string TransformSchemaQualification(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformCatalogQualification(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static QualifiedName TransformQualifiedName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static QualifiedName TransformCatalogReservedSchemaIdentifierOrStringLiteral(PEGTransformer &transformer,
																  optional_ptr<ParseResult> parse_result);
	static QualifiedName TransformSchemaReservedIdentifierOrStringLiteral(PEGTransformer &transformer,
	                                                                      optional_ptr<ParseResult> parse_result);
	static string TransformReservedIdentifierOrStringLiteral(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static QualifiedName TransformTableNameIdentifierOrStringLiteral(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static string TransformColIdOrString(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformColId(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformColLabelOrString(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformStringLiteral(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformIdentifierOrStringLiteral(PEGTransformer &transformer,
	                                                 optional_ptr<ParseResult> parse_result);
	static string TransformIdentifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	// Intermediate transforms returning semantic values
	static unique_ptr<SQLStatement> TransformStandardAssignment(PEGTransformer &transformer,
	                                                            optional_ptr<ParseResult> parse_result);
	static SettingInfo TransformSettingOrVariable(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static SettingInfo TransformSetSetting(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static SettingInfo TransformSetVariable(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformSetAssignment(PEGTransformer &transformer,
	                                                                   optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformVariableList(PEGTransformer &transformer,
	                                                                  optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformExpression(PEGTransformer &transformer,
	                                                        optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformRecursiveExpression(PEGTransformer &transformer,
	                                                                 optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformBaseExpression(PEGTransformer &transformer,
	                                                            optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformSingleExpression(PEGTransformer &transformer,
	                                                              optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformParenthesisExpression(PEGTransformer &transformer,
	                                                                   optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformPrefixExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformPrefixOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformStarExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static qualified_column_set_t TransformExcludeList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static qualified_column_set_t TransformExcludeNameList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static qualified_column_set_t TransformSingleExcludeName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static QualifiedColumnName TransformExcludeName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformIntervalLiteral(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);


	static unique_ptr<ParsedExpression> TransformLiteralExpression(PEGTransformer &transformer,
	                                                               optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformCastExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static bool TransformCastOrTryCast(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformColumnReference(PEGTransformer &transformer,
	                                                             optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformFunctionExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformSubqueryExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformTypeLiteral(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformCaseExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static CaseCheck TransformCaseWhenThen(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformCaseElse(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformFilterClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static bool TransformDistinctOrAll(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);


	static unique_ptr<ParsedExpression> TransformSpecialFunctionExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformCoalesceExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformUnpackExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformColumnsExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformExtractExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformLambdaExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformNullIfExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformRowExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformPositionExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);


	static QualifiedName TransformFunctionIdentifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static QualifiedName TransformCatalogReservedSchemaFunctionName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static QualifiedName TransformSchemaReservedFunctionName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformReservedFunctionName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static QualifiedName TransformFunctionName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformIdentifierOrKeyword(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<ParsedExpression> TransformListExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformBoundedListExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformStructExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformStructField(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);


	static unique_ptr<WindowExpression> TransformOverClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<WindowExpression> TransformWindowFrame(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<WindowExpression> TransformWindowFrameDefinition(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<WindowExpression> TransformWindowFrameContentsParens(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<WindowExpression> TransformWindowFrameNameContentsParens(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<WindowExpression> TransformWindowFrameContents(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformWindowPartition(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static vector<OrderByNode> TransformOrderByClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<OrderByNode> TransformOrderByExpressions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<OrderByNode> TransformOrderByExpressionList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<OrderByNode> TransformOrderByAll(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static OrderByNode TransformOrderByExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static OrderType TransformDescOrAsc(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static OrderByNullType TransformNullsFirstOrLast(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);


	static unique_ptr<SQLStatement> TransformPrepareStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformExecuteStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformExplainStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unordered_map<string, vector<Value>> TransformExplainOptions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformAnalyzeStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformDeleteStatement(PEGTransformer &transformer,
															 optional_ptr<ParseResult> parse_result);
	static unique_ptr<BaseTableRef> TransformTargetOptAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<TableRef>> TransformDeleteUsingClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SelectStatement> TransformDescribeStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<QueryNode> TransformShowSelect(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<QueryNode> TransformShowAllTables(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<QueryNode> TransformShowQualifiedName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static ShowType TransformShowOrDescribeOrSummarize(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static ShowType TransformShowOrDescribe(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static ShowType TransformSummarize(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformInsertStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static InsertColumnOrder TransformByNameOrPosition(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static InsertValues TransformInsertValues(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<OnConflictInfo> TransformOnConflictClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<BaseTableRef> TransformInsertTarget(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static OnConflictAction TransformOnConflictAction(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static OnConflictAction TransformOnConflictUpdate(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static OnConflictAction TransformOnConflictNothing(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static OnConflictExpressionTarget TransformOnConflictExpressionTarget(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformReturningClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformUpdateStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<BaseTableRef> TransformUpdateTarget(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<BaseTableRef> TransformUpdateTargetNoAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<BaseTableRef> TransformUpdateTargetAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<UpdateSetInfo> TransformUpdateSetClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<UpdateSetInfo> TransformUpdateSetElementList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<UpdateSetInfo> TransformUpdateSetColumnListExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static UpdateSetElement TransformUpdateSetElement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);



	static string TransformUpdateAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformDropStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<DropStatement> TransformDropEntries(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<DropStatement> TransformDropTable(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static CatalogType TransformTableOrView(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<DropStatement> TransformDropTableFunction(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<DropStatement> TransformDropFunction(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<DropStatement> TransformDropSchema(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static QualifiedName TransformQualifiedSchemaName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<DropStatement> TransformDropIndex(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static QualifiedName TransformQualifiedIndexName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<DropStatement> TransformDropSequence(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<DropStatement> TransformDropCollation(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<DropStatement> TransformDropType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static bool TransformDropBehavior(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<DropStatement> TransformDropSecret(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformDropSecretStorage(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);


	static unique_ptr<SQLStatement> TransformAlterStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<AlterInfo> TransformAlterOptions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<AlterInfo> TransformAlterTableStmt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<AlterInfo> TransformAlterTableOptions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<AlterInfo> TransformAddColumn(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<AlterInfo> TransformDropColumn(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<string> TransformNestedColumnName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<AlterInfo> TransformAlterColumn(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<AlterInfo> TransformRenameColumn(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<AlterInfo> TransformRenameAlter(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);


	//! Operator
	static ExpressionType TransformOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static ExpressionType TransformConjunctionOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static ExpressionType TransformIsOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static ExpressionType TransformLikeOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static ExpressionType TransformLikeOrSimilarTo(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static ExpressionType TransformInOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	//! Helper functions
	static bool ExpressionIsEmptyStar(ParsedExpression &expr);
	static optional_ptr<ParseResult> ExtractResultFromParens(optional_ptr<ParseResult> parse_result);
	static vector<optional_ptr<ParseResult>> ExtractParseResultsFromList(optional_ptr<ParseResult> parse_result);

	static string ExtractFormat(const string &file_path);
	static QualifiedName StringToQualifiedName(vector<string> input);

private:
	PEGParser parser;
	case_insensitive_map_t<PEGTransformer::AnyTransformFunction> sql_transform_functions;
	case_insensitive_map_t<unique_ptr<TransformEnumValue>> enum_mappings;
};

// Helper struct for qualified names.
struct PEGQualifiedName {
	string catalog;
	string schema;
	string name;

	static PEGQualifiedName FromIdentifierList(const ListParseResult &identifier_list_pr);
};

} // namespace duckdb
