#pragma once

#include "tokenizer.hpp"
#include "parse_result.hpp"
#include "transform_enum_result.hpp"
#include "transform_result.hpp"
#include "ast/column_element.hpp"
#include "ast/extension_repository_info.hpp"
#include "ast/generic_copy_option.hpp"
#include "ast/persist_type.hpp"
#include "ast/set_info.hpp"
#include "duckdb/parser/parsed_data/transaction_info.hpp"
#include "duckdb/parser/statement/copy_database_statement.hpp"
#include "duckdb/parser/statement/set_statement.hpp"
#include "duckdb/parser/statement/create_statement.hpp"
#include "duckdb/parser/tableref/basetableref.hpp"
#include "parser/peg_parser.hpp"
#include "duckdb/storage/arena_allocator.hpp"

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

	// Make overloads return raw pointers, as ownership is handled by the ArenaAllocator.
	template <class T, typename... Args>
	T *Make(Args &&...args) {
		return allocator.Make<T>(std::forward<Args>(args)...);
	}

public:
	ArenaAllocator &allocator;
	PEGTransformerState &state;
	const case_insensitive_map_t<PEGRule> &grammar_rules;
	const case_insensitive_map_t<AnyTransformFunction> &transform_functions;
	const case_insensitive_map_t<unique_ptr<TransformEnumValue>> &enum_mappings;
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
	// ... Add more overloads for 3, 4, etc., arguments as needed.
	static unique_ptr<SQLStatement> TransformStatement(PEGTransformer &, optional_ptr<ParseResult> list);

	static unique_ptr<SQLStatement> TransformUseStatement(PEGTransformer &, optional_ptr<ParseResult> identifier_pr);
	static unique_ptr<SQLStatement> TransformSetStatement(PEGTransformer &, optional_ptr<ParseResult> choice_pr);
	static unique_ptr<SQLStatement> TransformResetStatement(PEGTransformer &, optional_ptr<ParseResult> choice_pr);
	static QualifiedName TransformDottedIdentifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static QualifiedName TransformUseTarget(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformDeleteStatement(PEGTransformer &transformer,
	                                                         optional_ptr<ParseResult> parse_result);
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

	static unique_ptr<SQLStatement> TransformCreateStatement(PEGTransformer &transformer,
	                                                         optional_ptr<ParseResult> parse_result);
	static unique_ptr<CreateStatement> TransformCreateStatementVariation(PEGTransformer &transformer,
	                                                                  optional_ptr<ParseResult> parse_result);
	static unique_ptr<CreateStatement> TransformCreateTableStmt(PEGTransformer &transformer,
	                                                         optional_ptr<ParseResult> parse_result);
	static ColumnElements TransformCreateColumnList(PEGTransformer &transformer,
	                                                optional_ptr<ParseResult> parse_result);
	static ColumnElements TransformCreateTableColumnList(PEGTransformer &transformer,
	                                                     optional_ptr<ParseResult> parse_result);
	static PersistType TransformTemporary(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<CreateStatement> TransformCreateSchemaStmt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

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

	static unique_ptr<CreateStatement> TransformCreateSecretStmt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformSecretStorageSpecifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

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
	static unique_ptr<ParsedExpression> TransformLiteralExpression(PEGTransformer &transformer,
	                                                               optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformColumnReference(PEGTransformer &transformer,
	                                                             optional_ptr<ParseResult> parse_result);
	static string TransformIdentifierOrKeyword(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	//! Operator
	static ExpressionType TransformOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	//! Helper functions
	static optional_ptr<ParseResult> ExtractResultFromParens(optional_ptr<ParseResult> parse_result);
	static vector<optional_ptr<ParseResult>> ExtractParseResultsFromList(optional_ptr<ParseResult> parse_result);

	static string ExtractFormat(const string &file_path);

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
