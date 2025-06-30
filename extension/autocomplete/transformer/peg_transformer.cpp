#include "transformer/peg_transformer.hpp"
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
	auto &list_pr = parse_result.Cast<ChoiceParseResult>();
	ParseResult *child_pr = &list_pr.result.get();
	return transformer.Transform(child_pr->name, *child_pr);
}
unique_ptr<QualifiedName> PEGTransformerFactory::TransformQualifiedName(reference<ListParseResult> root) {
	auto result = make_uniq<QualifiedName>();
	auto children_size = root.get().children.size();
	if (children_size == 1) {
		result->catalog = INVALID_CATALOG;
		result->schema = INVALID_SCHEMA;
		result->name = root.get().Child<IdentifierParseResult>(0).identifier;
	} else if (children_size == 3) {
		result->catalog = INVALID_CATALOG;
		result->schema = root.get().Child<IdentifierParseResult>(0).identifier;
		result->name = root.get().Child<IdentifierParseResult>(2).identifier;
	} else if (children_size == 5) {
		result->catalog = root.get().Child<IdentifierParseResult>(0).identifier;
		result->schema = root.get().Child<IdentifierParseResult>(2).identifier;
		result->name = root.get().Child<IdentifierParseResult>(4).identifier;
	} else {
		throw ParserException("Unexpected number of children %d, expected 3 at most.", children_size);
	}
	return result;
}

unique_ptr<SetStatement> PEGTransformerFactory::TransformUseStatement(PEGTransformer &, ChoiceParseResult &use_target) {
	ParseResult &use_target_result = use_target.result.get();

	if (use_target_result.type == ParseResultType::LIST) {
		auto list_pr = use_target_result.Cast<ListParseResult>();
		auto qualified_name = TransformQualifiedName(list_pr);
		if (IsInvalidCatalog(qualified_name->catalog) && IsInvalidSchema(qualified_name->schema)) {
			throw ParserException("Expected \"USE database\" or \"USE database.schema\"");
		}
		string name;
		if (IsInvalidSchema(qualified_name->name)) {
			name = KeywordHelper::WriteOptionallyQuoted(qualified_name->name, '"');
		} else {
			name = KeywordHelper::WriteOptionallyQuoted(qualified_name->schema, '"') + "." +
				   KeywordHelper::WriteOptionallyQuoted(qualified_name->name, '"');
		}
		auto name_expr = make_uniq<ConstantExpression>(Value(name));
		return make_uniq<SetVariableStatement>("schema", std::move(name_expr), SetScope::AUTOMATIC);
	}
	if (use_target_result.type == ParseResultType::IDENTIFIER) {
		// Matched SchemaName
		auto &identifier = use_target_result.Cast<IdentifierParseResult>();
		auto schema = identifier.identifier;
		return make_uniq<SetVariableStatement>("schema", make_uniq<ConstantExpression>(Value(schema)), SetScope::AUTOMATIC);
	}
	throw ParserException("Unknown parse result encountered in UseStatement");
}

PEGTransformerFactory::PEGTransformerFactory(const char *grammar) : parser(grammar) {
	Register<ChoiceParseResult, 1>("UseStatement", &TransformUseStatement);
	Register("Root", &TransformRoot); // Note index is 0
}

ParseResult *PEGTransformer::MatchRule(const PEGExpression &expression) {
	idx_t initial_token_index = state.token_index;

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
	default:
		throw InternalException("Unimplemented PEG expression type for matching.");
	}
}

ParseResult *PEGTransformer::MatchRule(const string_t &rule_name) {
	auto it = grammar_rules.find(rule_name.GetString());
	if (it == grammar_rules.end()) {
		throw InternalException("PEG Grammar rule '%s' not found.", rule_name.GetString());
	}
	Printer::PrintF("Matching grammar rule: %s", rule_name.GetString());
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

unique_ptr<SQLStatement> PEGTransformerFactory::Transform(vector<MatcherToken> &tokens, const char *root_rule) {
	ArenaAllocator allocator(Allocator::DefaultAllocator());
	PEGTransformerState state(tokens);
	for (auto &token : tokens) {
		Printer::PrintF("%s", token.text);
	}
	PEGTransformer transformer(allocator, state, sql_transform_functions, parser.rules);
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
