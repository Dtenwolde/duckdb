

#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/parser/expression/comparison_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/operator_expression.hpp"
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

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformExpression(PEGTransformer &transformer, ParseResult &parse_result) {
	// Rule: BaseExpression RecursiveExpression*
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &base_expr_pr = list_pr.children[0].get();
	auto &recursion_list_pr = list_pr.Child<ListParseResult>(1);

	// First, transform the base of the expression (e.g., the initial column or literal).
	unique_ptr<ParsedExpression> current_expr = transformer.Transform<unique_ptr<ParsedExpression>>(base_expr_pr);

	// Now, loop through the recursive parts and wrap the expression.
	for (auto &recursive_expr_ref : recursion_list_pr.children) {
		// The recursive_expr_ref is a ParseResult for the rule "RecursiveExpression",
		// which has the structure: Operator Expression
		auto &recursive_list = recursive_expr_ref.get().Cast<ListParseResult>();
		auto &operator_pr = recursive_list.children[0].get();
		auto &right_expr_pr = recursive_list.children[1].get();

		// Delegate to transform the right-hand side and the operator.
		unique_ptr<ParsedExpression> right_expr = transformer.Transform<unique_ptr<ParsedExpression>>(right_expr_pr);
		string op_str = transformer.Transform<string>(operator_pr);

		// Use the helper to compose the new binary expression.
		// The result becomes the new "left" side for the next iteration.
		current_expr = CreateBinaryExpression(std::move(op_str), std::move(current_expr), std::move(right_expr));
	}

	return current_expr;
}

// 2. The transformer for the base of an expression
unique_ptr<ParsedExpression> PEGTransformerFactory::TransformBaseExpression(PEGTransformer &transformer, ParseResult &parse_result) {
    // Rule: SingleExpression Indirection*
    // For now, we will just transform the SingleExpression and ignore indirections.
    auto &list_pr = parse_result.Cast<ListParseResult>();
    auto &single_expr_pr = list_pr.children[0].get();

    return transformer.Transform<unique_ptr<ParsedExpression>>(single_expr_pr);
}

// 3. The dispatcher for the different types of single expressions
unique_ptr<ParsedExpression> PEGTransformerFactory::TransformSingleExpression(PEGTransformer &transformer, ParseResult &parse_result) {
    // This is a choice rule, so we unwrap the ChoiceParseResult and delegate.
    auto &choice_pr = parse_result.Cast<ChoiceParseResult>();
    auto &matched_child = choice_pr.result.get();

    return transformer.Transform<unique_ptr<ParsedExpression>>(matched_child);
}

// 4. A "Leaf" transformer for a specific expression type
unique_ptr<ParsedExpression> PEGTransformerFactory::TransformLiteralExpression(PEGTransformer &transformer, ParseResult &parse_result) {
	// Rule: StringLiteral / NumberLiteral / 'NULL'i / 'TRUE'i / 'FALSE'i
	auto &choice_pr = parse_result.Cast<ChoiceParseResult>();
	auto &matched_rule_result = choice_pr.result.get();

	if (matched_rule_result.name == "StringLiteral") {
		auto &literal_pr = matched_rule_result.Cast<IdentifierParseResult>();
		return make_uniq<ConstantExpression>(Value(literal_pr.identifier));
	}
	if (matched_rule_result.name == "NumberLiteral") {
		auto &literal_pr = matched_rule_result.Cast<NumberParseResult>();
		// todo(dtenwolde): handle decimals, etc.
		return make_uniq<ConstantExpression>(Value::BIGINT(std::stoll(literal_pr.number)));
	}
	if (matched_rule_result.name == "ConstantLiteral") {
		auto &constant_literal_pr = matched_rule_result.Cast<ChoiceParseResult>();
		return make_uniq<ConstantExpression>(transformer.TransformEnum<Value>(constant_literal_pr));
	}
	throw ParserException("Unrecognized literal type in TransformLiteralExpression");
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformColumnReference(PEGTransformer &transformer, ParseResult &parse_result) {
    // This would transform a DottedIdentifier into a ColumnRefExpression
    QualifiedName qn = transformer.Transform<QualifiedName>(parse_result);
    vector<string> column_name_parts;
    if (!qn.catalog.empty()) {
        column_name_parts.push_back(qn.catalog);
    }
    if (!qn.schema.empty()) {
        column_name_parts.push_back(qn.schema);
    }
    column_name_parts.push_back(qn.name);
    return make_uniq<ColumnRefExpression>(column_name_parts);
}

// This helper is needed to get the string value of an operator
string PEGTransformerFactory::TransformOperator(PEGTransformer &transformer, ParseResult &parse_result) {
    // The operator rule is a choice. We need to unwrap it and get the text.
    auto &choice_pr = parse_result.Cast<ChoiceParseResult>();
    auto &matched_child = choice_pr.result.get();

    // The leaf nodes of the operator grammar are KeywordParseResults
    return matched_child.Cast<KeywordParseResult>().keyword;
}
}