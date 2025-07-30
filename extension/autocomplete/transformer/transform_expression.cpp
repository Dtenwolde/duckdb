

#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/parser/expression/comparison_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/operator_expression.hpp"
#include "transformer/parse_result.hpp"

namespace duckdb {

unique_ptr<ParsedExpression> CreateBinaryExpression(string op, unique_ptr<ParsedExpression> left,
                                                    unique_ptr<ParsedExpression> right) {
	vector<unique_ptr<ParsedExpression>> children;
	children.push_back(std::move(left));
	children.push_back(std::move(right));

	auto op_type = OperatorToExpressionType(op);
	if (op_type != ExpressionType::INVALID) {
		return make_uniq<ComparisonExpression>(op_type, std::move(children[0]), std::move(children[1]));
	}

	auto result = make_uniq<FunctionExpression>(std::move(op), std::move(children));
	result->is_operator = true;
	return std::move(result);
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformExpression(PEGTransformer &transformer,
                                                                        optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &base_expr_pr = list_pr.Child<ListParseResult>(0);
	unique_ptr<ParsedExpression> current_expr = transformer.Transform<unique_ptr<ParsedExpression>>(base_expr_pr);
	auto &indirection_pr = list_pr.Child<OptionalParseResult>(1);
	if (indirection_pr.optional_result) {
		auto indirection_expr = transformer.Transform<unique_ptr<ParsedExpression>>(indirection_pr.optional_result);
	}

	return current_expr;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformBaseExpression(PEGTransformer &transformer,
                                                                            optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &single_expr_pr = list_pr.children[0];

	return transformer.Transform<unique_ptr<ParsedExpression>>(single_expr_pr);
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformSingleExpression(PEGTransformer &transformer,
                                                                              optional_ptr<ParseResult> parse_result) {
	// This is a choice rule, so we unwrap the ChoiceParseResult and delegate.
	auto &choice_pr = parse_result->Cast<ListParseResult>();
	auto &expression = choice_pr.Child<ChoiceParseResult>(0);

	return transformer.Transform<unique_ptr<ParsedExpression>>(expression);
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformLiteralExpression(PEGTransformer &transformer,
                                                                               optional_ptr<ParseResult> parse_result) {
	// Rule: StringLiteral / NumberLiteral / 'NULL'i / 'TRUE'i / 'FALSE'i
	auto &choice_pr = parse_result->Cast<ChoiceParseResult>();
	auto &choice_result = choice_pr.result->Cast<ListParseResult>();
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
	// TODO(dtenwolde) figure out how this rule works with all the options
	auto &choice_pr = parse_result->Cast<ChoiceParseResult>();
	auto &list_pr = choice_pr.result->Cast<ListParseResult>();
	auto &col_ref = list_pr.Child<ChoiceParseResult>(0);
	vector<string> column_name_parts;
	column_name_parts.push_back(col_ref.result->Cast<IdentifierParseResult>().identifier);
	return make_uniq<ColumnRefExpression>(column_name_parts);
}

string PEGTransformerFactory::TransformOperator(PEGTransformer &, optional_ptr<ParseResult> parse_result) {
	auto &choice_pr = parse_result->Cast<ChoiceParseResult>();
	auto &matched_child = choice_pr.result;
	return matched_child->Cast<KeywordParseResult>().keyword;
}
} // namespace duckdb
