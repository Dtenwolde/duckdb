#pragma once

#include "duckdb/common/case_insensitive_map.hpp"
#include "duckdb/common/string.hpp"

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
		return keyword_map.count(text) != 0;
	}
	void AddKeyword(const string &text, PEGKeywordCategory category) {
		keyword_map[text] |= CategoryToBitmask(category);
	}

private:
	static constexpr uint8_t KW_RESERVED = 1 << 0;
	static constexpr uint8_t KW_UNRESERVED = 1 << 1;
	static constexpr uint8_t KW_TYPE_FUNC = 1 << 2;
	static constexpr uint8_t KW_COL_NAME = 1 << 3;

	static uint8_t CategoryToBitmask(PEGKeywordCategory category) {
		switch (category) {
		case PEGKeywordCategory::KEYWORD_RESERVED:
			return KW_RESERVED;
		case PEGKeywordCategory::KEYWORD_UNRESERVED:
			return KW_UNRESERVED;
		case PEGKeywordCategory::KEYWORD_TYPE_FUNC:
			return KW_TYPE_FUNC;
		case PEGKeywordCategory::KEYWORD_COL_NAME:
			return KW_COL_NAME;
		default:
			return 0;
		}
	}

	PEGKeywordHelper();
	bool initialized;
	case_insensitive_map_t<uint8_t> keyword_map;
};
} // namespace duckdb
