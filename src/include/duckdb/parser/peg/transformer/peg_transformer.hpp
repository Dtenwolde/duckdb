#pragma once

#include "duckdb/parser/peg/ast/unpivot_name_values.hpp"
#include "duckdb/parser/peg/transformer/parse_result.hpp"
#include "duckdb/parser/peg/transformer/transform_enum_result.hpp"
#include "duckdb/parser/peg/transformer/transform_result.hpp"
#include "duckdb/parser/peg/ast/add_column_entry.hpp"
#include "duckdb/parser/peg/ast/column_constraint_entry.hpp"
#include "duckdb/parser/peg/ast/analyze_target.hpp"
#include "duckdb/parser/peg/ast/column_elements.hpp"
#include "duckdb/parser/peg/ast/create_table_column_element.hpp"
#include "duckdb/parser/peg/ast/create_table_definition.hpp"
#include "duckdb/parser/peg/ast/partition_sorted_options.hpp"
#include "duckdb/parser/peg/ast/distinct_clause.hpp"
#include "duckdb/parser/peg/ast/describe_target.hpp"
#include "duckdb/parser/peg/ast/extension_repository_info.hpp"
#include "duckdb/parser/peg/ast/generated_column_definition.hpp"
#include "duckdb/parser/peg/ast/generic_copy_option.hpp"
#include "duckdb/parser/peg/ast/generic_copy_option_value.hpp"
#include "duckdb/parser/peg/ast/insert_values.hpp"
#include "duckdb/parser/peg/ast/create_pivot_entry.hpp"
#include "duckdb/parser/peg/ast/join_prefix.hpp"
#include "duckdb/parser/peg/ast/join_qualifier.hpp"
#include "duckdb/parser/peg/ast/key_actions.hpp"
#include "duckdb/parser/peg/ast/limit_percent_result.hpp"
#include "duckdb/parser/peg/ast/macro_parameter.hpp"
#include "duckdb/parser/peg/ast/on_conflict_expression_target.hpp"
#include "duckdb/parser/peg/ast/sequence_option.hpp"
#include "duckdb/parser/peg/ast/setting_info.hpp"
#include "duckdb/parser/peg/ast/table_alias.hpp"
#include "duckdb/parser/peg/ast/cast_arguments.hpp"
#include "duckdb/parser/peg/ast/expression_chain.hpp"
#include "duckdb/parser/peg/ast/method_arguments.hpp"
#include "duckdb/parser/peg/ast/trim_arguments.hpp"
#include "duckdb/parser/peg/ast/trigger_event_info.hpp"
#include "duckdb/parser/peg/ast/trigger_table_referencing_info.hpp"
#include "duckdb/parser/peg/ast/window_frame.hpp"
#include "duckdb/function/macro_function.hpp"
#include "duckdb/common/optional.hpp"
#include "duckdb/parser/query_node/set_operation_node.hpp"
#include "duckdb/parser/parser_options.hpp"
#include "duckdb/common/stack_checker.hpp"
#include "duckdb/parser/expression/case_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/parameter_expression.hpp"
#include "duckdb/parser/expression/window_expression.hpp"
#include "duckdb/parser/parsed_data/connect_info.hpp"
#include "duckdb/parser/parsed_data/create_type_info.hpp"
#include "duckdb/parser/parsed_data/transaction_info.hpp"
#include "duckdb/parser/parsed_data/vacuum_info.hpp"
#include "duckdb/parser/statement/copy_database_statement.hpp"
#include "duckdb/parser/statement/set_statement.hpp"
#include "duckdb/parser/statement/create_statement.hpp"
#include "duckdb/parser/statement/transaction_statement.hpp"
#include "duckdb/parser/tableref/basetableref.hpp"
#include "duckdb/parser/peg/peg_parser.hpp"
#include "duckdb/storage/arena_allocator.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/statement/drop_statement.hpp"
#include "duckdb/parser/statement/insert_statement.hpp"
#include "duckdb/parser/statement/merge_into_statement.hpp"
#include "duckdb/parser/tableref/pivotref.hpp"

namespace duckdb {

// Forward declare
struct QualifiedName;
struct MatcherToken;
struct GroupingExpressionMap;
class Matcher;

enum class GroupByExpressionInfoType : uint8_t { EXPRESSION, EMPTY, CUBE, ROLLUP, GROUPING_SETS };

struct GroupByExpressionInfo {
	GroupByExpressionInfoType type = GroupByExpressionInfoType::EMPTY;
	unique_ptr<ParsedExpression> expression;
	vector<unique_ptr<ParsedExpression>> expressions;
	vector<GroupByExpressionInfo> children;
};

struct PEGTransformerState {
	explicit PEGTransformerState(const vector<MatcherToken> &tokens_p) : tokens(tokens_p), token_index(0) {
	}

	const vector<MatcherToken> &tokens;
	idx_t token_index;
};

class PEGTransformer;
class TransformStack;

enum class TransformFrameState : uint8_t { INITIALIZE, WAITING };

struct TransformStackFrame;

struct TransformFrameOps {
	const char *name;
	void (*Initialize)(PEGTransformer &, TransformStack &, TransformStackFrame &);
	unique_ptr<TransformResultValue> (*Finalize)(PEGTransformer &, TransformStackFrame &);
};

struct TransformStackFrame {
	TransformStackFrame(idx_t frame_index_p, ParseResult &parse_result_p, const TransformFrameOps &ops_p,
	                    idx_t parent_p, idx_t parent_slot_p);

	template <class T>
	T TakeResult(idx_t child_index) {
		if (child_index >= child_results.size() || !child_results[child_index]) {
			throw InternalException("Missing trampoline child result for rule '%s'", parse_result.name);
		}
		auto *typed_result = dynamic_cast<TypedTransformResult<T> *>(child_results[child_index].get());
		if (!typed_result) {
			throw InternalException("Unexpected trampoline child result type for rule '%s'", parse_result.name);
		}
		return std::move(typed_result->value);
	}

	idx_t frame_index;
	ParseResult &parse_result;
	const TransformFrameOps &ops;
	idx_t parent;
	idx_t parent_slot;
	TransformFrameState state = TransformFrameState::INITIALIZE;
	vector<unique_ptr<TransformResultValue>> child_results;
};

class TransformStack {
public:
	explicit TransformStack(PEGTransformer &transformer);

	unique_ptr<TransformResultValue> Execute(ParseResult &parse_result, const TransformFrameOps &ops);
	void PushFrame(ParseResult &parse_result, const TransformFrameOps &ops, TransformStackFrame &parent,
	               idx_t parent_slot);
	void PushFrame(ParseResult &parse_result, const TransformFrameOps &ops, idx_t parent_frame_index,
	               idx_t parent_slot);

private:
	void PushFrameInternal(ParseResult &parse_result, const TransformFrameOps &ops, idx_t parent, idx_t parent_slot);

private:
	PEGTransformer &transformer;
	vector<TransformStackFrame> frames;
	vector<idx_t> frame_stack;
};

class PEGTransformer {
public:
	using AnyTransformFunction = std::function<unique_ptr<TransformResultValue>(PEGTransformer &, ParseResult &)>;

	PEGTransformer(ArenaAllocator &allocator, PEGTransformerState &state,
	               const case_insensitive_map_t<AnyTransformFunction> &transform_functions,
	               const case_insensitive_map_t<PEGRule> &grammar_rules,
	               const case_insensitive_map_t<unique_ptr<TransformEnumValue>> &enum_mappings,
	               ParserOptions &options_p)
	    : allocator(allocator), state(state), grammar_rules(grammar_rules), transform_functions(transform_functions),
	      enum_mappings(enum_mappings), options(options_p) {
	}

public:
	template <typename T>
	T Transform(ParseResult &parse_result) {
		auto it = transform_functions.find(parse_result.name);
		if (it == transform_functions.end()) {
			throw NotImplementedException("No transformer function found for rule '%s'", parse_result.name);
		}
		auto &func = it->second;

		unique_ptr<TransformResultValue> base_result = func(*this, parse_result);
		if (!base_result) {
			throw InternalException("Transformer for rule '%s' returned a nullptr.", parse_result.name);
		}

		auto *typed_result_ptr = dynamic_cast<TypedTransformResult<T> *>(base_result.get());
		if (!typed_result_ptr) {
			// allow transparent bridging between string-typed and Identifier-typed rules
			auto bridged = TryBridgeTransformResult<T>(*base_result);
			if (bridged) {
				auto bridged_result = std::move(bridged->value);
				SetResultLocation(bridged_result, parse_result.offset);
				return bridged_result;
			}
			throw InternalException("Transformer for rule '" + parse_result.name + "' returned an unexpected type.");
		}

		auto result = std::move(typed_result_ptr->value);
		SetResultLocation(result, parse_result.offset);
		return result;
	}

	//! Bridge between string-typed and Identifier-typed rule results (and their vector forms).
	//! The generic form performs no bridging; the specializations below convert transparently.
	template <typename T>
	static unique_ptr<TypedTransformResult<T>> TryBridgeTransformResult(TransformResultValue &base_result) {
		return nullptr;
	}

	template <typename T>
	T Transform(ListParseResult &parse_result, idx_t child_index) {
		auto &child_parse_result = parse_result.GetChild(child_index);
		return Transform<T>(child_parse_result);
	}

	template <typename T>
	T TransformEnum(ParseResult &parse_result) {
		auto enum_rule_name = parse_result.name;

		auto rule_value = enum_mappings.find(enum_rule_name);
		if (rule_value == enum_mappings.end()) {
			throw ParserException("Enum transform failed: could not find mapping for '%s'", enum_rule_name);
		}

		auto *typed_enum_ptr = dynamic_cast<TypedTransformEnumResult<T> *>(rule_value->second.get());
		if (!typed_enum_ptr) {
			throw InternalException("Enum mapping for rule '%s' has an unexpected type.", enum_rule_name);
		}

		return typed_enum_ptr->value;
	}

	template <typename T>
	void TransformOptional(ListParseResult &list_pr, idx_t child_idx, T &target) {
		auto &opt = list_pr.Child<OptionalParseResult>(child_idx);
		if (opt.HasResult()) {
			target = Transform<T>(opt.GetResult());
		}
	}

	// Make overloads return raw pointers, as ownership is handled by the ArenaAllocator.
	template <class T, typename... Args>
	T *Make(Args &&...args) {
		return allocator.Make<T>(std::forward<Args>(args)...);
	}

	void Clear();
	void ClearParameters();
	static void ParamTypeCheck(PreparedParamType last_type, PreparedParamType new_type);
	void SetParam(const Identifier &name, idx_t index, PreparedParamType type);
	bool GetParam(const Identifier &name, idx_t &index, PreparedParamType type);
	void SetParamCount(idx_t new_count);
	idx_t ParamCount() const;
	unique_ptr<SQLStatement> CreatePivotStatement(unique_ptr<SQLStatement> statement);
	unique_ptr<SQLStatement> GenerateCreateEnumStmt(unique_ptr<CreatePivotEntry> entry);
	void PivotEntryCheck(const string &type);
	void ExtractCTEsRecursive(CommonTableExpressionMap &cte_map);
	bool IsWindowFrameDefault(WindowBoundary start, WindowBoundary end);
	unique_ptr<WindowExpression> GetWindowClause(const Identifier &window_name);
	void SetQueryLocation(ParsedExpression &expr, optional_idx query_location);
	void SetQueryLocation(TableRef &ref, optional_idx query_location);

private:
	template <typename T>
	void SetResultLocation(T &, optional_idx) {
	}
	void SetResultLocation(unique_ptr<ParsedExpression> &expr, optional_idx offset) {
		if (!expr) {
			return;
		}
		if (offset.IsValid() && !expr->GetQueryLocation().IsValid()) {
			SetQueryLocation(*expr, offset);
		}
	}
	void SetResultLocation(unique_ptr<TableRef> &ref, optional_idx offset) {
		if (!ref) {
			return;
		}
		if (offset.IsValid() && !ref->query_location.IsValid()) {
			SetQueryLocation(*ref, offset.GetIndex());
		}
	}

public:
	ArenaAllocator &allocator;
	PEGTransformerState &state;
	const case_insensitive_map_t<PEGRule> &grammar_rules;
	const case_insensitive_map_t<AnyTransformFunction> &transform_functions;
	const case_insensitive_map_t<unique_ptr<TransformEnumValue>> &enum_mappings;
	identifier_map_t<idx_t> named_parameter_map;
	idx_t prepared_statement_parameter_index = 0;
	PreparedParamType last_param_type = PreparedParamType::INVALID;

	identifier_map_t<unique_ptr<WindowExpression>> window_clauses;

	vector<unique_ptr<CreatePivotEntry>> pivot_entries;
	vector<reference<CommonTableExpressionMap>> stored_cte_map;

	bool in_window_definition = false;

	friend class StackChecker<PEGTransformer>;
	idx_t stack_depth = 0;

	StackChecker<PEGTransformer> StackCheck(idx_t extra_stack = 1) {
		if (stack_depth + extra_stack >= options.max_expression_depth) {
			throw ParserException(
			    "Max expression depth limit of %lld exceeded. Use \"SET max_expression_depth TO x\" to "
			    "increase the maximum expression depth.",
			    options.max_expression_depth);
		}
		return StackChecker<PEGTransformer>(*this, extra_stack);
	}

	ParserOptions options;
};

//! Transparent bridging between string-typed and Identifier-typed transform results.
template <>
inline unique_ptr<TypedTransformResult<string>>
PEGTransformer::TryBridgeTransformResult<string>(TransformResultValue &base_result) {
	if (auto *ident = dynamic_cast<TypedTransformResult<Identifier> *>(&base_result)) {
		return make_uniq<TypedTransformResult<string>>(ident->value.GetIdentifierName());
	}
	return nullptr;
}

template <>
inline unique_ptr<TypedTransformResult<Identifier>>
PEGTransformer::TryBridgeTransformResult<Identifier>(TransformResultValue &base_result) {
	if (auto *str = dynamic_cast<TypedTransformResult<string> *>(&base_result)) {
		return make_uniq<TypedTransformResult<Identifier>>(Identifier(str->value));
	}
	return nullptr;
}

template <>
inline unique_ptr<TypedTransformResult<vector<string>>>
PEGTransformer::TryBridgeTransformResult<vector<string>>(TransformResultValue &base_result) {
	if (auto *idents = dynamic_cast<TypedTransformResult<vector<Identifier>> *>(&base_result)) {
		return make_uniq<TypedTransformResult<vector<string>>>(IdentifiersToStrings(idents->value));
	}
	return nullptr;
}

template <>
inline unique_ptr<TypedTransformResult<vector<Identifier>>>
PEGTransformer::TryBridgeTransformResult<vector<Identifier>>(TransformResultValue &base_result) {
	if (auto *strs = dynamic_cast<TypedTransformResult<vector<string>> *>(&base_result)) {
		return make_uniq<TypedTransformResult<vector<Identifier>>>(StringsToIdentifiers(strs->value));
	}
	return nullptr;
}

typedef unique_ptr<TransformResultValue> (*transform_function_t)(PEGTransformer &transformer,
                                                                 ParseResult &parse_result);

struct TransformRule {
	const char *name;
	transform_function_t transform;
};

class PEGTransformerFactory {
public:
	explicit PEGTransformerFactory();

	//! Match a single TopLevelStatement from `tokens` starting at `token_cursor` and transform it
	//! into a SQLStatement. Returns nullptr if the matched TLS was separator-only (no statement).
	//! Throws on syntax error. `token_cursor` is in/out: it's the token index where matching
	//! starts, and on return holds the token index immediately past the last consumed token.
	unique_ptr<SQLStatement> TransformTopLevelStatement(vector<MatcherToken> &tokens, ParserOptions &options,
	                                                    Matcher &root_matcher, idx_t &token_cursor);
	static ParseResult &ExtractResultFromParens(ParseResult &parse_result);
	static vector<reference<ParseResult>> ExtractParseResultsFromList(ParseResult &parse_result);
	static bool ExpressionIsEmptyStar(const ParsedExpression &expr);
	static QualifiedName StringToQualifiedName(vector<string> input);
	static LogicalType GetIntervalTargetType(DatePartSpecifier date_part);
	static bool ConstructConstantFromExpression(const ParsedExpression &expr, Value &value);
	static unique_ptr<ParsedExpression> TryNegateValue(const ConstantExpression &expr);
	static unique_ptr<ParsedExpression> ConvertNumberToValue(string val);
	static void AddGroupByExpression(unique_ptr<ParsedExpression> expression, GroupingExpressionMap &map,
	                                 GroupByNode &result, vector<ProjectionIndex> &result_set);
	static vector<GroupingSet> GroupByExpressionUnfolding(GroupByExpressionInfo &group_by_expr,
	                                                      GroupingExpressionMap &map, GroupByNode &result);
	static unique_ptr<ResultModifier> VerifyLimitOffset(LimitPercentResult &limit, LimitPercentResult &offset);
	static unique_ptr<QueryNode> ToRecursiveCTE(unique_ptr<QueryNode> node, const Identifier &name,
	                                            vector<Identifier> &aliases,
	                                            vector<unique_ptr<ParsedExpression>> &key_targets);
	static void WrapRecursiveView(unique_ptr<CreateViewInfo> &info, unique_ptr<QueryNode> inner_node);
	static void ConvertToRecursiveView(unique_ptr<CreateViewInfo> &info, unique_ptr<QueryNode> &node);
	static void VerifyColumnRefs(const ParsedExpression &expr);
	static void RemoveOrderQualificationRecursive(unique_ptr<ParsedExpression> &root_expr);
	static void GetValueFromExpression(unique_ptr<ParsedExpression> &expr, vector<Value> &result);
	static bool TransformPivotInList(unique_ptr<ParsedExpression> &expr, PivotColumnEntry &entry);
	static void AddPivotEntry(PEGTransformer &transformer, string enum_name, unique_ptr<SelectNode> base,
	                          unique_ptr<ParsedExpression> column, unique_ptr<QueryNode> subquery, bool has_parameters);
	static Value GetConstantExpressionValue(unique_ptr<ParsedExpression> &expr);
	static void AddToMultiStatement(const unique_ptr<MultiStatement> &multi_statement,
	                                unique_ptr<AlterInfo> alter_info);
	static void AddUpdateToMultiStatement(const unique_ptr<MultiStatement> &multi_statement, const string &column_name,
	                                      const AlterEntryData &table_data,
	                                      const unique_ptr<ParsedExpression> &original_expression);
	static unique_ptr<MultiStatement> TransformAndMaterializeAlter(AlterEntryData &data,
	                                                               unique_ptr<AlterInfo> info_with_null_placeholder,
	                                                               const string &column_name,
	                                                               unique_ptr<ParsedExpression> expression);

	// Registration methods
	void RegisterComment();
	void RegisterCommon();
	void RegisterCreateMacro();
	void RegisterCreateTable();
	void RegisterExpression();
	void RegisterConnect();
	void RegisterPivot();
	void RegisterSelect();
	void RegisterKeywordsAndIdentifiers();
	void RegisterEnums();
	void RegisterGenerated();
	void RegisterGeneratedTrampoline();

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
		sql_transform_functions[rule_name] = [function](PEGTransformer &transformer,
		                                                ParseResult &parse_result) -> unique_ptr<TransformResultValue> {
			auto result_value = function(transformer, parse_result);
			return make_uniq<TypedTransformResult<decltype(result_value)>>(std::move(result_value));
		};
	}

	PEGTransformerFactory(const PEGTransformerFactory &) = delete;

	static unique_ptr<SQLStatement> TransformStatement(PEGTransformer &, ParseResult &list);
	static unique_ptr<TransformResultValue> TransformStatementTrampolineInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static const case_insensitive_map_t<const TransformFrameOps *> &GeneratedTrampolineOps();

	// BEGIN generated trampoline transformer declarations
	static const TransformFrameOps ALTER_STATEMENT_OPS;
	static void InitializeAlterStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALTER_OPTIONS_OPS;
	static void InitializeAlterOptions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterOptions(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALTER_TABLE_STMT_OPS;
	static void InitializeAlterTableStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterTableStmt(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALTER_SCHEMA_STMT_OPS;
	static void InitializeAlterSchemaStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterSchemaStmt(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALTER_TABLE_OPTIONS_OPS;
	static void InitializeAlterTableOptions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterTableOptions(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ADD_CONSTRAINT_OPS;
	static void InitializeAddConstraint(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAddConstraint(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ADD_COLUMN_OPS;
	static void InitializeAddColumn(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAddColumn(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ADD_COLUMN_ENTRY_OPS;
	static void InitializeAddColumnEntry(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAddColumnEntry(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_COLUMN_OPS;
	static void InitializeDropColumn(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropColumn(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALTER_COLUMN_OPS;
	static void InitializeAlterColumn(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterColumn(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RENAME_COLUMN_OPS;
	static void InitializeRenameColumn(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRenameColumn(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NESTED_COLUMN_NAME_OPS;
	static void InitializeNestedColumnName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNestedColumnName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IDENTIFIER_DOT_OPS;
	static void InitializeIdentifierDot(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIdentifierDot(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RENAME_ALTER_OPS;
	static void InitializeRenameAlter(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRenameAlter(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_PARTITIONED_BY_OPS;
	static void InitializeSetPartitionedBy(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetPartitionedBy(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RESET_PARTITIONED_BY_OPS;
	static void InitializeResetPartitionedBy(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeResetPartitionedBy(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_SORTED_BY_OPS;
	static void InitializeSetSortedBy(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetSortedBy(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RESET_SORTED_BY_OPS;
	static void InitializeResetSortedBy(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeResetSortedBy(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_OPTIONS_OPS;
	static void InitializeSetOptions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetOptions(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RESET_OPTIONS_OPS;
	static void InitializeResetOptions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeResetOptions(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALTER_COLUMN_ENTRY_OPS;
	static void InitializeAlterColumnEntry(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterColumnEntry(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ADD_OR_DROP_DEFAULT_OPS;
	static void InitializeAddOrDropDefault(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAddOrDropDefault(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ADD_DEFAULT_OPS;
	static void InitializeAddDefault(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAddDefault(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_DEFAULT_OPS;
	static void InitializeDropDefault(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropDefault(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CHANGE_NULLABILITY_OPS;
	static void InitializeChangeNullability(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeChangeNullability(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_OR_SET_OPS;
	static void InitializeDropOrSet(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropOrSet(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_NULLABILITY_OPS;
	static void InitializeDropNullability(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropNullability(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_NULLABILITY_OPS;
	static void InitializeSetNullability(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetNullability(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALTER_TYPE_OPS;
	static void InitializeAlterType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps USING_EXPRESSION_OPS;
	static void InitializeUsingExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUsingExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALTER_VIEW_STMT_OPS;
	static void InitializeAlterViewStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterViewStmt(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALTER_SEQUENCE_STMT_OPS;
	static void InitializeAlterSequenceStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterSequenceStmt(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFIED_SEQUENCE_NAME_OPS;
	static void InitializeQualifiedSequenceName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifiedSequenceName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALTER_SEQUENCE_OPTIONS_OPS;
	static void InitializeAlterSequenceOptions(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterSequenceOptions(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_SEQUENCE_OPTION_OPS;
	static void InitializeSetSequenceOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetSequenceOption(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALTER_DATABASE_STMT_OPS;
	static void InitializeAlterDatabaseStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAlterDatabaseStmt(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ANALYZE_STATEMENT_OPS;
	static void InitializeAnalyzeStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAnalyzeStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ANALYZE_TARGET_OPS;
	static void InitializeAnalyzeTarget(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAnalyzeTarget(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ANALYZE_VERBOSE_OPS;
	static void InitializeAnalyzeVerbose(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAnalyzeVerbose(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ATTACH_STATEMENT_OPS;
	static void InitializeAttachStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAttachStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DATABASE_PATH_OPS;
	static void InitializeDatabasePath(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDatabasePath(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ATTACH_ALIAS_OPS;
	static void InitializeAttachAlias(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAttachAlias(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ATTACH_OPTIONS_OPS;
	static void InitializeAttachOptions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAttachOptions(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CALL_STATEMENT_OPS;
	static void InitializeCallStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCallStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CHECKPOINT_STATEMENT_OPS;
	static void InitializeCheckpointStatement(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCheckpointStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CHECKPOINT_FORCE_OPS;
	static void InitializeCheckpointForce(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCheckpointForce(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_STATEMENT_OPS;
	static void InitializeCommentStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_ON_TYPE_OPS;
	static void InitializeCommentOnType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentOnType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_TABLE_OPS;
	static void InitializeCommentTable(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentTable(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_SEQUENCE_OPS;
	static void InitializeCommentSequence(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentSequence(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_FUNCTION_OPS;
	static void InitializeCommentFunction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentFunction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_MACRO_TABLE_OPS;
	static void InitializeCommentMacroTable(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentMacroTable(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_MACRO_OPS;
	static void InitializeCommentMacro(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentMacro(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_VIEW_OPS;
	static void InitializeCommentView(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentView(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_DATABASE_OPS;
	static void InitializeCommentDatabase(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentDatabase(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_INDEX_OPS;
	static void InitializeCommentIndex(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentIndex(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_SCHEMA_OPS;
	static void InitializeCommentSchema(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentSchema(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_TYPE_OPS;
	static void InitializeCommentType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_COLUMN_OPS;
	static void InitializeCommentColumn(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentColumn(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMENT_VALUE_OPS;
	static void InitializeCommentValue(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommentValue(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps STATEMENT_OPS;
	static void InitializeStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPRESSION_STATEMENT_OPS;
	static void InitializeExpressionStatement(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExpressionStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPRESSION_ALIAS_OPS;
	static void InitializeExpressionAlias(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExpressionAlias(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CONSTRAINT_NAME_OPS;
	static void InitializeConstraintName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeConstraintName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLLATION_NAME_OPS;
	static void InitializeCollationName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCollationName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TYPE_OPS;
	static void InitializeType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TYPE_VARIATIONS_OPS;
	static void InitializeTypeVariations(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTypeVariations(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SIMPLE_TYPE_OPS;
	static void InitializeSimpleType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSimpleType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CHARACTER_SIMPLE_TYPE_OPS;
	static void InitializeCharacterSimpleType(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCharacterSimpleType(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFIED_SIMPLE_TYPE_OPS;
	static void InitializeQualifiedSimpleType(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifiedSimpleType(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_TYPE_OPS;
	static void InitializeIntervalType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_INTERVAL_OPS;
	static void InitializeIntervalInterval(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalInterval(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_WITH_SPECIFIER_OPS;
	static void InitializeIntervalWithSpecifier(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalWithSpecifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_WITH_RANGE_SPECIFIER_OPS;
	static void InitializeIntervalWithRangeSpecifier(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalWithRangeSpecifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_WITH_SIMPLE_SPECIFIER_OPS;
	static void InitializeIntervalWithSimpleSpecifier(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalWithSimpleSpecifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_WITHOUT_SPECIFIER_OPS;
	static void InitializeIntervalWithoutSpecifier(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalWithoutSpecifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_TO_INTERVAL_AS_TYPE_OPS;
	static void InitializeIntervalToIntervalAsType(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalToIntervalAsType(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_NUMBER_OPS;
	static void InitializeIntervalNumber(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalNumber(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps YEAR_KEYWORD_OPS;
	static void InitializeYearKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeYearKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MONTH_KEYWORD_OPS;
	static void InitializeMonthKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMonthKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DAY_KEYWORD_OPS;
	static void InitializeDayKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDayKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps HOUR_KEYWORD_OPS;
	static void InitializeHourKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeHourKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MINUTE_KEYWORD_OPS;
	static void InitializeMinuteKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMinuteKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SECOND_KEYWORD_OPS;
	static void InitializeSecondKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSecondKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MILLISECOND_KEYWORD_OPS;
	static void InitializeMillisecondKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMillisecondKeyword(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MICROSECOND_KEYWORD_OPS;
	static void InitializeMicrosecondKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMicrosecondKeyword(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WEEK_KEYWORD_OPS;
	static void InitializeWeekKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWeekKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUARTER_KEYWORD_OPS;
	static void InitializeQuarterKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQuarterKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DECADE_KEYWORD_OPS;
	static void InitializeDecadeKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDecadeKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CENTURY_KEYWORD_OPS;
	static void InitializeCenturyKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCenturyKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MILLENNIUM_KEYWORD_OPS;
	static void InitializeMillenniumKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMillenniumKeyword(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_OPS;
	static void InitializeInterval(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInterval(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_TO_INTERVAL_OPS;
	static void InitializeIntervalToInterval(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalToInterval(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps YEAR_TO_MONTH_OPS;
	static void InitializeYearToMonth(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeYearToMonth(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DAY_TO_HOUR_OPS;
	static void InitializeDayToHour(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDayToHour(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DAY_TO_MINUTE_OPS;
	static void InitializeDayToMinute(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDayToMinute(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DAY_TO_SECOND_OPS;
	static void InitializeDayToSecond(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDayToSecond(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps HOUR_TO_MINUTE_OPS;
	static void InitializeHourToMinute(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeHourToMinute(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps HOUR_TO_SECOND_OPS;
	static void InitializeHourToSecond(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeHourToSecond(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MINUTE_TO_SECOND_OPS;
	static void InitializeMinuteToSecond(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMinuteToSecond(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BIT_TYPE_OPS;
	static void InitializeBitType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBitType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GEOMETRY_TYPE_OPS;
	static void InitializeGeometryType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGeometryType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VARIANT_TYPE_OPS;
	static void InitializeVariantType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeVariantType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NUMERIC_TYPE_OPS;
	static void InitializeNumericType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNumericType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SIMPLE_NUMERIC_TYPE_OPS;
	static void InitializeSimpleNumericType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSimpleNumericType(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DECIMAL_NUMERIC_TYPE_OPS;
	static void InitializeDecimalNumericType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDecimalNumericType(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INT_TYPE_OPS;
	static void InitializeIntType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTEGER_TYPE_OPS;
	static void InitializeIntegerType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntegerType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SMALLINT_TYPE_OPS;
	static void InitializeSmallintType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSmallintType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BIGINT_TYPE_OPS;
	static void InitializeBigintType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBigintType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REAL_TYPE_OPS;
	static void InitializeRealType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRealType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BOOLEAN_TYPE_OPS;
	static void InitializeBooleanType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBooleanType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DOUBLE_TYPE_OPS;
	static void InitializeDoubleType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDoubleType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FLOAT_TYPE_OPS;
	static void InitializeFloatType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFloatType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DECIMAL_TYPE_OPS;
	static void InitializeDecimalType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDecimalType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DEC_TYPE_OPS;
	static void InitializeDecType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDecType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NUMERIC_MOD_TYPE_OPS;
	static void InitializeNumericModType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNumericModType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFIED_TYPE_NAME_OPS;
	static void InitializeQualifiedTypeName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifiedTypeName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TYPE_NAME_AS_QUALIFIED_NAME_OPS;
	static void InitializeTypeNameAsQualifiedName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTypeNameAsQualifiedName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CATALOG_RESERVED_SCHEMA_TYPE_NAME_OPS;
	static void InitializeCatalogReservedSchemaTypeName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCatalogReservedSchemaTypeName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SCHEMA_RESERVED_TYPE_NAME_OPS;
	static void InitializeSchemaReservedTypeName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSchemaReservedTypeName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TYPE_MODIFIERS_OPS;
	static void InitializeTypeModifiers(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTypeModifiers(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ROW_TYPE_OPS;
	static void InitializeRowType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRowType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SETOF_TYPE_OPS;
	static void InitializeSetofType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetofType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNION_TYPE_OPS;
	static void InitializeUnionType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUnionType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_ID_TYPE_LIST_OPS;
	static void InitializeColIdTypeList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColIdTypeList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MAP_TYPE_OPS;
	static void InitializeMapType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMapType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_ID_TYPE_OPS;
	static void InitializeColIdType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColIdType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ARRAY_BOUNDS_OPS;
	static void InitializeArrayBounds(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeArrayBounds(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ARRAY_KEYWORD_OPS;
	static void InitializeArrayKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeArrayKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SQUARE_BRACKETS_ARRAY_OPS;
	static void InitializeSquareBracketsArray(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSquareBracketsArray(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TIME_TYPE_OPS;
	static void InitializeTimeType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTimeType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TIME_OR_TIMESTAMP_OPS;
	static void InitializeTimeOrTimestamp(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTimeOrTimestamp(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TIME_TYPE_ID_OPS;
	static void InitializeTimeTypeId(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTimeTypeId(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TIMESTAMP_TYPE_ID_OPS;
	static void InitializeTimestampTypeId(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTimestampTypeId(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TIME_ZONE_OPS;
	static void InitializeTimeZone(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTimeZone(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITH_OR_WITHOUT_OPS;
	static void InitializeWithOrWithout(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithOrWithout(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITH_RULE_OPS;
	static void InitializeWithRule(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithRule(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITHOUT_RULE_OPS;
	static void InitializeWithoutRule(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithoutRule(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CONNECT_STATEMENT_OPS;
	static void InitializeConnectStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeConnectStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DISCONNECT_STATEMENT_OPS;
	static void InitializeDisconnectStatement(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDisconnectStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SESSION_TARGET_OPS;
	static void InitializeSessionTarget(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSessionTarget(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LOCAL_SESSION_TARGET_OPS;
	static void InitializeLocalSessionTarget(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLocalSessionTarget(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps STRING_SESSION_TARGET_OPS;
	static void InitializeStringSessionTarget(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeStringSessionTarget(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CATALOG_SESSION_TARGET_OPS;
	static void InitializeCatalogSessionTarget(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCatalogSessionTarget(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_STATEMENT_OPS;
	static void InitializeCopyStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_VARIATIONS_OPS;
	static void InitializeCopyVariations(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyVariations(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_TABLE_OPS;
	static void InitializeCopyTable(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyTable(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FROM_OR_TO_OPS;
	static void InitializeFromOrTo(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFromOrTo(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_FROM_OPS;
	static void InitializeCopyFrom(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyFrom(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_TO_OPS;
	static void InitializeCopyTo(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyTo(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_SELECT_OPS;
	static void InitializeCopySelect(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopySelect(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_FILE_NAME_OPS;
	static void InitializeCopyFileName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyFileName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_FILE_NAME_EXPRESSION_OPS;
	static void InitializeCopyFileNameExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyFileNameExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_FILE_NAME_STRING_LITERAL_OPS;
	static void InitializeCopyFileNameStringLiteral(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyFileNameStringLiteral(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_FILE_NAME_IDENTIFIER_OPS;
	static void InitializeCopyFileNameIdentifier(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyFileNameIdentifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_FILE_NAME_IDENTIFIER_COL_ID_OPS;
	static void InitializeCopyFileNameIdentifierColId(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyFileNameIdentifierColId(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IDENTIFIER_COL_ID_OPS;
	static void InitializeIdentifierColId(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIdentifierColId(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_OPTIONS_OPS;
	static void InitializeCopyOptions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyOptions(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_OPTION_LIST_OPS;
	static void InitializeCopyOptionList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyOptionList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SPECIALIZED_OPTION_LIST_OPS;
	static void InitializeSpecializedOptionList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSpecializedOptionList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SPECIALIZED_OPTION_OPS;
	static void InitializeSpecializedOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSpecializedOption(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SINGLE_OPTION_OPS;
	static void InitializeSingleOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSingleOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BINARY_OPTION_OPS;
	static void InitializeBinaryOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBinaryOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FREEZE_OPTION_OPS;
	static void InitializeFreezeOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFreezeOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OIDS_OPTION_OPS;
	static void InitializeOidsOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOidsOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CSV_OPTION_OPS;
	static void InitializeCsvOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCsvOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps HEADER_OPTION_OPS;
	static void InitializeHeaderOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeHeaderOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NULL_AS_OPTION_OPS;
	static void InitializeNullAsOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNullAsOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DELIMITER_AS_OPTION_OPS;
	static void InitializeDelimiterAsOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDelimiterAsOption(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUOTE_AS_OPTION_OPS;
	static void InitializeQuoteAsOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQuoteAsOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ESCAPE_AS_OPTION_OPS;
	static void InitializeEscapeAsOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeEscapeAsOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ENCODING_OPTION_OPS;
	static void InitializeEncodingOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeEncodingOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FORCE_QUOTE_OPTION_OPS;
	static void InitializeForceQuoteOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeForceQuoteOption(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps STAR_SYMBOL_COLUMN_LIST_OPS;
	static void InitializeStarSymbolColumnList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeStarSymbolColumnList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FORCE_QUOTE_OPS;
	static void InitializeForceQuote(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeForceQuote(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PARTITION_BY_OPTION_OPS;
	static void InitializePartitionByOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePartitionByOption(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FORCE_NULL_OPTION_OPS;
	static void InitializeForceNullOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeForceNullOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FORCE_NOT_NULL_OPS;
	static void InitializeForceNotNull(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeForceNotNull(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GENERIC_COPY_OPTION_LIST_OPS;
	static void InitializeGenericCopyOptionList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGenericCopyOptionList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GENERIC_COPY_OPTION_OPS;
	static void InitializeGenericCopyOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGenericCopyOption(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GENERIC_COPY_OPTION_VALUE_OPS;
	static void InitializeGenericCopyOptionValue(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGenericCopyOptionValue(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GENERIC_COPY_OPTION_ORDER_LIST_OPS;
	static void InitializeGenericCopyOptionOrderList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGenericCopyOptionOrderList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GENERIC_COPY_OPTION_EXPRESSION_OPS;
	static void InitializeGenericCopyOptionExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGenericCopyOptionExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GENERIC_COPY_OPTION_PARENTHESIZED_EXPRESSION_LIST_OPS;
	static void InitializeGenericCopyOptionParenthesizedExpressionList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGenericCopyOptionParenthesizedExpressionList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_FROM_DATABASE_OPS;
	static void InitializeCopyFromDatabase(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyFromDatabase(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_FROM_DATABASE_WITH_FLAG_OPS;
	static void InitializeCopyFromDatabaseWithFlag(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyFromDatabaseWithFlag(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_FROM_DATABASE_WITHOUT_FLAG_OPS;
	static void InitializeCopyFromDatabaseWithoutFlag(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyFromDatabaseWithoutFlag(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_DATABASE_FLAG_OPS;
	static void InitializeCopyDatabaseFlag(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyDatabaseFlag(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SCHEMA_OR_DATA_OPS;
	static void InitializeSchemaOrData(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSchemaOrData(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_SCHEMA_OPS;
	static void InitializeCopySchema(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopySchema(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COPY_DATA_OPS;
	static void InitializeCopyData(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCopyData(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_INDEX_STMT_OPS;
	static void InitializeCreateIndexStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateIndexStmt(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITH_LIST_OPS;
	static void InitializeWithList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REL_OPTION_OR_OIDS_OPS;
	static void InitializeRelOptionOrOids(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRelOptionOrOids(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REL_OPTION_LIST_OPS;
	static void InitializeRelOptionList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRelOptionList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OIDS_OPS;
	static void InitializeOids(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOids(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITH_OR_WITHOUT_OIDS_OPS;
	static void InitializeWithOrWithoutOids(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithOrWithoutOids(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITH_OIDS_OPS;
	static void InitializeWithOids(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithOids(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITHOUT_OIDS_OPS;
	static void InitializeWithoutOids(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithoutOids(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INDEX_ELEMENT_OPS;
	static void InitializeIndexElement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIndexElement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNIQUE_INDEX_OPS;
	static void InitializeUniqueIndex(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUniqueIndex(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INDEX_TYPE_OPS;
	static void InitializeIndexType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIndexType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REL_OPTION_OPS;
	static void InitializeRelOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRelOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REL_OPTION_NAME_OPS;
	static void InitializeRelOptionName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRelOptionName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DOTTED_IDENTIFIER_STRING_OPS;
	static void InitializeDottedIdentifierString(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDottedIdentifierString(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REL_OPTION_ARGUMENT_OPT_OPS;
	static void InitializeRelOptionArgumentOpt(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRelOptionArgumentOpt(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DEF_ARG_OPS;
	static void InitializeDefArg(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDefArg(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DEF_ARG_NULL_OPS;
	static void InitializeDefArgNull(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDefArgNull(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DEF_ARG_KEYWORD_OPS;
	static void InitializeDefArgKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDefArgKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DEF_ARG_STRING_LITERAL_OPS;
	static void InitializeDefArgStringLiteral(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDefArgStringLiteral(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NONE_LITERAL_OPS;
	static void InitializeNoneLiteral(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNoneLiteral(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_MACRO_STMT_OPS;
	static void InitializeCreateMacroStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateMacroStmt(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MACRO_OR_FUNCTION_OPS;
	static void InitializeMacroOrFunction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMacroOrFunction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MACRO_KEYWORD_OPS;
	static void InitializeMacroKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMacroKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FUNCTION_KEYWORD_OPS;
	static void InitializeFunctionKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFunctionKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MACRO_DEFINITION_OPS;
	static void InitializeMacroDefinition(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMacroDefinition(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MACRO_DEFINITION_BODY_OPS;
	static void InitializeMacroDefinitionBody(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMacroDefinitionBody(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MACRO_PARAMETERS_OPS;
	static void InitializeMacroParameters(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMacroParameters(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MACRO_PARAMETER_OPS;
	static void InitializeMacroParameter(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMacroParameter(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SIMPLE_PARAMETER_OPS;
	static void InitializeSimpleParameter(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSimpleParameter(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SCALAR_MACRO_DEFINITION_OPS;
	static void InitializeScalarMacroDefinition(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeScalarMacroDefinition(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_MACRO_DEFINITION_OPS;
	static void InitializeTableMacroDefinition(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableMacroDefinition(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_SCHEMA_STMT_OPS;
	static void InitializeCreateSchemaStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateSchemaStmt(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_SECRET_STMT_OPS;
	static void InitializeCreateSecretStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateSecretStmt(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SECRET_STORAGE_SPECIFIER_OPS;
	static void InitializeSecretStorageSpecifier(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSecretStorageSpecifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SECRET_NAME_OPS;
	static void InitializeSecretName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSecretName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_SEQUENCE_STMT_OPS;
	static void InitializeCreateSequenceStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateSequenceStmt(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SEQUENCE_OPTION_OPS;
	static void InitializeSequenceOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSequenceOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SEQ_SET_CYCLE_OPS;
	static void InitializeSeqSetCycle(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSeqSetCycle(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SEQ_CYCLE_OPS;
	static void InitializeSeqCycle(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSeqCycle(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SEQ_NO_CYCLE_OPS;
	static void InitializeSeqNoCycle(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSeqNoCycle(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SEQ_SET_INCREMENT_OPS;
	static void InitializeSeqSetIncrement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSeqSetIncrement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SEQ_SET_MIN_MAX_OPS;
	static void InitializeSeqSetMinMax(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSeqSetMinMax(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SEQ_NO_MIN_MAX_OPS;
	static void InitializeSeqNoMinMax(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSeqNoMinMax(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SEQ_START_WITH_OPS;
	static void InitializeSeqStartWith(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSeqStartWith(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SEQ_OWNED_BY_OPS;
	static void InitializeSeqOwnedBy(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSeqOwnedBy(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SEQ_MIN_OR_MAX_OPS;
	static void InitializeSeqMinOrMax(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSeqMinOrMax(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MIN_VALUE_OPS;
	static void InitializeMinValue(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMinValue(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MAX_VALUE_OPS;
	static void InitializeMaxValue(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMaxValue(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_STATEMENT_OPS;
	static void InitializeCreateStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_STATEMENT_VARIATION_OPS;
	static void InitializeCreateStatementVariation(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateStatementVariation(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OR_REPLACE_OPS;
	static void InitializeOrReplace(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOrReplace(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TEMPORARY_OPS;
	static void InitializeTemporary(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTemporary(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PERSISTENT_OPS;
	static void InitializePersistent(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePersistent(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TEMP_PERSISTENT_OPS;
	static void InitializeTempPersistent(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTempPersistent(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TEMPORARY_PERSISTENT_OPS;
	static void InitializeTemporaryPersistent(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTemporaryPersistent(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_TABLE_STMT_OPS;
	static void InitializeCreateTableStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateTableStmt(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_TABLE_DEFINITION_OPS;
	static void InitializeCreateTableDefinition(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateTableDefinition(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_TABLE_AS_OPS;
	static void InitializeCreateTableAs(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateTableAs(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PARTITION_SORTED_OPTIONS_OPS;
	static void InitializePartitionSortedOptions(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePartitionSortedOptions(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PARTITION_OPT_SORTED_OPTIONS_OPS;
	static void InitializePartitionOptSortedOptions(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePartitionOptSortedOptions(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SORTED_OPT_PARTITION_OPTIONS_OPS;
	static void InitializeSortedOptPartitionOptions(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSortedOptPartitionOptions(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PARTITION_OPTIONS_OPS;
	static void InitializePartitionOptions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePartitionOptions(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SORTED_OPTIONS_OPS;
	static void InitializeSortedOptions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSortedOptions(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITH_DATA_OPS;
	static void InitializeWithData(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithData(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITH_DATA_ONLY_OPS;
	static void InitializeWithDataOnly(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithDataOnly(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITH_NO_DATA_OPS;
	static void InitializeWithNoData(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithNoData(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IDENTIFIER_LIST_OPS;
	static void InitializeIdentifierList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIdentifierList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_COLUMN_LIST_OPS;
	static void InitializeCreateColumnList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateColumnList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IF_NOT_EXISTS_OPS;
	static void InitializeIfNotExists(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIfNotExists(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFIED_NAME_OPS;
	static void InitializeQualifiedName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifiedName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SCHEMA_RESERVED_IDENTIFIER_OR_STRING_LITERAL_OPS;
	static void InitializeSchemaReservedIdentifierOrStringLiteral(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSchemaReservedIdentifierOrStringLiteral(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CATALOG_RESERVED_SCHEMA_IDENTIFIER_OPS;
	static void InitializeCatalogReservedSchemaIdentifier(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCatalogReservedSchemaIdentifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IDENTIFIER_OR_STRING_LITERAL_OPS;
	static void InitializeIdentifierOrStringLiteral(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIdentifierOrStringLiteral(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RESERVED_IDENTIFIER_OR_STRING_LITERAL_OPS;
	static void InitializeReservedIdentifierOrStringLiteral(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReservedIdentifierOrStringLiteral(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CATALOG_QUALIFICATION_OPS;
	static void InitializeCatalogQualification(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCatalogQualification(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SCHEMA_QUALIFICATION_OPS;
	static void InitializeSchemaQualification(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSchemaQualification(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RESERVED_SCHEMA_QUALIFICATION_OPS;
	static void InitializeReservedSchemaQualification(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReservedSchemaQualification(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_QUALIFICATION_OPS;
	static void InitializeTableQualification(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableQualification(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RESERVED_TABLE_QUALIFICATION_OPS;
	static void InitializeReservedTableQualification(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReservedTableQualification(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_TABLE_COLUMN_LIST_OPS;
	static void InitializeCreateTableColumnList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateTableColumnList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_TABLE_COLUMN_ELEMENT_OPS;
	static void InitializeCreateTableColumnElement(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateTableColumnElement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_TABLE_COLUMN_DEFINITION_OPS;
	static void InitializeCreateTableColumnDefinition(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateTableColumnDefinition(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_TABLE_CONSTRAINT_OPS;
	static void InitializeCreateTableConstraint(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateTableConstraint(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLUMN_DEFINITION_OPS;
	static void InitializeColumnDefinition(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColumnDefinition(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLUMN_CONSTRAINT_OPS;
	static void InitializeColumnConstraint(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColumnConstraint(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NOT_NULL_CONSTRAINT_OPS;
	static void InitializeNotNullConstraint(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNotNullConstraint(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NULL_CONSTRAINT_OPS;
	static void InitializeNullConstraint(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNullConstraint(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NOT_NULL_COLUMN_CONSTRAINT_OPS;
	static void InitializeNotNullColumnConstraint(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNotNullColumnConstraint(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNIQUE_CONSTRAINT_OPS;
	static void InitializeUniqueConstraint(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUniqueConstraint(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PRIMARY_KEY_CONSTRAINT_OPS;
	static void InitializePrimaryKeyConstraint(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePrimaryKeyConstraint(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DEFAULT_VALUE_OPS;
	static void InitializeDefaultValue(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDefaultValue(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CHECK_CONSTRAINT_OPS;
	static void InitializeCheckConstraint(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCheckConstraint(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FOREIGN_KEY_CONSTRAINT_OPS;
	static void InitializeForeignKeyConstraint(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeForeignKeyConstraint(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLUMN_COLLATION_OPS;
	static void InitializeColumnCollation(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColumnCollation(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLUMN_COMPRESSION_OPS;
	static void InitializeColumnCompression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColumnCompression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps KEY_ACTIONS_OPS;
	static void InitializeKeyActions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeKeyActions(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_ACTION_OPS;
	static void InitializeUpdateAction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateAction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DELETE_ACTION_OPS;
	static void InitializeDeleteAction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDeleteAction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps KEY_ACTION_OPS;
	static void InitializeKeyAction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeKeyAction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NO_KEY_ACTION_OPS;
	static void InitializeNoKeyAction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNoKeyAction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RESTRICT_KEY_ACTION_OPS;
	static void InitializeRestrictKeyAction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRestrictKeyAction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CASCADE_KEY_ACTION_OPS;
	static void InitializeCascadeKeyAction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCascadeKeyAction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_NULL_KEY_ACTION_OPS;
	static void InitializeSetNullKeyAction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetNullKeyAction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_DEFAULT_KEY_ACTION_OPS;
	static void InitializeSetDefaultKeyAction(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetDefaultKeyAction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TOP_LEVEL_CONSTRAINT_OPS;
	static void InitializeTopLevelConstraint(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTopLevelConstraint(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TOP_LEVEL_CONSTRAINT_LIST_OPS;
	static void InitializeTopLevelConstraintList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTopLevelConstraintList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TOP_PRIMARY_KEY_CONSTRAINT_OPS;
	static void InitializeTopPrimaryKeyConstraint(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTopPrimaryKeyConstraint(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TOP_UNIQUE_CONSTRAINT_OPS;
	static void InitializeTopUniqueConstraint(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTopUniqueConstraint(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TOP_FOREIGN_KEY_CONSTRAINT_OPS;
	static void InitializeTopForeignKeyConstraint(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTopForeignKeyConstraint(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLUMN_ID_LIST_OPS;
	static void InitializeColumnIdList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColumnIdList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DOTTED_IDENTIFIER_OPS;
	static void InitializeDottedIdentifier(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDottedIdentifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DOT_COL_LABEL_OPS;
	static void InitializeDotColLabel(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDotColLabel(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_ID_OPS;
	static void InitializeColId(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColId(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_ID_OR_STRING_OPS;
	static void InitializeColIdOrString(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColIdOrString(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TYPE_FUNC_NAME_OPS;
	static void InitializeTypeFuncName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTypeFuncName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_LABEL_OR_STRING_OPS;
	static void InitializeColLabelOrString(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColLabelOrString(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GENERATED_COLUMN_OPS;
	static void InitializeGeneratedColumn(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGeneratedColumn(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GENERATED_COLUMN_TYPE_OPS;
	static void InitializeGeneratedColumnType(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGeneratedColumnType(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMIT_ACTION_OPS;
	static void InitializeCommitAction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommitAction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PRESERVE_OR_DELETE_OPS;
	static void InitializePreserveOrDelete(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePreserveOrDelete(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PRESERVE_ROWS_OPS;
	static void InitializePreserveRows(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePreserveRows(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DELETE_ROWS_OPS;
	static void InitializeDeleteRows(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDeleteRows(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VIRTUAL_GENERATED_COLUMN_OPS;
	static void InitializeVirtualGeneratedColumn(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeVirtualGeneratedColumn(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps STORED_GENERATED_COLUMN_OPS;
	static void InitializeStoredGeneratedColumn(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeStoredGeneratedColumn(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_TRIGGER_STMT_OPS;
	static void InitializeCreateTriggerStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateTriggerStmt(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_BODY_OPS;
	static void InitializeTriggerBody(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerBody(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_NAME_OPS;
	static void InitializeTriggerName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REFERENCING_CLAUSE_OPS;
	static void InitializeReferencingClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReferencingClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REFERENCING_ITEM_OPS;
	static void InitializeReferencingItem(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReferencingItem(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REFERENCING_NEW_TABLE_AS_OPS;
	static void InitializeReferencingNewTableAs(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReferencingNewTableAs(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REFERENCING_OLD_TABLE_AS_OPS;
	static void InitializeReferencingOldTableAs(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReferencingOldTableAs(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_TIMING_OPS;
	static void InitializeTriggerTiming(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerTiming(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_BEFORE_OPS;
	static void InitializeTriggerBefore(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerBefore(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_AFTER_OPS;
	static void InitializeTriggerAfter(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerAfter(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_INSTEAD_OF_OPS;
	static void InitializeTriggerInsteadOf(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerInsteadOf(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_EVENT_OPS;
	static void InitializeTriggerEvent(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerEvent(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_EVENT_INSERT_OPS;
	static void InitializeTriggerEventInsert(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerEventInsert(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_EVENT_DELETE_OPS;
	static void InitializeTriggerEventDelete(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerEventDelete(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_EVENT_UPDATE_OPS;
	static void InitializeTriggerEventUpdate(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerEventUpdate(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_EVENT_UPDATE_OF_OPS;
	static void InitializeTriggerEventUpdateOf(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerEventUpdateOf(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIGGER_COLUMN_LIST_OPS;
	static void InitializeTriggerColumnList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTriggerColumnList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FOR_EACH_CLAUSE_OPS;
	static void InitializeForEachClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeForEachClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FOR_EACH_ROW_OPS;
	static void InitializeForEachRow(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeForEachRow(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FOR_EACH_STATEMENT_OPS;
	static void InitializeForEachStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeForEachStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_TYPE_STMT_OPS;
	static void InitializeCreateTypeStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateTypeStmt(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_TYPE_OPS;
	static void InitializeCreateType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_TYPE_FROM_TYPE_OPS;
	static void InitializeCreateTypeFromType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateTypeFromType(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ENUM_SELECT_TYPE_OPS;
	static void InitializeEnumSelectType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeEnumSelectType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ENUM_STRING_LITERAL_LIST_OPS;
	static void InitializeEnumStringLiteralList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeEnumStringLiteralList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_VIEW_STMT_OPS;
	static void InitializeCreateViewStmt(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateViewStmt(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CREATE_RECURSIVE_OPS;
	static void InitializeCreateRecursive(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCreateRecursive(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DEALLOCATE_STATEMENT_OPS;
	static void InitializeDeallocateStatement(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDeallocateStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DEALLOCATE_PREPARE_OPS;
	static void InitializeDeallocatePrepare(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDeallocatePrepare(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DELETE_STATEMENT_OPS;
	static void InitializeDeleteStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDeleteStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRUNCATE_STATEMENT_OPS;
	static void InitializeTruncateStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTruncateStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TARGET_OPT_ALIAS_OPS;
	static void InitializeTargetOptAlias(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTargetOptAlias(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DELETE_USING_CLAUSE_OPS;
	static void InitializeDeleteUsingClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDeleteUsingClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DESCRIBE_STATEMENT_OPS;
	static void InitializeDescribeStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDescribeStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SHOW_SELECT_OPS;
	static void InitializeShowSelect(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeShowSelect(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SHOW_ALL_TABLES_OPS;
	static void InitializeShowAllTables(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeShowAllTables(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SHOW_QUALIFIED_NAME_OPS;
	static void InitializeShowQualifiedName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeShowQualifiedName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SHOW_TABLES_OPS;
	static void InitializeShowTables(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeShowTables(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DESCRIBE_TARGET_OPS;
	static void InitializeDescribeTarget(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDescribeTarget(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DESCRIBE_BASE_TABLE_NAME_OPS;
	static void InitializeDescribeBaseTableName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDescribeBaseTableName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DESCRIBE_STRING_LITERAL_OPS;
	static void InitializeDescribeStringLiteral(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDescribeStringLiteral(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SHOW_OR_DESCRIBE_OR_SUMMARIZE_OPS;
	static void InitializeShowOrDescribeOrSummarize(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeShowOrDescribeOrSummarize(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUMMARIZE_OPS;
	static void InitializeSummarize(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSummarize(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUMMARIZE_RULE_OPS;
	static void InitializeSummarizeRule(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSummarizeRule(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SHOW_OR_DESCRIBE_OPS;
	static void InitializeShowOrDescribe(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeShowOrDescribe(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SHOW_RULE_OPS;
	static void InitializeShowRule(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeShowRule(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DESCRIBE_RULE_OPS;
	static void InitializeDescribeRule(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDescribeRule(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DESCRIBE_LONG_RULE_OPS;
	static void InitializeDescribeLongRule(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDescribeLongRule(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DESC_RULE_OPS;
	static void InitializeDescRule(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDescRule(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DETACH_STATEMENT_OPS;
	static void InitializeDetachStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDetachStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_STATEMENT_OPS;
	static void InitializeDropStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_ENTRIES_OPS;
	static void InitializeDropEntries(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropEntries(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_TRIGGER_OPS;
	static void InitializeDropTrigger(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropTrigger(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_TABLE_OPS;
	static void InitializeDropTable(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropTable(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_TABLE_FUNCTION_OPS;
	static void InitializeDropTableFunction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropTableFunction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_FUNCTION_OPS;
	static void InitializeDropFunction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropFunction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_SCHEMA_OPS;
	static void InitializeDropSchema(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropSchema(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_INDEX_OPS;
	static void InitializeDropIndex(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropIndex(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFIED_INDEX_NAME_OPS;
	static void InitializeQualifiedIndexName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifiedIndexName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFIED_INDEX_NAME_STRING_OPS;
	static void InitializeQualifiedIndexNameString(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifiedIndexNameString(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SCHEMA_RESERVED_INDEX_OPS;
	static void InitializeSchemaReservedIndex(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSchemaReservedIndex(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CATALOG_RESERVED_SCHEMA_INDEX_OPS;
	static void InitializeCatalogReservedSchemaIndex(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCatalogReservedSchemaIndex(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_SEQUENCE_OPS;
	static void InitializeDropSequence(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropSequence(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_COLLATION_OPS;
	static void InitializeDropCollation(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropCollation(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_TYPE_OPS;
	static void InitializeDropType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_SECRET_OPS;
	static void InitializeDropSecret(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropSecret(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_OR_VIEW_OPS;
	static void InitializeTableOrView(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableOrView(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MATERIALIZED_VIEW_ENTRY_OPS;
	static void InitializeMaterializedViewEntry(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMaterializedViewEntry(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FUNCTION_TYPE_MACRO_OPS;
	static void InitializeFunctionTypeMacro(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFunctionTypeMacro(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FUNCTION_TYPE_MACRO_KEYWORD_OPS;
	static void InitializeFunctionTypeMacroKeyword(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFunctionTypeMacroKeyword(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FUNCTION_TYPE_FUNCTION_OPS;
	static void InitializeFunctionTypeFunction(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFunctionTypeFunction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_BEHAVIOR_OPS;
	static void InitializeDropBehavior(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropBehavior(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CASCADE_DROP_BEHAVIOR_OPS;
	static void InitializeCascadeDropBehavior(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCascadeDropBehavior(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RESTRICT_DROP_BEHAVIOR_OPS;
	static void InitializeRestrictDropBehavior(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRestrictDropBehavior(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IF_EXISTS_OPS;
	static void InitializeIfExists(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIfExists(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFIED_SCHEMA_NAME_OPS;
	static void InitializeQualifiedSchemaName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifiedSchemaName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFIED_SCHEMA_NAME_STRING_OPS;
	static void InitializeQualifiedSchemaNameString(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifiedSchemaNameString(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CATALOG_RESERVED_SCHEMA_OPS;
	static void InitializeCatalogReservedSchema(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCatalogReservedSchema(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DROP_SECRET_STORAGE_OPS;
	static void InitializeDropSecretStorage(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDropSecretStorage(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXECUTE_STATEMENT_OPS;
	static void InitializeExecuteStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExecuteStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPLAIN_STATEMENT_OPS;
	static void InitializeExplainStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExplainStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPLAIN_ANALYZE_OPS;
	static void InitializeExplainAnalyze(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExplainAnalyze(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPLAIN_OPTION_LIST_OPS;
	static void InitializeExplainOptionList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExplainOptionList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPLAIN_OPTION_OPS;
	static void InitializeExplainOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExplainOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPLAIN_SELECT_STATEMENT_OPS;
	static void InitializeExplainSelectStatement(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExplainSelectStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPLAINABLE_STATEMENTS_OPS;
	static void InitializeExplainableStatements(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExplainableStatements(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPORT_STATEMENT_OPS;
	static void InitializeExportStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExportStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPORT_SOURCE_OPS;
	static void InitializeExportSource(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExportSource(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IMPORT_STATEMENT_OPS;
	static void InitializeImportStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeImportStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLUMN_REFERENCE_OPS;
	static void InitializeColumnReference(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColumnReference(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CATALOG_RESERVED_SCHEMA_TABLE_COLUMN_NAME_OPS;
	static void InitializeCatalogReservedSchemaTableColumnName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCatalogReservedSchemaTableColumnName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SCHEMA_RESERVED_TABLE_COLUMN_NAME_OPS;
	static void InitializeSchemaReservedTableColumnName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSchemaReservedTableColumnName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_RESERVED_COLUMN_NAME_OPS;
	static void InitializeTableReservedColumnName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableReservedColumnName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FUNCTION_EXPRESSION_OPS;
	static void InitializeFunctionExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFunctionExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FUNCTION_EXPRESSION_ARGUMENTS_OPS;
	static void InitializeFunctionExpressionArguments(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFunctionExpressionArguments(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FUNCTION_EXPRESSION_ARGUMENT_LIST_OPS;
	static void InitializeFunctionExpressionArgumentList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFunctionExpressionArgumentList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FUNCTION_ARGUMENT_LIST_OPS;
	static void InitializeFunctionArgumentList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFunctionArgumentList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FUNCTION_IDENTIFIER_OPS;
	static void InitializeFunctionIdentifier(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFunctionIdentifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CATALOG_RESERVED_SCHEMA_FUNCTION_NAME_OPS;
	static void InitializeCatalogReservedSchemaFunctionName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCatalogReservedSchemaFunctionName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SCHEMA_RESERVED_FUNCTION_NAME_OPS;
	static void InitializeSchemaReservedFunctionName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSchemaReservedFunctionName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DISTINCT_OR_ALL_OPS;
	static void InitializeDistinctOrAll(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDistinctOrAll(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DISTINCT_KEYWORD_OPS;
	static void InitializeDistinctKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDistinctKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALL_KEYWORD_OPS;
	static void InitializeAllKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAllKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITHIN_GROUP_CLAUSE_OPS;
	static void InitializeWithinGroupClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithinGroupClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FILTER_CLAUSE_OPS;
	static void InitializeFilterClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFilterClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FILTER_CLAUSE_EXPRESSION_OPS;
	static void InitializeFilterClauseExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFilterClauseExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FILTER_CLAUSE_CONTENTS_OPS;
	static void InitializeFilterClauseContents(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFilterClauseContents(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IGNORE_OR_RESPECT_NULLS_OPS;
	static void InitializeIgnoreOrRespectNulls(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIgnoreOrRespectNulls(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IGNORE_NULLS_OPS;
	static void InitializeIgnoreNulls(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIgnoreNulls(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RESPECT_NULLS_OPS;
	static void InitializeRespectNulls(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRespectNulls(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PARENTHESIS_EXPRESSION_OPS;
	static void InitializeParenthesisExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeParenthesisExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LITERAL_EXPRESSION_OPS;
	static void InitializeLiteralExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLiteralExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CONSTANT_LITERAL_OPS;
	static void InitializeConstantLiteral(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeConstantLiteral(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NULL_LITERAL_OPS;
	static void InitializeNullLiteral(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNullLiteral(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRUE_LITERAL_OPS;
	static void InitializeTrueLiteral(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTrueLiteral(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FALSE_LITERAL_OPS;
	static void InitializeFalseLiteral(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFalseLiteral(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CAST_EXPRESSION_OPS;
	static void InitializeCastExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCastExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CAST_ARGUMENTS_OPS;
	static void InitializeCastArguments(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCastArguments(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CAST_OR_TRY_CAST_OPS;
	static void InitializeCastOrTryCast(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCastOrTryCast(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CAST_KEYWORD_OPS;
	static void InitializeCastKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCastKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRY_CAST_KEYWORD_OPS;
	static void InitializeTryCastKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTryCastKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_ID_DOT_OPS;
	static void InitializeColIdDot(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColIdDot(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps STAR_EXPRESSION_OPS;
	static void InitializeStarExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeStarExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps STAR_QUALIFIER_LIST_OPS;
	static void InitializeStarQualifierList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeStarQualifierList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_LIST_OPS;
	static void InitializeExcludeList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_NAMES_OPS;
	static void InitializeExcludeNames(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeNames(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_NAME_LIST_OPS;
	static void InitializeExcludeNameList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeNameList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_NAME_SINGLE_OPS;
	static void InitializeExcludeNameSingle(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeNameSingle(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_NAME_OPS;
	static void InitializeExcludeName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_DOTTED_NAME_OPS;
	static void InitializeExcludeDottedName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeDottedName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_COLUMN_NAME_OPS;
	static void InitializeExcludeColumnName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeColumnName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REPLACE_LIST_OPS;
	static void InitializeReplaceList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReplaceList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REPLACE_ENTRIES_OPS;
	static void InitializeReplaceEntries(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReplaceEntries(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REPLACE_ENTRY_SINGLE_OPS;
	static void InitializeReplaceEntrySingle(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReplaceEntrySingle(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REPLACE_ENTRY_LIST_OPS;
	static void InitializeReplaceEntryList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReplaceEntryList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REPLACE_ENTRY_OPS;
	static void InitializeReplaceEntry(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReplaceEntry(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RENAME_LIST_OPS;
	static void InitializeRenameList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRenameList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RENAME_ENTRIES_OPS;
	static void InitializeRenameEntries(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRenameEntries(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RENAME_ENTRY_LIST_OPS;
	static void InitializeRenameEntryList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRenameEntryList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SINGLE_RENAME_ENTRY_OPS;
	static void InitializeSingleRenameEntry(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSingleRenameEntry(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RENAME_ENTRY_OPS;
	static void InitializeRenameEntry(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRenameEntry(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBQUERY_EXPRESSION_OPS;
	static void InitializeSubqueryExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubqueryExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBQUERY_NOT_OPS;
	static void InitializeSubqueryNot(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubqueryNot(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBQUERY_EXISTS_OPS;
	static void InitializeSubqueryExists(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubqueryExists(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CASE_EXPRESSION_OPS;
	static void InitializeCaseExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCaseExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CASE_WHEN_THEN_OPS;
	static void InitializeCaseWhenThen(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCaseWhenThen(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CASE_ELSE_OPS;
	static void InitializeCaseElse(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCaseElse(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TYPE_LITERAL_OPS;
	static void InitializeTypeLiteral(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTypeLiteral(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_LITERAL_OPS;
	static void InitializeIntervalLiteral(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalLiteral(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_PARAMETER_OPS;
	static void InitializeIntervalParameter(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalParameter(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERVAL_STRING_PARAMETER_OPS;
	static void InitializeIntervalStringParameter(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntervalStringParameter(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FRAME_CLAUSE_OPS;
	static void InitializeFrameClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFrameClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FRAMING_OPS;
	static void InitializeFraming(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFraming(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ROWS_FRAMING_OPS;
	static void InitializeRowsFraming(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRowsFraming(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RANGE_FRAMING_OPS;
	static void InitializeRangeFraming(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRangeFraming(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUPS_FRAMING_OPS;
	static void InitializeGroupsFraming(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupsFraming(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FRAME_EXTENT_OPS;
	static void InitializeFrameExtent(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFrameExtent(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SINGLE_FRAME_EXTENT_OPS;
	static void InitializeSingleFrameExtent(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSingleFrameExtent(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BETWEEN_FRAME_EXTENT_OPS;
	static void InitializeBetweenFrameExtent(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBetweenFrameExtent(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FRAME_BOUND_OPS;
	static void InitializeFrameBound(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFrameBound(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FRAME_UNBOUNDED_OPS;
	static void InitializeFrameUnbounded(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFrameUnbounded(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FRAME_EXPRESSION_OPS;
	static void InitializeFrameExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFrameExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FRAME_CURRENT_ROW_OPS;
	static void InitializeFrameCurrentRow(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFrameCurrentRow(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PRECEDING_OR_FOLLOWING_OPS;
	static void InitializePrecedingOrFollowing(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePrecedingOrFollowing(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PRECEDING_FRAME_OPS;
	static void InitializePrecedingFrame(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePrecedingFrame(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FOLLOWING_FRAME_OPS;
	static void InitializeFollowingFrame(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFollowingFrame(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WINDOW_EXCLUDE_CLAUSE_OPS;
	static void InitializeWindowExcludeClause(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWindowExcludeClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WINDOW_EXCLUDE_ELEMENT_OPS;
	static void InitializeWindowExcludeElement(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWindowExcludeElement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_CURRENT_ROW_OPS;
	static void InitializeExcludeCurrentRow(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeCurrentRow(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_GROUP_OPS;
	static void InitializeExcludeGroup(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeGroup(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_TIES_OPS;
	static void InitializeExcludeTies(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeTies(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_NO_OTHERS_OPS;
	static void InitializeExcludeNoOthers(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeNoOthers(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OVER_CLAUSE_OPS;
	static void InitializeOverClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOverClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WINDOW_FRAME_OPS;
	static void InitializeWindowFrame(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWindowFrame(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PARENS_IDENTIFIER_OPS;
	static void InitializeParensIdentifier(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeParensIdentifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WINDOW_FRAME_DEFINITION_OPS;
	static void InitializeWindowFrameDefinition(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWindowFrameDefinition(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WINDOW_FRAME_NAME_CONTENTS_PARENS_OPS;
	static void InitializeWindowFrameNameContentsParens(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWindowFrameNameContentsParens(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WINDOW_FRAME_NAME_CONTENTS_OPS;
	static void InitializeWindowFrameNameContents(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWindowFrameNameContents(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WINDOW_FRAME_CONTENTS_PARENS_OPS;
	static void InitializeWindowFrameContentsParens(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWindowFrameContentsParens(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WINDOW_FRAME_CONTENTS_OPS;
	static void InitializeWindowFrameContents(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWindowFrameContents(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BASE_WINDOW_NAME_OPS;
	static void InitializeBaseWindowName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBaseWindowName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WINDOW_PARTITION_OPS;
	static void InitializeWindowPartition(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWindowPartition(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIST_EXPRESSION_OPS;
	static void InitializeListExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeListExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ARRAY_BOUNDED_LIST_EXPRESSION_OPS;
	static void InitializeArrayBoundedListExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeArrayBoundedListExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ARRAY_PARENS_SELECT_OPS;
	static void InitializeArrayParensSelect(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeArrayParensSelect(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BOUNDED_LIST_EXPRESSION_OPS;
	static void InitializeBoundedListExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBoundedListExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps STRUCT_EXPRESSION_OPS;
	static void InitializeStructExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeStructExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps STRUCT_FIELD_OPS;
	static void InitializeStructField(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeStructField(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MAP_EXPRESSION_OPS;
	static void InitializeMapExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMapExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MAP_STRUCT_EXPRESSION_OPS;
	static void InitializeMapStructExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMapStructExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MAP_STRUCT_FIELD_OPS;
	static void InitializeMapStructField(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMapStructField(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUPING_EXPRESSION_OPS;
	static void InitializeGroupingExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupingExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUPING_OR_GROUPING_ID_OPS;
	static void InitializeGroupingOrGroupingId(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupingOrGroupingId(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUPING_KEYWORD_OPS;
	static void InitializeGroupingKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupingKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUPING_ID_KEYWORD_OPS;
	static void InitializeGroupingIdKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupingIdKeyword(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PARAMETER_OPS;
	static void InitializeParameter(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeParameter(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUESTION_MARK_NUMBERED_PARAMETER_OPS;
	static void InitializeQuestionMarkNumberedParameter(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQuestionMarkNumberedParameter(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ANONYMOUS_PARAMETER_OPS;
	static void InitializeAnonymousParameter(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAnonymousParameter(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NUMBERED_PARAMETER_OPS;
	static void InitializeNumberedParameter(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNumberedParameter(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_LABEL_PARAMETER_OPS;
	static void InitializeColLabelParameter(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColLabelParameter(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps POSITIONAL_EXPRESSION_OPS;
	static void InitializePositionalExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePositionalExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DEFAULT_EXPRESSION_OPS;
	static void InitializeDefaultExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDefaultExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIST_COMPREHENSION_EXPRESSION_OPS;
	static void InitializeListComprehensionExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeListComprehensionExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIST_COMPREHENSION_FILTER_OPS;
	static void InitializeListComprehensionFilter(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeListComprehensionFilter(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PARENS_EXPRESSION_OPS;
	static void InitializeParensExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeParensExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SINGLE_EXPRESSION_OPS;
	static void InitializeSingleExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSingleExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPRESSION_OPS;
	static void InitializeExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLUMN_DEFAULT_EXPR_OPS;
	static void InitializeColumnDefaultExpr(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColumnDefaultExpr(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LAMBDA_ARROW_EXPRESSION_OPS;
	static void InitializeLambdaArrowExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLambdaArrowExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SINGLE_ARROW_PAIR_OPS;
	static void InitializeSingleArrowPair(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSingleArrowPair(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LOGICAL_OR_EXPRESSION_OPS;
	static void InitializeLogicalOrExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLogicalOrExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LOGICAL_OR_EXPRESSION_TAIL_OPS;
	static void InitializeLogicalOrExpressionTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLogicalOrExpressionTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_DEF_OR_EXPR_OPS;
	static void InitializeColDefOrExpr(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColDefOrExpr(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_DEF_OR_EXPRESSION_TAIL_OPS;
	static void InitializeColDefOrExpressionTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColDefOrExpressionTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LOGICAL_AND_EXPRESSION_OPS;
	static void InitializeLogicalAndExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLogicalAndExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LOGICAL_AND_EXPRESSION_TAIL_OPS;
	static void InitializeLogicalAndExpressionTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLogicalAndExpressionTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_DEF_AND_EXPR_OPS;
	static void InitializeColDefAndExpr(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColDefAndExpr(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_DEF_AND_EXPRESSION_TAIL_OPS;
	static void InitializeColDefAndExpressionTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColDefAndExpressionTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LOGICAL_NOT_EXPRESSION_OPS;
	static void InitializeLogicalNotExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLogicalNotExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NOT_EXPRESSION_OPS;
	static void InitializeNotExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNotExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NOT_KEYWORD_OPS;
	static void InitializeNotKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNotKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IS_EXPRESSION_OPS;
	static void InitializeIsExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIsExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IS_TEST_OPS;
	static void InitializeIsTest(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIsTest(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IS_LITERAL_OPS;
	static void InitializeIsLiteral(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIsLiteral(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IS_LITERAL_VALUE_OPS;
	static void InitializeIsLiteralValue(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIsLiteralValue(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNKNOWN_LITERAL_OPS;
	static void InitializeUnknownLiteral(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUnknownLiteral(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NOT_NULL_OPS;
	static void InitializeNotNull(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNotNull(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NOT_NULL_KEYWORD_OPS;
	static void InitializeNotNullKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNotNullKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NOT_NULL_OPERATOR_OPS;
	static void InitializeNotNullOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNotNullOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IS_NULL_OPS;
	static void InitializeIsNull(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIsNull(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IS_NULL_OPERATOR_OPS;
	static void InitializeIsNullOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIsNullOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IS_DISTINCT_FROM_EXPRESSION_OPS;
	static void InitializeIsDistinctFromExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIsDistinctFromExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IS_DISTINCT_FROM_TAIL_OPS;
	static void InitializeIsDistinctFromTail(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIsDistinctFromTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IS_DISTINCT_FROM_OP_OPS;
	static void InitializeIsDistinctFromOp(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIsDistinctFromOp(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMPARISON_EXPRESSION_OPS;
	static void InitializeComparisonExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeComparisonExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMPARISON_EXPRESSION_TAIL_OPS;
	static void InitializeComparisonExpressionTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeComparisonExpressionTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMPARISON_OPERATOR_OPS;
	static void InitializeComparisonOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeComparisonOperator(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OPERATOR_EQUAL_OPS;
	static void InitializeOperatorEqual(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOperatorEqual(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OPERATOR_NOT_EQUAL_OPS;
	static void InitializeOperatorNotEqual(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOperatorNotEqual(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OPERATOR_LESS_THAN_OPS;
	static void InitializeOperatorLessThan(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOperatorLessThan(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OPERATOR_GREATER_THAN_OPS;
	static void InitializeOperatorGreaterThan(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOperatorGreaterThan(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OPERATOR_LESS_THAN_EQUALS_OPS;
	static void InitializeOperatorLessThanEquals(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOperatorLessThanEquals(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OPERATOR_GREATER_THAN_EQUALS_OPS;
	static void InitializeOperatorGreaterThanEquals(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOperatorGreaterThanEquals(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BETWEEN_IN_LIKE_EXPRESSION_OPS;
	static void InitializeBetweenInLikeExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBetweenInLikeExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BETWEEN_IN_LIKE_OP_OPS;
	static void InitializeBetweenInLikeOp(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBetweenInLikeOp(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BETWEEN_IN_LIKE_OP_EXPRESSION_OPS;
	static void InitializeBetweenInLikeOpExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBetweenInLikeOpExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIKE_CLAUSE_OPS;
	static void InitializeLikeClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLikeClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ESCAPE_CLAUSE_OPS;
	static void InitializeEscapeClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeEscapeClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIKE_VARIATIONS_OPS;
	static void InitializeLikeVariations(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLikeVariations(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIKE_TOKEN_OPS;
	static void InitializeLikeToken(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLikeToken(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps I_LIKE_TOKEN_OPS;
	static void InitializeILikeToken(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeILikeToken(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GLOB_TOKEN_OPS;
	static void InitializeGlobToken(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGlobToken(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SIMILAR_TO_TOKEN_OPS;
	static void InitializeSimilarToToken(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSimilarToToken(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NOT_I_LIKE_OP_OPS;
	static void InitializeNotILikeOp(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNotILikeOp(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NOT_LIKE_OP_OPS;
	static void InitializeNotLikeOp(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNotLikeOp(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NOT_SIMILAR_TO_OP_OPS;
	static void InitializeNotSimilarToOp(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNotSimilarToOp(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IN_CLAUSE_OPS;
	static void InitializeInClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IN_EXPRESSION_OPS;
	static void InitializeInExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IN_CONTAINS_EXPRESSION_OPS;
	static void InitializeInContainsExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInContainsExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IN_EXPRESSION_LIST_OPS;
	static void InitializeInExpressionList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInExpressionList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps IN_SELECT_STATEMENT_OPS;
	static void InitializeInSelectStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInSelectStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BETWEEN_CLAUSE_OPS;
	static void InitializeBetweenClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBetweenClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OTHER_OPERATOR_EXPRESSION_OPS;
	static void InitializeOtherOperatorExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOtherOperatorExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OTHER_OPERATOR_TAIL_OPS;
	static void InitializeOtherOperatorTail(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOtherOperatorTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OTHER_OPERATOR_OPS;
	static void InitializeOtherOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOtherOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ANY_ALL_OPERATOR_OPS;
	static void InitializeAnyAllOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAnyAllOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ANY_OR_ALL_OPS;
	static void InitializeAnyOrAll(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAnyOrAll(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBQUERY_ANY_OPS;
	static void InitializeSubqueryAny(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubqueryAny(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBQUERY_ALL_OPS;
	static void InitializeSubqueryAll(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubqueryAll(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INET_OPERATOR_OPS;
	static void InitializeInetOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInetOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps JSON_OPERATOR_OPS;
	static void InitializeJsonOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeJsonOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIST_OPERATOR_OPS;
	static void InitializeListOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeListOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps STRING_OPERATOR_OPS;
	static void InitializeStringOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeStringOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFIED_OPERATOR_OPS;
	static void InitializeQualifiedOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifiedOperator(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFIED_OPERATOR_CONTENTS_OPS;
	static void InitializeQualifiedOperatorContents(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifiedOperatorContents(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ANY_OP_OPS;
	static void InitializeAnyOp(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAnyOp(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BITWISE_EXPRESSION_OPS;
	static void InitializeBitwiseExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBitwiseExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BITWISE_EXPRESSION_TAIL_OPS;
	static void InitializeBitwiseExpressionTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBitwiseExpressionTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BIT_OPERATOR_OPS;
	static void InitializeBitOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBitOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ADDITIVE_EXPRESSION_OPS;
	static void InitializeAdditiveExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAdditiveExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ADDITIVE_EXPRESSION_TAIL_OPS;
	static void InitializeAdditiveExpressionTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAdditiveExpressionTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TERM_OPS;
	static void InitializeTerm(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTerm(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MULTIPLICATIVE_EXPRESSION_OPS;
	static void InitializeMultiplicativeExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMultiplicativeExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MULTIPLICATIVE_EXPRESSION_TAIL_OPS;
	static void InitializeMultiplicativeExpressionTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMultiplicativeExpressionTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FACTOR_OPS;
	static void InitializeFactor(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFactor(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPONENTIATION_EXPRESSION_OPS;
	static void InitializeExponentiationExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExponentiationExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPONENTIATION_EXPRESSION_TAIL_OPS;
	static void InitializeExponentiationExpressionTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExponentiationExpressionTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPONENT_OPERATOR_OPS;
	static void InitializeExponentOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExponentOperator(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLLATE_EXPRESSION_OPS;
	static void InitializeCollateExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCollateExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLLATE_EXPRESSION_TAIL_OPS;
	static void InitializeCollateExpressionTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCollateExpressionTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps AT_TIME_ZONE_EXPRESSION_OPS;
	static void InitializeAtTimeZoneExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAtTimeZoneExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps AT_TIME_ZONE_EXPRESSION_TAIL_OPS;
	static void InitializeAtTimeZoneExpressionTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAtTimeZoneExpressionTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PREFIX_EXPRESSION_OPS;
	static void InitializePrefixExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePrefixExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PREFIX_OPERATOR_OPS;
	static void InitializePrefixOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePrefixOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MINUS_PREFIX_OPERATOR_OPS;
	static void InitializeMinusPrefixOperator(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMinusPrefixOperator(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PLUS_PREFIX_OPERATOR_OPS;
	static void InitializePlusPrefixOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePlusPrefixOperator(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TILDE_PREFIX_OPERATOR_OPS;
	static void InitializeTildePrefixOperator(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTildePrefixOperator(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BASE_EXPRESSION_OPS;
	static void InitializeBaseExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBaseExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INDIRECTION_LIST_OPS;
	static void InitializeIndirectionList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIndirectionList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INDIRECTION_OPS;
	static void InitializeIndirection(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIndirection(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CAST_OPERATOR_OPS;
	static void InitializeCastOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCastOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DOT_OPERATOR_OPS;
	static void InitializeDotOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDotOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DOT_METHOD_OPERATOR_OPS;
	static void InitializeDotMethodOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDotMethodOperator(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DOT_COLUMN_OPERATOR_OPS;
	static void InitializeDotColumnOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDotColumnOperator(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps METHOD_EXPRESSION_OPS;
	static void InitializeMethodExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMethodExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps METHOD_EXPRESSION_ARGUMENTS_OPS;
	static void InitializeMethodExpressionArguments(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMethodExpressionArguments(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps METHOD_EXPRESSION_ARGUMENT_LIST_OPS;
	static void InitializeMethodExpressionArgumentList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMethodExpressionArgumentList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps METHOD_FUNCTION_ARGUMENTS_OPS;
	static void InitializeMethodFunctionArguments(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMethodFunctionArguments(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SLICE_EXPRESSION_OPS;
	static void InitializeSliceExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSliceExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SLICE_BOUND_OPS;
	static void InitializeSliceBound(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSliceBound(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps END_SLICE_BOUND_OPS;
	static void InitializeEndSliceBound(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeEndSliceBound(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps END_SLICE_VALUE_OPS;
	static void InitializeEndSliceValue(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeEndSliceValue(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps END_SLICE_MINUS_OPS;
	static void InitializeEndSliceMinus(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeEndSliceMinus(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps STEP_SLICE_BOUND_OPS;
	static void InitializeStepSliceBound(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeStepSliceBound(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps POSTFIX_OPERATOR_OPS;
	static void InitializePostfixOperator(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePostfixOperator(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SPECIAL_FUNCTION_EXPRESSION_OPS;
	static void InitializeSpecialFunctionExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSpecialFunctionExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COALESCE_EXPRESSION_OPS;
	static void InitializeCoalesceExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCoalesceExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNPACK_EXPRESSION_OPS;
	static void InitializeUnpackExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUnpackExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRY_EXPRESSION_OPS;
	static void InitializeTryExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTryExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLUMNS_EXPRESSION_OPS;
	static void InitializeColumnsExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColumnsExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXTRACT_EXPRESSION_OPS;
	static void InitializeExtractExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExtractExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXTRACT_ARGUMENTS_OPS;
	static void InitializeExtractArguments(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExtractArguments(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LAMBDA_EXPRESSION_OPS;
	static void InitializeLambdaExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLambdaExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NULL_IF_EXPRESSION_OPS;
	static void InitializeNullIfExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNullIfExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NULL_IF_ARGUMENTS_OPS;
	static void InitializeNullIfArguments(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNullIfArguments(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps POSITION_EXPRESSION_OPS;
	static void InitializePositionExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePositionExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps POSITION_ARGUMENTS_OPS;
	static void InitializePositionArguments(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePositionArguments(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ROW_EXPRESSION_OPS;
	static void InitializeRowExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRowExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBSTRING_EXPRESSION_OPS;
	static void InitializeSubstringExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubstringExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBSTRING_ARGUMENTS_OPS;
	static void InitializeSubstringArguments(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubstringArguments(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBSTRING_EXPRESSION_LIST_OPS;
	static void InitializeSubstringExpressionList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubstringExpressionList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBSTRING_PARAMETERS_OPS;
	static void InitializeSubstringParameters(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubstringParameters(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBSTRING_FROM_FOR_OPS;
	static void InitializeSubstringFromFor(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubstringFromFor(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBSTRING_FROM_OPTIONAL_FOR_OPS;
	static void InitializeSubstringFromOptionalFor(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubstringFromOptionalFor(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBSTRING_FOR_OPS;
	static void InitializeSubstringFor(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubstringFor(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIM_EXPRESSION_OPS;
	static void InitializeTrimExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTrimExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIM_ARGUMENTS_OPS;
	static void InitializeTrimArguments(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTrimArguments(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIM_DIRECTION_OPS;
	static void InitializeTrimDirection(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTrimDirection(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIM_BOTH_OPS;
	static void InitializeTrimBoth(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTrimBoth(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIM_LEADING_OPS;
	static void InitializeTrimLeading(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTrimLeading(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIM_TRAILING_OPS;
	static void InitializeTrimTrailing(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTrimTrailing(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRIM_SOURCE_OPS;
	static void InitializeTrimSource(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTrimSource(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OVERLAY_EXPRESSION_OPS;
	static void InitializeOverlayExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOverlayExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OVERLAY_ARGUMENTS_OPS;
	static void InitializeOverlayArguments(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOverlayArguments(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OVERLAY_PARAMETERS_OPS;
	static void InitializeOverlayParameters(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOverlayParameters(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FROM_EXPRESSION_OPS;
	static void InitializeFromExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFromExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FOR_EXPRESSION_OPS;
	static void InitializeForExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeForExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OVERLAY_EXPRESSION_LIST_OPS;
	static void InitializeOverlayExpressionList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOverlayExpressionList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXTRACT_ARGUMENT_OPS;
	static void InitializeExtractArgument(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExtractArgument(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXTRACT_DATE_PART_ARGUMENT_OPS;
	static void InitializeExtractDatePartArgument(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExtractDatePartArgument(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXTRACT_IDENTIFIER_ARGUMENT_OPS;
	static void InitializeExtractIdentifierArgument(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExtractIdentifierArgument(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXTRACT_STRING_ARGUMENT_OPS;
	static void InitializeExtractStringArgument(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExtractStringArgument(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXTRACT_DATE_PART_OPS;
	static void InitializeExtractDatePart(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExtractDatePart(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_STATEMENT_OPS;
	static void InitializeInsertStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OR_ACTION_OPS;
	static void InitializeOrAction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOrAction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_OR_REPLACE_OPS;
	static void InitializeInsertOrReplace(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertOrReplace(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_OR_IGNORE_OPS;
	static void InitializeInsertOrIgnore(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertOrIgnore(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BY_NAME_OR_POSITION_OPS;
	static void InitializeByNameOrPosition(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeByNameOrPosition(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_BY_NAME_ORDER_OPS;
	static void InitializeInsertByNameOrder(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertByNameOrder(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_BY_POSITION_ORDER_OPS;
	static void InitializeInsertByPositionOrder(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertByPositionOrder(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_BY_NAME_OPS;
	static void InitializeInsertByName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertByName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_BY_POSITION_OPS;
	static void InitializeInsertByPosition(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertByPosition(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_TARGET_OPS;
	static void InitializeInsertTarget(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertTarget(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_ALIAS_OPS;
	static void InitializeInsertAlias(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertAlias(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLUMN_LIST_OPS;
	static void InitializeColumnList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColumnList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_COLUMN_LIST_OPS;
	static void InitializeInsertColumnList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertColumnList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_VALUES_OPS;
	static void InitializeInsertValues(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertValues(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SELECT_INSERT_VALUES_OPS;
	static void InitializeSelectInsertValues(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSelectInsertValues(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DEFAULT_VALUES_OPS;
	static void InitializeDefaultValues(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDefaultValues(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ON_CONFLICT_CLAUSE_OPS;
	static void InitializeOnConflictClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOnConflictClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ON_CONFLICT_TARGET_OPS;
	static void InitializeOnConflictTarget(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOnConflictTarget(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ON_CONFLICT_EXPRESSION_TARGET_OPS;
	static void InitializeOnConflictExpressionTarget(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOnConflictExpressionTarget(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ON_CONFLICT_INDEX_TARGET_OPS;
	static void InitializeOnConflictIndexTarget(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOnConflictIndexTarget(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ON_CONFLICT_ACTION_OPS;
	static void InitializeOnConflictAction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOnConflictAction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ON_CONFLICT_UPDATE_OPS;
	static void InitializeOnConflictUpdate(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOnConflictUpdate(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ON_CONFLICT_NOTHING_OPS;
	static void InitializeOnConflictNothing(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOnConflictNothing(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RETURNING_CLAUSE_OPS;
	static void InitializeReturningClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReturningClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LOAD_STATEMENT_OPS;
	static void InitializeLoadStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLoadStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXTENSION_ALIAS_OPS;
	static void InitializeExtensionAlias(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExtensionAlias(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSTALL_STATEMENT_OPS;
	static void InitializeInstallStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInstallStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_EXTENSIONS_STATEMENT_OPS;
	static void InitializeUpdateExtensionsStatement(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateExtensionsStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FROM_SOURCE_OPS;
	static void InitializeFromSource(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFromSource(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FROM_SOURCE_IDENTIFIER_OPS;
	static void InitializeFromSourceIdentifier(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFromSourceIdentifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FROM_SOURCE_STRING_OPS;
	static void InitializeFromSourceString(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFromSourceString(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VERSION_NUMBER_OPS;
	static void InitializeVersionNumber(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeVersionNumber(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MERGE_INTO_STATEMENT_OPS;
	static void InitializeMergeIntoStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMergeIntoStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MERGE_INTO_USING_CLAUSE_OPS;
	static void InitializeMergeIntoUsingClause(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMergeIntoUsingClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MERGE_MATCH_OPS;
	static void InitializeMergeMatch(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMergeMatch(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MATCHED_CLAUSE_OPS;
	static void InitializeMatchedClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMatchedClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MATCHED_CLAUSE_ACTION_OPS;
	static void InitializeMatchedClauseAction(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMatchedClauseAction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_MATCH_CLAUSE_OPS;
	static void InitializeUpdateMatchClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateMatchClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_MATCH_INFO_OPS;
	static void InitializeUpdateMatchInfo(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateMatchInfo(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_MATCH_SET_ACTION_OPS;
	static void InitializeUpdateMatchSetAction(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateMatchSetAction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_BY_NAME_OR_POSITION_OPS;
	static void InitializeUpdateByNameOrPosition(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateByNameOrPosition(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DELETE_MATCH_CLAUSE_OPS;
	static void InitializeDeleteMatchClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDeleteMatchClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_MATCH_CLAUSE_OPS;
	static void InitializeInsertMatchClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertMatchClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_MATCH_INFO_OPS;
	static void InitializeInsertMatchInfo(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertMatchInfo(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_DEFAULT_VALUES_OPS;
	static void InitializeInsertDefaultValues(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertDefaultValues(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_BY_NAME_OR_POSITION_OPS;
	static void InitializeInsertByNameOrPosition(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertByNameOrPosition(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INSERT_VALUES_LIST_OPS;
	static void InitializeInsertValuesList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInsertValuesList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DO_NOTHING_MATCH_CLAUSE_OPS;
	static void InitializeDoNothingMatchClause(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDoNothingMatchClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ERROR_MATCH_CLAUSE_OPS;
	static void InitializeErrorMatchClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeErrorMatchClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_MATCH_SET_CLAUSE_OPS;
	static void InitializeUpdateMatchSetClause(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateMatchSetClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_MATCH_SET_INFO_OPS;
	static void InitializeUpdateMatchSetInfo(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateMatchSetInfo(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps AND_EXPRESSION_OPS;
	static void InitializeAndExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAndExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NOT_MATCHED_CLAUSE_OPS;
	static void InitializeNotMatchedClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNotMatchedClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BY_SOURCE_OR_TARGET_OPS;
	static void InitializeBySourceOrTarget(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBySourceOrTarget(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BY_SOURCE_OPS;
	static void InitializeBySource(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBySource(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BY_TARGET_OPS;
	static void InitializeByTarget(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeByTarget(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_STATEMENT_OPS;
	static void InitializePivotStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_ON_OPS;
	static void InitializePivotOn(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotOn(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_USING_OPS;
	static void InitializePivotUsing(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotUsing(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_COLUMN_LIST_OPS;
	static void InitializePivotColumnList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotColumnList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_COLUMN_ENTRY_OPS;
	static void InitializePivotColumnEntry(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotColumnEntry(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_COLUMN_EXPRESSION_OPS;
	static void InitializePivotColumnExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotColumnExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_COLUMN_SUBQUERY_OPS;
	static void InitializePivotColumnSubquery(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotColumnSubquery(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNPIVOT_STATEMENT_OPS;
	static void InitializeUnpivotStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUnpivotStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTO_NAME_VALUES_OPS;
	static void InitializeIntoNameValues(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntoNameValues(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INCLUDE_OR_EXCLUDE_NULLS_OPS;
	static void InitializeIncludeOrExcludeNulls(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIncludeOrExcludeNulls(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INCLUDE_NULLS_OPS;
	static void InitializeIncludeNulls(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIncludeNulls(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXCLUDE_NULLS_OPS;
	static void InitializeExcludeNulls(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExcludeNulls(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNPIVOT_HEADER_OPS;
	static void InitializeUnpivotHeader(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUnpivotHeader(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNPIVOT_HEADER_SINGLE_OPS;
	static void InitializeUnpivotHeaderSingle(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUnpivotHeaderSingle(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNPIVOT_HEADER_LIST_OPS;
	static void InitializeUnpivotHeaderList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUnpivotHeaderList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PRAGMA_STATEMENT_OPS;
	static void InitializePragmaStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePragmaStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PRAGMA_ASSIGN_OR_FUNCTION_OPS;
	static void InitializePragmaAssignOrFunction(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePragmaAssignOrFunction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PRAGMA_ASSIGN_OPS;
	static void InitializePragmaAssign(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePragmaAssign(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PRAGMA_FUNCTION_OPS;
	static void InitializePragmaFunction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePragmaFunction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PRAGMA_PARAMETERS_OPS;
	static void InitializePragmaParameters(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePragmaParameters(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PREPARE_STATEMENT_OPS;
	static void InitializePrepareStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePrepareStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TYPE_LIST_OPS;
	static void InitializeTypeList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTypeList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SELECT_STATEMENT_OPS;
	static void InitializeSelectStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSelectStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SELECT_STATEMENT_INTERNAL_OPS;
	static void InitializeSelectStatementInternal(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSelectStatementInternal(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SELECT_SET_OP_CHAIN_OPS;
	static void InitializeSelectSetOpChain(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSelectSetOpChain(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SELECT_SET_OP_CHAIN_TAIL_OPS;
	static void InitializeSelectSetOpChainTail(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSelectSetOpChainTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERSECT_CHAIN_OPS;
	static void InitializeIntersectChain(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntersectChain(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INTERSECT_CHAIN_TAIL_OPS;
	static void InitializeIntersectChainTail(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeIntersectChainTail(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_INTERSECT_CLAUSE_OPS;
	static void InitializeSetIntersectClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetIntersectClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SELECT_ATOM_OPS;
	static void InitializeSelectAtom(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSelectAtom(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SELECT_PARENS_OPS;
	static void InitializeSelectParens(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSelectParens(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SETOP_CLAUSE_OPS;
	static void InitializeSetopClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetopClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SETOP_TYPE_OPS;
	static void InitializeSetopType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetopType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SETOP_UNION_OPS;
	static void InitializeSetopUnion(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetopUnion(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SETOP_EXCEPT_OPS;
	static void InitializeSetopExcept(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetopExcept(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SELECT_STATEMENT_TYPE_OPS;
	static void InitializeSelectStatementType(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSelectStatementType(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RESULT_MODIFIERS_OPS;
	static void InitializeResultModifiers(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeResultModifiers(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIMIT_OFFSET_OPS;
	static void InitializeLimitOffset(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLimitOffset(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIMIT_OFFSET_CLAUSE_OPS;
	static void InitializeLimitOffsetClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLimitOffsetClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OFFSET_LIMIT_CLAUSE_OPS;
	static void InitializeOffsetLimitClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOffsetLimitClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_STATEMENT_OPS;
	static void InitializeTableStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OPTIONAL_PARENS_SIMPLE_SELECT_OPS;
	static void InitializeOptionalParensSimpleSelect(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOptionalParensSimpleSelect(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SIMPLE_SELECT_PARENS_OPS;
	static void InitializeSimpleSelectParens(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSimpleSelectParens(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SIMPLE_SELECT_OPS;
	static void InitializeSimpleSelect(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSimpleSelect(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SELECT_FROM_OPS;
	static void InitializeSelectFrom(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSelectFrom(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SELECT_FROM_CLAUSE_OPS;
	static void InitializeSelectFromClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSelectFromClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FROM_SELECT_CLAUSE_OPS;
	static void InitializeFromSelectClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFromSelectClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITH_STATEMENT_OPS;
	static void InitializeWithStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CTE_BODY_OPS;
	static void InitializeCTEBody(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCTEBody(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CTE_SELECT_BODY_OPS;
	static void InitializeCTESelectBody(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCTESelectBody(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CTEDML_BODY_OPS;
	static void InitializeCTEDMLBody(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCTEDMLBody(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps USING_KEY_OPS;
	static void InitializeUsingKey(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUsingKey(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps MATERIALIZED_OPS;
	static void InitializeMaterialized(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeMaterialized(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITH_CLAUSE_OPS;
	static void InitializeWithClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SELECT_CLAUSE_OPS;
	static void InitializeSelectClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSelectClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TARGET_LIST_OPS;
	static void InitializeTargetList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTargetList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COLUMN_ALIASES_OPS;
	static void InitializeColumnAliases(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColumnAliases(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DISTINCT_CLAUSE_OPS;
	static void InitializeDistinctClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDistinctClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DISTINCT_ALL_OPS;
	static void InitializeDistinctAll(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDistinctAll(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DISTINCT_ON_OPS;
	static void InitializeDistinctOn(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDistinctOn(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DISTINCT_ON_TARGETS_OPS;
	static void InitializeDistinctOnTargets(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDistinctOnTargets(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INNER_TABLE_REF_OPS;
	static void InitializeInnerTableRef(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInnerTableRef(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_REF_OPS;
	static void InitializeTableRef(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableRef(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_SUBQUERY_OPS;
	static void InitializeTableSubquery(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableSubquery(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BASE_TABLE_REF_OPS;
	static void InitializeBaseTableRef(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBaseTableRef(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_ALIAS_COLON_OPS;
	static void InitializeTableAliasColon(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableAliasColon(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VALUES_REF_OPS;
	static void InitializeValuesRef(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeValuesRef(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PARENS_TABLE_REF_OPS;
	static void InitializeParensTableRef(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeParensTableRef(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps JOIN_OR_PIVOT_OPS;
	static void InitializeJoinOrPivot(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeJoinOrPivot(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_PIVOT_CLAUSE_OPS;
	static void InitializeTablePivotClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTablePivotClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_PIVOT_CLAUSE_BODY_OPS;
	static void InitializeTablePivotClauseBody(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTablePivotClauseBody(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_GROUP_BY_LIST_OPS;
	static void InitializePivotGroupByList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotGroupByList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_UNPIVOT_CLAUSE_OPS;
	static void InitializeTableUnpivotClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableUnpivotClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_UNPIVOT_CLAUSE_BODY_OPS;
	static void InitializeTableUnpivotClauseBody(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableUnpivotClauseBody(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_HEADER_OPS;
	static void InitializePivotHeader(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotHeader(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_VALUE_LIST_OPS;
	static void InitializePivotValueList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotValueList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_VALUE_TARGET_OPS;
	static void InitializePivotValueTarget(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotValueTarget(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNPIVOT_VALUE_LIST_OPS;
	static void InitializeUnpivotValueList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUnpivotValueList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps PIVOT_TARGET_LIST_OPS;
	static void InitializePivotTargetList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePivotTargetList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNPIVOT_TARGET_LIST_OPS;
	static void InitializeUnpivotTargetList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUnpivotTargetList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LATERAL_OPS;
	static void InitializeLateral(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLateral(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BASE_TABLE_NAME_OPS;
	static void InitializeBaseTableName(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBaseTableName(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UNQUALIFIED_BASE_TABLE_NAME_OPS;
	static void InitializeUnqualifiedBaseTableName(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUnqualifiedBaseTableName(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SCHEMA_RESERVED_TABLE_OPS;
	static void InitializeSchemaReservedTable(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSchemaReservedTable(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CATALOG_RESERVED_SCHEMA_TABLE_OPS;
	static void InitializeCatalogReservedSchemaTable(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCatalogReservedSchemaTable(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_FUNCTION_OPS;
	static void InitializeTableFunction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableFunction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_FUNCTION_LATERAL_OPT_OPS;
	static void InitializeTableFunctionLateralOpt(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableFunctionLateralOpt(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_FUNCTION_ALIAS_COLON_OPS;
	static void InitializeTableFunctionAliasColon(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableFunctionAliasColon(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WITH_ORDINALITY_OPS;
	static void InitializeWithOrdinality(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWithOrdinality(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFIED_TABLE_FUNCTION_OPS;
	static void InitializeQualifiedTableFunction(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifiedTableFunction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_FUNCTION_ARGUMENTS_OPS;
	static void InitializeTableFunctionArguments(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableFunctionArguments(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FUNCTION_ARGUMENT_OPS;
	static void InitializeFunctionArgument(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFunctionArgument(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NAMED_FUNCTION_ARGUMENT_OPS;
	static void InitializeNamedFunctionArgument(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNamedFunctionArgument(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps POSITIONAL_FUNCTION_ARGUMENT_OPS;
	static void InitializePositionalFunctionArgument(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePositionalFunctionArgument(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NAMED_PARAMETER_OPS;
	static void InitializeNamedParameter(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNamedParameter(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_ALIAS_OPS;
	static void InitializeTableAlias(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableAlias(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_ALIAS_AS_OPS;
	static void InitializeTableAliasAs(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableAliasAs(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TABLE_ALIAS_WITHOUT_AS_OPS;
	static void InitializeTableAliasWithoutAs(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTableAliasWithoutAs(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps AT_CLAUSE_OPS;
	static void InitializeAtClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAtClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps AT_SPECIFIER_OPS;
	static void InitializeAtSpecifier(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAtSpecifier(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps AT_UNIT_OPS;
	static void InitializeAtUnit(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAtUnit(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VERSION_AT_UNIT_OPS;
	static void InitializeVersionAtUnit(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeVersionAtUnit(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TIMESTAMP_AT_UNIT_OPS;
	static void InitializeTimestampAtUnit(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTimestampAtUnit(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps JOIN_CLAUSE_OPS;
	static void InitializeJoinClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeJoinClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REGULAR_JOIN_CLAUSE_OPS;
	static void InitializeRegularJoinClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRegularJoinClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ASOF_OPS;
	static void InitializeAsof(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAsof(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps JOIN_WITHOUT_ON_CLAUSE_OPS;
	static void InitializeJoinWithoutOnClause(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeJoinWithoutOnClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps JOIN_QUALIFIER_OPS;
	static void InitializeJoinQualifier(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeJoinQualifier(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ON_CLAUSE_OPS;
	static void InitializeOnClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOnClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps USING_CLAUSE_OPS;
	static void InitializeUsingClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUsingClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps JOIN_TYPE_OPS;
	static void InitializeJoinType(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeJoinType(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps JOIN_PREFIX_OPS;
	static void InitializeJoinPrefix(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeJoinPrefix(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CROSS_JOIN_PREFIX_OPS;
	static void InitializeCrossJoinPrefix(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCrossJoinPrefix(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NATURAL_JOIN_PREFIX_OPS;
	static void InitializeNaturalJoinPrefix(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNaturalJoinPrefix(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps POSITIONAL_JOIN_PREFIX_OPS;
	static void InitializePositionalJoinPrefix(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizePositionalJoinPrefix(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FULL_JOIN_OPS;
	static void InitializeFullJoin(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFullJoin(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LEFT_JOIN_OPS;
	static void InitializeLeftJoin(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLeftJoin(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RIGHT_JOIN_OPS;
	static void InitializeRightJoin(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRightJoin(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SEMI_JOIN_OPS;
	static void InitializeSemiJoin(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSemiJoin(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ANTI_JOIN_OPS;
	static void InitializeAntiJoin(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAntiJoin(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps INNER_JOIN_OPS;
	static void InitializeInnerJoin(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeInnerJoin(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps FROM_CLAUSE_OPS;
	static void InitializeFromClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeFromClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WHERE_CLAUSE_OPS;
	static void InitializeWhereClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWhereClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUP_BY_CLAUSE_OPS;
	static void InitializeGroupByClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupByClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps HAVING_CLAUSE_OPS;
	static void InitializeHavingClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeHavingClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps QUALIFY_CLAUSE_OPS;
	static void InitializeQualifyClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeQualifyClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_CLAUSE_OPS;
	static void InitializeSampleClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSampleClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WINDOW_CLAUSE_OPS;
	static void InitializeWindowClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWindowClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps WINDOW_DEFINITION_OPS;
	static void InitializeWindowDefinition(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeWindowDefinition(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_ENTRY_OPS;
	static void InitializeSampleEntry(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSampleEntry(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_ENTRY_COUNT_OPS;
	static void InitializeSampleEntryCount(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSampleEntryCount(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_ENTRY_FUNCTION_OPS;
	static void InitializeSampleEntryFunction(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSampleEntryFunction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_FUNCTION_OPS;
	static void InitializeSampleFunction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSampleFunction(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_PROPERTIES_OPS;
	static void InitializeSampleProperties(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSampleProperties(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps REPEATABLE_SAMPLE_OPS;
	static void InitializeRepeatableSample(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRepeatableSample(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_SEED_OPS;
	static void InitializeSampleSeed(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSampleSeed(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_COUNT_OPS;
	static void InitializeSampleCount(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSampleCount(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_VALUE_OPS;
	static void InitializeSampleValue(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSampleValue(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_UNIT_OPS;
	static void InitializeSampleUnit(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSampleUnit(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_PERCENTAGE_OPS;
	static void InitializeSamplePercentage(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSamplePercentage(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SAMPLE_ROWS_OPS;
	static void InitializeSampleRows(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSampleRows(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUP_BY_EXPRESSIONS_OPS;
	static void InitializeGroupByExpressions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupByExpressions(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUP_BY_ALL_OPS;
	static void InitializeGroupByAll(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupByAll(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUP_BY_LIST_OPS;
	static void InitializeGroupByList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupByList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUP_BY_EXPRESSION_OPS;
	static void InitializeGroupByExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupByExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUP_BY_BASE_EXPRESSION_OPS;
	static void InitializeGroupByBaseExpression(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupByBaseExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EMPTY_GROUPING_ITEM_OPS;
	static void InitializeEmptyGroupingItem(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeEmptyGroupingItem(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CUBE_OR_ROLLUP_CLAUSE_OPS;
	static void InitializeCubeOrRollupClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCubeOrRollupClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CUBE_OR_ROLLUP_OPS;
	static void InitializeCubeOrRollup(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCubeOrRollup(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CUBE_KEYWORD_OPS;
	static void InitializeCubeKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCubeKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ROLLUP_KEYWORD_OPS;
	static void InitializeRollupKeyword(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRollupKeyword(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GROUPING_SETS_CLAUSE_OPS;
	static void InitializeGroupingSetsClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGroupingSetsClause(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SUBQUERY_REFERENCE_OPS;
	static void InitializeSubqueryReference(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSubqueryReference(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ORDER_BY_EXPRESSION_OPS;
	static void InitializeOrderByExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOrderByExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DESC_OR_ASC_OPS;
	static void InitializeDescOrAsc(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDescOrAsc(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DESCENDING_ORDER_OPS;
	static void InitializeDescendingOrder(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDescendingOrder(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ASCENDING_ORDER_OPS;
	static void InitializeAscendingOrder(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAscendingOrder(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NULLS_FIRST_OR_LAST_OPS;
	static void InitializeNullsFirstOrLast(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNullsFirstOrLast(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NULLS_FIRST_OPS;
	static void InitializeNullsFirst(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNullsFirst(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NULLS_LAST_OPS;
	static void InitializeNullsLast(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNullsLast(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ORDER_BY_CLAUSE_OPS;
	static void InitializeOrderByClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOrderByClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ORDER_BY_EXPRESSIONS_OPS;
	static void InitializeOrderByExpressions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOrderByExpressions(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ORDER_BY_EXPRESSION_LIST_OPS;
	static void InitializeOrderByExpressionList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOrderByExpressionList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ORDER_BY_ALL_OPS;
	static void InitializeOrderByAll(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOrderByAll(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIMIT_CLAUSE_OPS;
	static void InitializeLimitClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLimitClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OFFSET_CLAUSE_OPS;
	static void InitializeOffsetClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOffsetClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OFFSET_VALUE_OPS;
	static void InitializeOffsetValue(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOffsetValue(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIMIT_VALUE_OPS;
	static void InitializeLimitValue(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLimitValue(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIMIT_ALL_OPS;
	static void InitializeLimitAll(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLimitAll(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIMIT_LITERAL_PERCENT_OPS;
	static void InitializeLimitLiteralPercent(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLimitLiteralPercent(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LIMIT_EXPRESSION_OPS;
	static void InitializeLimitExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLimitExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ALIASED_EXPRESSION_OPS;
	static void InitializeAliasedExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeAliasedExpression(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COL_ID_EXPRESSION_OPS;
	static void InitializeColIdExpression(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeColIdExpression(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPRESSION_AS_COLLABEL_OPS;
	static void InitializeExpressionAsCollabel(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExpressionAsCollabel(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps EXPRESSION_OPT_IDENTIFIER_OPS;
	static void InitializeExpressionOptIdentifier(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeExpressionOptIdentifier(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VALUES_CLAUSE_OPS;
	static void InitializeValuesClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeValuesClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VALUES_EXPRESSIONS_OPS;
	static void InitializeValuesExpressions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeValuesExpressions(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_STATEMENT_OPS;
	static void InitializeSetStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_ASSIGNMENT_OR_TIME_ZONE_OPS;
	static void InitializeSetAssignmentOrTimeZone(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetAssignmentOrTimeZone(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps RESET_STATEMENT_OPS;
	static void InitializeResetStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeResetStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps STANDARD_ASSIGNMENT_OPS;
	static void InitializeStandardAssignment(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeStandardAssignment(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_VARIABLE_OR_SETTING_OPS;
	static void InitializeSetVariableOrSetting(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetVariableOrSetting(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_TIME_ZONE_OPS;
	static void InitializeSetTimeZone(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetTimeZone(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ZONE_VALUE_OPS;
	static void InitializeZoneValue(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeZoneValue(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ZONE_LOCAL_OPS;
	static void InitializeZoneLocal(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeZoneLocal(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ZONE_DEFAULT_OPS;
	static void InitializeZoneDefault(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeZoneDefault(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ZONE_STRING_LITERAL_OPS;
	static void InitializeZoneStringLiteral(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeZoneStringLiteral(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ZONE_IDENTIFIER_OPS;
	static void InitializeZoneIdentifier(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeZoneIdentifier(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ZONE_INTERVAL_WITH_INTERVAL_OPS;
	static void InitializeZoneIntervalWithInterval(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeZoneIntervalWithInterval(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ZONE_INTERVAL_WITH_PRECISION_OPS;
	static void InitializeZoneIntervalWithPrecision(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeZoneIntervalWithPrecision(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_SETTING_OPS;
	static void InitializeSetSetting(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetSetting(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_VARIABLE_OPS;
	static void InitializeSetVariable(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetVariable(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VARIABLE_SCOPE_OPS;
	static void InitializeVariableScope(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeVariableScope(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SETTING_SCOPE_OPS;
	static void InitializeSettingScope(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSettingScope(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps LOCAL_SCOPE_OPS;
	static void InitializeLocalScope(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeLocalScope(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SESSION_SCOPE_OPS;
	static void InitializeSessionScope(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSessionScope(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps GLOBAL_SCOPE_OPS;
	static void InitializeGlobalScope(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeGlobalScope(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SET_ASSIGNMENT_OPS;
	static void InitializeSetAssignment(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSetAssignment(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VARIABLE_LIST_OPS;
	static void InitializeVariableList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeVariableList(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps TRANSACTION_STATEMENT_OPS;
	static void InitializeTransactionStatement(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeTransactionStatement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BEGIN_TRANSACTION_OPS;
	static void InitializeBeginTransaction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBeginTransaction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps ROLLBACK_TRANSACTION_OPS;
	static void InitializeRollbackTransaction(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeRollbackTransaction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps COMMIT_TRANSACTION_OPS;
	static void InitializeCommitTransaction(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCommitTransaction(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps READ_OR_WRITE_OPS;
	static void InitializeReadOrWrite(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReadOrWrite(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps READ_ONLY_OR_READ_WRITE_OPS;
	static void InitializeReadOnlyOrReadWrite(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReadOnlyOrReadWrite(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps READ_ONLY_OPS;
	static void InitializeReadOnly(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReadOnly(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps READ_WRITE_OPS;
	static void InitializeReadWrite(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeReadWrite(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_STATEMENT_OPS;
	static void InitializeUpdateStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_TARGET_OPS;
	static void InitializeUpdateTarget(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateTarget(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BASE_TABLE_SET_OPS;
	static void InitializeBaseTableSet(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBaseTableSet(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps BASE_TABLE_ALIAS_SET_OPS;
	static void InitializeBaseTableAliasSet(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeBaseTableAliasSet(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_ALIAS_OPS;
	static void InitializeUpdateAlias(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateAlias(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_SET_CLAUSE_OPS;
	static void InitializeUpdateSetClause(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateSetClause(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_SET_TUPLE_OPS;
	static void InitializeUpdateSetTuple(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateSetTuple(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_SET_ELEMENT_LIST_OPS;
	static void InitializeUpdateSetElementList(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateSetElementList(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_SET_ELEMENT_OPS;
	static void InitializeUpdateSetElement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateSetElement(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps UPDATE_SET_COLUMN_TARGET_OPS;
	static void InitializeUpdateSetColumnTarget(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUpdateSetColumnTarget(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps USE_STATEMENT_OPS;
	static void InitializeUseStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUseStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps USE_TARGET_OPS;
	static void InitializeUseTarget(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUseTarget(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps SCHEMA_NAME_AS_USE_TARGET_OPS;
	static void InitializeSchemaNameAsUseTarget(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeSchemaNameAsUseTarget(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps CATALOG_NAME_AS_USE_TARGET_OPS;
	static void InitializeCatalogNameAsUseTarget(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeCatalogNameAsUseTarget(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps USE_TARGET_CATALOG_SCHEMA_OPS;
	static void InitializeUseTargetCatalogSchema(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeUseTargetCatalogSchema(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps DOT_IDENTIFIER_OPS;
	static void InitializeDotIdentifier(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeDotIdentifier(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VACUUM_STATEMENT_OPS;
	static void InitializeVacuumStatement(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeVacuumStatement(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VACUUM_OPTIONS_OPS;
	static void InitializeVacuumOptions(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeVacuumOptions(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VACUUM_PARENS_OPTIONS_OPS;
	static void InitializeVacuumParensOptions(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeVacuumParensOptions(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VACUUM_LEGACY_OPTIONS_OPS;
	static void InitializeVacuumLegacyOptions(
	    PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeVacuumLegacyOptions(
	    PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps VACUUM_OPTION_OPS;
	static void InitializeVacuumOption(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeVacuumOption(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OPT_ANALYZE_OPS;
	static void InitializeOptAnalyze(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOptAnalyze(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OPT_FULL_OPS;
	static void InitializeOptFull(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOptFull(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OPT_FREEZE_OPS;
	static void InitializeOptFreeze(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOptFreeze(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps OPT_VERBOSE_OPS;
	static void InitializeOptVerbose(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeOptVerbose(PEGTransformer &transformer, TransformStackFrame &frame);
	static const TransformFrameOps NAME_LIST_OPS;
	static void InitializeNameList(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> FinalizeNameList(PEGTransformer &transformer, TransformStackFrame &frame);
// END generated trampoline transformer declarations

	// comment.gram
	static Value TransformCommentValue(PEGTransformer &transformer, ParseResult &parse_result);

	// common.gram
	static unique_ptr<ParsedExpression> TransformNumberLiteral(PEGTransformer &transformer, ParseResult &parse_result);
	static string TransformStringLiteral(PEGTransformer &transformer, ParseResult &parse_result);
	static DatePartSpecifier TransformIntervalToIntervalAsType(PEGTransformer &transformer, ParseResult &parse_result);

	static string ExtractFormat(const string &file_path);

	// create_table.gram
	static string TransformColLabelOrString(PEGTransformer &transformer, ParseResult &parse_result);
	static string TransformIdentifier(PEGTransformer &transformer, ParseResult &parse_result);

	// expression.gram
	static unique_ptr<ParsedExpression> TransformExpression(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformExpressionTrampoline(PEGTransformer &transformer,
	                                                                  TransformStackFrame &frame);
	static unique_ptr<ParsedExpression> TransformPrefixExpression(PEGTransformer &transformer,
	                                                              ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformPrefixExpressionTrampoline(PEGTransformer &transformer,
	                                                                       TransformStackFrame &frame);
	static unique_ptr<WindowExpression> TransformOverClause(PEGTransformer &transformer, ParseResult &parse_result);

	// pivot.gram
	static unique_ptr<SelectStatement> TransformPivotStatement(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<SelectStatement> TransformUnpivotStatement(PEGTransformer &transformer,
	                                                             ParseResult &parse_result);

	// select.gram
	static unique_ptr<SelectStatement> TransformSelectStatementInternalRule(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<SelectStatement> TransformSelectStatementInternalTrampoline(PEGTransformer &transformer,
	                                                                              TransformStackFrame &frame);
	static unique_ptr<SelectStatement> TransformSimpleSelect(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<SelectStatement> TransformSimpleSelectTrampoline(PEGTransformer &transformer,
	                                                                    TransformStackFrame &frame);

	static unique_ptr<TableRef> TransformTableRef(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<TableRef> TransformTableRefTrampoline(PEGTransformer &transformer, TransformStackFrame &frame);

	static CommonTableExpressionMap TransformWithClause(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformWindowDefinition(PEGTransformer &transformer,
	                                                              ParseResult &parse_result);
	static string TransformIdentifierOrKeyword(PEGTransformer &transformer, ParseResult &parse_result);

	//===--------------------------------------------------------------------===//
	// START GENERATED RULES
	//===--------------------------------------------------------------------===//
	static unique_ptr<TransformResultValue> TransformAlterStatementInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformAlterStatement(PEGTransformer &transformer,
	                                                        unique_ptr<AlterInfo> alter_options);
	static unique_ptr<TransformResultValue> TransformAlterOptionsInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformAlterTableStmtInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<AlterInfo> TransformAlterTableStmt(PEGTransformer &transformer, const optional<bool> &if_exists,
	                                                     unique_ptr<BaseTableRef> base_table_name,
	                                                     vector<unique_ptr<AlterTableInfo>> alter_table_options);
	static unique_ptr<TransformResultValue> TransformAlterSchemaStmtInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<AlterInfo> TransformAlterSchemaStmt(PEGTransformer &transformer, const optional<bool> &if_exists,
	                                                      const QualifiedName &qualified_name,
	                                                      unique_ptr<AlterTableInfo> rename_alter);
	static unique_ptr<TransformResultValue> TransformAlterTableOptionsInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformAddConstraintInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformAddConstraint(PEGTransformer &transformer,
	                                                         unique_ptr<Constraint> top_level_constraint);
	static unique_ptr<TransformResultValue> TransformAddColumnInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformAddColumn(PEGTransformer &transformer, const bool &has_result,
	                                                     const optional<bool> &if_not_exists,
	                                                     AddColumnEntry add_column_entry);
	static unique_ptr<TransformResultValue> TransformAddColumnEntryInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static AddColumnEntry TransformAddColumnEntry(PEGTransformer &transformer, const vector<string> &dotted_identifier,
	                                              const optional<LogicalType> &type,
	                                              optional<GeneratedColumnDefinition> generated_column,
	                                              optional<vector<ColumnConstraintEntry>> column_constraint);
	static unique_ptr<TransformResultValue> TransformDropColumnInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformDropColumn(PEGTransformer &transformer, const bool &has_result,
	                                                      const optional<bool> &if_exists,
	                                                      unique_ptr<ColumnRefExpression> nested_column_name,
	                                                      const optional<bool> &drop_behavior);
	static unique_ptr<TransformResultValue> TransformAlterColumnInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformAlterColumn(PEGTransformer &transformer, const bool &has_result,
	                                                       unique_ptr<ColumnRefExpression> nested_column_name,
	                                                       unique_ptr<AlterTableInfo> alter_column_entry);
	static unique_ptr<TransformResultValue> TransformRenameColumnInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformRenameColumn(PEGTransformer &transformer, const bool &has_result,
	                                                        unique_ptr<ColumnRefExpression> nested_column_name,
	                                                        const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformNestedColumnNameInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<ColumnRefExpression> TransformNestedColumnName(PEGTransformer &transformer,
	                                                                 const optional<vector<Identifier>> &identifier_dot,
	                                                                 const Identifier &column_name);
	static unique_ptr<TransformResultValue> TransformIdentifierDotInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static Identifier TransformIdentifierDot(PEGTransformer &transformer, const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformRenameAlterInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformRenameAlter(PEGTransformer &transformer, const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformSetPartitionedByInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformSetPartitionedBy(PEGTransformer &transformer,
	                                                            vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformResetPartitionedByInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformResetPartitionedBy(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSetSortedByInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformSetSortedBy(PEGTransformer &transformer,
	                                                       vector<OrderByNode> order_by_expressions);
	static unique_ptr<TransformResultValue> TransformResetSortedByInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformResetSortedBy(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSetOptionsInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<AlterTableInfo>
	TransformSetOptions(PEGTransformer &transformer,
	                    case_insensitive_map_t<unique_ptr<ParsedExpression>> rel_option_list);
	static unique_ptr<TransformResultValue> TransformResetOptionsInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<AlterTableInfo>
	TransformResetOptions(PEGTransformer &transformer,
	                      case_insensitive_map_t<unique_ptr<ParsedExpression>> rel_option_list);
	static unique_ptr<TransformResultValue> TransformAlterColumnEntryInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformAddOrDropDefaultInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformAddDefaultInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformAddDefault(PEGTransformer &transformer,
	                                                      unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformDropDefaultInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformDropDefault(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformChangeNullabilityInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformChangeNullability(PEGTransformer &transformer,
	                                                             const string &drop_or_set);
	static unique_ptr<TransformResultValue> TransformDropOrSetInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformDropNullabilityInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static string TransformDropNullability(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSetNullabilityInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static string TransformSetNullability(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformAlterTypeInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<AlterTableInfo> TransformAlterType(PEGTransformer &transformer, const bool &has_result,
	                                                     const optional<LogicalType> &type,
	                                                     optional<unique_ptr<ParsedExpression>> using_expression);
	static unique_ptr<TransformResultValue> TransformUsingExpressionInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformUsingExpression(PEGTransformer &transformer,
	                                                             unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformAlterViewStmtInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<AlterInfo> TransformAlterViewStmt(PEGTransformer &transformer, const optional<bool> &if_exists,
	                                                    unique_ptr<BaseTableRef> base_table_name,
	                                                    unique_ptr<AlterTableInfo> rename_alter);
	static unique_ptr<TransformResultValue> TransformAlterSequenceStmtInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<AlterInfo> TransformAlterSequenceStmt(PEGTransformer &transformer,
	                                                        const optional<bool> &if_exists,
	                                                        const QualifiedName &qualified_sequence_name,
	                                                        unique_ptr<AlterInfo> alter_sequence_options);
	static unique_ptr<TransformResultValue> TransformQualifiedSequenceNameInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static QualifiedName TransformQualifiedSequenceName(PEGTransformer &transformer,
	                                                    const optional<Identifier> &catalog_qualification,
	                                                    const optional<Identifier> &schema_qualification,
	                                                    const Identifier &sequence_name);
	static unique_ptr<TransformResultValue> TransformAlterSequenceOptionsInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<AlterInfo> TransformAlterSequenceOptions(PEGTransformer &transformer, ParseResult &choice_result);
	static unique_ptr<AlterInfo> TransformAlterSequenceOptionsTrampoline(PEGTransformer &transformer,
	                                                                     TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> TransformSetSequenceOptionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<AlterInfo>
	TransformSetSequenceOption(PEGTransformer &transformer,
	                           vector<pair<string, unique_ptr<SequenceOption>>> sequence_option);
	static unique_ptr<TransformResultValue> TransformAlterDatabaseStmtInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<AlterInfo> TransformAlterDatabaseStmt(PEGTransformer &transformer,
	                                                        const optional<bool> &if_exists,
	                                                        const Identifier &identifier,
	                                                        const Identifier &identifier_1);
	static unique_ptr<TransformResultValue> TransformAnalyzeStatementInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformAnalyzeStatement(PEGTransformer &transformer,
	                                                          const optional<bool> &analyze_verbose,
	                                                          optional<AnalyzeTarget> analyze_target);
	static unique_ptr<TransformResultValue> TransformAnalyzeTargetInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static AnalyzeTarget TransformAnalyzeTarget(PEGTransformer &transformer, unique_ptr<BaseTableRef> base_table_name,
	                                            const optional<vector<string>> &name_list);
	static unique_ptr<TransformResultValue> TransformAnalyzeVerboseInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static bool TransformAnalyzeVerbose(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformAttachStatementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<SQLStatement>
	TransformAttachStatement(PEGTransformer &transformer, const optional<bool> &or_replace,
	                         const optional<bool> &if_not_exists, const bool &has_result,
	                         unique_ptr<ParsedExpression> database_path, const optional<Identifier> &attach_alias,
	                         const optional<vector<GenericCopyOption>> &attach_options);
	static unique_ptr<TransformResultValue> TransformDatabasePathInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformDatabasePath(PEGTransformer &transformer,
	                                                          unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformAttachAliasInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static Identifier TransformAttachAlias(PEGTransformer &transformer, const Identifier &col_id);
	static unique_ptr<TransformResultValue> TransformAttachOptionsInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static vector<GenericCopyOption> TransformAttachOptions(PEGTransformer &transformer,
	                                                        const vector<GenericCopyOption> &generic_copy_option_list);
	static unique_ptr<TransformResultValue> TransformCallStatementInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformCallStatement(PEGTransformer &transformer,
	                                                       const QualifiedName &qualified_table_function,
	                                                       vector<FunctionArgument> table_function_arguments);
	static unique_ptr<TransformResultValue> TransformCheckpointStatementInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformCheckpointStatement(PEGTransformer &transformer,
	                                                             const optional<bool> &checkpoint_force,
	                                                             const optional<Identifier> &catalog_name);
	static unique_ptr<TransformResultValue> TransformCheckpointForceInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static bool TransformCheckpointForce(PEGTransformer &transformer);

	static unique_ptr<TransformResultValue> TransformCommentStatementInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformCommentStatement(PEGTransformer &transformer,
	                                                          const CatalogType &comment_on_type,
	                                                          const vector<string> &dotted_identifier,
	                                                          const Value &comment_value);
	static unique_ptr<TransformResultValue> TransformCommentOnTypeInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCommentTableInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static CatalogType TransformCommentTable(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCommentSequenceInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static CatalogType TransformCommentSequence(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCommentFunctionInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static CatalogType TransformCommentFunction(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCommentMacroTableInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static CatalogType TransformCommentMacroTable(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCommentMacroInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static CatalogType TransformCommentMacro(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCommentViewInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static CatalogType TransformCommentView(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCommentDatabaseInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static CatalogType TransformCommentDatabase(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCommentIndexInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static CatalogType TransformCommentIndex(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCommentSchemaInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static CatalogType TransformCommentSchema(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCommentTypeInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static CatalogType TransformCommentType(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCommentColumnInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static CatalogType TransformCommentColumn(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformExpressionStatementInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformExpressionStatement(PEGTransformer &transformer,
	                                                             vector<unique_ptr<ParsedExpression>> expression_alias);
	static unique_ptr<TransformResultValue> TransformExpressionAliasInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformConstraintNameInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static Identifier TransformConstraintName(PEGTransformer &transformer, const Identifier &col_id_or_string);
	static unique_ptr<TransformResultValue> TransformCollationNameInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static Identifier TransformCollationName(PEGTransformer &transformer, const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformTypeInternal(PEGTransformer &transformer,
	                                                              ParseResult &parse_result);
	static LogicalType TransformType(PEGTransformer &transformer, unique_ptr<ParsedExpression> type_variations,
	                                 const optional<vector<int64_t>> &array_bounds);
	static unique_ptr<TransformResultValue> TransformTypeVariationsInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSimpleTypeInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCharacterSimpleTypeInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformCharacterSimpleType(PEGTransformer &transformer,
	                             optional<vector<unique_ptr<ParsedExpression>>> type_modifiers);
	static unique_ptr<TransformResultValue> TransformQualifiedSimpleTypeInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformQualifiedSimpleType(PEGTransformer &transformer, const QualifiedName &qualified_type_name,
	                             optional<vector<unique_ptr<ParsedExpression>>> type_modifiers);
	static unique_ptr<TransformResultValue> TransformIntervalTypeInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformIntervalIntervalInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformIntervalWithSpecifierInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformIntervalWithRangeSpecifierInternal(PEGTransformer &transformer,
	                                                                                    ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformIntervalWithRangeSpecifier(PEGTransformer &transformer,
	                                    const DatePartSpecifier &interval_to_interval_as_type);
	static unique_ptr<TransformResultValue> TransformIntervalWithSimpleSpecifierInternal(PEGTransformer &transformer,
	                                                                                     ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformIntervalWithSimpleSpecifier(PEGTransformer &transformer,
	                                                                         const DatePartSpecifier &interval);
	static unique_ptr<TransformResultValue> TransformIntervalWithoutSpecifierInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformIntervalWithoutSpecifier(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformYearKeywordInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static DatePartSpecifier TransformYearKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformMonthKeywordInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static DatePartSpecifier TransformMonthKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDayKeywordInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static DatePartSpecifier TransformDayKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformHourKeywordInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static DatePartSpecifier TransformHourKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformMinuteKeywordInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static DatePartSpecifier TransformMinuteKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSecondKeywordInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static DatePartSpecifier TransformSecondKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformMillisecondKeywordInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static DatePartSpecifier TransformMillisecondKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformMicrosecondKeywordInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static DatePartSpecifier TransformMicrosecondKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformWeekKeywordInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static DatePartSpecifier TransformWeekKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformQuarterKeywordInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static DatePartSpecifier TransformQuarterKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDecadeKeywordInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static DatePartSpecifier TransformDecadeKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCenturyKeywordInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static DatePartSpecifier TransformCenturyKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformMillenniumKeywordInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static DatePartSpecifier TransformMillenniumKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformIntervalInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformIntervalToIntervalInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformYearToMonthInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static DatePartSpecifier TransformYearToMonth(PEGTransformer &transformer, const DatePartSpecifier &year_keyword,
	                                              const DatePartSpecifier &month_keyword);
	static unique_ptr<TransformResultValue> TransformDayToHourInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static DatePartSpecifier TransformDayToHour(PEGTransformer &transformer, const DatePartSpecifier &day_keyword,
	                                            const DatePartSpecifier &hour_keyword);
	static unique_ptr<TransformResultValue> TransformDayToMinuteInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static DatePartSpecifier TransformDayToMinute(PEGTransformer &transformer, const DatePartSpecifier &day_keyword,
	                                              const DatePartSpecifier &minute_keyword);
	static unique_ptr<TransformResultValue> TransformDayToSecondInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static DatePartSpecifier TransformDayToSecond(PEGTransformer &transformer, const DatePartSpecifier &day_keyword,
	                                              const DatePartSpecifier &second_keyword);
	static unique_ptr<TransformResultValue> TransformHourToMinuteInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static DatePartSpecifier TransformHourToMinute(PEGTransformer &transformer, const DatePartSpecifier &hour_keyword,
	                                               const DatePartSpecifier &minute_keyword);
	static unique_ptr<TransformResultValue> TransformHourToSecondInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static DatePartSpecifier TransformHourToSecond(PEGTransformer &transformer, const DatePartSpecifier &hour_keyword,
	                                               const DatePartSpecifier &second_keyword);
	static unique_ptr<TransformResultValue> TransformMinuteToSecondInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static DatePartSpecifier TransformMinuteToSecond(PEGTransformer &transformer,
	                                                 const DatePartSpecifier &minute_keyword,
	                                                 const DatePartSpecifier &second_keyword);
	static unique_ptr<TransformResultValue> TransformBitTypeInternal(PEGTransformer &transformer,
	                                                                 ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformBitType(PEGTransformer &transformer, const bool &has_result,
	                                                     optional<vector<unique_ptr<ParsedExpression>>> expression);
	static unique_ptr<TransformResultValue> TransformGeometryTypeInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformGeometryType(PEGTransformer &transformer,
	                                                          optional<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformVariantTypeInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformVariantType(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNumericTypeInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSimpleNumericTypeInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformSimpleNumericType(PEGTransformer &transformer, const string &child);
	static unique_ptr<TransformResultValue> TransformDecimalNumericTypeInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformIntTypeInternal(PEGTransformer &transformer,
	                                                                 ParseResult &parse_result);
	static string TransformIntType(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformIntegerTypeInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static string TransformIntegerType(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSmallintTypeInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static string TransformSmallintType(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformBigintTypeInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static string TransformBigintType(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformRealTypeInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static string TransformRealType(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformBooleanTypeInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static string TransformBooleanType(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDoubleTypeInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static string TransformDoubleType(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformFloatTypeInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformFloatType(PEGTransformer &transformer,
	                                                       optional<unique_ptr<ParsedExpression>> number_literal);
	static unique_ptr<TransformResultValue> TransformDecimalTypeInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformDecimalType(PEGTransformer &transformer, optional<vector<unique_ptr<ParsedExpression>>> type_modifiers);
	static unique_ptr<TransformResultValue> TransformDecTypeInternal(PEGTransformer &transformer,
	                                                                 ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformDecType(PEGTransformer &transformer,
	                                                     optional<vector<unique_ptr<ParsedExpression>>> type_modifiers);
	static unique_ptr<TransformResultValue> TransformNumericModTypeInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformNumericModType(PEGTransformer &transformer, optional<vector<unique_ptr<ParsedExpression>>> type_modifiers);
	static unique_ptr<TransformResultValue> TransformQualifiedTypeNameInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTypeNameAsQualifiedNameInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static QualifiedName TransformTypeNameAsQualifiedName(PEGTransformer &transformer, const Identifier &type_name);
	static unique_ptr<TransformResultValue> TransformCatalogReservedSchemaTypeNameInternal(PEGTransformer &transformer,
	                                                                                       ParseResult &parse_result);
	static QualifiedName TransformCatalogReservedSchemaTypeName(PEGTransformer &transformer,
	                                                            const Identifier &catalog_qualification,
	                                                            const Identifier &reserved_schema_qualification,
	                                                            const Identifier &reserved_type_name);
	static unique_ptr<TransformResultValue> TransformSchemaReservedTypeNameInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static QualifiedName TransformSchemaReservedTypeName(PEGTransformer &transformer,
	                                                     const Identifier &schema_qualification,
	                                                     const Identifier &reserved_type_name);
	static unique_ptr<TransformResultValue> TransformTypeModifiersInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformTypeModifiers(PEGTransformer &transformer, optional<vector<unique_ptr<ParsedExpression>>> expression);
	static unique_ptr<TransformResultValue> TransformRowTypeInternal(PEGTransformer &transformer,
	                                                                 ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformRowType(PEGTransformer &transformer,
	                                                     const child_list_t<LogicalType> &col_id_type_list);
	static unique_ptr<TransformResultValue> TransformSetofTypeInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformSetofType(PEGTransformer &transformer, const LogicalType &type);
	static unique_ptr<TransformResultValue> TransformUnionTypeInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformUnionType(PEGTransformer &transformer,
	                                                       const child_list_t<LogicalType> &col_id_type_list);
	static unique_ptr<TransformResultValue> TransformColIdTypeListInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static child_list_t<LogicalType> TransformColIdTypeList(PEGTransformer &transformer,
	                                                        const vector<pair<Identifier, LogicalType>> &col_id_type);
	static unique_ptr<TransformResultValue> TransformMapTypeInternal(PEGTransformer &transformer,
	                                                                 ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformMapType(PEGTransformer &transformer, const vector<LogicalType> &type);
	static unique_ptr<TransformResultValue> TransformColIdTypeInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static pair<Identifier, LogicalType> TransformColIdType(PEGTransformer &transformer, const Identifier &col_id,
	                                                        const LogicalType &type);
	static unique_ptr<TransformResultValue> TransformArrayBoundsInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformArrayKeywordInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static int64_t TransformArrayKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSquareBracketsArrayInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static int64_t TransformSquareBracketsArray(PEGTransformer &transformer,
	                                            optional<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformTimeTypeInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformTimeType(PEGTransformer &transformer,
	                                                      const LogicalTypeId &time_or_timestamp,
	                                                      optional<vector<unique_ptr<ParsedExpression>>> type_modifiers,
	                                                      const optional<bool> &time_zone);
	static unique_ptr<TransformResultValue> TransformTimeOrTimestampInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTimeTypeIdInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static LogicalTypeId TransformTimeTypeId(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTimestampTypeIdInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static LogicalTypeId TransformTimestampTypeId(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTimeZoneInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static bool TransformTimeZone(PEGTransformer &transformer, const bool &with_or_without);
	static unique_ptr<TransformResultValue> TransformWithOrWithoutInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformWithRuleInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static bool TransformWithRule(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformWithoutRuleInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static bool TransformWithoutRule(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformConnectStatementInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformConnectStatement(PEGTransformer &transformer,
	                                                          optional<unique_ptr<ConnectInfo>> session_target);
	static unique_ptr<TransformResultValue> TransformDisconnectStatementInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformDisconnectStatement(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSessionTargetInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformLocalSessionTargetInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<ConnectInfo> TransformLocalSessionTarget(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformStringSessionTargetInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<ConnectInfo> TransformStringSessionTarget(PEGTransformer &transformer,
	                                                            const string &string_literal);
	static unique_ptr<TransformResultValue> TransformCatalogSessionTargetInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<ConnectInfo> TransformCatalogSessionTarget(PEGTransformer &transformer,
	                                                             const Identifier &catalog_name);
	static unique_ptr<TransformResultValue> TransformCopyStatementInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformCopyStatement(PEGTransformer &transformer,
	                                                       unique_ptr<SQLStatement> copy_variations);
	static unique_ptr<TransformResultValue> TransformCopyVariationsInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCopyTableInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformCopyTable(PEGTransformer &transformer,
	                                                   unique_ptr<BaseTableRef> base_table_name,
	                                                   const optional<vector<string>> &insert_column_list,
	                                                   const bool &from_or_to,
	                                                   unique_ptr<ParsedExpression> copy_file_name,
	                                                   const optional<vector<GenericCopyOption>> &copy_options);
	static unique_ptr<TransformResultValue> TransformFromOrToInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCopyFromInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static bool TransformCopyFrom(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCopyToInternal(PEGTransformer &transformer,
	                                                                ParseResult &parse_result);
	static bool TransformCopyTo(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCopySelectInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformCopySelect(PEGTransformer &transformer,
	                                                    unique_ptr<SelectStatement> select_statement_internal,
	                                                    unique_ptr<ParsedExpression> copy_file_name,
	                                                    const optional<vector<GenericCopyOption>> &copy_options);
	static unique_ptr<TransformResultValue> TransformCopyFileNameInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCopyFileNameExpressionInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCopyFileNameStringLiteralInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformCopyFileNameStringLiteral(PEGTransformer &transformer,
	                                                                       const string &string_literal);
	static unique_ptr<TransformResultValue> TransformCopyFileNameIdentifierInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformCopyFileNameIdentifier(PEGTransformer &transformer,
	                                                                    const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformCopyFileNameIdentifierColIdInternal(PEGTransformer &transformer,
	                                                                                     ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformCopyFileNameIdentifierColId(PEGTransformer &transformer,
	                                                                         const Identifier &identifier_col_id);
	static unique_ptr<TransformResultValue> TransformIdentifierColIdInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static Identifier TransformIdentifierColId(PEGTransformer &transformer, const Identifier &identifier,
	                                           const Identifier &col_id);
	static unique_ptr<TransformResultValue> TransformCopyOptionsInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static vector<GenericCopyOption> TransformCopyOptions(PEGTransformer &transformer, const bool &has_result,
	                                                      const vector<GenericCopyOption> &copy_option_list);
	static unique_ptr<TransformResultValue> TransformCopyOptionListInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSpecializedOptionListInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static vector<GenericCopyOption>
	TransformSpecializedOptionList(PEGTransformer &transformer,
	                               const optional<vector<GenericCopyOption>> &specialized_option);
	static unique_ptr<TransformResultValue> TransformSpecializedOptionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSingleOptionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformBinaryOptionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static GenericCopyOption TransformBinaryOption(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformFreezeOptionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static GenericCopyOption TransformFreezeOption(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformOidsOptionInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static GenericCopyOption TransformOidsOption(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCsvOptionInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static GenericCopyOption TransformCsvOption(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformHeaderOptionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static GenericCopyOption TransformHeaderOption(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNullAsOptionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static GenericCopyOption TransformNullAsOption(PEGTransformer &transformer, const bool &has_result,
	                                               const string &string_literal);
	static unique_ptr<TransformResultValue> TransformDelimiterAsOptionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static GenericCopyOption TransformDelimiterAsOption(PEGTransformer &transformer, const bool &has_result,
	                                                    const string &string_literal);
	static unique_ptr<TransformResultValue> TransformQuoteAsOptionInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static GenericCopyOption TransformQuoteAsOption(PEGTransformer &transformer, const bool &has_result,
	                                                const string &string_literal);
	static unique_ptr<TransformResultValue> TransformEscapeAsOptionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static GenericCopyOption TransformEscapeAsOption(PEGTransformer &transformer, const bool &has_result,
	                                                 const string &string_literal);
	static unique_ptr<TransformResultValue> TransformEncodingOptionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static GenericCopyOption TransformEncodingOption(PEGTransformer &transformer, const string &string_literal);
	static unique_ptr<TransformResultValue> TransformForceQuoteOptionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static GenericCopyOption TransformForceQuoteOption(PEGTransformer &transformer, const optional<bool> &force_quote,
	                                                   const vector<string> &star_symbol_column_list);
	static unique_ptr<TransformResultValue> TransformStarSymbolColumnListInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformForceQuoteInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static bool TransformForceQuote(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformPartitionByOptionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static GenericCopyOption TransformPartitionByOption(PEGTransformer &transformer,
	                                                    const vector<string> &star_symbol_column_list);
	static unique_ptr<TransformResultValue> TransformForceNullOptionInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static GenericCopyOption TransformForceNullOption(PEGTransformer &transformer, const optional<bool> &force_not_null,
	                                                  const vector<string> &column_list);
	static unique_ptr<TransformResultValue> TransformForceNotNullInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static bool TransformForceNotNull(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformGenericCopyOptionListInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static vector<GenericCopyOption>
	TransformGenericCopyOptionList(PEGTransformer &transformer, const vector<GenericCopyOption> &generic_copy_option);
	static unique_ptr<TransformResultValue> TransformGenericCopyOptionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static GenericCopyOption TransformGenericCopyOption(PEGTransformer &transformer, const Identifier &copy_option_name,
	                                                    optional<GenericCopyOptionValue> generic_copy_option_value);
	static unique_ptr<TransformResultValue> TransformGenericCopyOptionValueInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformGenericCopyOptionOrderListInternal(PEGTransformer &transformer,
	                                                                                    ParseResult &parse_result);
	static GenericCopyOptionValue
	TransformGenericCopyOptionOrderList(PEGTransformer &transformer,
	                                    vector<OrderByNode> generic_copy_option_parenthesized_expression_list);
	static unique_ptr<TransformResultValue> TransformGenericCopyOptionExpressionInternal(PEGTransformer &transformer,
	                                                                                     ParseResult &parse_result);
	static GenericCopyOptionValue TransformGenericCopyOptionExpression(PEGTransformer &transformer,
	                                                                   unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue>
	TransformGenericCopyOptionParenthesizedExpressionListInternal(PEGTransformer &transformer,
	                                                              ParseResult &parse_result);
	static vector<OrderByNode>
	TransformGenericCopyOptionParenthesizedExpressionList(PEGTransformer &transformer,
	                                                      vector<OrderByNode> order_by_expression_list);
	static unique_ptr<TransformResultValue> TransformCopyFromDatabaseInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCopyFromDatabaseWithFlagInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformCopyFromDatabaseWithFlag(PEGTransformer &transformer,
	                                                                  const Identifier &col_id,
	                                                                  const Identifier &col_id_1,
	                                                                  const CopyDatabaseType &copy_database_flag);
	static unique_ptr<TransformResultValue> TransformCopyFromDatabaseWithoutFlagInternal(PEGTransformer &transformer,
	                                                                                     ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformCopyFromDatabaseWithoutFlag(PEGTransformer &transformer,
	                                                                     const Identifier &col_id,
	                                                                     const Identifier &col_id_1);
	static unique_ptr<TransformResultValue> TransformCopyDatabaseFlagInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static CopyDatabaseType TransformCopyDatabaseFlag(PEGTransformer &transformer,
	                                                  const CopyDatabaseType &schema_or_data);
	static unique_ptr<TransformResultValue> TransformSchemaOrDataInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCopySchemaInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static CopyDatabaseType TransformCopySchema(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCopyDataInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static CopyDatabaseType TransformCopyData(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCreateIndexStmtInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<CreateStatement>
	TransformCreateIndexStmt(PEGTransformer &transformer, const optional<bool> &unique_index,
	                         const optional<bool> &if_not_exists, const optional<Identifier> &index_name,
	                         unique_ptr<BaseTableRef> base_table_name,
	                         const optional<vector<string>> &insert_column_list, const optional<Identifier> &index_type,
	                         optional<vector<unique_ptr<ParsedExpression>>> index_element,
	                         optional<case_insensitive_map_t<unique_ptr<ParsedExpression>>> with_list,
	                         optional<unique_ptr<ParsedExpression>> where_clause);
	static unique_ptr<TransformResultValue> TransformWithListInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static case_insensitive_map_t<unique_ptr<ParsedExpression>>
	TransformWithList(PEGTransformer &transformer,
	                  case_insensitive_map_t<unique_ptr<ParsedExpression>> rel_option_or_oids);
	static unique_ptr<TransformResultValue> TransformRelOptionOrOidsInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformRelOptionListInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static case_insensitive_map_t<unique_ptr<ParsedExpression>>
	TransformRelOptionList(PEGTransformer &transformer,
	                       vector<pair<Identifier, unique_ptr<ParsedExpression>>> rel_option);
	static unique_ptr<TransformResultValue> TransformOidsInternal(PEGTransformer &transformer,
	                                                              ParseResult &parse_result);
	static case_insensitive_map_t<unique_ptr<ParsedExpression>> TransformOids(PEGTransformer &transformer,
	                                                                          const bool &with_or_without_oids);
	static unique_ptr<TransformResultValue> TransformWithOrWithoutOidsInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformWithOidsInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static bool TransformWithOids(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformWithoutOidsInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static bool TransformWithoutOids(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformIndexElementInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformIndexElement(PEGTransformer &transformer,
	                                                          unique_ptr<ParsedExpression> expression,
	                                                          const optional<OrderType> &desc_or_asc,
	                                                          const optional<OrderByNullType> &nulls_first_or_last);
	static unique_ptr<TransformResultValue> TransformUniqueIndexInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static bool TransformUniqueIndex(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformIndexTypeInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static Identifier TransformIndexType(PEGTransformer &transformer, const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformRelOptionInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static pair<Identifier, unique_ptr<ParsedExpression>>
	TransformRelOption(PEGTransformer &transformer, const Identifier &rel_option_name,
	                   optional<unique_ptr<ParsedExpression>> rel_option_argument_opt);
	static unique_ptr<TransformResultValue> TransformRelOptionNameInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static Identifier TransformRelOptionName(PEGTransformer &transformer, const string &child);
	static unique_ptr<TransformResultValue> TransformDottedIdentifierStringInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static string TransformDottedIdentifierString(PEGTransformer &transformer, const vector<string> &dotted_identifier);
	static unique_ptr<TransformResultValue> TransformRelOptionArgumentOptInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformRelOptionArgumentOpt(PEGTransformer &transformer,
	                                                                  unique_ptr<ParsedExpression> def_arg);
	static unique_ptr<TransformResultValue> TransformDefArgInternal(PEGTransformer &transformer,
	                                                                ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformDefArgNullInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformDefArgNull(PEGTransformer &transformer, const Value &null_literal);
	static unique_ptr<TransformResultValue> TransformDefArgKeywordInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformDefArgKeyword(PEGTransformer &transformer,
	                                                           const string &reserved_keyword);
	static unique_ptr<TransformResultValue> TransformDefArgStringLiteralInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformDefArgStringLiteral(PEGTransformer &transformer,
	                                                                 const string &string_literal);
	static unique_ptr<TransformResultValue> TransformNoneLiteralInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformNoneLiteral(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCreateMacroStmtInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<CreateStatement> TransformCreateMacroStmt(PEGTransformer &transformer,
	                                                            const bool &macro_or_function,
	                                                            const optional<bool> &if_not_exists,
	                                                            const QualifiedName &qualified_name,
	                                                            vector<unique_ptr<MacroFunction>> macro_definition);
	static unique_ptr<TransformResultValue> TransformMacroOrFunctionInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformMacroKeywordInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static bool TransformMacroKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformFunctionKeywordInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static bool TransformFunctionKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformMacroDefinitionInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<MacroFunction> TransformMacroDefinition(PEGTransformer &transformer,
	                                                          optional<vector<MacroParameter>> macro_parameters,
	                                                          unique_ptr<MacroFunction> macro_definition_body);
	static unique_ptr<TransformResultValue> TransformMacroDefinitionBodyInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformMacroParametersInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static vector<MacroParameter> TransformMacroParameters(PEGTransformer &transformer,
	                                                       vector<MacroParameter> macro_parameter);
	static unique_ptr<TransformResultValue> TransformMacroParameterInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSimpleParameterInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static MacroParameter TransformSimpleParameter(PEGTransformer &transformer, const Identifier &type_func_name,
	                                               const optional<LogicalType> &type);
	static unique_ptr<TransformResultValue> TransformScalarMacroDefinitionInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<MacroFunction> TransformScalarMacroDefinition(PEGTransformer &transformer,
	                                                                unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformTableMacroDefinitionInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<MacroFunction>
	TransformTableMacroDefinition(PEGTransformer &transformer, unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformCreateSchemaStmtInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<CreateStatement> TransformCreateSchemaStmt(PEGTransformer &transformer,
	                                                             const optional<bool> &if_not_exists,
	                                                             const QualifiedName &qualified_name);
	static unique_ptr<TransformResultValue> TransformCreateSecretStmtInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<CreateStatement>
	TransformCreateSecretStmt(PEGTransformer &transformer, const optional<bool> &if_not_exists,
	                          const optional<Identifier> &secret_name,
	                          const optional<Identifier> &secret_storage_specifier,
	                          const vector<GenericCopyOption> &generic_copy_option_list);
	static unique_ptr<TransformResultValue> TransformSecretStorageSpecifierInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static Identifier TransformSecretStorageSpecifier(PEGTransformer &transformer, const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformSecretNameInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static Identifier TransformSecretName(PEGTransformer &transformer, const Identifier &col_id);
	static unique_ptr<TransformResultValue> TransformCreateSequenceStmtInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<CreateStatement>
	TransformCreateSequenceStmt(PEGTransformer &transformer, const optional<bool> &if_not_exists,
	                            const QualifiedName &qualified_name,
	                            optional<vector<pair<string, unique_ptr<SequenceOption>>>> sequence_option);
	static unique_ptr<TransformResultValue> TransformSequenceOptionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSeqSetCycleInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSeqCycleInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSeqCycle(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSeqNoCycleInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSeqNoCycle(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSeqSetIncrementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSeqSetIncrement(PEGTransformer &transformer,
	                                                                         const bool &has_result,
	                                                                         unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformSeqSetMinMaxInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSeqSetMinMax(PEGTransformer &transformer,
	                                                                      const string &seq_min_or_max,
	                                                                      unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformSeqNoMinMaxInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSeqNoMinMax(PEGTransformer &transformer,
	                                                                     const string &seq_min_or_max);
	static unique_ptr<TransformResultValue> TransformSeqStartWithInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static pair<string, unique_ptr<SequenceOption>>
	TransformSeqStartWith(PEGTransformer &transformer, const bool &has_result, unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformSeqOwnedByInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static pair<string, unique_ptr<SequenceOption>> TransformSeqOwnedBy(PEGTransformer &transformer,
	                                                                    const QualifiedName &qualified_name);
	static unique_ptr<TransformResultValue> TransformSeqMinOrMaxInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformMinValueInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static string TransformMinValue(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformMaxValueInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static string TransformMaxValue(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCreateStatementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformCreateStatement(PEGTransformer &transformer,
	                                                         const optional<bool> &or_replace,
	                                                         const optional<SecretPersistType> &temporary,
	                                                         unique_ptr<CreateStatement> create_statement_variation);
	static unique_ptr<TransformResultValue> TransformCreateStatementVariationInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformOrReplaceInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static bool TransformOrReplace(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTemporaryInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformPersistentInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static SecretPersistType TransformPersistent(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTempPersistentInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static SecretPersistType TransformTempPersistent(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTemporaryPersistentInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static SecretPersistType TransformTemporaryPersistent(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCreateTableStmtInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<CreateStatement> TransformCreateTableStmt(PEGTransformer &transformer,
	                                                            const optional<bool> &if_not_exists,
	                                                            const QualifiedName &qualified_name,
	                                                            CreateTableDefinition create_table_definition,
	                                                            const optional<bool> &commit_action);
	static unique_ptr<TransformResultValue> TransformCreateTableDefinitionInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCreateTableAsInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static CreateTableDefinition
	TransformCreateTableAs(PEGTransformer &transformer, optional<ColumnList> identifier_list,
	                       optional<PartitionSortedOptions> partition_sorted_options,
	                       optional<case_insensitive_map_t<unique_ptr<ParsedExpression>>> with_list,
	                       unique_ptr<SQLStatement> statement, const optional<bool> &with_data);
	static unique_ptr<TransformResultValue> TransformPartitionSortedOptionsInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformPartitionOptSortedOptionsInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static PartitionSortedOptions
	TransformPartitionOptSortedOptions(PEGTransformer &transformer,
	                                   vector<unique_ptr<ParsedExpression>> partition_options,
	                                   optional<vector<unique_ptr<ParsedExpression>>> sorted_options);
	static unique_ptr<TransformResultValue> TransformSortedOptPartitionOptionsInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static PartitionSortedOptions
	TransformSortedOptPartitionOptions(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> sorted_options,
	                                   optional<vector<unique_ptr<ParsedExpression>>> partition_options);
	static unique_ptr<TransformResultValue> TransformPartitionOptionsInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformPartitionOptions(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformSortedOptionsInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformSortedOptions(PEGTransformer &transformer,
	                                                                   vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformWithDataInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformWithDataOnlyInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static bool TransformWithDataOnly(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformWithNoDataInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static bool TransformWithNoData(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformIdentifierListInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static ColumnList TransformIdentifierList(PEGTransformer &transformer, const vector<Identifier> &identifier);
	static unique_ptr<TransformResultValue> TransformCreateColumnListInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static CreateTableDefinition
	TransformCreateColumnList(PEGTransformer &transformer, optional<ColumnElements> create_table_column_list,
	                          optional<PartitionSortedOptions> partition_sorted_options,
	                          optional<case_insensitive_map_t<unique_ptr<ParsedExpression>>> with_list);
	static unique_ptr<TransformResultValue> TransformIfNotExistsInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static bool TransformIfNotExists(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformQualifiedNameInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue>
	TransformSchemaReservedIdentifierOrStringLiteralInternal(PEGTransformer &transformer, ParseResult &parse_result);
	static QualifiedName
	TransformSchemaReservedIdentifierOrStringLiteral(PEGTransformer &transformer,
	                                                 const Identifier &schema_qualification,
	                                                 const Identifier &reserved_identifier_or_string_literal);
	static unique_ptr<TransformResultValue>
	TransformCatalogReservedSchemaIdentifierInternal(PEGTransformer &transformer, ParseResult &parse_result);
	static QualifiedName
	TransformCatalogReservedSchemaIdentifier(PEGTransformer &transformer, const Identifier &catalog_qualification,
	                                         const Identifier &reserved_schema_qualification,
	                                         const Identifier &reserved_identifier_or_string_literal);
	static unique_ptr<TransformResultValue> TransformIdentifierOrStringLiteralInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static QualifiedName TransformIdentifierOrStringLiteral(PEGTransformer &transformer, const string &child);
	static unique_ptr<TransformResultValue>
	TransformReservedIdentifierOrStringLiteralInternal(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCatalogQualificationInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static Identifier TransformCatalogQualification(PEGTransformer &transformer, const Identifier &catalog_name);
	static unique_ptr<TransformResultValue> TransformSchemaQualificationInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static Identifier TransformSchemaQualification(PEGTransformer &transformer, const Identifier &schema_name);
	static unique_ptr<TransformResultValue> TransformReservedSchemaQualificationInternal(PEGTransformer &transformer,
	                                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTableQualificationInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static Identifier TransformTableQualification(PEGTransformer &transformer, const Identifier &table_name);
	static unique_ptr<TransformResultValue> TransformReservedTableQualificationInternal(PEGTransformer &transformer,
	                                                                                    ParseResult &parse_result);
	static Identifier TransformReservedTableQualification(PEGTransformer &transformer,
	                                                      const Identifier &reserved_table_name);
	static unique_ptr<TransformResultValue> TransformCreateTableColumnListInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static ColumnElements TransformCreateTableColumnList(PEGTransformer &transformer,
	                                                     vector<CreateTableColumnElement> create_table_column_element);
	static unique_ptr<TransformResultValue> TransformCreateTableColumnElementInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCreateTableColumnDefinitionInternal(PEGTransformer &transformer,
	                                                                                     ParseResult &parse_result);
	static CreateTableColumnElement TransformCreateTableColumnDefinition(PEGTransformer &transformer,
	                                                                     ConstraintColumnDefinition column_definition);
	static unique_ptr<TransformResultValue> TransformCreateTableConstraintInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static CreateTableColumnElement TransformCreateTableConstraint(PEGTransformer &transformer,
	                                                               unique_ptr<Constraint> top_level_constraint);
	static unique_ptr<TransformResultValue> TransformColumnDefinitionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static ConstraintColumnDefinition
	TransformColumnDefinition(PEGTransformer &transformer, const vector<string> &dotted_identifier,
	                          const optional<LogicalType> &type, optional<GeneratedColumnDefinition> generated_column,
	                          const bool &has_result, optional<vector<ColumnConstraintEntry>> column_constraint);
	static unique_ptr<TransformResultValue> TransformColumnConstraintInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformNotNullConstraintInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static ColumnConstraintEntry TransformNotNullConstraint(PEGTransformer &transformer, const bool &child);
	static unique_ptr<TransformResultValue> TransformNullConstraintInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static bool TransformNullConstraint(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNotNullColumnConstraintInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static bool TransformNotNullColumnConstraint(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformUniqueConstraintInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static ColumnConstraintEntry TransformUniqueConstraint(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformPrimaryKeyConstraintInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static ColumnConstraintEntry TransformPrimaryKeyConstraint(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDefaultValueInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static ColumnConstraintEntry TransformDefaultValue(PEGTransformer &transformer,
	                                                   unique_ptr<ParsedExpression> column_default_expr);
	static unique_ptr<TransformResultValue> TransformCheckConstraintInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static ColumnConstraintEntry TransformCheckConstraint(PEGTransformer &transformer,
	                                                      unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformForeignKeyConstraintInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static ColumnConstraintEntry TransformForeignKeyConstraint(PEGTransformer &transformer,
	                                                           unique_ptr<BaseTableRef> base_table_name,
	                                                           const optional<vector<string>> &column_list,
	                                                           const KeyActions &key_actions);
	static unique_ptr<TransformResultValue> TransformColumnCollationInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static ColumnConstraintEntry TransformColumnCollation(PEGTransformer &transformer,
	                                                      const vector<string> &dotted_identifier);
	static unique_ptr<TransformResultValue> TransformColumnCompressionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static ColumnConstraintEntry TransformColumnCompression(PEGTransformer &transformer,
	                                                        const Identifier &col_id_or_string);
	static unique_ptr<TransformResultValue> TransformKeyActionsInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static KeyActions TransformKeyActions(PEGTransformer &transformer, const optional<string> &update_action,
	                                      const optional<string> &delete_action);
	static unique_ptr<TransformResultValue> TransformUpdateActionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static string TransformUpdateAction(PEGTransformer &transformer, const string &key_action);
	static unique_ptr<TransformResultValue> TransformDeleteActionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static string TransformDeleteAction(PEGTransformer &transformer, const string &key_action);
	static unique_ptr<TransformResultValue> TransformKeyActionInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformNoKeyActionInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static string TransformNoKeyAction(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformRestrictKeyActionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static string TransformRestrictKeyAction(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCascadeKeyActionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static string TransformCascadeKeyAction(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSetNullKeyActionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static string TransformSetNullKeyAction(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSetDefaultKeyActionInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static string TransformSetDefaultKeyAction(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTopLevelConstraintInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<Constraint> TransformTopLevelConstraint(PEGTransformer &transformer, const bool &has_result,
	                                                          unique_ptr<Constraint> top_level_constraint_list);
	static unique_ptr<TransformResultValue> TransformTopLevelConstraintListInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<Constraint> TransformTopLevelConstraintList(PEGTransformer &transformer,
	                                                              ParseResult &choice_result);
	static unique_ptr<Constraint> TransformTopLevelConstraintListTrampoline(PEGTransformer &transformer,
	                                                                        TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> TransformTopPrimaryKeyConstraintInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<Constraint> TransformTopPrimaryKeyConstraint(PEGTransformer &transformer,
	                                                               const vector<string> &column_id_list);
	static unique_ptr<TransformResultValue> TransformTopUniqueConstraintInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<Constraint> TransformTopUniqueConstraint(PEGTransformer &transformer,
	                                                           const vector<string> &column_id_list);
	static unique_ptr<TransformResultValue> TransformTopForeignKeyConstraintInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<Constraint> TransformTopForeignKeyConstraint(PEGTransformer &transformer,
	                                                               const vector<string> &column_id_list,
	                                                               ColumnConstraintEntry foreign_key_constraint);
	static unique_ptr<TransformResultValue> TransformColumnIdListInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static vector<string> TransformColumnIdList(PEGTransformer &transformer, const vector<Identifier> &col_id);
	static unique_ptr<TransformResultValue> TransformDottedIdentifierInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static vector<string> TransformDottedIdentifier(PEGTransformer &transformer, const Identifier &identifier,
	                                                const optional<vector<string>> &dot_col_label);
	static unique_ptr<TransformResultValue> TransformDotColLabelInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformColIdInternal(PEGTransformer &transformer,
	                                                               ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformColIdOrStringInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static Identifier TransformColIdOrString(PEGTransformer &transformer, ParseResult &choice_result);
	static Identifier TransformColIdOrStringTrampoline(PEGTransformer &transformer, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> TransformTypeFuncNameInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformGeneratedColumnInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static GeneratedColumnDefinition TransformGeneratedColumn(PEGTransformer &transformer, const bool &has_result,
	                                                          unique_ptr<ParsedExpression> expression,
	                                                          const optional<bool> &generated_column_type);
	static unique_ptr<TransformResultValue> TransformGeneratedColumnTypeInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCommitActionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static bool TransformCommitAction(PEGTransformer &transformer, const bool &preserve_or_delete);
	static unique_ptr<TransformResultValue> TransformPreserveOrDeleteInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformPreserveRowsInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static bool TransformPreserveRows(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDeleteRowsInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static bool TransformDeleteRows(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformVirtualGeneratedColumnInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static bool TransformVirtualGeneratedColumn(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformStoredGeneratedColumnInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static bool TransformStoredGeneratedColumn(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCreateTriggerStmtInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<CreateStatement>
	TransformCreateTriggerStmt(PEGTransformer &transformer, const optional<bool> &if_not_exists,
	                           const Identifier &trigger_name, const TriggerTiming &trigger_timing,
	                           const TriggerEventInfo &trigger_event, unique_ptr<BaseTableRef> base_table_name,
	                           const optional<TriggerTableReferencingInfo> &referencing_clause,
	                           const optional<TriggerForEach> &for_each_clause, unique_ptr<SQLStatement> trigger_body);
	static unique_ptr<TransformResultValue> TransformTriggerBodyInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTriggerNameInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static Identifier TransformTriggerName(PEGTransformer &transformer, const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformReferencingClauseInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static TriggerTableReferencingInfo
	TransformReferencingClause(PEGTransformer &transformer, const TriggerTableReferencingInfo &referencing_item,
	                           const optional<TriggerTableReferencingInfo> &referencing_item_1);
	static unique_ptr<TransformResultValue> TransformReferencingItemInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformReferencingNewTableAsInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static TriggerTableReferencingInfo TransformReferencingNewTableAs(PEGTransformer &transformer,
	                                                                  const Identifier &col_id);
	static unique_ptr<TransformResultValue> TransformReferencingOldTableAsInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static TriggerTableReferencingInfo TransformReferencingOldTableAs(PEGTransformer &transformer,
	                                                                  const Identifier &col_id);
	static unique_ptr<TransformResultValue> TransformTriggerTimingInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTriggerBeforeInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static TriggerTiming TransformTriggerBefore(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTriggerAfterInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static TriggerTiming TransformTriggerAfter(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTriggerInsteadOfInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static TriggerTiming TransformTriggerInsteadOf(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTriggerEventInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTriggerEventInsertInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static TriggerEventInfo TransformTriggerEventInsert(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTriggerEventDeleteInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static TriggerEventInfo TransformTriggerEventDelete(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTriggerEventUpdateInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static TriggerEventInfo TransformTriggerEventUpdate(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTriggerEventUpdateOfInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static TriggerEventInfo TransformTriggerEventUpdateOf(PEGTransformer &transformer,
	                                                      const vector<string> &trigger_column_list);
	static unique_ptr<TransformResultValue> TransformTriggerColumnListInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static vector<string> TransformTriggerColumnList(PEGTransformer &transformer, const vector<Identifier> &col_id);
	static unique_ptr<TransformResultValue> TransformForEachClauseInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformForEachRowInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static TriggerForEach TransformForEachRow(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformForEachStatementInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static TriggerForEach TransformForEachStatement(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCreateTypeStmtInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<CreateStatement> TransformCreateTypeStmt(PEGTransformer &transformer,
	                                                           const optional<bool> &if_not_exists,
	                                                           const QualifiedName &qualified_name,
	                                                           unique_ptr<CreateTypeInfo> create_type);
	static unique_ptr<TransformResultValue> TransformCreateTypeInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCreateTypeFromTypeInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<CreateTypeInfo> TransformCreateTypeFromType(PEGTransformer &transformer, const LogicalType &type);
	static unique_ptr<TransformResultValue> TransformEnumSelectTypeInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<CreateTypeInfo> TransformEnumSelectType(PEGTransformer &transformer,
	                                                          unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformEnumStringLiteralListInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<CreateTypeInfo> TransformEnumStringLiteralList(PEGTransformer &transformer,
	                                                                 const optional<vector<string>> &string_literal);
	static unique_ptr<TransformResultValue> TransformCreateViewStmtInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<CreateStatement>
	TransformCreateViewStmt(PEGTransformer &transformer, const optional<bool> &create_recursive,
	                        const optional<bool> &if_not_exists, const QualifiedName &qualified_name,
	                        const optional<vector<string>> &insert_column_list,
	                        optional<case_insensitive_map_t<unique_ptr<ParsedExpression>>> with_list,
	                        unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformCreateRecursiveInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static bool TransformCreateRecursive(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDeallocateStatementInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformDeallocateStatement(PEGTransformer &transformer,
	                                                             const optional<bool> &deallocate_prepare,
	                                                             const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformDeallocatePrepareInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static bool TransformDeallocatePrepare(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDeleteStatementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<SQLStatement>
	TransformDeleteStatement(PEGTransformer &transformer, optional<CommonTableExpressionMap> with_clause,
	                         unique_ptr<BaseTableRef> target_opt_alias,
	                         optional<vector<unique_ptr<TableRef>>> delete_using_clause,
	                         optional<unique_ptr<ParsedExpression>> where_clause,
	                         optional<vector<unique_ptr<ParsedExpression>>> returning_clause);
	static unique_ptr<TransformResultValue> TransformTruncateStatementInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformTruncateStatement(PEGTransformer &transformer, const bool &has_result,
	                                                           unique_ptr<BaseTableRef> base_table_name);
	static unique_ptr<TransformResultValue> TransformTargetOptAliasInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<BaseTableRef> TransformTargetOptAlias(PEGTransformer &transformer,
	                                                        unique_ptr<BaseTableRef> base_table_name,
	                                                        const bool &has_result, const optional<Identifier> &col_id);
	static unique_ptr<TransformResultValue> TransformDeleteUsingClauseInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static vector<unique_ptr<TableRef>> TransformDeleteUsingClause(PEGTransformer &transformer,
	                                                               vector<unique_ptr<TableRef>> table_ref);
	static unique_ptr<TransformResultValue> TransformDescribeStatementInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<SelectStatement> TransformDescribeStatement(PEGTransformer &transformer,
	                                                              unique_ptr<QueryNode> child);
	static unique_ptr<TransformResultValue> TransformShowSelectInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<QueryNode> TransformShowSelect(PEGTransformer &transformer,
	                                                 const ShowType &show_or_describe_or_summarize,
	                                                 unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformShowAllTablesInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<QueryNode> TransformShowAllTables(PEGTransformer &transformer, const ShowType &show_or_describe);
	static unique_ptr<TransformResultValue> TransformShowQualifiedNameInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<QueryNode> TransformShowQualifiedName(PEGTransformer &transformer,
	                                                        const ShowType &show_or_describe_or_summarize,
	                                                        optional<DescribeTarget> describe_target);
	static unique_ptr<TransformResultValue> TransformShowTablesInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<QueryNode> TransformShowTables(PEGTransformer &transformer, const ShowType &show_or_describe,
	                                                 const QualifiedName &qualified_name);
	static unique_ptr<TransformResultValue> TransformDescribeTargetInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformDescribeBaseTableNameInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static DescribeTarget TransformDescribeBaseTableName(PEGTransformer &transformer,
	                                                     unique_ptr<BaseTableRef> base_table_name);
	static unique_ptr<TransformResultValue> TransformDescribeStringLiteralInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static DescribeTarget TransformDescribeStringLiteral(PEGTransformer &transformer, const string &string_literal);
	static unique_ptr<TransformResultValue> TransformShowOrDescribeOrSummarizeInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSummarizeInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSummarizeRuleInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static ShowType TransformSummarizeRule(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformShowOrDescribeInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformShowRuleInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static ShowType TransformShowRule(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDescribeRuleInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformDescribeLongRuleInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static ShowType TransformDescribeLongRule(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDescRuleInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static ShowType TransformDescRule(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDetachStatementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformDetachStatement(PEGTransformer &transformer, const bool &has_result,
	                                                         const optional<bool> &if_exists,
	                                                         const Identifier &catalog_name);
	static unique_ptr<TransformResultValue> TransformDropStatementInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformDropStatement(PEGTransformer &transformer,
	                                                       unique_ptr<DropStatement> drop_entries,
	                                                       const optional<bool> &drop_behavior);
	static unique_ptr<TransformResultValue> TransformDropEntriesInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformDropTriggerInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<DropStatement> TransformDropTrigger(PEGTransformer &transformer, const optional<bool> &if_exists,
	                                                      const Identifier &trigger_name,
	                                                      unique_ptr<BaseTableRef> base_table_name);
	static unique_ptr<TransformResultValue> TransformDropTableInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<DropStatement> TransformDropTable(PEGTransformer &transformer, const CatalogType &table_or_view,
	                                                    const optional<bool> &if_exists,
	                                                    vector<unique_ptr<BaseTableRef>> base_table_name);
	static unique_ptr<TransformResultValue> TransformDropTableFunctionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<DropStatement> TransformDropTableFunction(PEGTransformer &transformer,
	                                                            const CatalogType &comment_macro_table,
	                                                            const optional<bool> &if_exists,
	                                                            const vector<Identifier> &table_function_name);
	static unique_ptr<TransformResultValue> TransformDropFunctionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<DropStatement> TransformDropFunction(PEGTransformer &transformer, const bool &function_type_macro,
	                                                       const optional<bool> &if_exists,
	                                                       const vector<QualifiedName> &function_identifier);
	static unique_ptr<TransformResultValue> TransformDropSchemaInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<DropStatement> TransformDropSchema(PEGTransformer &transformer, const optional<bool> &if_exists,
	                                                     const vector<QualifiedName> &qualified_schema_name);
	static unique_ptr<TransformResultValue> TransformDropIndexInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<DropStatement> TransformDropIndex(PEGTransformer &transformer, const optional<bool> &if_exists,
	                                                    const vector<QualifiedName> &qualified_index_name);
	static unique_ptr<TransformResultValue> TransformQualifiedIndexNameInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformQualifiedIndexNameStringInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static QualifiedName TransformQualifiedIndexNameString(PEGTransformer &transformer, const Identifier &index_name);
	static unique_ptr<TransformResultValue> TransformSchemaReservedIndexInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static QualifiedName TransformSchemaReservedIndex(PEGTransformer &transformer,
	                                                  const Identifier &schema_qualification,
	                                                  const Identifier &reserved_index_name);
	static unique_ptr<TransformResultValue> TransformCatalogReservedSchemaIndexInternal(PEGTransformer &transformer,
	                                                                                    ParseResult &parse_result);
	static QualifiedName TransformCatalogReservedSchemaIndex(PEGTransformer &transformer,
	                                                         const Identifier &catalog_qualification,
	                                                         const Identifier &reserved_schema_qualification,
	                                                         const Identifier &reserved_index_name);
	static unique_ptr<TransformResultValue> TransformDropSequenceInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<DropStatement> TransformDropSequence(PEGTransformer &transformer, const optional<bool> &if_exists,
	                                                       const vector<QualifiedName> &qualified_sequence_name);
	static unique_ptr<TransformResultValue> TransformDropCollationInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<DropStatement> TransformDropCollation(PEGTransformer &transformer,
	                                                        const optional<bool> &if_exists,
	                                                        const vector<Identifier> &collation_name);
	static unique_ptr<TransformResultValue> TransformDropTypeInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static unique_ptr<DropStatement> TransformDropType(PEGTransformer &transformer, const optional<bool> &if_exists,
	                                                   const vector<QualifiedName> &qualified_type_name);
	static unique_ptr<TransformResultValue> TransformDropSecretInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<DropStatement> TransformDropSecret(PEGTransformer &transformer,
	                                                     const optional<SecretPersistType> &temporary,
	                                                     const optional<bool> &if_exists, const Identifier &secret_name,
	                                                     const optional<Identifier> &drop_secret_storage);
	static unique_ptr<TransformResultValue> TransformTableOrViewInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformMaterializedViewEntryInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static CatalogType TransformMaterializedViewEntry(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformFunctionTypeMacroInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformFunctionTypeMacroKeywordInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static bool TransformFunctionTypeMacroKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformFunctionTypeFunctionInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static bool TransformFunctionTypeFunction(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDropBehaviorInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCascadeDropBehaviorInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static bool TransformCascadeDropBehavior(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformRestrictDropBehaviorInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static bool TransformRestrictDropBehavior(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformIfExistsInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static bool TransformIfExists(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformQualifiedSchemaNameInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformQualifiedSchemaNameStringInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static QualifiedName TransformQualifiedSchemaNameString(PEGTransformer &transformer, const Identifier &schema_name);
	static unique_ptr<TransformResultValue> TransformCatalogReservedSchemaInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static QualifiedName TransformCatalogReservedSchema(PEGTransformer &transformer,
	                                                    const Identifier &catalog_qualification,
	                                                    const Identifier &reserved_schema_name);
	static unique_ptr<TransformResultValue> TransformDropSecretStorageInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static Identifier TransformDropSecretStorage(PEGTransformer &transformer, const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformExecuteStatementInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SQLStatement>
	TransformExecuteStatement(PEGTransformer &transformer, const Identifier &identifier,
	                          optional<vector<FunctionArgument>> table_function_arguments);
	static unique_ptr<TransformResultValue> TransformExplainStatementInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SQLStatement>
	TransformExplainStatement(PEGTransformer &transformer, const optional<bool> &explain_analyze,
	                          const optional<vector<GenericCopyOption>> &explain_option_list,
	                          unique_ptr<SQLStatement> explainable_statements);
	static unique_ptr<TransformResultValue> TransformExplainAnalyzeInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static bool TransformExplainAnalyze(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformExplainOptionListInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static vector<GenericCopyOption> TransformExplainOptionList(PEGTransformer &transformer,
	                                                            const vector<GenericCopyOption> &explain_option);
	static unique_ptr<TransformResultValue> TransformExplainOptionInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static GenericCopyOption TransformExplainOption(PEGTransformer &transformer, const Identifier &explain_option_name,
	                                                optional<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformExplainSelectStatementInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<SQLStatement>
	TransformExplainSelectStatement(PEGTransformer &transformer, unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformExplainableStatementsInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformExportStatementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<SQLStatement>
	TransformExportStatement(PEGTransformer &transformer, const optional<string> &export_source,
	                         const string &string_literal,
	                         const optional<vector<GenericCopyOption>> &generic_copy_option_list);
	static unique_ptr<TransformResultValue> TransformExportSourceInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static string TransformExportSource(PEGTransformer &transformer, const Identifier &catalog_name);
	static unique_ptr<TransformResultValue> TransformImportStatementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformImportStatement(PEGTransformer &transformer, const string &string_literal);
	static unique_ptr<TransformResultValue> TransformColumnReferenceInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformColumnReference(PEGTransformer &transformer,
	                                                             unique_ptr<ColumnRefExpression> child);
	static unique_ptr<TransformResultValue>
	TransformCatalogReservedSchemaTableColumnNameInternal(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<ColumnRefExpression>
	TransformCatalogReservedSchemaTableColumnName(PEGTransformer &transformer, const Identifier &catalog_qualification,
	                                              const Identifier &reserved_schema_qualification,
	                                              const Identifier &reserved_table_qualification,
	                                              const Identifier &reserved_column_name);
	static unique_ptr<TransformResultValue> TransformSchemaReservedTableColumnNameInternal(PEGTransformer &transformer,
	                                                                                       ParseResult &parse_result);
	static unique_ptr<ColumnRefExpression>
	TransformSchemaReservedTableColumnName(PEGTransformer &transformer, const Identifier &schema_qualification,
	                                       const Identifier &reserved_table_qualification,
	                                       const Identifier &reserved_column_name);
	static unique_ptr<TransformResultValue> TransformTableReservedColumnNameInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<ColumnRefExpression> TransformTableReservedColumnName(PEGTransformer &transformer,
	                                                                        const Identifier &table_qualification,
	                                                                        const Identifier &reserved_column_name);
	static unique_ptr<TransformResultValue> TransformFunctionExpressionInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformFunctionExpression(PEGTransformer &transformer, const QualifiedName &function_identifier,
	                            MethodArguments function_expression_arguments,
	                            optional<vector<OrderByNode>> within_group_clause,
	                            optional<unique_ptr<ParsedExpression>> filter_clause, const bool &has_result,
	                            optional<unique_ptr<WindowExpression>> over_clause);
	static unique_ptr<TransformResultValue> TransformFunctionExpressionArgumentsInternal(PEGTransformer &transformer,
	                                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformFunctionExpressionArgumentListInternal(PEGTransformer &transformer,
	                                                                                        ParseResult &parse_result);
	static MethodArguments
	TransformFunctionExpressionArgumentList(PEGTransformer &transformer, const optional<bool> &distinct_or_all,
	                                        optional<vector<FunctionArgument>> function_argument_list,
	                                        optional<vector<OrderByNode>> order_by_clause,
	                                        const optional<bool> &ignore_or_respect_nulls);
	static unique_ptr<TransformResultValue> TransformFunctionArgumentListInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformFunctionIdentifierInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static QualifiedName TransformFunctionIdentifier(PEGTransformer &transformer, ParseResult &choice_result);
	static QualifiedName TransformFunctionIdentifierTrampoline(PEGTransformer &transformer, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue>
	TransformCatalogReservedSchemaFunctionNameInternal(PEGTransformer &transformer, ParseResult &parse_result);
	static QualifiedName
	TransformCatalogReservedSchemaFunctionName(PEGTransformer &transformer, const Identifier &catalog_qualification,
	                                           const optional<Identifier> &reserved_schema_qualification,
	                                           const Identifier &reserved_function_name);
	static unique_ptr<TransformResultValue> TransformSchemaReservedFunctionNameInternal(PEGTransformer &transformer,
	                                                                                    ParseResult &parse_result);
	static QualifiedName TransformSchemaReservedFunctionName(PEGTransformer &transformer,
	                                                         const Identifier &schema_qualification,
	                                                         const Identifier &reserved_function_name);
	static unique_ptr<TransformResultValue> TransformDistinctOrAllInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformDistinctKeywordInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static bool TransformDistinctKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformAllKeywordInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static bool TransformAllKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformWithinGroupClauseInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static vector<OrderByNode> TransformWithinGroupClause(PEGTransformer &transformer,
	                                                      vector<OrderByNode> order_by_clause);
	static unique_ptr<TransformResultValue> TransformFilterClauseInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformFilterClause(PEGTransformer &transformer,
	                                                          unique_ptr<ParsedExpression> filter_clause_expression);
	static unique_ptr<TransformResultValue> TransformFilterClauseExpressionInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformFilterClauseExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> filter_clause_contents);
	static unique_ptr<TransformResultValue> TransformFilterClauseContentsInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformFilterClauseContents(PEGTransformer &transformer,
	                                                                  const bool &has_result,
	                                                                  unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformIgnoreOrRespectNullsInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformIgnoreNullsInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static bool TransformIgnoreNulls(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformRespectNullsInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static bool TransformRespectNulls(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformParenthesisExpressionInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformParenthesisExpression(PEGTransformer &transformer,
	                                                                   vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformLiteralExpressionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformLiteralExpression(PEGTransformer &transformer,
	                                                               ParseResult &choice_result);
	static unique_ptr<ParsedExpression> TransformLiteralExpressionTrampoline(PEGTransformer &transformer,
	                                                                         TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> TransformConstantLiteralInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformConstantLiteral(PEGTransformer &transformer, const Value &child);
	static unique_ptr<TransformResultValue> TransformNullLiteralInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static Value TransformNullLiteral(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTrueLiteralInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static Value TransformTrueLiteral(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformFalseLiteralInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static Value TransformFalseLiteral(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCastExpressionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformCastExpression(PEGTransformer &transformer, const bool &cast_or_try_cast, CastArguments cast_arguments);
	static unique_ptr<TransformResultValue> TransformCastArgumentsInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static CastArguments TransformCastArguments(PEGTransformer &transformer, unique_ptr<ParsedExpression> expression,
	                                            const LogicalType &type);
	static unique_ptr<TransformResultValue> TransformCastOrTryCastInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCastKeywordInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static bool TransformCastKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTryCastKeywordInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static bool TransformTryCastKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformColIdDotInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static string TransformColIdDot(PEGTransformer &transformer, const Identifier &col_id);
	static unique_ptr<TransformResultValue> TransformStarExpressionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformStarExpression(PEGTransformer &transformer, const optional<vector<string>> &star_qualifier_list,
	                        const optional<qualified_column_set_t> &exclude_list,
	                        optional<case_insensitive_map_t<unique_ptr<ParsedExpression>>> replace_list,
	                        const optional<qualified_column_map_t<string>> &rename_list);
	static unique_ptr<TransformResultValue> TransformStarQualifierListInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformExcludeListInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static qualified_column_set_t TransformExcludeList(PEGTransformer &transformer,
	                                                   const qualified_column_set_t &exclude_names);
	static unique_ptr<TransformResultValue> TransformExcludeNamesInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformExcludeNameListInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static qualified_column_set_t TransformExcludeNameList(PEGTransformer &transformer,
	                                                       const vector<QualifiedColumnName> &exclude_name);
	static unique_ptr<TransformResultValue> TransformExcludeNameSingleInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static qualified_column_set_t TransformExcludeNameSingle(PEGTransformer &transformer,
	                                                         const QualifiedColumnName &exclude_name);
	static unique_ptr<TransformResultValue> TransformExcludeNameInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformExcludeDottedNameInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static QualifiedColumnName TransformExcludeDottedName(PEGTransformer &transformer,
	                                                      const vector<string> &dotted_identifier);
	static unique_ptr<TransformResultValue> TransformExcludeColumnNameInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static QualifiedColumnName TransformExcludeColumnName(PEGTransformer &transformer,
	                                                      const Identifier &col_id_or_string);
	static unique_ptr<TransformResultValue> TransformReplaceListInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static case_insensitive_map_t<unique_ptr<ParsedExpression>>
	TransformReplaceList(PEGTransformer &transformer,
	                     case_insensitive_map_t<unique_ptr<ParsedExpression>> replace_entries);
	static unique_ptr<TransformResultValue> TransformReplaceEntriesInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformReplaceEntrySingleInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static case_insensitive_map_t<unique_ptr<ParsedExpression>>
	TransformReplaceEntrySingle(PEGTransformer &transformer, pair<string, unique_ptr<ParsedExpression>> replace_entry);
	static unique_ptr<TransformResultValue> TransformReplaceEntryListInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static case_insensitive_map_t<unique_ptr<ParsedExpression>>
	TransformReplaceEntryList(PEGTransformer &transformer,
	                          vector<pair<string, unique_ptr<ParsedExpression>>> replace_entry);
	static unique_ptr<TransformResultValue> TransformReplaceEntryInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static pair<string, unique_ptr<ParsedExpression>>
	TransformReplaceEntry(PEGTransformer &transformer, unique_ptr<ParsedExpression> expression,
	                      unique_ptr<ParsedExpression> column_reference);
	static unique_ptr<TransformResultValue> TransformRenameListInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static qualified_column_map_t<string> TransformRenameList(PEGTransformer &transformer,
	                                                          const qualified_column_map_t<string> &rename_entries);
	static unique_ptr<TransformResultValue> TransformRenameEntriesInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformRenameEntryListInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static qualified_column_map_t<string>
	TransformRenameEntryList(PEGTransformer &transformer,
	                         const vector<pair<QualifiedColumnName, string>> &rename_entry);
	static unique_ptr<TransformResultValue> TransformSingleRenameEntryInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static qualified_column_map_t<string>
	TransformSingleRenameEntry(PEGTransformer &transformer, const pair<QualifiedColumnName, string> &rename_entry);
	static unique_ptr<TransformResultValue> TransformRenameEntryInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static pair<QualifiedColumnName, string> TransformRenameEntry(PEGTransformer &transformer,
	                                                              const QualifiedColumnName &exclude_name,
	                                                              const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformSubqueryExpressionInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformSubqueryExpression(PEGTransformer &transformer,
	                                                                const optional<bool> &subquery_not,
	                                                                const optional<bool> &subquery_exists,
	                                                                unique_ptr<TableRef> subquery_reference);
	static unique_ptr<TransformResultValue> TransformSubqueryNotInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static bool TransformSubqueryNot(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSubqueryExistsInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static bool TransformSubqueryExists(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCaseExpressionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformCaseExpression(PEGTransformer &transformer,
	                                                            optional<unique_ptr<ParsedExpression>> expression,
	                                                            vector<CaseCheck> case_when_then,
	                                                            optional<unique_ptr<ParsedExpression>> case_else);
	static unique_ptr<TransformResultValue> TransformCaseWhenThenInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static CaseCheck TransformCaseWhenThen(PEGTransformer &transformer, unique_ptr<ParsedExpression> expression,
	                                       unique_ptr<ParsedExpression> expression_1);
	static unique_ptr<TransformResultValue> TransformCaseElseInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformCaseElse(PEGTransformer &transformer,
	                                                      unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformTypeLiteralInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformTypeLiteral(PEGTransformer &transformer, const Identifier &col_id,
	                                                         const string &string_literal);
	static unique_ptr<TransformResultValue> TransformIntervalLiteralInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformIntervalLiteral(PEGTransformer &transformer,
	                                                             unique_ptr<ParsedExpression> interval_parameter,
	                                                             const optional<DatePartSpecifier> &interval);
	static unique_ptr<TransformResultValue> TransformIntervalParameterInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformIntervalStringParameterInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformIntervalStringParameter(PEGTransformer &transformer,
	                                                                     const string &string_literal);
	static unique_ptr<TransformResultValue> TransformFrameClauseInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static WindowFrame TransformFrameClause(PEGTransformer &transformer, const string &framing,
	                                        vector<WindowBoundaryExpression> frame_extent,
	                                        const optional<WindowExcludeMode> &window_exclude_clause);
	static unique_ptr<TransformResultValue> TransformFramingInternal(PEGTransformer &transformer,
	                                                                 ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformRowsFramingInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static string TransformRowsFraming(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformRangeFramingInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static string TransformRangeFraming(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformGroupsFramingInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static string TransformGroupsFraming(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformFrameExtentInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSingleFrameExtentInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static vector<WindowBoundaryExpression> TransformSingleFrameExtent(PEGTransformer &transformer,
	                                                                   WindowBoundaryExpression frame_bound);
	static unique_ptr<TransformResultValue> TransformBetweenFrameExtentInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static vector<WindowBoundaryExpression> TransformBetweenFrameExtent(PEGTransformer &transformer,
	                                                                    WindowBoundaryExpression frame_bound,
	                                                                    WindowBoundaryExpression frame_bound_1);
	static unique_ptr<TransformResultValue> TransformFrameBoundInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformFrameUnboundedInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static WindowBoundaryExpression TransformFrameUnbounded(PEGTransformer &transformer,
	                                                        const bool &preceding_or_following);
	static unique_ptr<TransformResultValue> TransformFrameExpressionInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static WindowBoundaryExpression TransformFrameExpression(PEGTransformer &transformer,
	                                                         unique_ptr<ParsedExpression> expression,
	                                                         const bool &preceding_or_following);
	static unique_ptr<TransformResultValue> TransformFrameCurrentRowInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static WindowBoundaryExpression TransformFrameCurrentRow(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformPrecedingOrFollowingInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformPrecedingFrameInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static bool TransformPrecedingFrame(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformFollowingFrameInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static bool TransformFollowingFrame(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformWindowExcludeClauseInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static WindowExcludeMode TransformWindowExcludeClause(PEGTransformer &transformer,
	                                                      const WindowExcludeMode &window_exclude_element);
	static unique_ptr<TransformResultValue> TransformWindowExcludeElementInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformExcludeCurrentRowInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static WindowExcludeMode TransformExcludeCurrentRow(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformExcludeGroupInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static WindowExcludeMode TransformExcludeGroup(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformExcludeTiesInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static WindowExcludeMode TransformExcludeTies(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformExcludeNoOthersInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static WindowExcludeMode TransformExcludeNoOthers(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformWindowFrameInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<WindowExpression> TransformWindowFrame(PEGTransformer &transformer, ParseResult &choice_result);
	static unique_ptr<WindowExpression> TransformWindowFrameTrampoline(PEGTransformer &transformer,
	                                                                   TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> TransformParensIdentifierInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<WindowExpression> TransformParensIdentifier(PEGTransformer &transformer,
	                                                              const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformWindowFrameDefinitionInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformWindowFrameNameContentsParensInternal(PEGTransformer &transformer,
	                                                                                       ParseResult &parse_result);
	static unique_ptr<WindowExpression>
	TransformWindowFrameNameContentsParens(PEGTransformer &transformer,
	                                       unique_ptr<WindowExpression> window_frame_name_contents);
	static unique_ptr<TransformResultValue> TransformWindowFrameNameContentsInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<WindowExpression>
	TransformWindowFrameNameContents(PEGTransformer &transformer, const optional<Identifier> &base_window_name,
	                                 unique_ptr<WindowExpression> window_frame_contents);
	static unique_ptr<TransformResultValue> TransformWindowFrameContentsParensInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static unique_ptr<WindowExpression>
	TransformWindowFrameContentsParens(PEGTransformer &transformer, unique_ptr<WindowExpression> window_frame_contents);
	static unique_ptr<TransformResultValue> TransformWindowFrameContentsInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<WindowExpression>
	TransformWindowFrameContents(PEGTransformer &transformer,
	                             optional<vector<unique_ptr<ParsedExpression>>> window_partition,
	                             optional<vector<OrderByNode>> order_by_clause, optional<WindowFrame> frame_clause);
	static unique_ptr<TransformResultValue> TransformBaseWindowNameInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static Identifier TransformBaseWindowName(PEGTransformer &transformer, const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformWindowPartitionInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformWindowPartition(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformListExpressionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformArrayBoundedListExpressionInternal(PEGTransformer &transformer,
	                                                                                    ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformArrayBoundedListExpression(PEGTransformer &transformer, const bool &has_result,
	                                    vector<unique_ptr<ParsedExpression>> bounded_list_expression);
	static unique_ptr<TransformResultValue> TransformArrayParensSelectInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformArrayParensSelect(PEGTransformer &transformer, unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformBoundedListExpressionInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformBoundedListExpression(PEGTransformer &transformer,
	                               optional<vector<unique_ptr<ParsedExpression>>> expression);
	static unique_ptr<TransformResultValue> TransformStructExpressionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformStructExpression(PEGTransformer &transformer,
	                                                              vector<FunctionArgument> struct_field);
	static unique_ptr<TransformResultValue> TransformStructFieldInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static FunctionArgument TransformStructField(PEGTransformer &transformer, const Identifier &col_id_or_string,
	                                             unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformMapExpressionInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformMapExpression(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> map_struct_expression);
	static unique_ptr<TransformResultValue> TransformMapStructExpressionInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformMapStructExpression(PEGTransformer &transformer,
	                             optional<vector<vector<unique_ptr<ParsedExpression>>>> map_struct_field);
	static unique_ptr<TransformResultValue> TransformMapStructFieldInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformMapStructField(PEGTransformer &transformer,
	                                                                    unique_ptr<ParsedExpression> expression,
	                                                                    unique_ptr<ParsedExpression> expression_1);
	static unique_ptr<TransformResultValue> TransformGroupingExpressionInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformGroupingExpression(PEGTransformer &transformer, const bool &grouping_or_grouping_id,
	                            optional<vector<unique_ptr<ParsedExpression>>> expression);
	static unique_ptr<TransformResultValue> TransformGroupingOrGroupingIdInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformGroupingKeywordInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static bool TransformGroupingKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformGroupingIdKeywordInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static bool TransformGroupingIdKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformParameterInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformQuestionMarkNumberedParameterInternal(PEGTransformer &transformer,
	                                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformQuestionMarkNumberedParameter(PEGTransformer &transformer, unique_ptr<ParsedExpression> number_literal);
	static unique_ptr<TransformResultValue> TransformAnonymousParameterInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformAnonymousParameter(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNumberedParameterInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformNumberedParameter(PEGTransformer &transformer,
	                                                               unique_ptr<ParsedExpression> number_literal);
	static unique_ptr<TransformResultValue> TransformColLabelParameterInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformColLabelParameter(PEGTransformer &transformer,
	                                                               const string &col_label);
	static unique_ptr<TransformResultValue> TransformPositionalExpressionInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformPositionalExpression(PEGTransformer &transformer,
	                                                                  unique_ptr<ParsedExpression> number_literal);
	static unique_ptr<TransformResultValue> TransformDefaultExpressionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformDefaultExpression(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformListComprehensionExpressionInternal(PEGTransformer &transformer,
	                                                                                     ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformListComprehensionExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> expression,
	                                     const vector<Identifier> &col_id_or_string,
	                                     unique_ptr<ParsedExpression> expression_1,
	                                     optional<unique_ptr<ParsedExpression>> list_comprehension_filter);
	static unique_ptr<TransformResultValue> TransformListComprehensionFilterInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformListComprehensionFilter(PEGTransformer &transformer,
	                                                                     unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformParensExpressionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformParensExpression(PEGTransformer &transformer,
	                                                              unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformSingleExpressionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformColumnDefaultExprInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformLambdaArrowExpressionInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformLambdaArrowExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> logical_or_expression,
	                               optional<vector<unique_ptr<ParsedExpression>>> single_arrow_pair);
	static unique_ptr<TransformResultValue> TransformSingleArrowPairInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformLogicalOrExpressionInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformLogicalOrExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> logical_and_expression,
	                             optional<vector<unique_ptr<ParsedExpression>>> logical_or_expression_tail);
	static unique_ptr<TransformResultValue> TransformLogicalOrExpressionTailInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformColDefOrExprInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformColDefOrExpr(PEGTransformer &transformer, unique_ptr<ParsedExpression> col_def_and_expr,
	                      optional<vector<unique_ptr<ParsedExpression>>> col_def_or_expression_tail);
	static unique_ptr<TransformResultValue> TransformColDefOrExpressionTailInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformLogicalAndExpressionInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformLogicalAndExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> logical_not_expression,
	                              optional<vector<unique_ptr<ParsedExpression>>> logical_and_expression_tail);
	static unique_ptr<TransformResultValue> TransformLogicalAndExpressionTailInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformColDefAndExprInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformColDefAndExpr(PEGTransformer &transformer, unique_ptr<ParsedExpression> is_distinct_from_expression,
	                       optional<vector<unique_ptr<ParsedExpression>>> col_def_and_expression_tail);
	static unique_ptr<TransformResultValue> TransformColDefAndExpressionTailInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformLogicalNotExpressionInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformLogicalNotExpression(PEGTransformer &transformer,
	                                                                  optional<vector<bool>> not_expression,
	                                                                  unique_ptr<ParsedExpression> is_expression);
	static unique_ptr<TransformResultValue> TransformNotExpressionInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformNotKeywordInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static bool TransformNotKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformIsExpressionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformIsExpression(PEGTransformer &transformer,
	                                                          unique_ptr<ParsedExpression> is_distinct_from_expression,
	                                                          optional<vector<unique_ptr<ParsedExpression>>> is_test);
	static unique_ptr<TransformResultValue> TransformIsTestInternal(PEGTransformer &transformer,
	                                                                ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformIsLiteralInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformIsLiteral(PEGTransformer &transformer, const bool &has_result,
	                                                       const Value &is_literal_value);
	static unique_ptr<TransformResultValue> TransformIsLiteralValueInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformUnknownLiteralInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static Value TransformUnknownLiteral(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNotNullInternal(PEGTransformer &transformer,
	                                                                 ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformNotNullKeywordInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformNotNullKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNotNullOperatorInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformNotNullOperator(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformIsNullInternal(PEGTransformer &transformer,
	                                                                ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformIsNullOperatorInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformIsNullOperator(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformIsDistinctFromExpressionInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformIsDistinctFromExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> comparison_expression,
	                                  optional<vector<IsDistinctFromTail>> is_distinct_from_tail);
	static unique_ptr<TransformResultValue> TransformIsDistinctFromTailInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static IsDistinctFromTail TransformIsDistinctFromTail(PEGTransformer &transformer,
	                                                      const ExpressionType &is_distinct_from_op,
	                                                      unique_ptr<ParsedExpression> comparison_expression);
	static unique_ptr<TransformResultValue> TransformIsDistinctFromOpInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static ExpressionType TransformIsDistinctFromOp(PEGTransformer &transformer, const bool &has_result);
	static unique_ptr<TransformResultValue> TransformComparisonExpressionInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformComparisonExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> between_in_like_expression,
	                              optional<vector<ComparisonExpressionTail>> comparison_expression_tail);
	static unique_ptr<TransformResultValue> TransformComparisonExpressionTailInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static ComparisonExpressionTail
	TransformComparisonExpressionTail(PEGTransformer &transformer, const ExpressionType &comparison_operator,
	                                  optional<vector<bool>> not_expression,
	                                  unique_ptr<ParsedExpression> between_in_like_expression);
	static unique_ptr<TransformResultValue> TransformComparisonOperatorInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformOperatorEqualInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static ExpressionType TransformOperatorEqual(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformOperatorNotEqualInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static ExpressionType TransformOperatorNotEqual(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformOperatorLessThanInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static ExpressionType TransformOperatorLessThan(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformOperatorGreaterThanInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static ExpressionType TransformOperatorGreaterThan(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformOperatorLessThanEqualsInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static ExpressionType TransformOperatorLessThanEquals(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformOperatorGreaterThanEqualsInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static ExpressionType TransformOperatorGreaterThanEquals(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformBetweenInLikeExpressionInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformBetweenInLikeExpression(PEGTransformer &transformer,
	                                 unique_ptr<ParsedExpression> other_operator_expression,
	                                 optional<BetweenInLikeOperator> between_in_like_op);
	static unique_ptr<TransformResultValue> TransformBetweenInLikeOpInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static BetweenInLikeOperator TransformBetweenInLikeOp(PEGTransformer &transformer, const bool &has_result,
	                                                      unique_ptr<ParsedExpression> between_in_like_op_expression);
	static unique_ptr<TransformResultValue> TransformBetweenInLikeOpExpressionInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformLikeClauseInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformLikeClause(PEGTransformer &transformer, const string &like_variations,
	                                                        unique_ptr<ParsedExpression> other_operator_expression,
	                                                        optional<unique_ptr<ParsedExpression>> escape_clause);
	static unique_ptr<TransformResultValue> TransformEscapeClauseInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformEscapeClause(PEGTransformer &transformer,
	                                                          unique_ptr<ParsedExpression> comparison_expression);
	static unique_ptr<TransformResultValue> TransformLikeVariationsInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformLikeTokenInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static string TransformLikeToken(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformILikeTokenInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static string TransformILikeToken(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformGlobTokenInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static string TransformGlobToken(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSimilarToTokenInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static string TransformSimilarToToken(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNotILikeOpInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static string TransformNotILikeOp(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNotLikeOpInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static string TransformNotLikeOp(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNotSimilarToOpInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static string TransformNotSimilarToOp(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformInClauseInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformInClause(PEGTransformer &transformer,
	                                                      unique_ptr<ParsedExpression> in_expression);
	static unique_ptr<TransformResultValue> TransformInExpressionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformInContainsExpressionInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformInContainsExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> other_operator_expression);
	static unique_ptr<TransformResultValue> TransformInExpressionListInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformInExpressionList(PEGTransformer &transformer,
	                                                              vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformInSelectStatementInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformInSelectStatement(PEGTransformer &transformer, unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformBetweenClauseInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformBetweenClause(PEGTransformer &transformer, unique_ptr<ParsedExpression> other_operator_expression,
	                       unique_ptr<ParsedExpression> other_operator_expression_1);
	static unique_ptr<TransformResultValue> TransformOtherOperatorExpressionInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformOtherOperatorExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> bitwise_expression,
	                                 optional<vector<OtherOperatorTail>> other_operator_tail);
	static unique_ptr<TransformResultValue> TransformOtherOperatorTailInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static OtherOperatorTail TransformOtherOperatorTail(PEGTransformer &transformer, ParsedOperator other_operator,
	                                                    unique_ptr<ParsedExpression> bitwise_expression);
	static unique_ptr<TransformResultValue> TransformOtherOperatorInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static ParsedOperator TransformOtherOperator(PEGTransformer &transformer, ParseResult &choice_result);
	static ParsedOperator TransformOtherOperatorTrampoline(PEGTransformer &transformer, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> TransformAnyAllOperatorInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static pair<string, bool> TransformAnyAllOperator(PEGTransformer &transformer, const string &any_op,
	                                                  const bool &any_or_all);
	static unique_ptr<TransformResultValue> TransformAnyOrAllInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSubqueryAnyInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static bool TransformSubqueryAny(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSubqueryAllInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static bool TransformSubqueryAll(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformInetOperatorInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformJsonOperatorInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformListOperatorInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformStringOperatorInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformQualifiedOperatorInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static string TransformQualifiedOperator(PEGTransformer &transformer, const string &qualified_operator_contents);
	static unique_ptr<TransformResultValue> TransformQualifiedOperatorContentsInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static string TransformQualifiedOperatorContents(PEGTransformer &transformer,
	                                                 const optional<vector<string>> &col_id_dot, const string &any_op);
	static unique_ptr<TransformResultValue> TransformAnyOpInternal(PEGTransformer &transformer,
	                                                               ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformBitwiseExpressionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformBitwiseExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> additive_expression,
	                           optional<vector<BinaryExpressionTail>> bitwise_expression_tail);
	static unique_ptr<TransformResultValue> TransformBitwiseExpressionTailInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static BinaryExpressionTail TransformBitwiseExpressionTail(PEGTransformer &transformer, const string &bit_operator,
	                                                           unique_ptr<ParsedExpression> additive_expression);
	static unique_ptr<TransformResultValue> TransformBitOperatorInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformAdditiveExpressionInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformAdditiveExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> multiplicative_expression,
	                            optional<vector<BinaryExpressionTail>> additive_expression_tail);
	static unique_ptr<TransformResultValue> TransformAdditiveExpressionTailInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static BinaryExpressionTail TransformAdditiveExpressionTail(PEGTransformer &transformer, const string &term,
	                                                            unique_ptr<ParsedExpression> multiplicative_expression,
	                                                            optional_idx query_location);
	static unique_ptr<TransformResultValue> TransformTermInternal(PEGTransformer &transformer,
	                                                              ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformMultiplicativeExpressionInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformMultiplicativeExpression(PEGTransformer &transformer,
	                                  unique_ptr<ParsedExpression> exponentiation_expression,
	                                  optional<vector<BinaryExpressionTail>> multiplicative_expression_tail);
	static unique_ptr<TransformResultValue> TransformMultiplicativeExpressionTailInternal(PEGTransformer &transformer,
	                                                                                      ParseResult &parse_result);
	static BinaryExpressionTail
	TransformMultiplicativeExpressionTail(PEGTransformer &transformer, const string &factor,
	                                      unique_ptr<ParsedExpression> exponentiation_expression);
	static unique_ptr<TransformResultValue> TransformFactorInternal(PEGTransformer &transformer,
	                                                                ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformExponentiationExpressionInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformExponentiationExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> collate_expression,
	                                  optional<vector<BinaryExpressionTail>> exponentiation_expression_tail);
	static unique_ptr<TransformResultValue> TransformExponentiationExpressionTailInternal(PEGTransformer &transformer,
	                                                                                      ParseResult &parse_result);
	static BinaryExpressionTail TransformExponentiationExpressionTail(PEGTransformer &transformer,
	                                                                  const string &exponent_operator,
	                                                                  unique_ptr<ParsedExpression> collate_expression);
	static unique_ptr<TransformResultValue> TransformExponentOperatorInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCollateExpressionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformCollateExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> at_time_zone_expression,
	                           optional<vector<unique_ptr<ParsedExpression>>> collate_expression_tail);
	static unique_ptr<TransformResultValue> TransformCollateExpressionTailInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformAtTimeZoneExpressionInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformAtTimeZoneExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> prefix_expression,
	                              optional<vector<unique_ptr<ParsedExpression>>> at_time_zone_expression_tail);
	static unique_ptr<TransformResultValue> TransformAtTimeZoneExpressionTailInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformPrefixOperatorInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformMinusPrefixOperatorInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformPlusPrefixOperatorInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTildePrefixOperatorInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformBaseExpressionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformBaseExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> single_expression,
	                        optional<vector<unique_ptr<ParsedExpression>>> indirection_list);
	static unique_ptr<TransformResultValue> TransformIndirectionListInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformIndirectionInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCastOperatorInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformCastOperator(PEGTransformer &transformer, const LogicalType &type);
	static unique_ptr<TransformResultValue> TransformDotOperatorInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformDotMethodOperatorInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformDotMethodOperator(PEGTransformer &transformer,
	                                                               unique_ptr<ParsedExpression> method_expression);
	static unique_ptr<TransformResultValue> TransformDotColumnOperatorInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformDotColumnOperator(PEGTransformer &transformer,
	                                                               const string &col_label);
	static unique_ptr<TransformResultValue> TransformMethodExpressionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformMethodExpression(PEGTransformer &transformer, const string &col_label,
	                                                              MethodArguments method_expression_arguments);
	static unique_ptr<TransformResultValue> TransformMethodExpressionArgumentsInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static MethodArguments TransformMethodExpressionArguments(PEGTransformer &transformer,
	                                                          MethodArguments method_expression_argument_list);
	static unique_ptr<TransformResultValue> TransformMethodExpressionArgumentListInternal(PEGTransformer &transformer,
	                                                                                      ParseResult &parse_result);
	static MethodArguments
	TransformMethodExpressionArgumentList(PEGTransformer &transformer, const optional<bool> &distinct_or_all,
	                                      optional<vector<FunctionArgument>> method_function_arguments,
	                                      optional<vector<OrderByNode>> order_by_clause,
	                                      const optional<bool> &ignore_or_respect_nulls);
	static unique_ptr<TransformResultValue> TransformMethodFunctionArgumentsInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static vector<FunctionArgument> TransformMethodFunctionArguments(PEGTransformer &transformer,
	                                                                 vector<FunctionArgument> function_argument);
	static unique_ptr<TransformResultValue> TransformSliceExpressionInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformSliceExpression(PEGTransformer &transformer,
	                                                             vector<unique_ptr<ParsedExpression>> slice_bound);
	static unique_ptr<TransformResultValue> TransformSliceBoundInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformSliceBound(PEGTransformer &transformer, optional<unique_ptr<ParsedExpression>> expression,
	                    optional<unique_ptr<ParsedExpression>> end_slice_bound,
	                    optional<unique_ptr<ParsedExpression>> step_slice_bound);
	static unique_ptr<TransformResultValue> TransformEndSliceBoundInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformEndSliceBound(PEGTransformer &transformer,
	                                                           optional<unique_ptr<ParsedExpression>> end_slice_value);
	static unique_ptr<TransformResultValue> TransformEndSliceValueInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformEndSliceMinusInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformEndSliceMinus(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformStepSliceBoundInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformStepSliceBound(PEGTransformer &transformer,
	                                                            optional<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformPostfixOperatorInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformPostfixOperator(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSpecialFunctionExpressionInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCoalesceExpressionInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformCoalesceExpression(PEGTransformer &transformer,
	                                                                vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformUnpackExpressionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformUnpackExpression(PEGTransformer &transformer,
	                                                              unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformTryExpressionInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformTryExpression(PEGTransformer &transformer,
	                                                           unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformColumnsExpressionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformColumnsExpression(PEGTransformer &transformer, const bool &has_result,
	                                                               unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformExtractExpressionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformExtractExpression(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> extract_arguments);
	static unique_ptr<TransformResultValue> TransformExtractArgumentsInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformExtractArguments(PEGTransformer &transformer,
	                                                                      unique_ptr<ParsedExpression> extract_argument,
	                                                                      unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformLambdaExpressionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformLambdaExpression(PEGTransformer &transformer,
	                                                              const vector<Identifier> &col_id_or_string,
	                                                              unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformNullIfExpressionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformNullIfExpression(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> null_if_arguments);
	static unique_ptr<TransformResultValue> TransformNullIfArgumentsInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformNullIfArguments(PEGTransformer &transformer,
	                                                                     unique_ptr<ParsedExpression> expression,
	                                                                     unique_ptr<ParsedExpression> expression_1);
	static unique_ptr<TransformResultValue> TransformPositionExpressionInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformPositionExpression(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> position_arguments);
	static unique_ptr<TransformResultValue> TransformPositionArgumentsInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformPositionArguments(PEGTransformer &transformer, unique_ptr<ParsedExpression> single_expression,
	                           unique_ptr<ParsedExpression> single_expression_1);
	static unique_ptr<TransformResultValue> TransformRowExpressionInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformRowExpression(PEGTransformer &transformer, optional<vector<unique_ptr<ParsedExpression>>> expression);
	static unique_ptr<TransformResultValue> TransformSubstringExpressionInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformSubstringExpression(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> substring_arguments);
	static unique_ptr<TransformResultValue> TransformSubstringArgumentsInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSubstringExpressionListInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformSubstringExpressionList(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformSubstringParametersInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformSubstringParameters(PEGTransformer &transformer, unique_ptr<ParsedExpression> expression,
	                             vector<unique_ptr<ParsedExpression>> substring_from_for);
	static unique_ptr<TransformResultValue> TransformSubstringFromForInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSubstringFromOptionalForInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformSubstringFromOptionalFor(PEGTransformer &transformer, unique_ptr<ParsedExpression> from_expression,
	                                  optional<unique_ptr<ParsedExpression>> for_expression);
	static unique_ptr<TransformResultValue> TransformSubstringForInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformSubstringFor(PEGTransformer &transformer,
	                                                                  unique_ptr<ParsedExpression> for_expression);
	static unique_ptr<TransformResultValue> TransformTrimExpressionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformTrimExpression(PEGTransformer &transformer,
	                                                            TrimArguments trim_arguments);
	static unique_ptr<TransformResultValue> TransformTrimArgumentsInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static TrimArguments TransformTrimArguments(PEGTransformer &transformer, const optional<string> &trim_direction,
	                                            optional<unique_ptr<ParsedExpression>> trim_source,
	                                            vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformTrimDirectionInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTrimBothInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static string TransformTrimBoth(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTrimLeadingInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static string TransformTrimLeading(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTrimTrailingInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static string TransformTrimTrailing(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTrimSourceInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformTrimSource(PEGTransformer &transformer,
	                                                        optional<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformOverlayExpressionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression>
	TransformOverlayExpression(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> overlay_arguments);
	static unique_ptr<TransformResultValue> TransformOverlayArgumentsInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformOverlayParametersInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformOverlayParameters(PEGTransformer &transformer, unique_ptr<ParsedExpression> expression,
	                           unique_ptr<ParsedExpression> expression_1, unique_ptr<ParsedExpression> from_expression,
	                           optional<unique_ptr<ParsedExpression>> for_expression);
	static unique_ptr<TransformResultValue> TransformFromExpressionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformFromExpression(PEGTransformer &transformer,
	                                                            unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformForExpressionInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformForExpression(PEGTransformer &transformer,
	                                                           unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformOverlayExpressionListInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformOverlayExpressionList(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformExtractArgumentInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformExtractDatePartArgumentInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformExtractDatePartArgument(PEGTransformer &transformer,
	                                                                     const DatePartSpecifier &extract_date_part);
	static unique_ptr<TransformResultValue> TransformExtractIdentifierArgumentInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformExtractIdentifierArgument(PEGTransformer &transformer,
	                                                                       const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformExtractStringArgumentInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformExtractStringArgument(PEGTransformer &transformer,
	                                                                   const string &string_literal);
	static unique_ptr<TransformResultValue> TransformExtractDatePartInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformInsertStatementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<SQLStatement>
	TransformInsertStatement(PEGTransformer &transformer, optional<CommonTableExpressionMap> with_clause,
	                         const optional<OnConflictAction> &or_action, unique_ptr<BaseTableRef> insert_target,
	                         const optional<InsertColumnOrder> &by_name_or_position,
	                         const optional<vector<string>> &insert_column_list, InsertValues insert_values,
	                         optional<unique_ptr<OnConflictInfo>> on_conflict_clause,
	                         optional<vector<unique_ptr<ParsedExpression>>> returning_clause);
	static unique_ptr<TransformResultValue> TransformOrActionInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformInsertOrReplaceInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static OnConflictAction TransformInsertOrReplace(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformInsertOrIgnoreInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static OnConflictAction TransformInsertOrIgnore(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformByNameOrPositionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformInsertByNameOrderInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformInsertByPositionOrderInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformInsertByNameInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static InsertColumnOrder TransformInsertByName(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformInsertByPositionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static InsertColumnOrder TransformInsertByPosition(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformInsertTargetInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<BaseTableRef> TransformInsertTarget(PEGTransformer &transformer,
	                                                      unique_ptr<BaseTableRef> base_table_name,
	                                                      const optional<Identifier> &insert_alias);
	static unique_ptr<TransformResultValue> TransformInsertAliasInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static Identifier TransformInsertAlias(PEGTransformer &transformer, const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformColumnListInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static vector<string> TransformColumnList(PEGTransformer &transformer, const vector<Identifier> &col_id);
	static unique_ptr<TransformResultValue> TransformInsertColumnListInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static vector<string> TransformInsertColumnList(PEGTransformer &transformer, const vector<string> &column_list);
	static unique_ptr<TransformResultValue> TransformInsertValuesInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSelectInsertValuesInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static InsertValues TransformSelectInsertValues(PEGTransformer &transformer,
	                                                unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformDefaultValuesInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static InsertValues TransformDefaultValues(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformOnConflictClauseInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<OnConflictInfo> TransformOnConflictClause(PEGTransformer &transformer,
	                                                            optional<OnConflictExpressionTarget> on_conflict_target,
	                                                            unique_ptr<OnConflictInfo> on_conflict_action);
	static unique_ptr<TransformResultValue> TransformOnConflictTargetInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformOnConflictExpressionTargetInternal(PEGTransformer &transformer,
	                                                                                    ParseResult &parse_result);
	static OnConflictExpressionTarget
	TransformOnConflictExpressionTarget(PEGTransformer &transformer, const vector<string> &column_id_list,
	                                    optional<unique_ptr<ParsedExpression>> where_clause);
	static unique_ptr<TransformResultValue> TransformOnConflictIndexTargetInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static OnConflictExpressionTarget TransformOnConflictIndexTarget(PEGTransformer &transformer,
	                                                                 const Identifier &constraint_name);
	static unique_ptr<TransformResultValue> TransformOnConflictActionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformOnConflictUpdateInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<OnConflictInfo> TransformOnConflictUpdate(PEGTransformer &transformer,
	                                                            unique_ptr<UpdateSetInfo> update_set_clause,
	                                                            optional<unique_ptr<ParsedExpression>> where_clause);
	static unique_ptr<TransformResultValue> TransformOnConflictNothingInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<OnConflictInfo> TransformOnConflictNothing(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformReturningClauseInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformReturningClause(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> target_list);
	static unique_ptr<TransformResultValue> TransformLoadStatementInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformLoadStatement(PEGTransformer &transformer,
	                                                       const Identifier &col_id_or_string,
	                                                       const optional<Identifier> &extension_alias);
	static unique_ptr<TransformResultValue> TransformExtensionAliasInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static Identifier TransformExtensionAlias(PEGTransformer &transformer, const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformInstallStatementInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformInstallStatement(PEGTransformer &transformer, const bool &has_result,
	                                                          const QualifiedName &identifier_or_string_literal,
	                                                          const optional<ExtensionRepositoryInfo> &from_source,
	                                                          const optional<string> &version_number);
	static unique_ptr<TransformResultValue> TransformUpdateExtensionsStatementInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformUpdateExtensionsStatement(PEGTransformer &transformer,
	                                                                   const optional<vector<Identifier>> &identifier);
	static unique_ptr<TransformResultValue> TransformFromSourceInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformFromSourceIdentifierInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static ExtensionRepositoryInfo TransformFromSourceIdentifier(PEGTransformer &transformer,
	                                                             const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformFromSourceStringInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static ExtensionRepositoryInfo TransformFromSourceString(PEGTransformer &transformer, const string &string_literal);
	static unique_ptr<TransformResultValue> TransformVersionNumberInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static string TransformVersionNumber(PEGTransformer &transformer,
	                                     const QualifiedName &identifier_or_string_literal);
	static unique_ptr<TransformResultValue> TransformMergeIntoStatementInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<SQLStatement>
	TransformMergeIntoStatement(PEGTransformer &transformer, optional<CommonTableExpressionMap> with_clause,
	                            unique_ptr<BaseTableRef> target_opt_alias, unique_ptr<TableRef> merge_into_using_clause,
	                            JoinQualifier join_qualifier,
	                            vector<pair<MergeActionCondition, unique_ptr<MergeIntoAction>>> merge_match,
	                            optional<vector<unique_ptr<ParsedExpression>>> returning_clause);
	static unique_ptr<TransformResultValue> TransformMergeIntoUsingClauseInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<TableRef> TransformMergeIntoUsingClause(PEGTransformer &transformer,
	                                                          unique_ptr<TableRef> table_ref);
	static unique_ptr<TransformResultValue> TransformMergeMatchInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformMatchedClauseInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static pair<MergeActionCondition, unique_ptr<MergeIntoAction>>
	TransformMatchedClause(PEGTransformer &transformer, optional<unique_ptr<ParsedExpression>> and_expression,
	                       unique_ptr<MergeIntoAction> matched_clause_action);
	static unique_ptr<TransformResultValue> TransformMatchedClauseActionInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformUpdateMatchClauseInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<MergeIntoAction>
	TransformUpdateMatchClause(PEGTransformer &transformer, optional<unique_ptr<MergeIntoAction>> update_match_info);
	static unique_ptr<TransformResultValue> TransformUpdateMatchInfoInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformUpdateMatchSetActionInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<MergeIntoAction> TransformUpdateMatchSetAction(PEGTransformer &transformer,
	                                                                 unique_ptr<UpdateSetInfo> update_match_set_clause);
	static unique_ptr<TransformResultValue> TransformUpdateByNameOrPositionInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<MergeIntoAction> TransformUpdateByNameOrPosition(PEGTransformer &transformer,
	                                                                   const InsertColumnOrder &by_name_or_position);
	static unique_ptr<TransformResultValue> TransformDeleteMatchClauseInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<MergeIntoAction> TransformDeleteMatchClause(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformInsertMatchClauseInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<MergeIntoAction>
	TransformInsertMatchClause(PEGTransformer &transformer, optional<unique_ptr<MergeIntoAction>> insert_match_info);
	static unique_ptr<TransformResultValue> TransformInsertMatchInfoInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformInsertDefaultValuesInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<MergeIntoAction> TransformInsertDefaultValues(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformInsertByNameOrPositionInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<MergeIntoAction>
	TransformInsertByNameOrPosition(PEGTransformer &transformer, const optional<InsertColumnOrder> &by_name_or_position,
	                                const bool &has_result);
	static unique_ptr<TransformResultValue> TransformInsertValuesListInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<MergeIntoAction> TransformInsertValuesList(PEGTransformer &transformer,
	                                                             const optional<vector<string>> &insert_column_list,
	                                                             vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformDoNothingMatchClauseInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<MergeIntoAction> TransformDoNothingMatchClause(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformErrorMatchClauseInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<MergeIntoAction> TransformErrorMatchClause(PEGTransformer &transformer,
	                                                             optional<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformUpdateMatchSetClauseInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformUpdateMatchSetInfoInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformAndExpressionInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformAndExpression(PEGTransformer &transformer,
	                                                           unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformNotMatchedClauseInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static pair<MergeActionCondition, unique_ptr<MergeIntoAction>>
	TransformNotMatchedClause(PEGTransformer &transformer, const optional<MergeActionCondition> &by_source_or_target,
	                          optional<unique_ptr<ParsedExpression>> and_expression,
	                          unique_ptr<MergeIntoAction> matched_clause_action);
	static unique_ptr<TransformResultValue> TransformBySourceOrTargetInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformBySourceInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static MergeActionCondition TransformBySource(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformByTargetInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static MergeActionCondition TransformByTarget(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformPivotOnInternal(PEGTransformer &transformer,
	                                                                 ParseResult &parse_result);
	static vector<PivotColumn> TransformPivotOn(PEGTransformer &transformer, vector<PivotColumn> pivot_column_list);
	static unique_ptr<TransformResultValue> TransformPivotUsingInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformPivotUsing(PEGTransformer &transformer,
	                                                                vector<unique_ptr<ParsedExpression>> target_list);
	static unique_ptr<TransformResultValue> TransformPivotColumnListInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static vector<PivotColumn> TransformPivotColumnList(PEGTransformer &transformer,
	                                                    vector<PivotColumn> pivot_column_entry);
	static unique_ptr<TransformResultValue> TransformPivotColumnEntryInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformPivotColumnExpressionInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static PivotColumn TransformPivotColumnExpression(PEGTransformer &transformer,
	                                                  unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformPivotColumnSubqueryInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static PivotColumn TransformPivotColumnSubquery(PEGTransformer &transformer,
	                                                unique_ptr<ParsedExpression> base_expression,
	                                                unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformIntoNameValuesInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static UnpivotNameValues TransformIntoNameValues(PEGTransformer &transformer, const Identifier &col_id_or_string,
	                                                 const vector<Identifier> &identifier);
	static unique_ptr<TransformResultValue> TransformIncludeOrExcludeNullsInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformIncludeNullsInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static bool TransformIncludeNulls(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformExcludeNullsInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static bool TransformExcludeNulls(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformUnpivotHeaderInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformUnpivotHeaderSingleInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static vector<string> TransformUnpivotHeaderSingle(PEGTransformer &transformer, const Identifier &col_id_or_string);
	static unique_ptr<TransformResultValue> TransformUnpivotHeaderListInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static vector<string> TransformUnpivotHeaderList(PEGTransformer &transformer,
	                                                 const vector<Identifier> &col_id_or_string);
	static unique_ptr<TransformResultValue> TransformPragmaStatementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformPragmaStatement(PEGTransformer &transformer,
	                                                         unique_ptr<SQLStatement> pragma_assign_or_function);
	static unique_ptr<TransformResultValue> TransformPragmaAssignOrFunctionInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformPragmaAssignInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformPragmaAssign(PEGTransformer &transformer, const Identifier &setting_name,
	                                                      vector<unique_ptr<ParsedExpression>> variable_list);
	static unique_ptr<TransformResultValue> TransformPragmaFunctionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<SQLStatement>
	TransformPragmaFunction(PEGTransformer &transformer, const Identifier &pragma_name,
	                        optional<vector<unique_ptr<ParsedExpression>>> pragma_parameters);
	static unique_ptr<TransformResultValue> TransformPragmaParametersInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformPragmaParameters(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformPrepareStatementInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformPrepareStatement(PEGTransformer &transformer, const Identifier &identifier,
	                                                          const optional<vector<LogicalType>> &type_list,
	                                                          unique_ptr<SQLStatement> statement);
	static unique_ptr<TransformResultValue> TransformTypeListInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static vector<LogicalType> TransformTypeList(PEGTransformer &transformer, const vector<LogicalType> &type);
	static unique_ptr<TransformResultValue> TransformSelectStatementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformSelectStatement(PEGTransformer &transformer,
	                                                         unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformSelectSetOpChainInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SelectStatement> TransformSelectSetOpChain(
	    PEGTransformer &transformer, unique_ptr<SelectStatement> intersect_chain,
	    optional<vector<pair<unique_ptr<SetOperationNode>, unique_ptr<SelectStatement>>>> select_set_op_chain_tail);
	static unique_ptr<TransformResultValue> TransformSelectSetOpChainTailInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static pair<unique_ptr<SetOperationNode>, unique_ptr<SelectStatement>>
	TransformSelectSetOpChainTail(PEGTransformer &transformer, unique_ptr<SetOperationNode> setop_clause,
	                              unique_ptr<SelectStatement> intersect_chain);
	static unique_ptr<TransformResultValue> TransformIntersectChainInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<SelectStatement> TransformIntersectChain(
	    PEGTransformer &transformer, unique_ptr<SelectStatement> select_atom,
	    optional<vector<pair<unique_ptr<SetOperationNode>, unique_ptr<SelectStatement>>>> intersect_chain_tail);
	static unique_ptr<TransformResultValue> TransformIntersectChainTailInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static pair<unique_ptr<SetOperationNode>, unique_ptr<SelectStatement>>
	TransformIntersectChainTail(PEGTransformer &transformer, unique_ptr<SetOperationNode> set_intersect_clause,
	                            unique_ptr<SelectStatement> select_atom);
	static unique_ptr<TransformResultValue> TransformSetIntersectClauseInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<SetOperationNode> TransformSetIntersectClause(PEGTransformer &transformer,
	                                                                const optional<bool> &distinct_or_all);
	static unique_ptr<TransformResultValue> TransformSelectAtomInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSelectParensInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<SelectStatement> TransformSelectParens(PEGTransformer &transformer,
	                                                         unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformSetopClauseInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<SetOperationNode> TransformSetopClause(PEGTransformer &transformer,
	                                                         const SetOperationType &setop_type,
	                                                         const optional<bool> &distinct_or_all,
	                                                         const bool &has_result);
	static unique_ptr<TransformResultValue> TransformSetopTypeInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSetopUnionInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static SetOperationType TransformSetopUnion(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSetopExceptInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static SetOperationType TransformSetopExcept(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSelectStatementTypeInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformResultModifiersInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static vector<unique_ptr<ResultModifier>>
	TransformResultModifiers(PEGTransformer &transformer, optional<vector<OrderByNode>> order_by_clause,
	                         optional<unique_ptr<ResultModifier>> limit_offset);
	static unique_ptr<TransformResultValue> TransformLimitOffsetInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformLimitOffsetClauseInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ResultModifier> TransformLimitOffsetClause(PEGTransformer &transformer,
	                                                             LimitPercentResult limit_clause,
	                                                             optional<LimitPercentResult> offset_clause);
	static unique_ptr<TransformResultValue> TransformOffsetLimitClauseInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ResultModifier> TransformOffsetLimitClause(PEGTransformer &transformer,
	                                                             LimitPercentResult offset_clause,
	                                                             optional<LimitPercentResult> limit_clause);
	static unique_ptr<TransformResultValue> TransformTableStatementInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<SelectStatement> TransformTableStatement(PEGTransformer &transformer,
	                                                           unique_ptr<BaseTableRef> base_table_name);
	static unique_ptr<TransformResultValue> TransformOptionalParensSimpleSelectInternal(PEGTransformer &transformer,
	                                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSimpleSelectParensInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<SelectStatement> TransformSimpleSelectParens(PEGTransformer &transformer,
	                                                               unique_ptr<SelectStatement> simple_select);
	static unique_ptr<TransformResultValue> TransformSelectFromInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSelectFromClauseInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SelectNode> TransformSelectFromClause(PEGTransformer &transformer,
	                                                        unique_ptr<SelectNode> select_clause,
	                                                        optional<unique_ptr<TableRef>> from_clause);
	static unique_ptr<TransformResultValue> TransformFromSelectClauseInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SelectNode> TransformFromSelectClause(PEGTransformer &transformer,
	                                                        unique_ptr<TableRef> from_clause,
	                                                        optional<unique_ptr<SelectNode>> select_clause);
	static unique_ptr<TransformResultValue> TransformWithStatementInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static pair<Identifier, unique_ptr<CommonTableExpressionInfo>>
	TransformWithStatement(PEGTransformer &transformer, const Identifier &col_id_or_string,
	                       const optional<vector<string>> &insert_column_list,
	                       optional<vector<unique_ptr<ParsedExpression>>> using_key, const optional<bool> &materialized,
	                       unique_ptr<TableRef> cte_body);
	static unique_ptr<TransformResultValue> TransformCTEBodyInternal(PEGTransformer &transformer,
	                                                                 ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCTESelectBodyInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TableRef> TransformCTESelectBody(PEGTransformer &transformer,
	                                                   unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformCTEDMLBodyInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TableRef> TransformCTEDMLBody(PEGTransformer &transformer, unique_ptr<SQLStatement> statement);
	static unique_ptr<TransformResultValue> TransformUsingKeyInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformUsingKey(PEGTransformer &transformer,
	                                                              vector<unique_ptr<ParsedExpression>> target_list);
	static unique_ptr<TransformResultValue> TransformMaterializedInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static bool TransformMaterialized(PEGTransformer &transformer, const bool &has_result);
	static unique_ptr<TransformResultValue> TransformSelectClauseInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<SelectNode> TransformSelectClause(PEGTransformer &transformer,
	                                                    optional<DistinctClause> distinct_clause,
	                                                    optional<vector<unique_ptr<ParsedExpression>>> target_list);
	static unique_ptr<TransformResultValue> TransformTargetListInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformTargetList(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> aliased_expression);
	static unique_ptr<TransformResultValue> TransformColumnAliasesInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static vector<string> TransformColumnAliases(PEGTransformer &transformer,
	                                             const vector<Identifier> &col_id_or_string);
	static unique_ptr<TransformResultValue> TransformDistinctClauseInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformDistinctAllInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static DistinctClause TransformDistinctAll(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformDistinctOnInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static DistinctClause TransformDistinctOn(PEGTransformer &transformer,
	                                          optional<vector<unique_ptr<ParsedExpression>>> distinct_on_targets);
	static unique_ptr<TransformResultValue> TransformDistinctOnTargetsInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformDistinctOnTargets(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformInnerTableRefInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTableSubqueryInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TableRef> TransformTableSubquery(PEGTransformer &transformer, const optional<bool> &lateral,
	                                                   unique_ptr<TableRef> subquery_reference,
	                                                   const optional<TableAlias> &table_alias);
	static unique_ptr<TransformResultValue> TransformBaseTableRefInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TableRef>
	TransformBaseTableRef(PEGTransformer &transformer, const optional<Identifier> &table_alias_colon,
	                      unique_ptr<BaseTableRef> base_table_name, const optional<TableAlias> &table_alias,
	                      optional<unique_ptr<AtClause>> at_clause, optional<unique_ptr<SampleOptions>> sample_clause);
	static unique_ptr<TransformResultValue> TransformTableAliasColonInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static Identifier TransformTableAliasColon(PEGTransformer &transformer, const Identifier &col_id_or_string);
	static unique_ptr<TransformResultValue> TransformValuesRefInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<TableRef> TransformValuesRef(PEGTransformer &transformer,
	                                               unique_ptr<SelectStatement> values_clause,
	                                               const optional<TableAlias> &table_alias);
	static unique_ptr<TransformResultValue> TransformParensTableRefInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<TableRef> TransformParensTableRef(PEGTransformer &transformer,
	                                                    const optional<Identifier> &table_alias_colon,
	                                                    unique_ptr<TableRef> table_ref,
	                                                    const optional<TableAlias> &table_alias,
	                                                    optional<unique_ptr<SampleOptions>> sample_clause);
	static unique_ptr<TransformResultValue> TransformJoinOrPivotInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTablePivotClauseInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TableRef> TransformTablePivotClause(PEGTransformer &transformer,
	                                                      unique_ptr<TableRef> table_pivot_clause_body,
	                                                      const optional<TableAlias> &table_alias);
	static unique_ptr<TransformResultValue> TransformTablePivotClauseBodyInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<TableRef> TransformTablePivotClauseBody(PEGTransformer &transformer,
	                                                          vector<unique_ptr<ParsedExpression>> target_list,
	                                                          vector<PivotColumn> pivot_value_list,
	                                                          const optional<vector<string>> &pivot_group_by_list);
	static unique_ptr<TransformResultValue> TransformPivotGroupByListInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static vector<string> TransformPivotGroupByList(PEGTransformer &transformer,
	                                                const vector<Identifier> &col_id_or_string);
	static unique_ptr<TransformResultValue> TransformTableUnpivotClauseInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<TableRef> TransformTableUnpivotClause(PEGTransformer &transformer,
	                                                        const optional<bool> &include_or_exclude_nulls,
	                                                        unique_ptr<TableRef> table_unpivot_clause_body,
	                                                        const optional<TableAlias> &table_alias);
	static unique_ptr<TransformResultValue> TransformTableUnpivotClauseBodyInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static unique_ptr<TableRef> TransformTableUnpivotClauseBody(PEGTransformer &transformer,
	                                                            const vector<string> &unpivot_header,
	                                                            vector<PivotColumn> unpivot_value_list);
	static unique_ptr<TransformResultValue> TransformPivotHeaderInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformPivotHeader(PEGTransformer &transformer,
	                                                         unique_ptr<ParsedExpression> base_expression);
	static unique_ptr<TransformResultValue> TransformPivotValueListInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static PivotColumn TransformPivotValueList(PEGTransformer &transformer, unique_ptr<ParsedExpression> pivot_header,
	                                           PivotColumn pivot_value_target);
	static unique_ptr<TransformResultValue> TransformPivotValueTargetInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static PivotColumn TransformPivotValueTarget(PEGTransformer &transformer, ParseResult &choice_result);
	static PivotColumn TransformPivotValueTargetTrampoline(PEGTransformer &transformer, TransformStackFrame &frame);
	static unique_ptr<TransformResultValue> TransformUnpivotValueListInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static PivotColumn TransformUnpivotValueList(PEGTransformer &transformer, const vector<string> &unpivot_header,
	                                             vector<PivotColumnEntry> unpivot_target_list);
	static unique_ptr<TransformResultValue> TransformPivotTargetListInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static vector<PivotColumnEntry> TransformPivotTargetList(PEGTransformer &transformer,
	                                                         vector<unique_ptr<ParsedExpression>> target_list);
	static unique_ptr<TransformResultValue> TransformUnpivotTargetListInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static vector<PivotColumnEntry> TransformUnpivotTargetList(PEGTransformer &transformer,
	                                                           vector<unique_ptr<ParsedExpression>> target_list);
	static unique_ptr<TransformResultValue> TransformLateralInternal(PEGTransformer &transformer,
	                                                                 ParseResult &parse_result);
	static bool TransformLateral(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformBaseTableNameInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformUnqualifiedBaseTableNameInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static unique_ptr<BaseTableRef> TransformUnqualifiedBaseTableName(PEGTransformer &transformer,
	                                                                  const Identifier &table_name);
	static unique_ptr<TransformResultValue> TransformSchemaReservedTableInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<BaseTableRef> TransformSchemaReservedTable(PEGTransformer &transformer,
	                                                             const Identifier &schema_qualification,
	                                                             const Identifier &reserved_table_name);
	static unique_ptr<TransformResultValue> TransformCatalogReservedSchemaTableInternal(PEGTransformer &transformer,
	                                                                                    ParseResult &parse_result);
	static unique_ptr<BaseTableRef> TransformCatalogReservedSchemaTable(PEGTransformer &transformer,
	                                                                    const Identifier &catalog_qualification,
	                                                                    const Identifier &reserved_schema_qualification,
	                                                                    const Identifier &reserved_table_name);
	static unique_ptr<TransformResultValue> TransformTableFunctionInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTableFunctionLateralOptInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<TableRef> TransformTableFunctionLateralOpt(PEGTransformer &transformer,
	                                                             const optional<bool> &lateral,
	                                                             const QualifiedName &qualified_table_function,
	                                                             vector<FunctionArgument> table_function_arguments,
	                                                             const optional<bool> &with_ordinality,
	                                                             const optional<TableAlias> &table_alias);
	static unique_ptr<TransformResultValue> TransformTableFunctionAliasColonInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<TableRef> TransformTableFunctionAliasColon(PEGTransformer &transformer,
	                                                             const Identifier &table_alias_colon,
	                                                             const QualifiedName &qualified_table_function,
	                                                             vector<FunctionArgument> table_function_arguments,
	                                                             const optional<bool> &with_ordinality,
	                                                             optional<unique_ptr<SampleOptions>> sample_clause);
	static unique_ptr<TransformResultValue> TransformWithOrdinalityInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static bool TransformWithOrdinality(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformQualifiedTableFunctionInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static QualifiedName TransformQualifiedTableFunction(PEGTransformer &transformer,
	                                                     const optional<Identifier> &catalog_qualification,
	                                                     const optional<Identifier> &schema_qualification,
	                                                     const Identifier &table_function_name);
	static unique_ptr<TransformResultValue> TransformTableFunctionArgumentsInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static vector<FunctionArgument>
	TransformTableFunctionArguments(PEGTransformer &transformer, optional<vector<FunctionArgument>> function_argument);
	static unique_ptr<TransformResultValue> TransformFunctionArgumentInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformNamedFunctionArgumentInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static FunctionArgument TransformNamedFunctionArgument(PEGTransformer &transformer, MacroParameter named_parameter);
	static unique_ptr<TransformResultValue> TransformPositionalFunctionArgumentInternal(PEGTransformer &transformer,
	                                                                                    ParseResult &parse_result);
	static FunctionArgument TransformPositionalFunctionArgument(PEGTransformer &transformer,
	                                                            unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformNamedParameterInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static MacroParameter TransformNamedParameter(PEGTransformer &transformer, const Identifier &type_func_name,
	                                              const optional<LogicalType> &type,
	                                              unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformTableAliasInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformTableAliasAsInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static TableAlias TransformTableAliasAs(PEGTransformer &transformer,
	                                        const QualifiedName &identifier_or_string_literal,
	                                        const optional<vector<string>> &column_aliases);
	static unique_ptr<TransformResultValue> TransformTableAliasWithoutAsInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static TableAlias TransformTableAliasWithoutAs(PEGTransformer &transformer, const Identifier &identifier,
	                                               const optional<vector<string>> &column_aliases);
	static unique_ptr<TransformResultValue> TransformAtClauseInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static unique_ptr<AtClause> TransformAtClause(PEGTransformer &transformer, unique_ptr<AtClause> at_specifier);
	static unique_ptr<TransformResultValue> TransformAtSpecifierInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<AtClause> TransformAtSpecifier(PEGTransformer &transformer, const string &at_unit,
	                                                 unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformAtUnitInternal(PEGTransformer &transformer,
	                                                                ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformVersionAtUnitInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static string TransformVersionAtUnit(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformTimestampAtUnitInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static string TransformTimestampAtUnit(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformJoinClauseInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformRegularJoinClauseInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TableRef> TransformRegularJoinClause(PEGTransformer &transformer, const optional<bool> &asof,
	                                                       const optional<JoinType> &join_type,
	                                                       unique_ptr<TableRef> table_ref,
	                                                       JoinQualifier join_qualifier);
	static unique_ptr<TransformResultValue> TransformAsofInternal(PEGTransformer &transformer,
	                                                              ParseResult &parse_result);
	static bool TransformAsof(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformJoinWithoutOnClauseInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<TableRef> TransformJoinWithoutOnClause(PEGTransformer &transformer, const JoinPrefix &join_prefix,
	                                                         unique_ptr<TableRef> table_ref);
	static unique_ptr<TransformResultValue> TransformJoinQualifierInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformOnClauseInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static JoinQualifier TransformOnClause(PEGTransformer &transformer, unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformUsingClauseInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static JoinQualifier TransformUsingClause(PEGTransformer &transformer, const vector<Identifier> &column_name);
	static unique_ptr<TransformResultValue> TransformJoinTypeInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformJoinPrefixInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCrossJoinPrefixInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static JoinPrefix TransformCrossJoinPrefix(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNaturalJoinPrefixInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static JoinPrefix TransformNaturalJoinPrefix(PEGTransformer &transformer, const optional<JoinType> &join_type);
	static unique_ptr<TransformResultValue> TransformPositionalJoinPrefixInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static JoinPrefix TransformPositionalJoinPrefix(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformFullJoinInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static JoinType TransformFullJoin(PEGTransformer &transformer, const bool &has_result);
	static unique_ptr<TransformResultValue> TransformLeftJoinInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static JoinType TransformLeftJoin(PEGTransformer &transformer, const bool &has_result);
	static unique_ptr<TransformResultValue> TransformRightJoinInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static JoinType TransformRightJoin(PEGTransformer &transformer, const bool &has_result);
	static unique_ptr<TransformResultValue> TransformSemiJoinInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static JoinType TransformSemiJoin(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformAntiJoinInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static JoinType TransformAntiJoin(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformInnerJoinInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static JoinType TransformInnerJoin(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformFromClauseInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TableRef> TransformFromClause(PEGTransformer &transformer,
	                                                vector<unique_ptr<TableRef>> table_ref);
	static unique_ptr<TransformResultValue> TransformWhereClauseInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformWhereClause(PEGTransformer &transformer,
	                                                         unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformGroupByClauseInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static GroupByNode TransformGroupByClause(PEGTransformer &transformer, GroupByNode group_by_expressions);
	static unique_ptr<TransformResultValue> TransformHavingClauseInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformHavingClause(PEGTransformer &transformer,
	                                                          unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformQualifyClauseInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformQualifyClause(PEGTransformer &transformer,
	                                                           unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformSampleClauseInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<SampleOptions> TransformSampleClause(PEGTransformer &transformer,
	                                                       unique_ptr<SampleOptions> sample_entry);
	static unique_ptr<TransformResultValue> TransformWindowClauseInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformWindowClause(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> window_definition);
	static unique_ptr<TransformResultValue> TransformSampleEntryInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSampleEntryCountInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SampleOptions>
	TransformSampleEntryCount(PEGTransformer &transformer, unique_ptr<SampleOptions> sample_count,
	                          const optional<pair<SampleMethod, optional_idx>> &sample_properties);
	static unique_ptr<TransformResultValue> TransformSampleEntryFunctionInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<SampleOptions> TransformSampleEntryFunction(PEGTransformer &transformer,
	                                                              const optional<SampleMethod> &sample_function,
	                                                              unique_ptr<SampleOptions> sample_count,
	                                                              const optional<optional_idx> &repeatable_sample);
	static unique_ptr<TransformResultValue> TransformSampleFunctionInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static SampleMethod TransformSampleFunction(PEGTransformer &transformer, const Identifier &col_id);
	static unique_ptr<TransformResultValue> TransformSamplePropertiesInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static pair<SampleMethod, optional_idx> TransformSampleProperties(PEGTransformer &transformer,
	                                                                  const Identifier &col_id,
	                                                                  const optional<optional_idx> &sample_seed);
	static unique_ptr<TransformResultValue> TransformRepeatableSampleInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static optional_idx TransformRepeatableSample(PEGTransformer &transformer, const optional_idx &sample_seed);
	static unique_ptr<TransformResultValue> TransformSampleSeedInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static optional_idx TransformSampleSeed(PEGTransformer &transformer, unique_ptr<ParsedExpression> number_literal);
	static unique_ptr<TransformResultValue> TransformSampleCountInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<SampleOptions> TransformSampleCount(PEGTransformer &transformer,
	                                                      unique_ptr<ParsedExpression> sample_value,
	                                                      const optional<bool> &sample_unit);
	static unique_ptr<TransformResultValue> TransformSampleValueInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSampleUnitInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSamplePercentageInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static bool TransformSamplePercentage(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSampleRowsInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static bool TransformSampleRows(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformGroupByExpressionsInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformGroupByAllInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static GroupByNode TransformGroupByAll(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformGroupByListInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static GroupByNode TransformGroupByList(PEGTransformer &transformer,
	                                        vector<GroupByExpressionInfo> group_by_expression);
	static unique_ptr<TransformResultValue> TransformGroupByExpressionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformGroupByBaseExpressionInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static GroupByExpressionInfo TransformGroupByBaseExpression(PEGTransformer &transformer,
	                                                            unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformEmptyGroupingItemInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static GroupByExpressionInfo TransformEmptyGroupingItem(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformCubeOrRollupClauseInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static GroupByExpressionInfo TransformCubeOrRollupClause(PEGTransformer &transformer, const string &cube_or_rollup,
	                                                         optional<vector<unique_ptr<ParsedExpression>>> expression);
	static unique_ptr<TransformResultValue> TransformCubeOrRollupInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformCubeKeywordInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static string TransformCubeKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformRollupKeywordInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static string TransformRollupKeyword(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformGroupingSetsClauseInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static GroupByExpressionInfo TransformGroupingSetsClause(PEGTransformer &transformer,
	                                                         vector<GroupByExpressionInfo> group_by_expression);
	static unique_ptr<TransformResultValue> TransformSubqueryReferenceInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TableRef> TransformSubqueryReference(PEGTransformer &transformer,
	                                                       unique_ptr<SelectStatement> select_statement_internal);
	static unique_ptr<TransformResultValue> TransformOrderByExpressionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static OrderByNode TransformOrderByExpression(PEGTransformer &transformer, unique_ptr<ParsedExpression> expression,
	                                              const optional<OrderType> &desc_or_asc,
	                                              const optional<OrderByNullType> &nulls_first_or_last);
	static unique_ptr<TransformResultValue> TransformDescOrAscInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformDescendingOrderInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static OrderType TransformDescendingOrder(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformAscendingOrderInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static OrderType TransformAscendingOrder(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNullsFirstOrLastInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformNullsFirstInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static OrderByNullType TransformNullsFirst(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNullsLastInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static OrderByNullType TransformNullsLast(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformOrderByClauseInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static vector<OrderByNode> TransformOrderByClause(PEGTransformer &transformer,
	                                                  vector<OrderByNode> order_by_expressions);
	static unique_ptr<TransformResultValue> TransformOrderByExpressionsInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformOrderByExpressionListInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static vector<OrderByNode> TransformOrderByExpressionList(PEGTransformer &transformer,
	                                                          vector<OrderByNode> order_by_expression);
	static unique_ptr<TransformResultValue> TransformOrderByAllInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static vector<OrderByNode> TransformOrderByAll(PEGTransformer &transformer, const optional<OrderType> &desc_or_asc,
	                                               const optional<OrderByNullType> &nulls_first_or_last);
	static unique_ptr<TransformResultValue> TransformLimitClauseInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static LimitPercentResult TransformLimitClause(PEGTransformer &transformer, LimitPercentResult limit_value);
	static unique_ptr<TransformResultValue> TransformOffsetClauseInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static LimitPercentResult TransformOffsetClause(PEGTransformer &transformer, LimitPercentResult offset_value);
	static unique_ptr<TransformResultValue> TransformOffsetValueInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static LimitPercentResult TransformOffsetValue(PEGTransformer &transformer, unique_ptr<ParsedExpression> expression,
	                                               const bool &has_result);
	static unique_ptr<TransformResultValue> TransformLimitValueInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformLimitAllInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static LimitPercentResult TransformLimitAll(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformLimitLiteralPercentInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static LimitPercentResult TransformLimitLiteralPercent(PEGTransformer &transformer,
	                                                       unique_ptr<ParsedExpression> number_literal);
	static unique_ptr<TransformResultValue> TransformLimitExpressionInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static LimitPercentResult TransformLimitExpression(PEGTransformer &transformer,
	                                                   unique_ptr<ParsedExpression> expression, const bool &has_result);
	static unique_ptr<TransformResultValue> TransformAliasedExpressionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformColIdExpressionInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformColIdExpression(PEGTransformer &transformer, const Identifier &col_id,
	                                                             unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformExpressionAsCollabelInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformExpressionAsCollabel(PEGTransformer &transformer,
	                                                                  unique_ptr<ParsedExpression> expression,
	                                                                  const Identifier &col_label_or_string);
	static unique_ptr<TransformResultValue> TransformExpressionOptIdentifierInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformExpressionOptIdentifier(PEGTransformer &transformer,
	                                                                     unique_ptr<ParsedExpression> expression,
	                                                                     const optional<Identifier> &identifier);
	static unique_ptr<TransformResultValue> TransformValuesClauseInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<SelectStatement>
	TransformValuesClause(PEGTransformer &transformer, vector<vector<unique_ptr<ParsedExpression>>> values_expressions);
	static unique_ptr<TransformResultValue> TransformValuesExpressionsInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformValuesExpressions(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformSetStatementInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformSetStatement(PEGTransformer &transformer,
	                                                      unique_ptr<SetStatement> set_assignment_or_time_zone);
	static unique_ptr<TransformResultValue> TransformSetAssignmentOrTimeZoneInternal(PEGTransformer &transformer,
	                                                                                 ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformResetStatementInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformResetStatement(PEGTransformer &transformer,
	                                                        const SettingInfo &set_variable_or_setting);
	static unique_ptr<TransformResultValue> TransformStandardAssignmentInternal(PEGTransformer &transformer,
	                                                                            ParseResult &parse_result);
	static unique_ptr<SetStatement> TransformStandardAssignment(PEGTransformer &transformer,
	                                                            const SettingInfo &set_variable_or_setting,
	                                                            vector<unique_ptr<ParsedExpression>> set_assignment);
	static unique_ptr<TransformResultValue> TransformSetVariableOrSettingInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSetTimeZoneInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<SetStatement> TransformSetTimeZone(PEGTransformer &transformer,
	                                                     unique_ptr<ParsedExpression> zone_value);
	static unique_ptr<TransformResultValue> TransformZoneValueInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformZoneLocalInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformZoneLocal(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformZoneDefaultInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformZoneDefault(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformZoneStringLiteralInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformZoneStringLiteral(PEGTransformer &transformer,
	                                                               const string &string_literal);
	static unique_ptr<TransformResultValue> TransformZoneIdentifierInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformZoneIdentifier(PEGTransformer &transformer,
	                                                            const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformZoneIntervalWithIntervalInternal(PEGTransformer &transformer,
	                                                                                  ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformZoneIntervalWithInterval(PEGTransformer &transformer,
	                                                                      const string &string_literal,
	                                                                      const optional<DatePartSpecifier> &interval);
	static unique_ptr<TransformResultValue> TransformZoneIntervalWithPrecisionInternal(PEGTransformer &transformer,
	                                                                                   ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformZoneIntervalWithPrecision(PEGTransformer &transformer,
	                                                                       unique_ptr<ParsedExpression> number_literal,
	                                                                       const string &string_literal);
	static unique_ptr<TransformResultValue> TransformSetSettingInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static SettingInfo TransformSetSetting(PEGTransformer &transformer, const optional<SetScope> &setting_scope,
	                                       const Identifier &setting_name);
	static unique_ptr<TransformResultValue> TransformSetVariableInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static SettingInfo TransformSetVariable(PEGTransformer &transformer, const SetScope &variable_scope,
	                                        const Identifier &identifier);
	static unique_ptr<TransformResultValue> TransformVariableScopeInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static SetScope TransformVariableScope(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSettingScopeInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformLocalScopeInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static SetScope TransformLocalScope(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSessionScopeInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static SetScope TransformSessionScope(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformGlobalScopeInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static SetScope TransformGlobalScope(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformSetAssignmentInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>>
	TransformSetAssignment(PEGTransformer &transformer, vector<unique_ptr<ParsedExpression>> variable_list);
	static unique_ptr<TransformResultValue> TransformVariableListInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformVariableList(PEGTransformer &transformer,
	                                                                  vector<unique_ptr<ParsedExpression>> expression);
	static unique_ptr<TransformResultValue> TransformTransactionStatementInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformBeginTransactionInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformBeginTransaction(PEGTransformer &transformer, const bool &has_result,
	                                                          const optional<TransactionModifierType> &read_or_write);
	static unique_ptr<TransformResultValue> TransformRollbackTransactionInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformRollbackTransaction(PEGTransformer &transformer, const bool &has_result);
	static unique_ptr<TransformResultValue> TransformCommitTransactionInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformCommitTransaction(PEGTransformer &transformer, const bool &has_result);
	static unique_ptr<TransformResultValue> TransformReadOrWriteInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static TransactionModifierType TransformReadOrWrite(PEGTransformer &transformer,
	                                                    const TransactionModifierType &read_only_or_read_write);
	static unique_ptr<TransformResultValue> TransformReadOnlyOrReadWriteInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformReadOnlyInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static TransactionModifierType TransformReadOnly(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformReadWriteInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static TransactionModifierType TransformReadWrite(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformUpdateStatementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<SQLStatement>
	TransformUpdateStatement(PEGTransformer &transformer, optional<CommonTableExpressionMap> with_clause,
	                         unique_ptr<TableRef> update_target, unique_ptr<UpdateSetInfo> update_set_clause,
	                         optional<unique_ptr<TableRef>> from_clause,
	                         optional<unique_ptr<ParsedExpression>> where_clause,
	                         optional<vector<unique_ptr<ParsedExpression>>> returning_clause);
	static unique_ptr<TransformResultValue> TransformUpdateTargetInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformBaseTableSetInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TableRef> TransformBaseTableSet(PEGTransformer &transformer,
	                                                  unique_ptr<BaseTableRef> base_table_name);
	static unique_ptr<TransformResultValue> TransformBaseTableAliasSetInternal(PEGTransformer &transformer,
	                                                                           ParseResult &parse_result);
	static unique_ptr<TableRef> TransformBaseTableAliasSet(PEGTransformer &transformer,
	                                                       unique_ptr<BaseTableRef> base_table_name,
	                                                       const optional<Identifier> &update_alias);
	static unique_ptr<TransformResultValue> TransformUpdateAliasInternal(PEGTransformer &transformer,
	                                                                     ParseResult &parse_result);
	static Identifier TransformUpdateAlias(PEGTransformer &transformer, const bool &has_result,
	                                       const Identifier &col_id);
	static unique_ptr<TransformResultValue> TransformUpdateSetClauseInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformUpdateSetTupleInternal(PEGTransformer &transformer,
	                                                                        ParseResult &parse_result);
	static unique_ptr<UpdateSetInfo> TransformUpdateSetTuple(PEGTransformer &transformer,
	                                                         const vector<Identifier> &column_name,
	                                                         unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformUpdateSetElementListInternal(PEGTransformer &transformer,
	                                                                              ParseResult &parse_result);
	static unique_ptr<UpdateSetInfo>
	TransformUpdateSetElementList(PEGTransformer &transformer,
	                              vector<pair<string, unique_ptr<ParsedExpression>>> update_set_element);
	static unique_ptr<TransformResultValue> TransformUpdateSetElementInternal(PEGTransformer &transformer,
	                                                                          ParseResult &parse_result);
	static pair<string, unique_ptr<ParsedExpression>>
	TransformUpdateSetElement(PEGTransformer &transformer, const string &update_set_column_target,
	                          unique_ptr<ParsedExpression> expression);
	static unique_ptr<TransformResultValue> TransformUpdateSetColumnTargetInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static string TransformUpdateSetColumnTarget(PEGTransformer &transformer, const Identifier &column_name,
	                                             const optional<vector<Identifier>> &dot_identifier);
	static unique_ptr<TransformResultValue> TransformUseStatementInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformUseStatement(PEGTransformer &transformer, const QualifiedName &use_target);
	static unique_ptr<TransformResultValue> TransformUseTargetInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformSchemaNameAsUseTargetInternal(PEGTransformer &transformer,
	                                                                               ParseResult &parse_result);
	static QualifiedName TransformSchemaNameAsUseTarget(PEGTransformer &transformer, const Identifier &schema_name);
	static unique_ptr<TransformResultValue> TransformCatalogNameAsUseTargetInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static QualifiedName TransformCatalogNameAsUseTarget(PEGTransformer &transformer, const Identifier &catalog_name);
	static unique_ptr<TransformResultValue> TransformUseTargetCatalogSchemaInternal(PEGTransformer &transformer,
	                                                                                ParseResult &parse_result);
	static QualifiedName TransformUseTargetCatalogSchema(PEGTransformer &transformer, const Identifier &catalog_name,
	                                                     const Identifier &reserved_schema_name,
	                                                     const optional<vector<Identifier>> &dot_identifier);
	static unique_ptr<TransformResultValue> TransformDotIdentifierInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static Identifier TransformDotIdentifier(PEGTransformer &transformer, const Identifier &identifier);

	static unique_ptr<TransformResultValue> TransformVacuumStatementInternal(PEGTransformer &transformer,
	                                                                         ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformVacuumStatement(PEGTransformer &transformer,
	                                                         const optional<VacuumOptions> &vacuum_options,
	                                                         optional<AnalyzeTarget> analyze_target);
	static unique_ptr<TransformResultValue> TransformVacuumOptionsInternal(PEGTransformer &transformer,
	                                                                       ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformVacuumParensOptionsInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static VacuumOptions TransformVacuumParensOptions(PEGTransformer &transformer, const vector<string> &vacuum_option);
	static unique_ptr<TransformResultValue> TransformVacuumLegacyOptionsInternal(PEGTransformer &transformer,
	                                                                             ParseResult &parse_result);
	static VacuumOptions TransformVacuumLegacyOptions(PEGTransformer &transformer, const optional<string> &opt_full,
	                                                  const optional<string> &opt_freeze,
	                                                  const optional<string> &opt_verbose,
	                                                  const optional<string> &opt_analyze);
	static unique_ptr<TransformResultValue> TransformVacuumOptionInternal(PEGTransformer &transformer,
	                                                                      ParseResult &parse_result);
	static unique_ptr<TransformResultValue> TransformOptAnalyzeInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static string TransformOptAnalyze(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformOptFullInternal(PEGTransformer &transformer,
	                                                                 ParseResult &parse_result);
	static string TransformOptFull(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformOptFreezeInternal(PEGTransformer &transformer,
	                                                                   ParseResult &parse_result);
	static string TransformOptFreeze(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformOptVerboseInternal(PEGTransformer &transformer,
	                                                                    ParseResult &parse_result);
	static string TransformOptVerbose(PEGTransformer &transformer);
	static unique_ptr<TransformResultValue> TransformNameListInternal(PEGTransformer &transformer,
	                                                                  ParseResult &parse_result);
	static vector<string> TransformNameList(PEGTransformer &transformer, const vector<Identifier> &col_id);
	//===--------------------------------------------------------------------===//
	// END GENERATED RULES
	//===--------------------------------------------------------------------===//

private:
	PEGParser parser;
	case_insensitive_map_t<PEGTransformer::AnyTransformFunction> sql_transform_functions;
	case_insensitive_map_t<PEGTransformer::AnyTransformFunction> trampoline_transform_functions;
	case_insensitive_map_t<unique_ptr<TransformEnumValue>> enum_mappings;
};

} // namespace duckdb
