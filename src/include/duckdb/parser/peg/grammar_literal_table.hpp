//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/peg/grammar_literal_table.hpp
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/case_insensitive_map.hpp"
#include "duckdb/parser/peg/literal_info.hpp"

namespace duckdb {

class ParsedGrammar;

//! Immutable after construction, including literals only present in keyword-category rules.
class GrammarLiteralTable {
public:
	DUCKDB_API GrammarLiteralTable(const ParsedGrammar &grammar, const case_insensitive_map_t<LiteralInfo> &keywords);
	GrammarLiteralTable(const GrammarLiteralTable &) = delete;
	GrammarLiteralTable &operator=(const GrammarLiteralTable &) = delete;

	uint64_t CacheId() const {
		return cache_id;
	}

	//! Calls fun(info) for every literal in the table
	template <class FUN>
	void ForEachLiteral(FUN &&fun) const {
		for (auto &entry : literals) {
			fun(entry.second);
		}
	}

	LiteralInfo Lookup(const string &text) const {
		// most tokens that are not literals (numbers, strings, long identifiers) are rejected without hashing
		if (text.empty() || text.size() > max_literal_length || !first_bytes[static_cast<uint8_t>(text[0])]) {
			return LiteralInfo();
		}
		if (text.size() == 1) {
			return single_byte[static_cast<uint8_t>(text[0])];
		}
		auto entry = literals.find(text);
		return entry == literals.end() ? LiteralInfo() : entry->second;
	}

private:
	void Register(const string &text, keyword_categories_t categories = keyword_categories_t());
	void Index();

private:
	const uint64_t cache_id;
	idx_t max_literal_length = 0;
	case_insensitive_map_t<LiteralInfo> literals;
	//! Bytes (case-folded) that start some literal, and the literals of exactly one byte
	bool first_bytes[256] = {};
	LiteralInfo single_byte[256];
};

} // namespace duckdb
