#include "keyword_helper.hpp"

namespace duckdb {
PEGKeywordHelper &PEGKeywordHelper::Instance() {
	static PEGKeywordHelper instance;
	return instance;
}

PEGKeywordHelper::PEGKeywordHelper() {
	InitializeKeywordMaps();
}

bool PEGKeywordHelper::KeywordCategoryType(const std::string &text, const PEGKeywordCategory type) const {
	auto it = keyword_map.find(text);
	if (it == keyword_map.end()) {
		return false;
	}
	return (it->second & CategoryToBitmask(type)) != 0;
}
} // namespace duckdb
