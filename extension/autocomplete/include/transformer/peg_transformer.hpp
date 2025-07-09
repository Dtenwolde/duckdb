#pragma once

#include "tokenizer.hpp"
#include "parse_result.hpp"
#include "transform_enum_result.hpp"
#include "transform_result.hpp"
#include "ast/set_info.hpp"
#include "duckdb/parser/statement/set_statement.hpp"
#include "parser/peg_parser.hpp"
#include "duckdb/storage/arena_allocator.hpp"
#include <functional>

namespace duckdb {

// Forward declare
struct QualifiedName;

struct PEGTransformerState {
    explicit PEGTransformerState(const vector<MatcherToken> &tokens_p)
       : tokens(tokens_p), token_index(0) {
    }

    const vector<MatcherToken> &tokens;
    idx_t token_index;
};

class PEGTransformer {
public:
	// The core dispatch function type. It now returns a unique_ptr to solve the ownership issue.
	using AnyTransformFunction = std::function<unique_ptr<TransformResultValue>(PEGTransformer &, ParseResult &)>;

	PEGTransformer(ArenaAllocator &allocator, PEGTransformerState &state,
				   const case_insensitive_map_t<AnyTransformFunction> &transform_functions,
				   const case_insensitive_map_t<PEGRule> &grammar_rules,
				   const case_insensitive_map_t<case_insensitive_map_t<unique_ptr<TransformEnumValue>>> &enum_mappings)
		: allocator(allocator), state(state), grammar_rules(grammar_rules), transform_functions(transform_functions),
		  enum_mappings(enum_mappings) {
	}

public:

	template<typename T>
	T Transform(ParseResult &parse_result);

	template <typename T>
	T TransformEnum(ParseResult &parse_result);

	ParseResult *MatchRule(const string_t &rule_name);
	ParseResult *MatchRule(const PEGExpression &expression);

	// Make overloads return raw pointers, as ownership is handled by the ArenaAllocator.
	template <class T, typename... Args>
	T *Make(Args &&...args) {
		return allocator.Make<T>(std::forward<Args>(args)...);
	}

private:
	const PEGExpression *FindSubstitution(const string_t &name);

public:
	ArenaAllocator &allocator;
	PEGTransformerState &state;
	// The order of declaration here MATTERS for the constructor initializer list.
	const case_insensitive_map_t<PEGRule> &grammar_rules;
	const case_insensitive_map_t<AnyTransformFunction> &transform_functions;
	const case_insensitive_map_t<case_insensitive_map_t<unique_ptr<TransformEnumValue>>> &enum_mappings;
	vector<string_map_t<const PEGExpression *>> substitution_stack;
};
// PEGTransformerFactory is now a real class that holds the transformation state.
class PEGTransformerFactory {
public:
    // The factory is constructed with a specific grammar.
    explicit PEGTransformerFactory(const char *grammar);

    // Transforms a sequence of tokens into a SQL statement.
    unique_ptr<SQLStatement> Transform(vector<MatcherToken> &tokens, const char* root_rule = "Statement");

private:
	template <typename T>
	void RegisterEnum(const string &rule_name, const unordered_map<string, T> &mapping) {
		for (const auto &pair : mapping) {
			enum_mappings[rule_name][pair.first] = make_uniq<TypedTransformEnumResult<T>>(pair.second);
		}
	}

	template <class FUNC>
	void Register(const string &rule_name, FUNC function) {
		sql_transform_functions[rule_name] =
			[function](PEGTransformer &transformer, ParseResult &parse_result) -> unique_ptr<TransformResultValue> {
				auto result_value = function(transformer, parse_result);
				return make_uniq<TypedTransformResult<decltype(result_value)>>(std::move(result_value));
		};
	}

    // Register for functions taking one ParseResult child
    template <class RETURN_TYPE, class A, idx_t INDEX_A, class FUNC>
    void Register(const string &rule_name, FUNC function) {
        sql_transform_functions[rule_name] = [function](PEGTransformer& transformer, ParseResult &parse_result) -> unique_ptr<TransformResultValue> {
            auto &list_pr = parse_result.Cast<ListParseResult>();
            auto &child_a = list_pr.Child<A>(INDEX_A);
        	RETURN_TYPE result_value = function(transformer, child_a);
            return make_uniq<TypedTransformResult<RETURN_TYPE>>(std::move(result_value));
        };
    }

    // Register for functions taking two ParseResult children
    template <class RETURN_TYPE, class A, idx_t INDEX_A, class B, idx_t INDEX_B, class FUNC>
    void Register(const string &rule_name, FUNC function) {
        sql_transform_functions[rule_name] = [function](PEGTransformer& transformer, ParseResult &parse_result) -> unique_ptr<TransformResultValue> {
            auto &list_pr = parse_result.Cast<ListParseResult>();
            auto &child_a = list_pr.Child<A>(INDEX_A);
            auto &child_b = list_pr.Child<B>(INDEX_B);
        	RETURN_TYPE result_value = function(transformer, child_a, child_b);
            return make_uniq<TypedTransformResult<RETURN_TYPE>>(std::move(result_value));
        };
    }
    // ... Add more overloads for 3, 4, etc., arguments as needed.

	static unique_ptr<SQLStatement> TransformStatement(PEGTransformer&, ParseResult &list);

    static unique_ptr<SQLStatement> TransformUseStatement(PEGTransformer&, ParseResult &identifier_pr);
	static unique_ptr<SQLStatement> TransformSetStatement(PEGTransformer&, ParseResult &choice_pr);
	static unique_ptr<SQLStatement> TransformResetStatement(PEGTransformer &, ParseResult &choice_pr);
	static QualifiedName TransformDottedIdentifier(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<SQLStatement> TransformDeleteStatement(PEGTransformer &transformer, ParseResult &parse_result);

	// Intermediate transforms returning semantic values
	static unique_ptr<SQLStatement> TransformStandardAssignment(PEGTransformer &transformer, ParseResult &parse_result);
	static SettingInfo TransformSettingOrVariable(PEGTransformer &transformer, ParseResult &parse_result);
	static SettingInfo TransformSetSetting(PEGTransformer &transformer, ParseResult &parse_result);
	static SettingInfo TransformSetVariable(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformSetAssignment(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformVariableList(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformExpression(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformBaseExpression(PEGTransformer &, ParseResult& parse_result);
	static unique_ptr<ParsedExpression> TransformSingleExpression(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformLiteralExpression(PEGTransformer &transformer, ParseResult &parse_result);
	static unique_ptr<ParsedExpression> TransformColumnReference(PEGTransformer &transformer, ParseResult &parse_result);
	static string TransformOperator(PEGTransformer &transformer, ParseResult &parse_result);
	static string TransformIdentifierOrKeyword(PEGTransformer &transformer, ParseResult &parse_result);

	static string TransformNumberLiteral(PEGTransformer &transformer, ParseResult &parse_result);

private:
    PEGParser parser;
	case_insensitive_map_t<PEGTransformer::AnyTransformFunction> sql_transform_functions;
	case_insensitive_map_t<case_insensitive_map_t<unique_ptr<TransformEnumValue>>> enum_mappings;
};

// Helper struct for qualified names.
struct PEGQualifiedName {
    string catalog;
    string schema;
    string name;

    static PEGQualifiedName FromIdentifierList(const ListParseResult& identifier_list_pr);
};

} // namespace duckdb
