#pragma once

#include "duckdb/common/string_map_set.hpp"
#include "duckdb/common/types/string_type.hpp"
#include "duckdb/common/case_insensitive_map.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/vector.hpp"

namespace duckdb {

enum class PEGExpressionType {
    KEYWORD,
    RULE_REFERENCE,
    SEQUENCE,
    CHOICE,
    OPTIONAL,
    ZERO_OR_MORE,
    ONE_OR_MORE,
    AND_PREDICATE, // &
    NOT_PREDICATE,  // !
	IDENTIFIER
};

struct PEGExpression {
    explicit PEGExpression(PEGExpressionType type) : type(type) {}
    virtual ~PEGExpression() = default;

    template <class TARGET>
    TARGET &Cast() { return reinterpret_cast<TARGET &>(*this); }

	template <class TARGET>
		const TARGET &Cast() const { return reinterpret_cast<const TARGET &>(*this); }

    PEGExpressionType type;
};

struct PEGKeywordExpression : PEGExpression {
    explicit PEGKeywordExpression(string keyword_p)
        : PEGExpression(PEGExpressionType::KEYWORD), keyword(std::move(keyword_p)) {}
    string keyword;
};

struct PEGRuleReferenceExpression : PEGExpression {
    explicit PEGRuleReferenceExpression(string rule_name_p)
        : PEGExpression(PEGExpressionType::RULE_REFERENCE), rule_name(std::move(rule_name_p)) {}
    string rule_name;
};

struct PEGSequenceExpression : PEGExpression {
    explicit PEGSequenceExpression(vector<unique_ptr<PEGExpression>> children_p)
        : PEGExpression(PEGExpressionType::SEQUENCE), expressions(std::move(children_p)) {}
    vector<unique_ptr<PEGExpression>> expressions;
};

struct PEGChoiceExpression : PEGExpression {
    explicit PEGChoiceExpression(vector<unique_ptr<PEGExpression>> children_p)
        : PEGExpression(PEGExpressionType::CHOICE), expressions(std::move(children_p)) {}
    vector<unique_ptr<PEGExpression>> expressions;
};

struct PEGOptionalExpression : PEGExpression {
    explicit PEGOptionalExpression(unique_ptr<PEGExpression> child_p)
        : PEGExpression(PEGExpressionType::OPTIONAL), expression(std::move(child_p)) {}
    unique_ptr<PEGExpression> expression;
};

struct PEGZeroOrMoreExpression : PEGExpression {
    explicit PEGZeroOrMoreExpression(unique_ptr<PEGExpression> child_p)
        : PEGExpression(PEGExpressionType::ZERO_OR_MORE), expression(std::move(child_p)) {}
    unique_ptr<PEGExpression> expression;
};

struct PEGOneOrMoreExpression : PEGExpression {
    explicit PEGOneOrMoreExpression(unique_ptr<PEGExpression> child_p)
        : PEGExpression(PEGExpressionType::ONE_OR_MORE), expression(std::move(child_p)) {}
    unique_ptr<PEGExpression> expression;
};

struct PEGAndPredicateExpression : PEGExpression {
	explicit PEGAndPredicateExpression(unique_ptr<PEGExpression> child_p)
	    : PEGExpression(PEGExpressionType::AND_PREDICATE), expression(std::move(child_p)) {}
	unique_ptr<PEGExpression> expression;
};

struct PEGNotPredicateExpression : PEGExpression {
	explicit PEGNotPredicateExpression(unique_ptr<PEGExpression> child_p)
	    : PEGExpression(PEGExpressionType::NOT_PREDICATE), expression(std::move(child_p)) {}
	unique_ptr<PEGExpression> expression;
};

struct PEGIdentifierExpression : PEGExpression {
	explicit PEGIdentifierExpression(string pattern_p)
		: PEGExpression(PEGExpressionType::IDENTIFIER), identifier(std::move(pattern_p)) {}
	string identifier;
};


// --- PEG Parser ---

enum class PEGTokenType { LITERAL, REFERENCE, OPERATOR, IDENTIFIER };

struct PEGToken {
    PEGTokenType type;
    string text;
};

struct PEGRule {
	vector<string> parameters;
    unique_ptr<PEGExpression> expression;

	vector<PEGToken> raw_tokens;
};

class PEGParser {
public:
    PEGParser() = default;
    explicit PEGParser(const char *grammar) {
        ParseRules(grammar);
    }
    void ParseRules(const char *grammar);
    case_insensitive_map_t<PEGRule> rules;
};

} // namespace duckdb
