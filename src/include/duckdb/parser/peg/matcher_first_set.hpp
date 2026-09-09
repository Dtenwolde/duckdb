//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/peg/matcher_first_set.hpp
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/helper.hpp"
#include "duckdb/common/vector.hpp"

namespace duckdb {

class GrammarLiteralTable;
class Matcher;
class FirstChoiceMatcher;

//! Conservative literal starts. Unknown is the default, including for custom matchers.
class MatcherFirstSet {
public:
	MatcherFirstSet() = default;
	DUCKDB_API explicit MatcherFirstSet(vector<uint32_t> literals, bool nullable = false);

	bool IsUnknown() const {
		return unknown;
	}
	bool CanMatchEmpty() const {
		return nullable;
	}
	DUCKDB_API bool CanStartWith(uint32_t literal) const;

private:
	friend class MatcherFirstSetBuilder;
	void Merge(const MatcherFirstSet &other);
	bool operator==(const MatcherFirstSet &other) const;

private:
	vector<uint32_t> literals;
	bool unknown = true;
	bool nullable = true;
};

void InitializeMatcherFirstSets(const vector<reference<Matcher>> &builtins,
                                const vector<reference<FirstChoiceMatcher>> &choices, const GrammarLiteralTable &table);

} // namespace duckdb
