#include "parser/peg_parser.hpp"

#include "duckdb/common/printer.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

class RuleParser {
public:
	explicit RuleParser(const vector<PEGToken> &tokens_p) : tokens(tokens_p), pos(0) {
	}
	unique_ptr<PEGExpression> Parse() {
		return ParseChoice();
	}

private:
	const vector<PEGToken> &tokens;
	idx_t pos;
	bool Match(const string &op) {
		if (pos < tokens.size() && tokens[pos].type == PEGTokenType::OPERATOR && tokens[pos].text == op) {
			pos++;
			return true;
		}
		return false;
	}
	unique_ptr<PEGExpression> ParseSuffix() {
		auto expr = ParsePrimary();
		if (Match("?")) {
			return make_uniq<PEGOptionalExpression>(std::move(expr));
		}
		if (Match("*")) {
			return make_uniq<PEGZeroOrMoreExpression>(std::move(expr));
		}
		if (Match("+")) {
			return make_uniq<PEGOneOrMoreExpression>(std::move(expr));
		}
		return expr;
	}
	unique_ptr<PEGExpression> ParseSequence() {
		vector<unique_ptr<PEGExpression>> children;
		children.push_back(ParseSuffix());
		while (pos < tokens.size() && tokens[pos].text != "/" && tokens[pos].text != ")") {
			children.push_back(ParseSuffix());
		}
		if (children.size() == 1) {
			return std::move(children[0]);
		}
		return make_uniq<PEGSequenceExpression>(std::move(children));
	}
	unique_ptr<PEGExpression> ParseChoice() {
		vector<unique_ptr<PEGExpression>> children;
		children.push_back(ParseSequence());
		while (Match("/")) {
			children.push_back(ParseSequence());
		}
		if (children.size() == 1) {
			return std::move(children[0]);
		}
		return make_uniq<PEGChoiceExpression>(std::move(children));
	}
	unique_ptr<PEGExpression> ParsePrimary() {
		if (pos >= tokens.size()) {
			throw InternalException("Unexpected end of rule definition");
		}
		auto &token = tokens[pos];
		if (token.type == PEGTokenType::OPERATOR) {
			if (token.text == "(") {
				pos++;
				auto expr = ParseChoice();
				if (!Match(")")) {
					throw InternalException("Expected ')' in rule definition");
				}
				return expr;
			}
			if (token.text == "!") {
				pos++;
				return make_uniq<PEGNotPredicateExpression>(ParseSuffix());
			}
			if (token.text == "&") {
				pos++;
				return make_uniq<PEGAndPredicateExpression>(ParseSuffix());
			}
		}
		if (token.type == PEGTokenType::LITERAL) {
			pos++;
			return make_uniq<PEGKeywordExpression>(token.text);
		}
		if (token.type == PEGTokenType::REFERENCE) {
			pos++;
			return make_uniq<PEGRuleReferenceExpression>(token.text);
		}
		if (token.type == PEGTokenType::IDENTIFIER) {
			pos++;
			return make_uniq<PEGIdentifierExpression>(token.text);
		}
		throw InternalException("Unexpected token in rule definition: %s", token.text);
	}
};

static bool IsPEGOperator(char c) {
	switch (c) {
	case '/':
	case '?':
	case '(':
	case ')':
	case '*':
	case '!':
	case '+':
	case '%':
	case '&':
		return true;
	default:
		return false;
	}
}

void PEGParser::ParseRules(const char *grammar) {
	enum class PEGParseState { RULE_NAME, RULE_SEPARATOR, RULE_DEFINITION };

	rules.clear();
	string current_rule_name;
	vector<string> current_params;
	vector<PEGToken> current_tokens;
	PEGParseState parse_state = PEGParseState::RULE_NAME;
	idx_t bracket_count = 0;
	bool in_or_clause = false;
	idx_t c = 0;

	auto AddRule = [&](string &name, vector<string> &params, vector<PEGToken> &tokens) {
		if (name.empty() || tokens.empty()) {
			return;
		}

		PEGRule new_rule;
		// If the rule has parameters, we store the raw tokens for template expansion later.
		if (!params.empty()) {
			new_rule.parameters = std::move(params);
			new_rule.raw_tokens = tokens;
		}

		// Always parse the tokens into a structured expression tree.
		RuleParser rule_parser(tokens);
		new_rule.expression = rule_parser.Parse();

		if (rules.find(name) != rules.end()) {
			throw InternalException("Duplicate rule name: %s", name);
		}
		rules[name] = std::move(new_rule);
	};

	while (grammar[c]) {
		// --- Comment and Whitespace Handling ---
		if (grammar[c] == '#') {
			while (grammar[c] && !StringUtil::CharacterIsNewline(grammar[c])) {
				c++;
			}
			continue;
		}
		if (parse_state == PEGParseState::RULE_DEFINITION && StringUtil::CharacterIsNewline(grammar[c]) &&
		    bracket_count == 0 && !in_or_clause && !current_tokens.empty()) {
			AddRule(current_rule_name, current_params, current_tokens);
			current_rule_name.clear();
			current_params.clear();
			current_tokens.clear();
			parse_state = PEGParseState::RULE_NAME;
			c++;
			continue;
		}
		if (StringUtil::CharacterIsSpace(grammar[c])) {
			c++;
			continue;
		}

		// --- State Machine ---
		switch (parse_state) {
		case PEGParseState::RULE_NAME: {
			idx_t start_pos = c;
			while (grammar[c] && (StringUtil::CharacterIsAlphaNumeric(grammar[c]) || grammar[c] == '_')) {
				c++;
			}
			if (c == start_pos) {
				throw InternalException("Failed to parse grammar: expected an alpha-numeric rule name (pos %d)", c);
			}
			current_rule_name = string(grammar + start_pos, c - start_pos);
			current_tokens.clear();
			current_params.clear();
			parse_state = PEGParseState::RULE_SEPARATOR;
			break;
		}
		case PEGParseState::RULE_SEPARATOR: {
			// Check for parameters, e.g., List(D)
			if (grammar[c] == '(') {
				c++;
				while (true) {
					while (grammar[c] && StringUtil::CharacterIsSpace(grammar[c])) {
						c++;
					}
					idx_t param_start = c;
					while (grammar[c] && StringUtil::CharacterIsAlphaNumeric(grammar[c])) {
						c++;
					}
					current_params.push_back(string(grammar + param_start, c - param_start));
					while (grammar[c] && StringUtil::CharacterIsSpace(grammar[c])) {
						c++;
					}
					if (grammar[c] == ')') {
						c++;
						break;
					}
					if (grammar[c] == ',') {
						c++;
						continue;
					}
					throw InternalException("Expected ',' or ')' in parameter list for rule '%s'", current_rule_name);
				}
			}
			while (grammar[c] && StringUtil::CharacterIsSpace(grammar[c])) {
				c++;
			}
			// After handling optional parameters, look for the arrow.
			if (grammar[c] != '<' || grammar[c + 1] != '-') {
				throw InternalException("Failed to parse grammar: expected '<-' (pos %d)", c);
			}
			c += 2;
			parse_state = PEGParseState::RULE_DEFINITION;
			break;
		}
		case PEGParseState::RULE_DEFINITION: {
			in_or_clause = false;
			if (grammar[c] == '\'') {
				c++;
				idx_t literal_start = c;
				while (grammar[c] && grammar[c] != '\'') {
					c++;
				}
				if (!grammar[c]) {
					throw InternalException("Unclosed literal quote at pos %d", c);
				}
				current_tokens.push_back({PEGTokenType::LITERAL, string(grammar + literal_start, c - literal_start)});
				c++;
				if (grammar[c] == 'i') {
					c++;
				}
			} else if (StringUtil::CharacterIsAlpha(grammar[c])) {
				idx_t rule_start = c;
				while (grammar[c] && (StringUtil::CharacterIsAlphaNumeric(grammar[c]) || grammar[c] == '_')) {
					c++;
				}
				string invoked_rule_name(grammar + rule_start, c - rule_start);

				bool is_parameter = false;
				for (const auto &param : current_params) {
					if (param == invoked_rule_name) {
						is_parameter = true;
						break;
					}
				}

				if (is_parameter) {
					current_tokens.push_back({PEGTokenType::REFERENCE, invoked_rule_name});
				} else {
					// FIX: A parameterized call must have no space between the name and the '('.
					if (grammar[c] == '(') { // Parameterized rule call
						c++;
						vector<PEGToken> arguments;
						while (true) {
							while (grammar[c] && StringUtil::CharacterIsSpace(grammar[c])) {
								c++;
							}
							if (!StringUtil::CharacterIsAlpha(grammar[c])) {
								throw InternalException("Expected a rule reference as an argument for '%s'",
														invoked_rule_name);
							}
							idx_t arg_start = c;
							while (grammar[c] &&
								   (StringUtil::CharacterIsAlphaNumeric(grammar[c]) || grammar[c] == '_')) {
								c++;
								   }
							arguments.push_back(
								{PEGTokenType::REFERENCE, string(grammar + arg_start, c - arg_start)});

							while (grammar[c] && StringUtil::CharacterIsSpace(grammar[c])) {
								c++;
							}
							if (grammar[c] == ')') {
								c++;
								break;
							}
							if (grammar[c] == ',') {
								c++;
								continue;
							}
							throw InternalException("Expected ',' or ')' in argument list for call to '%s'",
													invoked_rule_name);
						}

						auto it = rules.find(invoked_rule_name);
						if (it == rules.end() || it->second.parameters.empty()) {
							throw InternalException("Call to undefined or non-parameterized rule '%s'",
													invoked_rule_name);
						}
						auto &template_rule = it->second;
						if (template_rule.parameters.size() != arguments.size()) {
							throw InternalException("Argument count mismatch for rule '%s': expected %d, got %d",
													invoked_rule_name, template_rule.parameters.size(),
													arguments.size());
						}

						for (const auto &token_template : template_rule.raw_tokens) {
							bool substituted = false;
							if (token_template.type == PEGTokenType::REFERENCE) {
								for (size_t i = 0; i < template_rule.parameters.size(); ++i) {
									if (token_template.text == template_rule.parameters[i]) {
										current_tokens.push_back(arguments[i]);
										substituted = true;
										break;
									}
								}
							}
							if (!substituted) {
								current_tokens.push_back(token_template);
							}
						}
					} else { // Normal rule reference
						current_tokens.push_back({PEGTokenType::REFERENCE, invoked_rule_name});
					}
				}
			} else if (grammar[c] == '[' || grammar[c] == '<') {
				idx_t rule_start = c;
				char final_char = grammar[c] == '[' ? ']' : '>';
				c++;
				while (grammar[c] && grammar[c] != final_char) {
					if (grammar[c] == '\\') {
						c++;
					}
					if (grammar[c]) {
						c++;
					}
				}
				if (grammar[c] == final_char) {
					c++;
				}
				current_tokens.push_back({PEGTokenType::IDENTIFIER, "Identifier"});
			} else if (IsPEGOperator(grammar[c])) {
				if (grammar[c] == '(') {
					bracket_count++;
				} else if (grammar[c] == ')') {
					if (bracket_count == 0) {
						throw InternalException("Unclosed bracket");
					}
					bracket_count--;
				} else if (grammar[c] == '/') {
					in_or_clause = true;
				}
				current_tokens.push_back({PEGTokenType::OPERATOR, string(1, grammar[c])});
				c++;
			} else {
				throw InternalException("Unrecognized rule contents in rule %s (character %s)", current_rule_name,
										string(1, grammar[c]));
			}
			break;
		}
		}
	}
	// Finalize the very last rule in the file
	AddRule(current_rule_name, current_params, current_tokens);
}

} // namespace duckdb
