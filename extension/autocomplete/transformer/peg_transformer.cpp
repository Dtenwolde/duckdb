#include "transformer/peg_transformer.hpp"

#include "duckdb/parser/statement/set_statement.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/common/exception/binder_exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/printer.hpp"

namespace duckdb {


const PEGExpression *PEGTransformer::FindSubstitution(const string_t &name) {
	// Search from the inside out (most recent call)
	for (auto it = substitution_stack.rbegin(); it != substitution_stack.rend(); ++it) {
		auto &map = *it;
		auto entry = map.find(name);
		if (entry != map.end()) {
			return entry->second;
		}
	}
	return nullptr;
}


bool IsValidIdentifier(const string &text) {
	if (text.empty()) {
		return false;
	}
	if (!StringUtil::CharacterIsAlpha(text[0]) && text[0] != '_') {
		return false;
	}
	for (size_t i = 1; i < text.length(); ++i) {
		if (!StringUtil::CharacterIsAlphaNumeric(text[i]) && text[i] != '_') {
			return false;
		}
	}
	return true;
}

bool IsValidNumber(const string &text) {
	if (text.empty()) {
		return false;
	}
	for (char c : text) {
		if (!StringUtil::CharacterIsDigit(c) && c != '.' && c != '+' && c != '-') {
			return false;
		}
	}
	return true;
}

bool IsValidOperator(const string &text) {
	if (text.empty()) {
		return false;
	}
	for (auto &c : text) {
		switch (c) {
		case '+':
		case '-':
		case '*':
		case '/':
		case '%':
		case '^':
		case '<':
		case '>':
		case '=':
		case '~':
		case '!':
		case '@':
		case '&':
		case '|':
			break;
		default:
			return false;
		}
	}
	return true;
}



ParseResult *PEGTransformer::MatchRule(const PEGExpression &expression) {
	idx_t initial_token_index = state.token_index;
	Printer::PrintF("Matching rule type: %d", expression.type);
	switch (expression.type) {
	case PEGExpressionType::KEYWORD: {
		auto &keyword_expr = expression.Cast<PEGKeywordExpression>();
		if (state.token_index < state.tokens.size() &&
		    StringUtil::Lower(state.tokens[state.token_index].text) == StringUtil::Lower(keyword_expr.keyword)) {
			state.token_index++;
			return Make<KeywordParseResult>(keyword_expr.keyword);
		}
		return nullptr;
	}
	case PEGExpressionType::RULE_REFERENCE: {
		auto &rule_ref_expr = expression.Cast<PEGRuleReferenceExpression>();

		auto substitution = FindSubstitution(rule_ref_expr.rule_name);
		if (substitution) {
			// It's a parameter (like 'D' in List(D)). Match the expression that was passed as an argument.
			return MatchRule(*substitution);
		}

		auto it = grammar_rules.find(rule_ref_expr.rule_name);
		if (it == grammar_rules.end()) {
			throw InternalException("Undefined rule referenced: %s", rule_ref_expr.rule_name);
		}
		auto parse_result = MatchRule(*it->second.expression);
		if (parse_result) {
			parse_result->name = rule_ref_expr.rule_name;
		}
		return parse_result;
	}
	case PEGExpressionType::SEQUENCE: {
		auto &seq_expr = expression.Cast<PEGSequenceExpression>();
		vector<reference<ParseResult>> children_results;
		for (const auto &sub_expr_ptr : seq_expr.expressions) {
			ParseResult *child_res = MatchRule(*sub_expr_ptr);
			if (!child_res) {
				state.token_index = initial_token_index;
				return nullptr;
			}
			children_results.emplace_back(*child_res);
		}
		return Make<ListParseResult>(std::move(children_results));
	}
	case PEGExpressionType::CHOICE: {
		auto &choice_expr = expression.Cast<PEGChoiceExpression>();
		idx_t selected_option_idx = 0;
		for (const auto &sub_expr_ptr : choice_expr.expressions) {
			ParseResult *child_res = MatchRule(*sub_expr_ptr);
			if (child_res) {
				return Make<ChoiceParseResult>(*child_res, selected_option_idx);
			}
			selected_option_idx++;
		}
		return nullptr;
	}
	case PEGExpressionType::OPTIONAL: {
		auto &optional_expr = expression.Cast<PEGOptionalExpression>();
		ParseResult *child_res = MatchRule(*optional_expr.expression);
		return Make<OptionalParseResult>(child_res);
	}
	case PEGExpressionType::IDENTIFIER: {
		auto &regex_expr = expression.Cast<PEGIdentifierExpression>();
		if (state.token_index >= state.tokens.size()) {
			return nullptr;
		}
		auto &token = state.tokens[state.token_index];
		if (token.type == TokenType::WORD) {
			state.token_index++;
			Printer::PrintF("Found an identifier expression, %s", token.text.c_str());
			return Make<IdentifierParseResult>(token.text);
		}
		return nullptr;
	}
	case PEGExpressionType::ZERO_OR_MORE: {
		auto &zero_or_more_expr = expression.Cast<PEGZeroOrMoreExpression>();
		vector<reference<ParseResult>> children_results;
		while (true) {
			// Try to match the child expression.
			ParseResult *child_res = MatchRule(*zero_or_more_expr.expression);
			if (!child_res) {
				// If the child doesn't match, we are done repeating. Break the loop.
				break;
			}
			children_results.emplace_back(*child_res);
		}
		// The ZERO_OR_MORE rule always succeeds, even with zero matches.
		// We return a ListParseResult containing all the successful matches.
		return Make<ListParseResult>(std::move(children_results));
	}
	case PEGExpressionType::PARAMETERIZED_RULE: {
		auto &param_expr = expression.Cast<PEGParameterizedRuleExpression>();

		auto it = grammar_rules.find(param_expr.rule_name);
		if (it == grammar_rules.end() || it->second.parameters.empty()) {
			throw InternalException("Call to undefined or non-parameterized rule '%s'", param_expr.rule_name);
		}
		auto &template_rule = it->second;

		if (template_rule.parameters.size() != param_expr.expressions.size()) {
			throw InternalException("Argument count mismatch for rule '%s': expected %d, got %d",
									param_expr.rule_name, template_rule.parameters.size(),
									param_expr.expressions.size());
		}

		string_map_t<const PEGExpression *> substitutions;
		for (const auto &param_entry : template_rule.parameters) {
			auto param_name = param_entry.first;
			const idx_t param_index = param_entry.second;

			if (param_index >= param_expr.expressions.size()) {
				throw InternalException("Parameter index out of bounds for rule '%s'", param_expr.rule_name);
			}
			substitutions[param_name] = param_expr.expressions[param_index].get();
		}

		substitution_stack.push_back(substitutions);
		ParseResult *result = MatchRule(*template_rule.expression);
		substitution_stack.pop_back();

		return result;
	}
	case PEGExpressionType::NOT_PREDICATE: {
		auto &not_expr = expression.Cast<PEGNotPredicateExpression>();
		// Attempt to match the child expression.
		ParseResult *child_res = MatchRule(*not_expr.expression);
		// Crucially, always reset the token index. Predicates do not consume input.
		state.token_index = initial_token_index;
		// The NOT predicate succeeds if its child FAILS to match.
		if (!child_res) {
			// Return a non-null result to signal success without consuming tokens.
			// An empty list is perfect for this "successful empty match".
			return Make<ListParseResult>(vector<reference<ParseResult>>());
		}
		// The child matched, so the NOT predicate fails.
		return nullptr;
	}
	case PEGExpressionType::NUMBER: {
		if (state.token_index >= state.tokens.size()) {
			return nullptr;
		}
		auto &token = state.tokens[state.token_index];
		if (token.type == TokenType::WORD && IsValidNumber(token.text)) {
			state.token_index++;
			return Make<NumberParseResult>(token.text);
		}
		return nullptr;
	}
	default:
		throw InternalException("Unimplemented PEG expression type for matching.");
	}
}

ParseResult *PEGTransformer::MatchRule(const string_t &rule_name) {
	Printer::PrintF("Matching grammar rule: %s", rule_name.GetString());
	auto it = grammar_rules.find(rule_name.GetString());
	if (it == grammar_rules.end()) {
		throw InternalException("PEG Grammar rule '%s' not found.", rule_name.GetString());
	}
	return MatchRule(*it->second.expression);
}

template <typename T>
T PEGTransformer::Transform(ParseResult &parse_result) {
	auto it = transform_functions.find(parse_result.name);
	if (it == transform_functions.end()) {
		throw InternalException("No transformer function found for rule '%s'", parse_result.name);
	}
	auto &func = it->second;

	unique_ptr<TransformResultValue> base_result = func(*this, parse_result);
	if (!base_result) {
		throw InternalException("Transformer for rule '%s' returned a nullptr.", parse_result.name);
	}

	auto *typed_result_ptr = dynamic_cast<TypedTransformResult<T> *>(base_result.get());
	if (!typed_result_ptr) {
		throw InternalException("Transformer for rule '" + parse_result.name + "' returned an unexpected type.");
	}

	return std::move(typed_result_ptr->value);
}

template<typename T>
T PEGTransformer::TransformEnum(ParseResult &parse_result) {
	string_t result;
	if (parse_result.type == ParseResultType::CHOICE) {
		auto &choice_pr = parse_result.Cast<ChoiceParseResult>();
		auto &matched_rule_result = choice_pr.result.get();
		result = matched_rule_result.name;
	} else if (parse_result.type == ParseResultType::KEYWORD) {
		auto &keyword_pr = parse_result.Cast<KeywordParseResult>();
		result = keyword_pr.name;
	}

	auto &rule_mapping = enum_mappings.at(parse_result.name);
	auto it = rule_mapping.find(result);
	if (it == rule_mapping.end()) {
		throw ParserException("Enum transform failed: could not map rule '%s'", result.GetString());
	}

	// 4. Cast the found integer back to the strong enum type and return it.
	return static_cast<T>(it->second);
}


} // namespace duckdb
