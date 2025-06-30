#pragma once

#include "tokenizer.hpp"
#include "parse_result.hpp"
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
    using TransformDispatchFunction = std::function<unique_ptr<SQLStatement>(PEGTransformer&, ParseResult&)>;

    PEGTransformer(ArenaAllocator &allocator, PEGTransformerState &state,
                   const string_map_t<TransformDispatchFunction> &transform_functions,
                   const string_map_t<PEGRule> &grammar_rules)
        : allocator(allocator), state(state), transform_functions(transform_functions), grammar_rules(grammar_rules) {
    }

public:
    // The main entry point to transform a matched rule. Returns ownership of the statement.
    unique_ptr<SQLStatement> Transform(const string_t &rule_name, ParseResult &matched_parse_result);

    // Core PEG Parser function. Returns a raw pointer owned by the ArenaAllocator.
    ParseResult *MatchRule(const string_t &rule_name);
    ParseResult *MatchRule(const PEGExpression &expression);

    // Make overloads return raw pointers, as ownership is handled by the ArenaAllocator.
    template<class T, typename... Args>
    T* Make(Args&&... args) {
        return allocator.Make<T>(std::forward<Args>(args)...);
    }

public:
    ArenaAllocator &allocator;
    PEGTransformerState &state;

private:
    const string_map_t<TransformDispatchFunction> &transform_functions;
    const string_map_t<PEGRule> &grammar_rules;
};

// PEGTransformerFactory is now a real class that holds the transformation state.
class PEGTransformerFactory {
public:
    // The factory is constructed with a specific grammar.
    explicit PEGTransformerFactory(const char *grammar);

    // Transforms a sequence of tokens into a SQL statement.
    unique_ptr<SQLStatement> Transform(vector<MatcherToken> &tokens, const char* root_rule = "Root");

private:
	// New overload for dispatcher functions that take the raw ParseResult (e.g., TransformRoot)
	template <class FUNC>
	void Register(const string &rule_name, FUNC function) {
		sql_transform_functions[rule_name] = function;
	}

    // Register for functions taking one ParseResult child
    template <class A, idx_t INDEX_A, class FUNC>
    void Register(const string &rule_name, FUNC function) {
        sql_transform_functions[rule_name] = [function](PEGTransformer& transformer, ParseResult &parse_result) -> unique_ptr<SQLStatement> {
            auto &list_pr = parse_result.Cast<ListParseResult>();
            auto &child_a = list_pr.Child<A>(INDEX_A);
            return function(transformer, child_a);
        };
    }

    // Register for functions taking two ParseResult children
    template <class A, idx_t INDEX_A, class B, idx_t INDEX_B, class FUNC>
    void Register(const string &rule_name, FUNC function) {
        sql_transform_functions[rule_name] = [function](PEGTransformer& transformer, ParseResult &parse_result) -> unique_ptr<SQLStatement> {
            auto &list_pr = parse_result.Cast<ListParseResult>();
            auto &child_a = list_pr.Child<A>(INDEX_A);
            auto &child_b = list_pr.Child<B>(INDEX_B);
            return function(transformer, child_a, child_b);
        };
    }
    // ... Add more overloads for 3, 4, etc., arguments as needed.

    // --- SPECIFIC TRANSFORM FUNCTIONS (static and decoupled) ---
    static unique_ptr<SetStatement> TransformUseStatement(PEGTransformer&, ChoiceParseResult &identifier_pr);
	static unique_ptr<SQLStatement> TransformRoot(PEGTransformer&, ParseResult &list);

	static unique_ptr<QualifiedName> TransformQualifiedName(reference<ListParseResult> root);

private:
    // Each factory instance owns its parsed grammar and transform function map.
    PEGParser parser;
    string_map_t<PEGTransformer::TransformDispatchFunction> sql_transform_functions;
};

// Helper struct for qualified names.
struct PEGQualifiedName {
    string catalog;
    string schema;
    string name;

    static PEGQualifiedName FromIdentifierList(const ListParseResult& identifier_list_pr);
};

} // namespace duckdb
