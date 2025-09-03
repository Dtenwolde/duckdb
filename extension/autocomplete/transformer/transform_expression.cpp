#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/parser/expression/comparison_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/operator_expression.hpp"
#include "transformer/parse_result.hpp"

namespace duckdb {

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformExpression(PEGTransformer &transformer,
                                                                        optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &base_expr_pr = list_pr.Child<ListParseResult>(0);
	unique_ptr<ParsedExpression> current_expr = transformer.Transform<unique_ptr<ParsedExpression>>(base_expr_pr);
	auto &indirection_pr = list_pr.Child<OptionalParseResult>(1);
	if (indirection_pr.HasResult()) {
		auto repeat_expression_pr = indirection_pr.optional_result->Cast<RepeatParseResult>();
		vector<unique_ptr<ParsedExpression>> expr_children;
		for (auto &child : repeat_expression_pr.children) {
			auto operator_expr =
			    std::move(transformer.Transform<unique_ptr<ParsedExpression>>(child)->Cast<OperatorExpression>());
			current_expr = make_uniq<OperatorExpression>(operator_expr.type, std::move(current_expr),
			                                             std::move(operator_expr.children[0]));
		}
	}

	return current_expr;
}

unique_ptr<ParsedExpression>
PEGTransformerFactory::TransformRecursiveExpression(PEGTransformer &transformer,
                                                    optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto operator_expr = transformer.Transform<ExpressionType>(list_pr.Child<ListParseResult>(0));
	vector<unique_ptr<ParsedExpression>> expr_children;
	expr_children.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(1)));
	return make_uniq<OperatorExpression>(operator_expr, std::move(expr_children));
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformBaseExpression(PEGTransformer &transformer,
                                                                            optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &single_expr_pr = list_pr.children[0];

	return transformer.Transform<unique_ptr<ParsedExpression>>(single_expr_pr);
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformSingleExpression(PEGTransformer &transformer,
                                                                              optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &expression = list_pr.Child<ChoiceParseResult>(0);

	return transformer.Transform<unique_ptr<ParsedExpression>>(expression.result);
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformPrefixExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto prefix = transformer.Transform<string>(list_pr.Child<ListParseResult>(0));
	auto expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(1));
	if (prefix == "NOT") {
		return make_uniq<OperatorExpression>(ExpressionType::OPERATOR_NOT, std::move(expr));
	}
	vector<unique_ptr<ParsedExpression>> expr_children;
	expr_children.push_back(std::move(expr));
	auto func_expr = make_uniq<FunctionExpression>(prefix, std::move(expr_children));
	func_expr->is_operator = true;
	return func_expr;
}

string PEGTransformerFactory::TransformPrefixOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto choice_pr = list_pr.Child<ChoiceParseResult>(0);
	return transformer.TransformEnum<string>(choice_pr.result);
}

unique_ptr<ParsedExpression>
PEGTransformerFactory::TransformParenthesisExpression(PEGTransformer &transformer,
                                                      optional_ptr<ParseResult> parse_result) {
	// ParenthesisExpression <- Parens(List(Expression))
	vector<unique_ptr<ParsedExpression>> children;

	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto expressions = ExtractParseResultsFromList(ExtractResultFromParens(list_pr.Child<ListParseResult>(0)));

	for (auto &expression : expressions) {
		children.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(expression));
	}
	return make_uniq<OperatorExpression>(ExpressionType::PLACEHOLDER, std::move(children));
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformLiteralExpression(PEGTransformer &transformer,
                                                                               optional_ptr<ParseResult> parse_result) {
	// Rule: StringLiteral / NumberLiteral / 'NULL'i / 'TRUE'i / 'FALSE'i
	auto &choice_result = parse_result->Cast<ListParseResult>();
	auto &matched_rule_result = choice_result.Child<ChoiceParseResult>(0);

	if (matched_rule_result.name == "StringLiteral") {
		auto &literal_pr = matched_rule_result.result->Cast<StringLiteralParseResult>();
		return make_uniq<ConstantExpression>(Value(literal_pr.result));
	}
	if (matched_rule_result.name == "NumberLiteral") {
		auto &literal_pr = matched_rule_result.result->Cast<NumberParseResult>();
		// todo(dtenwolde): handle decimals, etc.
		return make_uniq<ConstantExpression>(Value::BIGINT(std::stoll(literal_pr.number)));
	}
	if (matched_rule_result.name == "ConstantLiteral") {
		auto &list_pr = matched_rule_result.result->Cast<ListParseResult>();
		auto &constant_literal = list_pr.Child<ChoiceParseResult>(0);
		return make_uniq<ConstantExpression>(transformer.TransformEnum<Value>(constant_literal.result));
	}
	throw ParserException("Unrecognized literal type in TransformLiteralExpression");
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformColumnReference(PEGTransformer &transformer,
                                                                             optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto identifiers = transformer.Transform<vector<string>>(list_pr.Child<ListParseResult>(0));
	return make_uniq<ColumnRefExpression>(std::move(identifiers));
}

} // namespace duckdb
