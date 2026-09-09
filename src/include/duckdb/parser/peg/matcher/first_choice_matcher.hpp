//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/peg/matcher/first_choice_matcher.hpp
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/parser/peg/matcher/choice_matcher.hpp"

namespace duckdb {

class FirstChoiceMatcher final : public ChoiceMatcher {
public:
	FirstChoiceMatcher(vector<reference<Matcher>> &&matchers, const GrammarLiteralTable &table)
	    : ChoiceMatcher(std::move(matchers)), table(table) {
	}

	arena_ptr<MatchProcess> StartMatch(MatchState &state) const override;
	idx_t NextChild(MatchState &state, idx_t child_index) const;
	void SetFirstSets(vector<MatcherFirstSet> sets);

private:
	const GrammarLiteralTable &table;
	vector<MatcherFirstSet> first_sets;
};

} // namespace duckdb
