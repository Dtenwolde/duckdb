#pragma once

#include "parse_result.hpp"
#include "duckdb/common/case_insensitive_map.hpp"

#include <functional>
#include <unordered_map>

namespace duckdb {

class PEGTransformer {
public:
	using TransformFunction = std::function<unique_ptr<ParseResult>(PEGTransformer &, ParseResult &)>;

public:
	// Register a rule
	void AddRule(const string &rule_name, TransformFunction function);

	// Apply a rule
	unique_ptr<ParseResult> Transform(const string &rule_name, ParseResult &input);

	unique_ptr<ParseResult> TransformStatement(ParseResult &input);
	unique_ptr<ParseResult> TransformUseStatement(ParseResult &input);

	void Initialize(const char* grammar, const char* root_rule);

	// Create a new result
	template <class T, class... Args>
	T &Make(Args &&...args) {
		auto result = make_uniq<T>(std::forward<Args>(args)...);
		auto &ref = *result;
		allocated.emplace_back(std::move(result));
		return ref;
	}


private:
	case_insensitive_map_t<TransformFunction> rules;
	ArenaAllocator<reference<ParseResult>> allocated;
};

} // namespace duckdb