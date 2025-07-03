#include "transformer/peg_transformer.hpp"

#include "ast/set_info.hpp"
#include "duckdb/parser/statement/set_statement.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/common/exception/binder_exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/printer.hpp"

namespace duckdb {

bool IsIdentifier(const string &pattern, const string &text) {
	return true;
}

// This function correctly dispatches to the appropriate sub-transformer.
unique_ptr<SQLStatement> PEGTransformerFactory::TransformRoot(PEGTransformer &transformer, ParseResult &parse_result) {
	auto &choice_pr = parse_result.Cast<ChoiceParseResult>();
	ParseResult *child_pr = &choice_pr.result.get();
	return transformer.Transform(child_pr->name, *child_pr);
}
unique_ptr<QualifiedName> PEGTransformerFactory::TransformQualifiedName(vector<string> &root) {
	auto result = make_uniq<QualifiedName>();
	auto children_size = root.size();
	if (children_size == 1) {
		result->catalog = INVALID_CATALOG;
		result->schema = INVALID_SCHEMA;
		result->name = root[0];
	} else if (children_size == 2) {
		result->catalog = INVALID_CATALOG;
		result->schema = root[0];
		result->name = root[1];
	} else if (children_size == 3) {
		result->catalog = root[0];
		result->schema = root[1];
		result->name = root[2];
	} else {
		throw ParserException("Unexpected number of children %d, expected 3 at most.", children_size);
	}
	return result;
}

vector<string> PEGTransformerFactory::TransformDottedIdentifier(reference<ListParseResult> root) {
	vector<string> result;
	auto first_element = root.get().Child<IdentifierParseResult>(0).identifier;
	result.push_back(root.get().Child<IdentifierParseResult>(0).identifier);
	auto sub_elements = root.get().Child<ListParseResult>(1);
	for (const auto &sub_element : sub_elements.children) {
		auto sub_list = sub_element.get().Cast<ListParseResult>();
		auto identifier = sub_list.Child<IdentifierParseResult>(1).identifier;
		result.push_back(identifier);
	}
	return result;
}

unique_ptr<SetStatement> PEGTransformerFactory::TransformUseStatement(PEGTransformer &, ListParseResult &use_target) {
	if (use_target.type != ParseResultType::LIST) {
		throw InternalException("Unknown parse result encountered in UseStatement");
	}
	auto list_pr = use_target.Cast<ListParseResult>();
	auto dotted_identifier = TransformDottedIdentifier(list_pr);
	auto qualified_name = TransformQualifiedName(dotted_identifier);
	if (!IsInvalidCatalog(qualified_name->catalog)) {
		throw ParserException("Expected \"USE database\" or \"USE database.schema\"");
	}
	string name;
	if (IsInvalidSchema(qualified_name->schema)) {
		name = KeywordHelper::WriteOptionallyQuoted(qualified_name->name, '"');
	} else {
		name = KeywordHelper::WriteOptionallyQuoted(qualified_name->schema, '"') + "." +
			   KeywordHelper::WriteOptionallyQuoted(qualified_name->name, '"');
	}
	auto name_expr = make_uniq<ConstantExpression>(Value(name));
	return make_uniq<SetVariableStatement>("schema", std::move(name_expr), SetScope::AUTOMATIC);
}

void PEGTransformerFactory::TransformSettingOrVariable(PEGTransformer& transformer, ChoiceParseResult &variable_or_setting, string &setting_name, SetScope &scope) {
	if (variable_or_setting.selected_idx == 0) {
		// Case: SetVariable <- 'VARIABLE'i Identifier
		auto &set_variable = variable_or_setting.result.get().Cast<ListParseResult>();
		setting_name = set_variable.Child<IdentifierParseResult>(1).identifier;
		scope = SetScope::VARIABLE;
	} else if (variable_or_setting.selected_idx == 1) {
		// Case: SetSetting <- Scope? Identifier
		auto &set_setting = variable_or_setting.result.get().Cast<ListParseResult>();
		auto &setting_scope_pr = set_setting.Child<OptionalParseResult>(0);

		if (setting_scope_pr.optional_result) {
			scope = transformer.TransformEnum<SetScope>(*setting_scope_pr.optional_result);
		}
		setting_name = set_setting.Child<IdentifierParseResult>(1).identifier;
	} else {
		throw ParserException("Unexpected choice in SetVariableStatement");
	}
}

SettingInfo TransformSetSetting(PEGTransformer &transformer, ParseResult &parse_result) {
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &optional_scope_pr = list_pr.Child<OptionalParseResult>(0);
	auto &identifier_pr = list_pr.Child<IdentifierParseResult>(1);

	SettingInfo result; // `result.scope` is already AUTOMATIC
	result.name = identifier_pr.identifier;

	if (optional_scope_pr.optional_result) {
		// A scope was provided, so we overwrite the default.
		result.scope = transformer.Transform<SetScope>(*optional_scope_pr.optional_result);
	}

	return result;
}

unique_ptr<SetStatement> PEGTransformerFactory::TransformSetStatement(PEGTransformer &transformer, ChoiceParseResult &choice_pr) {
    string setting_name;
    unique_ptr<ParsedExpression> value_expr;
    SetScope scope = SetScope::AUTOMATIC; // Default scope

    if (choice_pr.selected_idx == 0) {
        // This block corresponds to: StandardAssignment <- (SetVariable / SetSetting) SetAssignment
        auto &standard_assignment = choice_pr.result.get().Cast<ListParseResult>();
        auto &set_assignment = standard_assignment.Child<ListParseResult>(1);
        auto &variable_or_setting = standard_assignment.Child<ChoiceParseResult>(0);
		TransformSettingOrVariable(transformer, variable_or_setting, setting_name, scope);
        auto &value_identifier = set_assignment.Child<IdentifierParseResult>(1);
        value_expr = make_uniq<ConstantExpression>(Value(value_identifier.identifier));
    } else if (choice_pr.selected_idx == 1) {
        // Case: SetTimeZone <- 'TIME'i 'ZONE'i Expression
        throw NotImplementedException("SET TIME ZONE is not yet implemented.");
    } else {
        throw ParserException("Unexpected index selected in TransformSetStatement");
    }

    if (setting_name.empty() || !value_expr) {
        throw ParserException("Failed to parse all required parts of the SET statement.");
    }

    return make_uniq<SetVariableStatement>(setting_name, std::move(value_expr), scope);
}

unique_ptr<SetStatement> PEGTransformerFactory::TransformResetStatement(PEGTransformer &transformer, ChoiceParseResult &choice_pr) {
	string setting_name;
	SetScope scope = SetScope::AUTOMATIC;
	TransformSettingOrVariable(transformer, choice_pr, setting_name, scope);
	return make_uniq<ResetVariableStatement>(setting_name, scope);
}

PEGTransformerFactory::PEGTransformerFactory(const char *grammar) : parser(grammar) {
	Register<ListParseResult, 1>("UseStatement", &TransformUseStatement);
	Register<ChoiceParseResult, 1>("SetStatement", &TransformSetStatement);
	Register<ChoiceParseResult, 1>("ResetStatement", &TransformResetStatement);
	Register("Root", &TransformRoot); // Note index is 0

	RegisterEnum<SetScope>("SettingScope", {
	{"LocalScope", SetScope::LOCAL},
	{"SessionScope", SetScope::SESSION},
	{"GlobalScope", SetScope::GLOBAL},
	{"VariableScope", SetScope::VARIABLE}});
}

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
		if (token.type == TokenType::WORD && IsIdentifier(regex_expr.identifier, token.text)) {
			state.token_index++;
			Printer::PrintF("Found an identifier expression");
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

		unordered_map<string_t, const PEGExpression *> substitutions;
		for (const auto &param_entry : template_rule.parameters) {
			auto param_name = param_entry.first;
			const idx_t param_index = param_entry.second;

			if (param_index >= param_expr.expressions.size()) {
				throw InternalException("Parameter index out of bounds for rule '%s'", param_expr.rule_name);
			}
			substitutions[param_name] = param_expr.expressions[param_index].get();
		}

		substitution_stack.push_back(substitutions);
		ParseResult *result = MatchRule(*param_expr.expressions[0]);
		substitution_stack.pop_back();

		return result;
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

unique_ptr<SQLStatement> PEGTransformer::Transform(const string_t &rule_name, ParseResult &matched_parse_result) {
	auto it = transform_functions.find(rule_name.GetString());
	if (it == transform_functions.end()) {
		throw InternalException("No SQL transformer dispatch function found for rule '%s'", rule_name.GetString());
	}
	Printer::PrintF("Matching rule: %s", rule_name.GetString());
	return it->second(*this, matched_parse_result);
}

template<typename T>
T PEGTransformer::TransformEnum(ParseResult &parse_result) {
	// 1. The result of a rule like "A / B / C" is a ChoiceParseResult.
	auto &choice_pr = parse_result.Cast<ChoiceParseResult>();

	// 2. We get the result of the sub-rule that actually matched (e.g., LocalScope).
	auto &matched_rule_result = choice_pr.result.get();
	const string& matched_rule_name = matched_rule_result.name;

	// 3. We look up the name of the overall choice rule (e.g., "SettingScope")
	//    and then the specific sub-rule name ("LocalScope") to find the integer value.
	auto &rule_mapping = enum_mappings.at(parse_result.name);
	auto it = rule_mapping.find(matched_rule_name);
	if (it == rule_mapping.end()) {
		throw ParserException("Enum transform failed: could not map rule '%s'", matched_rule_name);
	}

	// 4. Cast the found integer back to the strong enum type and return it.
	return static_cast<>(it->second);
}

unique_ptr<SQLStatement> PEGTransformerFactory::Transform(vector<MatcherToken> &tokens, const char *root_rule) {
	ArenaAllocator allocator(Allocator::DefaultAllocator());
	PEGTransformerState state(tokens);
	for (auto &token : tokens) {
		Printer::PrintF("%s", token.text);
	}
	PEGTransformer transformer(allocator, state, sql_transform_functions, parser.rules, enum_mappings);
	ParseResult *root_parse_result = transformer.MatchRule(root_rule);
	if (!root_parse_result) {
		throw ParserException("Failed to parse string: No match found for root rule '%s'.", root_rule);
	}
	Printer::Print("Successfully parsed string");
	if (state.token_index < tokens.size()) {
		throw ParserException("Failed to parse string: Unconsumed tokens remaining.");
	}
	return transformer.Transform(root_rule, *root_parse_result);
}

} // namespace duckdb
