#include "duckdb/common/operator/cast_operators.hpp"
#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/parser/expression/comparison_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/operator_expression.hpp"
#include "transformer/parse_result.hpp"
#include "duckdb/parser/expression/cast_expression.hpp"
#include "duckdb/parser/expression/window_expression.hpp"
#include "duckdb/parser/expression/subquery_expression.hpp"
#include "duckdb/parser/expression/case_expression.hpp"

namespace duckdb {

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformExpression(PEGTransformer &transformer,
                                                                        optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &base_expr_pr = list_pr.Child<ListParseResult>(0);
	unique_ptr<ParsedExpression> base_expr = transformer.Transform<unique_ptr<ParsedExpression>>(base_expr_pr);
	auto &indirection_pr = list_pr.Child<OptionalParseResult>(1);
	if (indirection_pr.HasResult()) {
		auto repeat_expression_pr = indirection_pr.optional_result->Cast<RepeatParseResult>();
		vector<unique_ptr<ParsedExpression>> expr_children;
		for (auto &child : repeat_expression_pr.children) {
			auto expr =
			    transformer.Transform<unique_ptr<ParsedExpression>>(child);
			if (expr->expression_class == ExpressionClass::COMPARISON) {
				auto compare_expr = unique_ptr_cast<ParsedExpression, ComparisonExpression>(std::move(expr));
				compare_expr->left = std::move(base_expr);
				base_expr = std::move(compare_expr);
			} else if (expr->expression_class == ExpressionClass::FUNCTION) {
				auto func_expr = unique_ptr_cast<ParsedExpression, FunctionExpression>(std::move(expr));
				func_expr->children.insert(func_expr->children.begin(), std::move(base_expr));
				base_expr = std::move(func_expr);
			} else {
				base_expr = make_uniq<OperatorExpression>(expr->type, std::move(base_expr),
															 std::move(expr));
			}
		}
	}

	return base_expr;
}

unique_ptr<ParsedExpression>
PEGTransformerFactory::TransformRecursiveExpression(PEGTransformer &transformer,
                                                    optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto operator_expr = transformer.Transform<ExpressionType>(list_pr.Child<ListParseResult>(0));
	vector<unique_ptr<ParsedExpression>> expr_children;
	auto right_expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(1));

	if (operator_expr >= ExpressionType::OPERATOR_LIKE && operator_expr <= ExpressionType::OPERATOR_GLOB) {
		expr_children.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(1)));
		auto function_name = ExpressionTypeToString(operator_expr);
		if (function_name == "~") {
			function_name = "regexp_full_match";
		} else if (function_name == "!~") {
			throw NotImplementedException("NOT SIMILAR TO operator not implemented");
		}
		return make_uniq<FunctionExpression>(function_name, std::move(expr_children));
	}
	if (operator_expr != ExpressionType::INVALID) {
		return make_uniq<ComparisonExpression>(operator_expr, nullptr, std::move(right_expr));
	}
	// Fall back to generic operator expression
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
		string_t str_val(literal_pr.number);
		bool try_cast_as_integer = true;
		bool try_cast_as_decimal = true;
		optional_idx decimal_position = optional_idx::Invalid();
		idx_t num_underscores = 0;
		idx_t num_integer_underscores = 0;
		for (idx_t i = 0; i < str_val.GetSize(); i++) {
			if (literal_pr.number[i] == '.') {
				// decimal point: cast as either decimal or double
				try_cast_as_integer = false;
				decimal_position = i;
			}
			if (literal_pr.number[i] == 'e' || literal_pr.number[i] == 'E') {
				// found exponent, cast as double
				try_cast_as_integer = false;
				try_cast_as_decimal = false;
			}
			if (literal_pr.number[i] == '_') {
				num_underscores++;
				if (!decimal_position.IsValid()) {
					num_integer_underscores++;
				}
			}
		}
		if (try_cast_as_integer) {
			int64_t bigint_value;
			// try to cast as bigint first
			if (TryCast::Operation<string_t, int64_t>(str_val, bigint_value)) {
				// successfully cast to bigint: bigint value
				return make_uniq<ConstantExpression>(Value::BIGINT(bigint_value));
			}
			hugeint_t hugeint_value;
			// if that is not successful; try to cast as hugeint
			if (TryCast::Operation<string_t, hugeint_t>(str_val, hugeint_value)) {
				// successfully cast to bigint: bigint value
				return make_uniq<ConstantExpression>(Value::HUGEINT(hugeint_value));
			}
			uhugeint_t uhugeint_value;
			// if that is not successful; try to cast as uhugeint
			if (TryCast::Operation<string_t, uhugeint_t>(str_val, uhugeint_value)) {
				// successfully cast to bigint: bigint value
				return make_uniq<ConstantExpression>(Value::UHUGEINT(uhugeint_value));
			}
		}
		idx_t decimal_offset = literal_pr.number[0] == '-' ? 3 : 2;
		if (try_cast_as_decimal && decimal_position.IsValid() &&
		    str_val.GetSize() - num_underscores < Decimal::MAX_WIDTH_DECIMAL + decimal_offset) {
			// figure out the width/scale based on the decimal position
			auto width = NumericCast<uint8_t>(str_val.GetSize() - 1 - num_underscores);
			auto scale = NumericCast<uint8_t>(width - decimal_position.GetIndex() + num_integer_underscores);
			if (literal_pr.number[0] == '-') {
				width--;
			}
			if (width <= Decimal::MAX_WIDTH_DECIMAL) {
				// we can cast the value as a decimal
				Value val = Value(str_val);
				val = val.DefaultCastAs(LogicalType::DECIMAL(width, scale));
				return make_uniq<ConstantExpression>(std::move(val));
			}
		}
		// if there is a decimal or the value is too big to cast as either hugeint or bigint
		double dbl_value = Cast::Operation<string_t, double>(str_val);
		return make_uniq<ConstantExpression>(Value::DOUBLE(dbl_value));
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

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformListExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	bool is_array = list_pr.Child<OptionalParseResult>(0).HasResult();
	auto list_expr = transformer.Transform<vector<unique_ptr<ParsedExpression>>>(list_pr.Child<ListParseResult>(1));
	if (!is_array) {
		return make_uniq<FunctionExpression>(INVALID_CATALOG, "main", "list_value", std::move(list_expr));
	} else {
		throw NotImplementedException("Array is not yet supported for list expression");
	}
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformStructExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto struct_children_pr = list_pr.Child<OptionalParseResult>(1);

	auto func_name = "struct_pack";
	vector<unique_ptr<ParsedExpression>> struct_children;
	if (struct_children_pr.HasResult()) {
		auto struct_children_list = ExtractParseResultsFromList(struct_children_pr.optional_result);
		for (auto struct_child : struct_children_list) {
			struct_children.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(struct_child));
		}
	}

	return make_uniq<FunctionExpression>(INVALID_CATALOG, "main", func_name, std::move(struct_children));

}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformSubqueryExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	bool is_not = list_pr.Child<OptionalParseResult>(0).HasResult();
	bool is_exists = list_pr.Child<OptionalParseResult>(1).HasResult();
	auto subquery_reference = transformer.Transform<unique_ptr<TableRef>>(list_pr.Child<ListParseResult>(2));

	auto result = make_uniq<SubqueryExpression>();
	if (is_exists) {
		result->subquery_type = SubqueryType::EXISTS;
	} else {
		result->subquery_type = SubqueryType::SCALAR;
	}
	auto select_statement = make_uniq<SelectStatement>();
	auto select_node = make_uniq<SelectNode>();
	select_node->select_list.push_back(make_uniq<StarExpression>());
	select_node->from_table = std::move(subquery_reference);
	select_statement->node = std::move(select_node);
	result->subquery = std::move(select_statement);
	if (is_not) {
		vector<unique_ptr<ParsedExpression>> children;
		children.push_back(std::move(result));
		auto not_operator = make_uniq<OperatorExpression>(ExpressionType::OPERATOR_NOT, std::move(children));
		return not_operator;
	}
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformTypeLiteral(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto colid = transformer.Transform<string>(list_pr.Child<ListParseResult>(0));
	auto type = LogicalType(TransformStringToLogicalTypeId(colid));
	if (type == LogicalTypeId::USER) {
		type = LogicalType::USER(colid);
	}
	auto string_literal = list_pr.Child<StringLiteralParseResult>(1).result;
	auto child = make_uniq<ConstantExpression>(Value(string_literal));
	auto result = make_uniq<CastExpression>(type, std::move(child));
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformStructField(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto alias = transformer.Transform<string>(list_pr.Child<ListParseResult>(0));
	auto expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(2));
	expr->SetAlias(alias);
	return expr;
}

vector<unique_ptr<ParsedExpression>> PEGTransformerFactory::TransformBoundedListExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto has_expr = list_pr.Child<OptionalParseResult>(1);
	vector<unique_ptr<ParsedExpression>> list_children;
	if (has_expr.HasResult()) {
		auto expr_list = ExtractParseResultsFromList(has_expr.optional_result);
		for (auto &expr : expr_list) {
			list_children.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(expr));
		}
	}
	return list_children;
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

	if (function_children.size() == 1 && ExpressionIsEmptyStar(*function_children[0])) {
		// COUNT(*) gets converted into COUNT()
		function_children.clear();
	}

	vector<OrderByNode> order_by;
	transformer.TransformOptional<vector<OrderByNode>>(list_pr, 2, order_by);
	auto ignore_nulls_opt = extract_parens.Child<OptionalParseResult>(3);
	if (ignore_nulls_opt.HasResult()) {
		throw NotImplementedException("Ignore nulls has not yet been implemented");
	}
	auto within_group_opt = list_pr.Child<OptionalParseResult>(2);
	if (within_group_opt.HasResult()) {
		throw NotImplementedException("Within group has not yet been implemented");
	}
	unique_ptr<ParsedExpression> filter_expr;
	transformer.TransformOptional<unique_ptr<ParsedExpression>>(list_pr, 3, filter_expr);
	auto export_opt = list_pr.Child<OptionalParseResult>(4);
	if (export_opt.HasResult()) {
		throw NotImplementedException("Export has not yet been implemented");
	}
	auto over_opt = list_pr.Child<OptionalParseResult>(5);
	if (over_opt.HasResult()) {
		auto window_function = transformer.Transform<unique_ptr<WindowExpression>>(over_opt.optional_result);
		window_function->catalog = qualified_function.catalog;
		window_function->schema = qualified_function.schema;
		window_function->function_name = qualified_function.name;
		window_function->children = std::move(function_children);
		window_function->type = WindowExpression::WindowToExpressionType(window_function->function_name);
		return window_function;
	}

	auto result = make_uniq<FunctionExpression>(qualified_function.catalog,
		qualified_function.schema,
		qualified_function.name,
		std::move(function_children));

	if (!order_by.empty()) {
		auto order_by_modifier = make_uniq<OrderModifier>();
		order_by_modifier->orders = std::move(order_by);
		result->order_bys = std::move(order_by_modifier);
	}
	if (filter_expr) {
		result->filter = std::move(filter_expr);
	}
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformFilterClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto nested_list = ExtractResultFromParens(list_pr.Child<ListParseResult>(1))->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<ParsedExpression>>(nested_list.Child<ListParseResult>(1));
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformCaseExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = make_uniq<CaseExpression>();
	unique_ptr<ParsedExpression> opt_expr;
	transformer.TransformOptional<unique_ptr<ParsedExpression>>(list_pr, 1, opt_expr);
	transformer.TransformOptional<unique_ptr<ParsedExpression>>(list_pr, 3, result->else_expr);

	auto cases_pr = list_pr.Child<RepeatParseResult>(2).children;
	for (auto &case_pr : cases_pr) {
		auto case_expr = transformer.Transform<CaseCheck>(case_pr);
		CaseCheck new_case;
		if (opt_expr) {
			new_case.when_expr = make_uniq<ComparisonExpression>(ExpressionType::COMPARE_EQUAL, opt_expr->Copy(), std::move(case_expr.when_expr));
		} else {
			new_case.when_expr = std::move(case_expr.when_expr);
		}
		new_case.then_expr = std::move(case_expr.then_expr);
		result->case_checks.push_back(std::move(new_case));
	}
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformCaseElse(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(1));
}

CaseCheck PEGTransformerFactory::TransformCaseWhenThen(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	CaseCheck result;
	result.when_expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(1));
	result.then_expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(3));
	return result;
}


unique_ptr<WindowExpression> PEGTransformerFactory::TransformOverClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<WindowExpression>>(list_pr.Child<ListParseResult>(1));
}

unique_ptr<WindowExpression> PEGTransformerFactory::TransformWindowFrame(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto choice_pr = list_pr.Child<ChoiceParseResult>(0);
	if (choice_pr.result->type == ParseResultType::IDENTIFIER) {
		throw NotImplementedException("Identifier in Window Function has not yet been implemented");
	}
	return transformer.Transform<unique_ptr<WindowExpression>>(choice_pr.result);
}

unique_ptr<WindowExpression> PEGTransformerFactory::TransformWindowFrameDefinition(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<WindowExpression>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<WindowExpression> PEGTransformerFactory::TransformWindowFrameContentsParens(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	return transformer.Transform<unique_ptr<WindowExpression>>(extract_parens);
}

unique_ptr<WindowExpression> PEGTransformerFactory::TransformWindowFrameNameContentsParens(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(0))->Cast<ListParseResult>();
	auto window_name_opt = extract_parens.Child<OptionalParseResult>(0);
	if (window_name_opt.HasResult()) {
		throw NotImplementedException("Window name has not yet been implemented");
	}
	// TODO(Dtenwolde) Use the window name
	auto window_frame_contents = transformer.Transform<unique_ptr<WindowExpression>>(extract_parens.Child<ListParseResult>(1));
	return window_frame_contents;
}

unique_ptr<WindowExpression> PEGTransformerFactory::TransformWindowFrameContents(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	//! Create a dummy result to add modifiers to
	auto result = make_uniq<WindowExpression>(ExpressionType::WINDOW_AGGREGATE, INVALID_CATALOG, INVALID_SCHEMA, string());
	auto partition_opt = list_pr.Child<OptionalParseResult>(0);
	if (partition_opt.HasResult()) {
		result->partitions = transformer.Transform<vector<unique_ptr<ParsedExpression>>>(partition_opt.optional_result);
	}
	auto order_by_opt = list_pr.Child<OptionalParseResult>(1);
	if (order_by_opt.HasResult()) {
		result->orders = transformer.Transform<vector<OrderByNode>>(order_by_opt.optional_result);
	}
	auto frame_opt = list_pr.Child<OptionalParseResult>(2);
	if (frame_opt.HasResult()) {
		throw NotImplementedException("Frame has not yet been implemented");
	} else {
		result->start = WindowBoundary::UNBOUNDED_PRECEDING;
		result->end = WindowBoundary::CURRENT_ROW_RANGE;
	}
	return result;
}

vector<unique_ptr<ParsedExpression>> PEGTransformerFactory::TransformWindowPartition(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto expression_list = ExtractParseResultsFromList(list_pr.Child<ListParseResult>(2));
	vector<unique_ptr<ParsedExpression>> result;
	for (auto expression : expression_list) {
		result.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(expression));
	}
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
	transformer.TransformOptional<qualified_column_set_t>(list_pr, 2, result->exclude_list);
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

qualified_column_set_t PEGTransformerFactory::TransformExcludeList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto nested_list = list_pr.Child<ListParseResult>(1);
	return transformer.Transform<qualified_column_set_t>(nested_list.Child<ChoiceParseResult>(0).result);
}

qualified_column_set_t PEGTransformerFactory::TransformExcludeNameList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	auto exclude_name_list = ExtractParseResultsFromList(extract_parens);
	qualified_column_set_t result;
	for (auto exclude_name : exclude_name_list) {
		result.insert(transformer.Transform<QualifiedColumnName>(exclude_name));
	}
	return result;
}

qualified_column_set_t PEGTransformerFactory::TransformSingleExcludeName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	qualified_column_set_t result;
	result.insert(transformer.Transform<QualifiedColumnName>(list_pr.Child<ListParseResult>(0)));
	return result;
}

QualifiedColumnName PEGTransformerFactory::TransformExcludeName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto choice_pr = list_pr.Child<ChoiceParseResult>(0).result;
	vector<string> result;
	if (choice_pr->name == "DottedIdentifier") {
		result = transformer.Transform<vector<string>>(choice_pr);

	} else if (choice_pr->name == "ColIdOrString") {
		result.push_back(transformer.Transform<string>(choice_pr));
	}
	if (result.size() == 1) {
		return QualifiedColumnName(result[0]);
	} else if (result.size() == 2) {
		return QualifiedColumnName(result[0], result[1]);
	} else {
		// TODO(Dtenwolde) Work on this some more.
		throw ParserException("Unexpected amount for ExcludeName");
	}
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformSpecialFunctionExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformCoalesceExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = make_uniq<OperatorExpression>(ExpressionType::OPERATOR_COALESCE);
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(1));
	auto expr_list = ExtractParseResultsFromList(extract_parens);
	for (auto expr : expr_list) {
		result->children.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(expr));
	}
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformUnpackExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(1));
	auto result = make_uniq<OperatorExpression>(ExpressionType::OPERATOR_UNPACK);
	result->children.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(extract_parens));
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformColumnsExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto result = make_uniq<StarExpression>();
	auto expr = transformer.Transform<unique_ptr<ParsedExpression>>(ExtractResultFromParens(list_pr.Child<ListParseResult>(2)));
	if (expr->GetExpressionType() == ExpressionType::STAR) {
		result = unique_ptr_cast<ParsedExpression, StarExpression>(std::move(expr));
	} else {
		result->expr = std::move(expr);
	}
	result->columns = true;
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformExtractExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	throw NotImplementedException("Extract not implemented");
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformLambdaExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto col_id_list = ExtractParseResultsFromList(list_pr.Child<ListParseResult>(1));
	vector<string> parameters;
	for (auto colid : col_id_list) {
		parameters.push_back(transformer.Transform<string>(colid));
	}
	auto rhs_expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(3));
	auto result = make_uniq<LambdaExpression>(parameters, std::move(rhs_expr));
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformNullIfExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(1));
	auto nested_list = extract_parens->Cast<ListParseResult>();
	auto result = make_uniq<OperatorExpression>(ExpressionType::OPERATOR_NULLIF);
	result->children.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(nested_list.Child<ListParseResult>(0)));
	result->children.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(nested_list.Child<ListParseResult>(2)));
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformRowExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(1));
	auto expr_list = ExtractParseResultsFromList(extract_parens);
	vector<unique_ptr<ParsedExpression>> results;
	for (auto expr : expr_list) {
		results.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(expr));
	}
	auto func_expr = make_uniq<FunctionExpression>("row", std::move(results));
	return func_expr;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformPositionExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(1));
	auto position_values = extract_parens->Cast<ListParseResult>();
	vector<unique_ptr<ParsedExpression>> results;
	//! search_string IN string
	results.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(position_values.Child<ListParseResult>(2)));
	results.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(position_values.Child<ListParseResult>(0)));
	return make_uniq<FunctionExpression>("position", std::move(results));
}

} // namespace duckdb
