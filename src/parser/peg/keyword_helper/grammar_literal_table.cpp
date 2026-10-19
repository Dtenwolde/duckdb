#include "duckdb/parser/peg/grammar_literal_table.hpp"

#include "duckdb/common/atomic.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/parser/peg/parsed_grammar.hpp"

namespace duckdb {

static uint64_t NextLiteralTableId() {
	static atomic<uint64_t> next_id {1};
	auto result = next_id.fetch_add(1, std::memory_order_relaxed);
	D_ASSERT(result != 0);
	return result;
}

GrammarLiteralTable::GrammarLiteralTable(const ParsedGrammar &grammar,
                                         const case_insensitive_map_t<LiteralInfo> &keywords)
    : cache_id(NextLiteralTableId()) {
	vector<reference<const PEGExpression>> pending;
	for (auto &entry : grammar.rules) {
		pending.push_back(entry.second->recipe.expression);
	}
	while (!pending.empty()) {
		auto &expression = pending.back().get();
		pending.pop_back();
		if (expression.type == PEGExpression::Type::LITERAL) {
			Register(expression.text.GetString());
		}
		for (auto &child : expression.children) {
			pending.push_back(child);
		}
	}
	for (auto &entry : keywords) {
		Register(entry.first, entry.second.CategoryFlags());
	}
	Index();
}

void GrammarLiteralTable::Index() {
	for (auto &entry : literals) {
		auto &text = entry.first;
		auto first = static_cast<uint8_t>(text[0]);
		first_bytes[first] = true;
		first_bytes[static_cast<uint8_t>(StringUtil::CharacterToLower(static_cast<char>(first)))] = true;
		first_bytes[static_cast<uint8_t>(StringUtil::CharacterToUpper(static_cast<char>(first)))] = true;
		if (text.size() == 1) {
			single_byte[first] = entry.second;
			single_byte[static_cast<uint8_t>(StringUtil::CharacterToLower(static_cast<char>(first)))] = entry.second;
			single_byte[static_cast<uint8_t>(StringUtil::CharacterToUpper(static_cast<char>(first)))] = entry.second;
		}
	}
}

void GrammarLiteralTable::Register(const string &text, keyword_categories_t categories) {
	auto entry = literals.find(text);
	if (entry == literals.end()) {
		if (literals.size() >= LiteralInfo::MAX_LITERAL_ID) {
			throw InvalidInputException("Grammar has too many distinct literals");
		}
		auto id = static_cast<uint16_t>(literals.size() + 1);
		entry = literals.emplace(text, LiteralInfo(id)).first;
		max_literal_length = MaxValue<idx_t>(max_literal_length, text.size());
	}
	entry->second.AddCategories(categories);
}

} // namespace duckdb
