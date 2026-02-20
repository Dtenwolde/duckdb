#pragma once

#include "duckdb/common/case_insensitive_map.hpp"
#include "duckdb/common/string.hpp"
#include "duckdb/common/vector.hpp"

namespace duckdb {
enum class PEGKeywordCategory : uint8_t {
	KEYWORD_NONE,
	KEYWORD_UNRESERVED,
	KEYWORD_RESERVED,
	KEYWORD_TYPE_FUNC,
	KEYWORD_COL_NAME
};

class PEGKeywordHelper {
public:
	static PEGKeywordHelper &Instance();
	bool KeywordCategoryType(const string &text, PEGKeywordCategory type) const;
	void InitializeKeywordMaps();
	bool IsKeyword(const string &text) {
		if (reserved_keyword_map.count(text) != 0 || unreserved_keyword_map.count(text) != 0 ||
		    colname_keyword_map.count(text) != 0 || typefunc_keyword_map.count(text) != 0) {
			return true;
		}
		return false;
	};
	//! Returns reserved keywords that start with the given prefix (case-insensitive).
	//! Returns an empty vector if no matches are found.
	vector<string> FindReservedKeywordsByPrefix(const string &prefix) const;

private:
	PEGKeywordHelper();
	bool initialized;
	case_insensitive_set_t reserved_keyword_map;
	case_insensitive_set_t unreserved_keyword_map;
	case_insensitive_set_t colname_keyword_map;
	case_insensitive_set_t typefunc_keyword_map;
};
} // namespace duckdb
