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

unique_ptr<SQLStatement> PEGTransformerFactory::TransformRoot(PEGTransformer &transformer, ParseResult &parse_result) {
	auto &choice_pr = parse_result.Cast<ChoiceParseResult>();
	return transformer.Transform<unique_ptr<SQLStatement>>(choice_pr.result.get());
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformUseStatement(PEGTransformer &transformer, ParseResult &parse_result) {
	// Rule: 'USE'i UseTarget
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &use_target_pr = list_pr.children[1].get();

	// Delegate the transformation of the DottedIdentifier to its own function
	QualifiedName qn = transformer.Transform<QualifiedName>(use_target_pr);

	// Build the final SetStatement from the structured QualifiedName
	if (!qn.catalog.empty()) {
		throw ParserException("Expected \"USE database\" or \"USE database.schema\"");
	}

	string value_str;
	if (qn.schema.empty()) {
		// Case: USE database
		value_str = KeywordHelper::WriteOptionallyQuoted(qn.name, '"');
	} else {
		// Case: USE database.schema
		value_str = KeywordHelper::WriteOptionallyQuoted(qn.schema, '"') + "." +
					KeywordHelper::WriteOptionallyQuoted(qn.name, '"');
	}

	auto value_expr = make_uniq<ConstantExpression>(Value(value_str));
	return make_uniq<SetVariableStatement>("schema", std::move(value_expr), SetScope::AUTOMATIC);
}

QualifiedName PEGTransformerFactory::TransformDottedIdentifier(PEGTransformer &, ParseResult &parse_result) {
	// Rule: Identifier ('.' Identifier)*
	auto &list_pr = parse_result.Cast<ListParseResult>();
	vector<string> parts;

	// Add the first identifier
	parts.push_back(list_pr.Child<IdentifierParseResult>(0).identifier);

	// Add the rest of the identifiers from the optional repetition
	auto &repetition_list = list_pr.Child<ListParseResult>(1);
	for (auto &child_ref : repetition_list.children) {
		// Each child in the repetition is a ListParseResult from the sequence "'.' Identifier"
		auto &sub_list = child_ref.get().Cast<ListParseResult>();
		parts.push_back(sub_list.Child<IdentifierParseResult>(1).identifier);
	}

	QualifiedName result;
	if (parts.size() == 1) {
		result.name = parts[0];
	} else if (parts.size() == 2) {
		result.schema = parts[0];
		result.name = parts[1];
	} else if (parts.size() == 3) {
		result.catalog = parts[0];
		result.schema = parts[1];
		result.name = parts[2];
	} else if (parts.size() > 3) {
		throw ParserException("Too many parts in identifier, expected a maximum of 3 (catalog.schema.name)");
	}
	return result;
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformSetStatement(PEGTransformer &transformer, ParseResult &parse_result) {
	// Dispatcher: 'SET' (StandardAssignment / SetTimeZone)
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(1);
	auto &matched_child = choice_pr.result.get();

	if (matched_child.name == "StandardAssignment") {
		return transformer.Transform<unique_ptr<SQLStatement>>(matched_child);
	}
	// Handle SetTimeZone
	throw NotImplementedException("SET TIME ZONE is not yet implemented.");
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformResetStatement(PEGTransformer &transformer, ParseResult &parse_result) {
	// Composer: 'RESET' (SetVariable / SetSetting)
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &child_pr = list_pr.children[1].get().Cast<ChoiceParseResult>();

	// Delegate to get the setting info, then create the Reset statement
	SettingInfo setting_info = transformer.Transform<SettingInfo>(child_pr.result);
	return make_uniq<ResetVariableStatement>(setting_info.name, setting_info.scope);
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformDeleteStatement(PEGTransformer &transformer, ParseResult &parse_result) {
	throw NotImplementedException("DELETE statement not implemented.");
}

// --- Intermediate and Semantic Value Transformers ---

unique_ptr<SQLStatement> PEGTransformerFactory::TransformStandardAssignment(PEGTransformer &transformer, ParseResult &parse_result) {
	// Composer: (SetVariable / SetSetting) SetAssignment
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &setting_or_var_pr = list_pr.children[0].get().Cast<ChoiceParseResult>();
	auto &set_assignment_pr = list_pr.children[1];

	// Delegate to get the parts
	SettingInfo setting_info = transformer.Transform<SettingInfo>(setting_or_var_pr.result);
	unique_ptr<ParsedExpression> value = transformer.Transform<unique_ptr<ParsedExpression>>(set_assignment_pr);

	// Compose the final result
	return make_uniq<SetVariableStatement>(setting_info.name, std::move(value), setting_info.scope);
}

SettingInfo PEGTransformerFactory::TransformSettingOrVariable(PEGTransformer &transformer, ParseResult &parse_result) {
	// Dispatcher: SetVariable / SetSetting
	auto &choice_pr = parse_result.Cast<ChoiceParseResult>();
	auto &matched_child = choice_pr.result.get();
	return transformer.Transform<SettingInfo>(matched_child);
}

SettingInfo PEGTransformerFactory::TransformSetSetting(PEGTransformer &transformer, ParseResult &parse_result) {
	// Leaf: SettingScope? SettingName
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &optional_scope_pr = list_pr.Child<OptionalParseResult>(0);
	auto &name_pr = list_pr.Child<IdentifierParseResult>(1);

	SettingInfo result;
	result.name = name_pr.identifier;
	if (optional_scope_pr.optional_result) {
		result.scope = transformer.TransformEnum<SetScope>(*optional_scope_pr.optional_result);
	}
	return result;
}

SettingInfo PEGTransformerFactory::TransformSetVariable(PEGTransformer &transformer, ParseResult &parse_result) {
	// Leaf: VariableScope SettingName
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &scope_pr = list_pr.children[0];
	auto &name_pr = list_pr.Child<IdentifierParseResult>(1);

	SettingInfo result;
	result.name = name_pr.identifier;
	result.scope = transformer.TransformEnum<SetScope>(scope_pr);
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformSetAssignment(PEGTransformer &transformer, ParseResult &parse_result) {
	// Dispatcher: VariableAssign VariableList
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &variable_list_pr = list_pr.children[1];
	return transformer.Transform<unique_ptr<ParsedExpression>>(variable_list_pr);
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformVariableList(PEGTransformer &transformer, ParseResult &parse_result) {
	// For now, we assume VariableList -> List(Expression) and Expression -> Identifier
	// This will just transform the first expression in the list.
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &expression_pr = list_pr.children[0];
	return transformer.Transform<unique_ptr<ParsedExpression>>(expression_pr);
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformExpression(PEGTransformer &transformer, ParseResult &parse_result) {
	// Leaf: Identifier
	auto &identifier_pr = parse_result.Cast<IdentifierParseResult>();
	return make_uniq<ConstantExpression>(Value(identifier_pr.identifier));
}


PEGTransformerFactory::PEGTransformerFactory(const char *grammar) : parser(grammar) {
	// Register all transform functions with their expected return types
	Register("Root", &TransformRoot);
	Register("UseStatement", &TransformUseStatement);
	Register("DottedIdentifier", &TransformDottedIdentifier);
	Register("SetStatement", &TransformSetStatement);
	Register("ResetStatement", &TransformResetStatement);
	Register("DeleteStatement", &TransformDeleteStatement);

	// Intermediate composers
	Register("StandardAssignment", &TransformStandardAssignment);
	Register("SetAssignment", &TransformSetAssignment);

	// Dispatchers that return semantic structs/values
	Register("SettingOrVariable", &TransformSettingOrVariable);
	Register("VariableList", &TransformVariableList);
	Register("Expression", &TransformExpression);

	// Leaf transformers that return semantic structs/values
	Register("SetSetting", &TransformSetSetting);
	Register("SetVariable", &TransformSetVariable);

	// Enum registration
	RegisterEnum<SetScope>("SettingScope",
						   {{"LocalScope", SetScope::LOCAL},
							{"SessionScope", SetScope::SESSION},
							{"GlobalScope", SetScope::GLOBAL}
						   });
	RegisterEnum<SetScope>("VariableScope", {{"VariableScope", SetScope::VARIABLE}});
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
	// 1. Look up the type-erased function from the map using the rule's name.
	auto it = transform_functions.find(parse_result.name);
	if (it == transform_functions.end()) {
		throw InternalException("No transformer function found for rule '%s'", parse_result.name);
	}
	auto &func = it->second;

	// 2. Call the function, which returns a unique_ptr to the generic base class wrapper.
	unique_ptr<TransformResultValue> base_result = func(*this, parse_result);
	if (!base_result) {
		throw InternalException("Transformer for rule '%s' returned a nullptr.", parse_result.name);
	}

	// 3. Use dynamic_cast to safely downcast the generic pointer to the specific
	//    wrapper type we expect (e.g., TypedTransformResult<SettingInfo>).
	//    dynamic_cast is the C++11 way to do this safely; it returns nullptr on failure.
	auto *typed_result_ptr = dynamic_cast<TypedTransformResult<T> *>(base_result.get());
	if (!typed_result_ptr) {
		throw InternalException("Transformer for rule '" + parse_result.name + "' returned an unexpected type.");
	}

	// 4. Move the strongly-typed value out of the wrapper and return it.
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

	// 2. We get the result of the sub-rule that actually matched (e.g., LocalScope).

	// 3. We look up the name of the overall choice rule (e.g., "SettingScope")
	//    and then the specific sub-rule name ("LocalScope") to find the integer value.
	auto &rule_mapping = enum_mappings.at(parse_result.name);
	auto it = rule_mapping.find(result);
	if (it == rule_mapping.end()) {
		throw ParserException("Enum transform failed: could not map rule '%s'", result.GetString());
	}

	// 4. Cast the found integer back to the strong enum type and return it.
	return static_cast<T>(it->second);
}

unique_ptr<SQLStatement> PEGTransformerFactory::Transform(vector<MatcherToken> &tokens, const char *root_rule) {
	ArenaAllocator allocator(Allocator::DefaultAllocator());
	PEGTransformerState state(tokens);
	string token_stream;
	for (auto &token : tokens) {
		token_stream += token.text + " ";
	}
	Printer::PrintF("Tokens: %s", token_stream.c_str());
	PEGTransformer transformer(allocator, state, sql_transform_functions, parser.rules, enum_mappings);
	ParseResult *root_parse_result = transformer.MatchRule(root_rule);
	if (!root_parse_result) {
		throw ParserException("Failed to parse string: No match found for root rule '%s'.", root_rule);
	}
	Printer::Print("Successfully parsed string");
	if (state.token_index < tokens.size()) {
		throw ParserException("Failed to parse string: Unconsumed tokens remaining.");
	}
	root_parse_result->name = root_rule;
	return transformer.Transform<unique_ptr<SQLStatement>>(*root_parse_result);
}

} // namespace duckdb
