#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/parser/expression/comparison_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/operator_expression.hpp"
#include "transformer/parse_result.hpp"
#include "duckdb/parser/expression/cast_expression.hpp"

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

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformCastExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	bool try_cast = transformer.Transform<bool>(list_pr.Child<ListParseResult>(0));
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(1))->Cast<ListParseResult>();
	auto expr = transformer.Transform<unique_ptr<ParsedExpression>>(extract_parens.Child<ListParseResult>(0));
	auto type = transformer.Transform<LogicalType>(extract_parens.Child<ListParseResult>(2));
	return make_uniq<CastExpression>(type, std::move(expr), try_cast);
}

bool PEGTransformerFactory::TransformCastOrTryCast(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto choice_pr = list_pr.Child<ChoiceParseResult>(0);
	return StringUtil::Lower(choice_pr.result->Cast<KeywordParseResult>().keyword) == "try_cast";
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformFunctionExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	// TODO(Dtenwolde) Not everything here is used yet.
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto qualified_function = transformer.Transform<QualifiedName>(list_pr.Child<ListParseResult>(0));
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(1))->Cast<ListParseResult>();
	auto distinct_or_all_opt = extract_parens.Child<OptionalParseResult>(0);
	if (distinct_or_all_opt.HasResult()) {
		// TODO(Dtenwolde)
		throw NotImplementedException("Distinct or All has not yet been implemented");
	}
	auto function_arg_opt = extract_parens.Child<OptionalParseResult>(1);
	vector<unique_ptr<ParsedExpression>> function_children;
	if (function_arg_opt.HasResult()) {
		auto function_argument_list = ExtractParseResultsFromList(function_arg_opt.optional_result);
		for (auto function_argument : function_argument_list) {
			function_children.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(function_argument));
		}
	}
	auto order_by_opt = extract_parens.Child<OptionalParseResult>(2);
	if (order_by_opt.HasResult()) {
		throw NotImplementedException("Order by has not yet been implemented");
	}
	auto ignore_nulls_opt = extract_parens.Child<OptionalParseResult>(3);
	if (ignore_nulls_opt.HasResult()) {
		throw NotImplementedException("Ignore nulls has not yet been implemented");
	}
	auto within_group_opt = list_pr.Child<OptionalParseResult>(2);
	if (within_group_opt.HasResult()) {
		throw NotImplementedException("Within group has not yet been implemented");
	}
	auto filter_opt = list_pr.Child<OptionalParseResult>(3);
	if (filter_opt.HasResult()) {
		throw NotImplementedException("Filter has not yet been implemented");
	}
	auto export_opt = list_pr.Child<OptionalParseResult>(4);
	if (export_opt.HasResult()) {
		throw NotImplementedException("Export has not yet been implemented");
	}
	auto over_opt = list_pr.Child<OptionalParseResult>(5);
	if (over_opt.HasResult()) {
		throw NotImplementedException("Over has not yet been implemented");
	}

	auto result = make_uniq<FunctionExpression>(qualified_function.catalog,
		qualified_function.schema,
		qualified_function.name,
		std::move(function_children));

	return result;
}

QualifiedName PEGTransformerFactory::TransformFunctionIdentifier(PEGTransformer &transformer,
                                                                 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto choice_pr = list_pr.Child<ChoiceParseResult>(0);
	if (choice_pr.result->type == ParseResultType::IDENTIFIER) {
		QualifiedName result;
		result.catalog = INVALID_CATALOG;
		result.schema = INVALID_SCHEMA;
		result.name = choice_pr.result->Cast<IdentifierParseResult>().identifier;
		return result;
	}
	return transformer.Transform<QualifiedName>(list_pr.Child<ChoiceParseResult>(0).result);
}

QualifiedName PEGTransformerFactory::TransformCatalogReservedSchemaFunctionName(PEGTransformer &transformer,
																 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	QualifiedName result;
	result.catalog = transformer.Transform<string>(list_pr.Child<ListParseResult>(0));
	auto schema_opt = list_pr.Child<OptionalParseResult>(1);
	if (schema_opt.HasResult()) {
		result.schema = transformer.Transform<string>(schema_opt.optional_result);
	} else {
		result.schema = INVALID_SCHEMA;
	}
	result.name = transformer.Transform<string>(list_pr.Child<ListParseResult>(2));
	return result;
}

QualifiedName PEGTransformerFactory::TransformSchemaReservedFunctionName(PEGTransformer &transformer,
																 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	QualifiedName result;
	result.catalog = INVALID_CATALOG;
	result.schema = transformer.Transform<string>(list_pr.Child<ListParseResult>(0));
	result.name = transformer.Transform<string>(list_pr.Child<ListParseResult>(1));
	return result;
}

string PEGTransformerFactory::TransformReservedFunctionName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return list_pr.Child<IdentifierParseResult>(0).identifier;
}

QualifiedName PEGTransformerFactory::TransformFunctionName(PEGTransformer &transformer,
																 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	QualifiedName result;
	result.catalog = INVALID_CATALOG;
	result.schema = INVALID_SCHEMA;
	result.name = list_pr.Child<IdentifierParseResult>(0).identifier;
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformStarExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto result = make_uniq<StarExpression>();
	auto repeat_colid_opt = list_pr.Child<OptionalParseResult>(0);
	if (repeat_colid_opt.HasResult()) {
		auto repeat_colid = repeat_colid_opt.optional_result->Cast<RepeatParseResult>();
	}
	auto exclude_list_opt = list_pr.Child<OptionalParseResult>(2);
	if (exclude_list_opt.HasResult()) {
		result->exclude_list = transformer.Transform<qualified_column_set_t>(exclude_list_opt.optional_result);
	}
	auto replace_list_opt = list_pr.Child<OptionalParseResult>(3);
	if (replace_list_opt.HasResult()) {
		result->replace_list = transformer.Transform<case_insensitive_map_t<unique_ptr<ParsedExpression>>>(replace_list_opt.optional_result);
	}
	auto rename_list_opt = list_pr.Child<OptionalParseResult>(4);
	if (rename_list_opt.HasResult()) {
		result->rename_list = transformer.Transform<qualified_column_map_t<string>>(rename_list_opt.optional_result);
	}
	return result;
}


} // namespace duckdb
