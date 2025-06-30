#include "parser/peg_parser.hpp"

#include "duckdb/common/printer.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

class RuleParser {
public:
    explicit RuleParser(const vector<PEGToken>& tokens_p) : tokens(tokens_p), pos(0) {}
    unique_ptr<PEGExpression> Parse() { return ParseChoice(); }
private:
    const vector<PEGToken>& tokens;
    idx_t pos;
    bool Match(const string& op) {
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
        auto& token = tokens[pos];
        if (token.type == PEGTokenType::OPERATOR) {
            if (token.text == "(") {
                pos++;
                auto expr = ParseChoice();
                if (!Match(")")) { throw InternalException("Expected ')' in rule definition"); }
                return expr;
            }
            if (token.text == "!") { pos++; return make_uniq<PEGNotPredicateExpression>(ParseSuffix()); }
            if (token.text == "&") { pos++; return make_uniq<PEGAndPredicateExpression>(ParseSuffix()); }
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

void PEGParser::ParseRules(const char *grammar) {
    enum class ParserState {
        RULE_NAME,
        RULE_SEPARATOR,
        RULE_DEFINITION
    };

    rules.clear();
    string current_rule_name;
    vector<PEGToken> current_tokens;
    ParserState parse_state = ParserState::RULE_NAME;

    idx_t c = 0;
    while(true) {
        // Skip whitespace and comments
        while (grammar[c] && (StringUtil::CharacterIsSpace(grammar[c]) || StringUtil::CharacterIsNewline(grammar[c]))) {
            c++;
        }
        if (grammar[c] == '#') {
            while (grammar[c] && !StringUtil::CharacterIsNewline(grammar[c])) {
	            c++;
            }
            continue;
        }

        if (grammar[c] == '\0') {
            break; // End of grammar string
        }

        // Lookahead to check if we are at the start of a new rule definition.
        idx_t lookahead_pos = c;
        if (StringUtil::CharacterIsAlpha(grammar[lookahead_pos])) {
            idx_t name_end = lookahead_pos;
            while(grammar[name_end] && (StringUtil::CharacterIsAlphaNumeric(grammar[name_end]) || grammar[name_end] == '_')) {
                name_end++;
            }
            idx_t arrow_pos = name_end;
            while(grammar[arrow_pos] && StringUtil::CharacterIsSpace(grammar[arrow_pos])) {
                arrow_pos++;
            }
            if (grammar[arrow_pos] == '<' && grammar[arrow_pos+1] == '-') {
                // This is definitively a new rule. Finalize the previous one if it exists.
                if (parse_state == ParserState::RULE_DEFINITION && !current_tokens.empty()) {
                    RuleParser rule_parser(current_tokens);
                    PEGRule new_rule;
                    new_rule.expression = rule_parser.Parse();
                    if (rules.find(current_rule_name) != rules.end()) {
	                    throw InternalException("Duplicate rule name: %s", current_rule_name);
                    }
                	Printer::PrintF("Adding rule %s", current_rule_name.c_str());
                    rules[current_rule_name] = std::move(new_rule);
                }
                // Reset for the new rule.
                current_tokens.clear();
                parse_state = ParserState::RULE_NAME;
            }
        }

        if (parse_state == ParserState::RULE_NAME) {
            idx_t name_start = c;
            while(grammar[c] && (StringUtil::CharacterIsAlphaNumeric(grammar[c]) || grammar[c] == '_')) {
	            c++;
            }
            current_rule_name = string(grammar + name_start, c - name_start);
            if (current_rule_name.empty()) {
	            throw InternalException("Failed to parse grammar: expected rule name");
            }
            parse_state = ParserState::RULE_SEPARATOR;
            continue; // Go to next iteration to skip whitespace before arrow
        }

        if (parse_state == ParserState::RULE_SEPARATOR) {
        	if (grammar[c] != '<' || grammar[c + 1] != '-') {
	             throw InternalException("Expected '<-' after rule name '%s'", current_rule_name);
        	}
            c += 2;
            parse_state = ParserState::RULE_DEFINITION;
            continue; // Go to next iteration to start parsing rule definition
        }

        if (parse_state == ParserState::RULE_DEFINITION) {
            char current_char = grammar[c];
            if (current_char == '\'') {
                c++;
                idx_t literal_start = c;
                while (grammar[c] && grammar[c] != '\'') {
	                c++;
                }
                current_tokens.push_back({PEGTokenType::LITERAL, string(grammar + literal_start, c - literal_start)});
                if (grammar[c] == '\'') {
	                c++;
                }
                if (grammar[c] == 'i') {
	                c++;
                }
            } else if (StringUtil::CharacterIsAlpha(current_char)) {
                idx_t rule_start = c;
                while (StringUtil::CharacterIsAlphaNumeric(grammar[c]) || grammar[c] == '_') {
	                c++;
                }
                current_tokens.push_back({PEGTokenType::REFERENCE, string(grammar + rule_start, c - rule_start)});
            } else if (current_char == '[' || current_char == '<') {
                idx_t regex_start = c;
                char closer = (current_char == '[') ? ']' : '>';
                c++;
				while (grammar[c] && grammar[c] != closer) {
					if (grammar[c] == '\\') {
						c++;
					}
					c++;
				}
				if (grammar[c] == closer) {
					c++;
				}
				current_tokens.push_back({PEGTokenType::IDENTIFIER, string("Identifier")});
            } else if (strchr("/?*+()!&%", current_char)) {
                current_tokens.push_back({PEGTokenType::OPERATOR, string(1, current_char)});
                c++;
            } else {
                 throw InternalException("Unrecognized character in rule definition: %c", current_char);
            }
        }
    }

    // Finalize the very last rule in the file after the loop ends
    if (parse_state == ParserState::RULE_DEFINITION && !current_tokens.empty()) {
        RuleParser rule_parser(current_tokens);
        PEGRule new_rule;
        new_rule.expression = rule_parser.Parse();
        if (rules.find(current_rule_name) != rules.end()) {
	        throw InternalException("Duplicate rule name: %s", current_rule_name);
        }
    	Printer::PrintF("adding rule %s", current_rule_name.c_str());
        rules[current_rule_name] = std::move(new_rule);
    }
}

} // namespace duckdb
