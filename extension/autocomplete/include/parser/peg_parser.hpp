#pragma once

#include "duckdb/common/case_insensitive_map.hpp"
#include "duckdb/common/string_map_set.hpp"
#include "duckdb/common/types/string_type.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/common/printer.hpp"

namespace duckdb {

// --- PEG Expression Tree ---

enum class PEGExpressionType {
	KEYWORD,
	RULE_REFERENCE,
	SEQUENCE,
	CHOICE,
	OPTIONAL,
	ZERO_OR_MORE,
	ONE_OR_MORE,
	AND_PREDICATE,
	NOT_PREDICATE,
	IDENTIFIER,
	REGEX,
	PARAMETERIZED_RULE,
	NUMBER,
	STRING,
	ERROR
};

struct PEGExpression {
	explicit PEGExpression(PEGExpressionType type) : type(type) {
	}
	virtual ~PEGExpression() = default;
	template <class TARGET>
	TARGET &Cast() {
		return reinterpret_cast<TARGET &>(*this);
	}
	template <class TARGET>
	const TARGET &Cast() const {
		return reinterpret_cast<const TARGET &>(*this);
	}
	PEGExpressionType type;
};

struct PEGKeywordExpression : PEGExpression {
	explicit PEGKeywordExpression(string keyword_p)
	    : PEGExpression(PEGExpressionType::KEYWORD), keyword(std::move(keyword_p)) {
	}
	string keyword;
};

struct PEGRuleReferenceExpression : PEGExpression {
	explicit PEGRuleReferenceExpression(string rule_name_p)
	    : PEGExpression(PEGExpressionType::RULE_REFERENCE), rule_name(std::move(rule_name_p)) {
	}
	string rule_name;
};

struct PEGSequenceExpression : PEGExpression {
	explicit PEGSequenceExpression(vector<unique_ptr<PEGExpression>> children_p)
	    : PEGExpression(PEGExpressionType::SEQUENCE), expressions(std::move(children_p)) {
	}
	vector<unique_ptr<PEGExpression>> expressions;
};

struct PEGChoiceExpression : PEGExpression {
	explicit PEGChoiceExpression(vector<unique_ptr<PEGExpression>> children_p)
	    : PEGExpression(PEGExpressionType::CHOICE), expressions(std::move(children_p)) {
	}
	vector<unique_ptr<PEGExpression>> expressions;
};

struct PEGOptionalExpression : PEGExpression {
	explicit PEGOptionalExpression(unique_ptr<PEGExpression> child_p)
	    : PEGExpression(PEGExpressionType::OPTIONAL), expression(std::move(child_p)) {
	}
	unique_ptr<PEGExpression> expression;
};

struct PEGZeroOrMoreExpression : PEGExpression {
	explicit PEGZeroOrMoreExpression(unique_ptr<PEGExpression> child_p)
	    : PEGExpression(PEGExpressionType::ZERO_OR_MORE), expression(std::move(child_p)) {
	}
	unique_ptr<PEGExpression> expression;
};

struct PEGOneOrMoreExpression : PEGExpression {
	explicit PEGOneOrMoreExpression(unique_ptr<PEGExpression> child_p)
	    : PEGExpression(PEGExpressionType::ONE_OR_MORE), expression(std::move(child_p)) {
	}
	unique_ptr<PEGExpression> expression;
};

struct PEGAndPredicateExpression : PEGExpression {
	explicit PEGAndPredicateExpression(unique_ptr<PEGExpression> child_p)
	    : PEGExpression(PEGExpressionType::AND_PREDICATE), expression(std::move(child_p)) {
	}
	unique_ptr<PEGExpression> expression;
};

struct PEGNotPredicateExpression : PEGExpression {
	explicit PEGNotPredicateExpression(unique_ptr<PEGExpression> child_p)
	    : PEGExpression(PEGExpressionType::NOT_PREDICATE), expression(std::move(child_p)) {
	}
	unique_ptr<PEGExpression> expression;
};

struct PEGIdentifierExpression : PEGExpression {
	explicit PEGIdentifierExpression(string identifier_p)
	    : PEGExpression(PEGExpressionType::IDENTIFIER), identifier(std::move(identifier_p)) {
	}
	string identifier;
};

struct PEGNumberExpression : PEGExpression {
	explicit PEGNumberExpression() : PEGExpression(PEGExpressionType::NUMBER) {
	}
};

struct PEGStringExpression : PEGExpression {
	explicit PEGStringExpression() : PEGExpression(PEGExpressionType::STRING) {
	}
};

struct PEGParameterizedRuleExpression : PEGExpression {
	PEGParameterizedRuleExpression(string rule_name_p, vector<unique_ptr<PEGExpression>> children_p)
	    : PEGExpression(PEGExpressionType::PARAMETERIZED_RULE), rule_name(std::move(rule_name_p)),
	      expressions(std::move(children_p)) {
	}
	string rule_name;
	vector<unique_ptr<PEGExpression>> expressions;
};

struct PEGErrorExpression : PEGExpression {
	explicit PEGErrorExpression(unique_ptr<PEGExpression> error_p)
		: PEGExpression(PEGExpressionType::ERROR), error(std::move(error_p)) {

	}
	unique_ptr<PEGExpression> error;
};

// --- PEG Parser ---

enum class PEGTokenType {
	LITERAL,
	REFERENCE,
	OPERATOR,
	IDENTIFIER,
	FUNCTION_CALL,
	REGEX,
	NUMBER_LITERAL,
	STRING_LITERAL,
	ERROR_MESSAGE
};

struct PEGToken {
	PEGTokenType type;
	string text;
};

struct PEGRule {
	unique_ptr<PEGExpression> expression;
	string_map_t<idx_t> parameters;
	vector<PEGToken> tokens;

	void Clear() {
		parameters.clear();
		tokens.clear();
	}
};

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

	unique_ptr<PEGExpression> ParsePrimary() {
		if (pos >= tokens.size()) {
			throw InternalException("Unexpected end of rule definition");
		}
		auto &token = tokens[pos];
		Printer::PrintF("%s", token.text);
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
		if (token.type == PEGTokenType::NUMBER_LITERAL) {
			pos++;
			return make_uniq<PEGNumberExpression>();
		}
		if (token.type == PEGTokenType::STRING_LITERAL) {
			pos++;
			return make_uniq<PEGStringExpression>();
		}
		if (token.type == PEGTokenType::IDENTIFIER) {
			pos++;
			return make_uniq<PEGIdentifierExpression>(token.text);
		}
		if (token.type == PEGTokenType::FUNCTION_CALL) {
			string rule_name = token.text;
			pos++; // Consume function name token
			if (!Match("(")) {
				throw InternalException("Expected '(' after function call");
			}
			vector<unique_ptr<PEGExpression>> arguments;
			if (!Match(")")) { // Arguments are present
				while (true) {
					arguments.push_back(ParseChoice());
					if (Match(")")) {
						break;
					}
					if (!Match(",")) {
						throw InternalException("Expected ',' or ')' in argument list for call to '%s'", rule_name);
					}
				}
			}
			return make_uniq<PEGParameterizedRuleExpression>(rule_name, std::move(arguments));
		}
		if (token.type == PEGTokenType::REFERENCE) {
			pos++;
			return make_uniq<PEGRuleReferenceExpression>(token.text);
		}

		throw InternalException("Unexpected token in rule definition: %s", token.text);
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
};

class PEGParser {
public:
	PEGParser() = default;
	explicit PEGParser(const char *grammar) {
		ParseRules(grammar);

		AddRuleOverride("NumberLiteral", make_uniq<PEGNumberExpression>());
		AddRuleOverride("StringLiteral", make_uniq<PEGStringExpression>());
	}

	void AddRuleOverride(const string &rule_name, unique_ptr<PEGExpression> rule_expression) {
		PEGRule rule;
		rule.expression = std::move(rule_expression);
		rules[rule_name] = std::move(rule);
	}

	void AddRule(string &rule_name, PEGRule rule) {
		RuleParser rule_parser(rule.tokens);
		rule.expression = rule_parser.Parse();

		auto entry = rules.find(rule_name);
		if (entry != rules.end()) {
			throw InternalException("Failed to parse grammar - duplicate rule name %s", rule_name);
		}
		rules.insert(make_pair(rule_name, std::move(rule)));
	}
	void ParseRules(const char *grammar);
	case_insensitive_map_t<PEGRule> rules;
};

} // namespace duckdb
