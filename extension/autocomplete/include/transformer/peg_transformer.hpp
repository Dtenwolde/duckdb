#pragma once

#include "parse_result.hpp"
#include "matcher.hpp"
#include "parser/peg_parser.hpp"
#include "duckdb/parser/statement/set_statement.hpp"


#include <functional>

namespace duckdb {

struct PEGTransformerState {
	explicit PEGTransformerState(vector<MatcherToken> &tokens)
		: tokens(tokens), token_index(0) {
	}
	explicit PEGTransformerState(MatchState &state)
		: tokens(state.tokens), token_index(state.token_index) {
	}

	vector<MatcherToken> &tokens;
	idx_t token_index;
};

class PEGTransformer {
public:
	using TransformFunction = std::function<reference<SQLStatement>(PEGTransformer &)>;

	PEGTransformer(Allocator &allocator, PEGTransformerState &state,
	               string_map_t<TransformFunction> &rules)
	    : allocator(allocator), state(state), rules(rules) {
	}

public:
	template <class T, class... ARGS>
	reference<SQLStatement> Make(ARGS &&... args) {
		static_assert(std::is_base_of<SQLStatement, T>::value, "T must be a SQLStatement");
		auto ptr = make_uniq<T>(*this, std::forward<ARGS>(args)...);
		reference<T> ref = ptr.get();
		owned_results.push_back(std::move(ptr));
		return ref;
	}

	template <class T>
	T &Children() {
		if (!current_parse_result || current_parse_result->Type() == ParseResultType::INVALID) {
			throw InternalException("PEGTransformer::Children() called on an unexpected state");
		}
		return current_parse_result->Cast<T>();
	}

	static reference<ParseResult> RootTransformer(PEGTransformerState &state, Allocator &allocator);

	// Apply a rule
	reference<ParseResult> Transform(const string_t &rule_name);

	static unique_ptr<SQLStatement> TransformStatement(PEGTransformer &transformer);
	static unique_ptr<SetStatement> TransformUseStatement(PEGTransformer &transformer);


public:
	unique_ptr<ParseResult> current_parse_result;
	Allocator &allocator;
	PEGTransformerState &state;
	vector<unique_ptr<ParseResult>> owned_results;

private:
	string_map_t<TransformFunction> &rules;
};

class PEGTransformerFactory {
public:
	static reference<ParseResult> CreateTransformer(const char *grammar, const char *root_rule, PEGTransformerState &state, Allocator &allocator);

	string_map_t<PEGTransformer::TransformFunction> transform_functions = {
		{"UseStatement", PEGTransformer::TransformUseStatement}
	};

private:
	reference<ParseResult> CreateTransformer(PEGParser &parser, const string_t &rule_name, PEGTransformerState &state, Allocator &allocator);

};

} // namespace duckdb