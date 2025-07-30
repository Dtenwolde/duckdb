#pragma once

#include "tokenizer.hpp"
#include "parse_result.hpp"
#include "transform_enum_result.hpp"
#include "transform_result.hpp"
#include "ast/generic_copy_option.hpp"
#include "ast/set_info.hpp"
#include "duckdb/parser/statement/set_statement.hpp"
#include "parser/peg_parser.hpp"
#include "duckdb/storage/arena_allocator.hpp"
#include <functional>

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
	using AnyTransformFunction = std::function<unique_ptr<TransformResultValue>(PEGTransformer &, optional_ptr<ParseResult>)>;

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
		enum_mappings[rule_name] = make_uniq<TypedTransformEnumResult<T>>(value);
	}

	template <class FUNC>
	void Register(const string &rule_name, FUNC function) {
		sql_transform_functions[rule_name] = [function](PEGTransformer &transformer,
		                                                optional_ptr<ParseResult> parse_result) -> unique_ptr<TransformResultValue> {
			auto result_value = function(transformer, parse_result);
			return make_uniq<TypedTransformResult<decltype(result_value)>>(std::move(result_value));
		};
	}

	// Register for functions taking one ParseResult child
	template <class RETURN_TYPE, class A, idx_t INDEX_A, class FUNC>
	void Register(const string &rule_name, FUNC function) {
		sql_transform_functions[rule_name] = [function](PEGTransformer &transformer,
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
		sql_transform_functions[rule_name] = [function](PEGTransformer &transformer,
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
	static unique_ptr<SQLStatement> TransformDeleteStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformPragmaStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformPragmaAssign(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformPragmaFunction(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformPragmaParameters(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformDetachStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformAttachStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformAttachAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unordered_map<string, Value> TransformAttachOptions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unordered_map<string, Value> TransformGenericCopyOptionList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static GenericCopyOption TransformGenericCopyOption(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformCheckpointStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	static unique_ptr<SQLStatement> TransformExportStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<SQLStatement> TransformImportStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformExportSource(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

	// Intermediate transforms returning semantic values
	static unique_ptr<SQLStatement> TransformStandardAssignment(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static SettingInfo TransformSettingOrVariable(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static SettingInfo TransformSetSetting(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static SettingInfo TransformSetVariable(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformSetAssignment(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static vector<unique_ptr<ParsedExpression>> TransformVariableList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformBaseExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformSingleExpression(PEGTransformer &transformer,
	                                                              optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformLiteralExpression(PEGTransformer &transformer,
	                                                               optional_ptr<ParseResult> parse_result);
	static unique_ptr<ParsedExpression> TransformColumnReference(PEGTransformer &transformer,
	                                                             optional_ptr<ParseResult> parse_result);
	static string TransformOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);
	static string TransformIdentifierOrKeyword(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result);

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
