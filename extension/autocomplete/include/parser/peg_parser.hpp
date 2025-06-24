#pragma once
#include "duckdb/common/string_map_set.hpp"
#include "duckdb/common/types/string_type.hpp"

namespace duckdb {
enum class PEGRuleType {
	LITERAL,   // literal rule ('Keyword'i)
	REFERENCE, // reference to another rule (Rule)
	OPTIONAL,  // optional rule (Rule?)
	OR,        // or rule (Rule1 / Rule2)
	REPEAT     // repeat rule (Rule1*
};

enum class PEGTokenType {
	LITERAL,       // literal token ('Keyword'i)
	REFERENCE,     // reference token (Rule)
	OPERATOR,      // operator token (/ or )
	FUNCTION_CALL, // start of function call (i.e. Function(...))
	REGEX          // regular expression ([ \t\n\r] or <[a-z_]i[a-z0-9_]i>)
};

struct PEGToken {
	PEGTokenType type;
	string_t text;
};

struct PEGRule {
	string_map_t<idx_t> parameters;
	vector<PEGToken> tokens;

	void Clear() {
		parameters.clear();
		tokens.clear();
	}
};

enum class PEGParseState {
	RULE_NAME,      // Rule name
	RULE_SEPARATOR, // look for <-
	RULE_DEFINITION // part of rule definition
};

struct PEGParser {
public:
	void ParseRules(const char *grammar);

	void AddRule(string_t rule_name, PEGRule rule) {
		auto entry = rules.find(rule_name);
		if (entry != rules.end()) {
			throw InternalException("Failed to parse grammar - duplicate rule name %s", rule_name.GetString());
		}
		rules.insert(make_pair(rule_name, std::move(rule)));
	}

	string_map_t<PEGRule> rules;
};
}