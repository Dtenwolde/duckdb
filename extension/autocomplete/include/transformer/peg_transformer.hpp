#pragma once

#include "tokenizer.hpp"
#include "parse_result.hpp"
#include "transform_result.hpp"
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
				   const string_map_t<string_map_t<int16_t>> &enum_mappings)
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
	const string_map_t<string_map_t<int16_t>> &enum_mappings;

private:
	const case_insensitive_map_t<AnyTransformFunction> &transform_functions;
	const case_insensitive_map_t<PEGRule> &grammar_rules;
	// The substitution stack for handling nested parameterized rule calls.
	vector<string_map_t<const PEGExpression *>> substitution_stack;
};

// PEGTransformerFactory is now a real class that holds the transformation state.
class PEGTransformerFactory {
public:
    // The factory is constructed with a specific grammar.
    explicit PEGTransformerFactory(const char *grammar);

    // Transforms a sequence of tokens into a SQL statement.
    unique_ptr<SQLStatement> Transform(vector<MatcherToken> &tokens, const char* root_rule = "Root");

private:
	template <typename T>
	void RegisterEnum(const string_t &rule_name, const string_map_t<T> &mapping) {
		for (const auto &pair : mapping) {
			enum_mappings[rule_name][pair.first] = static_cast<int16_t>(pair.second);
		}
	}


	template <class FUNC>
	void Register(const string &rule_name, FUNC function) {
		sql_transform_functions[rule_name] = [function](PEGTransformer& transformer, ParseResult &parse_result) -> unique_ptr<TransformResultValue> {
			unique_ptr<SQLStatement> result_value = function(transformer, parse_result);
			return make_uniq<TypedTransformResult<unique_ptr<SQLStatement>>>(std::move(result_value));
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

	static unique_ptr<SQLStatement> TransformRoot(PEGTransformer&, ParseResult &list);

    static unique_ptr<SQLStatement> TransformUseStatement(PEGTransformer&, ListParseResult &identifier_pr);
	static unique_ptr<SQLStatement> TransformSetStatement(PEGTransformer&, ChoiceParseResult &choice_pr);
	static unique_ptr<SQLStatement> TransformResetStatement(PEGTransformer &, ChoiceParseResult &choice_pr);
	static unique_ptr<QualifiedName> TransformQualifiedName(vector<string> &root);
	static void TransformSettingOrVariable(PEGTransformer& transformer, ChoiceParseResult &variable_or_setting, string &setting_name, SetScope &scope);

	static vector<string> TransformDottedIdentifier(reference<ListParseResult> root);

private:
    PEGParser parser;
	case_insensitive_map_t<PEGTransformer::AnyTransformFunction> sql_transform_functions;
	string_map_t<string_map_t<int16_t>> enum_mappings;
};

// Helper struct for qualified names.
struct PEGQualifiedName {
    string catalog;
    string schema;
    string name;

    static PEGQualifiedName FromIdentifierList(const ListParseResult& identifier_list_pr);
};

} // namespace duckdb
