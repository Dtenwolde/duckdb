#include "ast/table_alias.hpp"
#include "transformer/peg_transformer.hpp"
#include "duckdb/parser/tableref/emptytableref.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/tableref/joinref.hpp"
#include "duckdb/parser/tableref/expressionlistref.hpp"
#include "duckdb/parser/tableref/subqueryref.hpp"
#include "duckdb/parser/tableref/table_function_ref.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformSelectStatement(PEGTransformer &transformer,
																		 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto select_statement = transformer.Transform<unique_ptr<SelectStatement>>(list_pr.Child<ListParseResult>(0));
	// SetOperationNode
	// auto repeat_setop_select = transformer.Transform<unique_ptr<SelectStatement>>(list_pr.Child<RepeatParseResult>(1));
	// vector<unique_ptr<ResultModifier>>
	// auto modifiers = transformer.Transform<vector<unique_ptr<ResultModifier>>>(list_pr.Child<ListParseResult>(2));
	return select_statement;
}

unique_ptr<SelectStatement> PEGTransformerFactory::TransformSelectOrParens(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<SelectStatement>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<SelectStatement> PEGTransformerFactory::TransformSelectParens(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	return transformer.Transform<unique_ptr<SelectStatement>>(extract_parens);
}

unique_ptr<SelectStatement> PEGTransformerFactory::TransformBaseSelect(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto with_clause = list_pr.Child<OptionalParseResult>(0);
	auto select_statement = transformer.Transform<unique_ptr<SelectStatement>>(list_pr.Child<ListParseResult>(1));
	// auto modifiers = transformer.Transform<vector<unique_ptr<ResultModifier>>>(list_pr.Child<ListParseResult>(2));
	return select_statement;
}

unique_ptr<SelectStatement> PEGTransformerFactory::TransformSelectStatementType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<SelectStatement>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<SelectStatement> PEGTransformerFactory::TransformOptionalParensSimpleSelect(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<SelectStatement>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<SelectStatement> PEGTransformerFactory::TransformSimpleSelectParens(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	return transformer.Transform<unique_ptr<SelectStatement>>(extract_parens);
}

unique_ptr<SelectStatement> PEGTransformerFactory::TransformSimpleSelect(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto select_from = transformer.Transform<unique_ptr<SelectNode>>(list_pr.Child<ListParseResult>(0));
	auto opt_where_clause = list_pr.Child<OptionalParseResult>(1);
	if (opt_where_clause.HasResult()) {
		select_from->where_clause = transformer.Transform<unique_ptr<ParsedExpression>>(opt_where_clause.optional_result);
	}
	auto opt_group_by = list_pr.Child<OptionalParseResult>(2);
	if (opt_group_by.HasResult()) {
		select_from->groups = transformer.Transform<GroupByNode>(opt_group_by.optional_result);
	}
	auto opt_having_clause = list_pr.Child<OptionalParseResult>(3);
	if (opt_having_clause.HasResult()) {
		select_from->having = transformer.Transform<unique_ptr<ParsedExpression>>(opt_having_clause.optional_result);
	}
	auto opt_window_clause = list_pr.Child<OptionalParseResult>(4);
	if (opt_having_clause.HasResult()) {
		// TODO(Dtenwolde) Window function
	}

	auto opt_qualify_clause = list_pr.Child<OptionalParseResult>(4);
	if (opt_qualify_clause.HasResult()) {
		select_from->qualify = transformer.Transform<unique_ptr<ParsedExpression>>(opt_qualify_clause.optional_result);
	}

	auto opt_sample_clause = list_pr.Child<OptionalParseResult>(4);
	if (opt_sample_clause.HasResult()) {
		select_from->sample = transformer.Transform<unique_ptr<SampleOptions>>(opt_sample_clause.optional_result);
	}
	auto select_statement = make_uniq<SelectStatement>();
	select_statement->node = std::move(select_from);
	return select_statement;
}

unique_ptr<SelectNode> PEGTransformerFactory::TransformSelectFrom(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<SelectNode>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<SelectNode> PEGTransformerFactory::TransformSelectFromClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto select_node = make_uniq<SelectNode>();
	select_node->select_list = transformer.Transform<vector<unique_ptr<ParsedExpression>>>(list_pr.Child<ListParseResult>(0));
	auto opt_from = list_pr.Child<OptionalParseResult>(1);
	if (opt_from.HasResult()) {
		select_node->from_table = transformer.Transform<unique_ptr<TableRef>>(opt_from.optional_result);
	} else {
		select_node->from_table = make_uniq<EmptyTableRef>();
	}
	return select_node;
}

unique_ptr<SelectNode> PEGTransformerFactory::TransformFromSelectClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto select_node = make_uniq<SelectNode>();
	select_node->from_table = transformer.Transform<unique_ptr<TableRef>>(list_pr.Child<ListParseResult>(0));
	auto opt_select = list_pr.Child<OptionalParseResult>(1);
	if (opt_select.HasResult()) {
		select_node->select_list = transformer.Transform<vector<unique_ptr<ParsedExpression>>>(opt_select.optional_result);
	} else {
		select_node->select_list.push_back(make_uniq<StarExpression>());
	}
	return select_node;
}

unique_ptr<TableRef> PEGTransformerFactory::TransformFromClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto table_ref_list = ExtractParseResultsFromList(list_pr.Child<ListParseResult>(1));
	auto result_table_ref = transformer.Transform<unique_ptr<TableRef>>(table_ref_list[0]);
	if (table_ref_list.size() == 1) {
		return result_table_ref;
	}
	for (idx_t i = 1; i < table_ref_list.size(); i++) {
		auto table_ref = transformer.Transform<unique_ptr<TableRef>>(table_ref_list[i]);
		auto cross_product = make_uniq<JoinRef>();
		cross_product->left = std::move(result_table_ref);
		cross_product->right = std::move(table_ref);
		cross_product->ref_type = JoinRefType::CROSS;
		result_table_ref = std::move(cross_product);
	}
	return result_table_ref;
}

unique_ptr<TableRef> PEGTransformerFactory::TransformTableRef(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto inner_table_ref = transformer.Transform<unique_ptr<TableRef>>(list_pr.Child<ListParseResult>(0));
	// auto join_or_pivot
	auto opt_table_alias = list_pr.Child<OptionalParseResult>(2);
	if (opt_table_alias.HasResult()) {
		auto table_alias = transformer.Transform<TableAlias>(opt_table_alias.optional_result);
		inner_table_ref->alias = table_alias.name;
		inner_table_ref->column_name_alias = table_alias.column_name_alias;
	}
	return inner_table_ref;
}

unique_ptr<TableRef> PEGTransformerFactory::TransformInnerTableRef(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<TableRef>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<TableRef> PEGTransformerFactory::TransformTableFunction(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<TableRef>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<TableRef> PEGTransformerFactory::TransformTableFunctionLateralOpt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto result = make_uniq<TableFunctionRef>();

	// TODO(Dtenwolde) Figure out what to do with lateral
	auto lateral_opt = list_pr.Child<OptionalParseResult>(0).HasResult();
	auto qualified_table_function = transformer.Transform<QualifiedName>(list_pr.Child<ListParseResult>(1));
	auto table_function_arguments = transformer.Transform<vector<unique_ptr<ParsedExpression>>>(list_pr.Child<ListParseResult>(2));

	result->function = make_uniq<FunctionExpression>(
														qualified_table_function.catalog,
														qualified_table_function.schema,
														qualified_table_function.name,
														std::move(table_function_arguments));
	auto table_alias_opt = list_pr.Child<OptionalParseResult>(3);
	if (table_alias_opt.HasResult()) {
		auto table_alias = transformer.Transform<TableAlias>(table_alias_opt.optional_result);
		result->alias = table_alias.name;
		result->column_name_alias = table_alias.column_name_alias;
	}
	return result;
}

unique_ptr<TableRef> PEGTransformerFactory::TransformTableFunctionAliasColon(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto table_alias = transformer.Transform<string>(list_pr.Child<ListParseResult>(0));

	auto qualified_table_function = transformer.Transform<QualifiedName>(list_pr.Child<ListParseResult>(1));
	auto table_function_arguments = transformer.Transform<vector<unique_ptr<ParsedExpression>>>(list_pr.Child<ListParseResult>(2));

	auto result = make_uniq<TableFunctionRef>();
	result->function = make_uniq<FunctionExpression>(
														qualified_table_function.catalog,
														qualified_table_function.schema,
														qualified_table_function.name,
														std::move(table_function_arguments));
	result->alias = table_alias;
	return result;
}

string PEGTransformerFactory::TransformTableAliasColon(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<string>(list_pr.Child<ListParseResult>(0));
}

QualifiedName PEGTransformerFactory::TransformQualifiedTableFunction(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	QualifiedName result;
	auto opt_catalog = list_pr.Child<OptionalParseResult>(0);
	if (opt_catalog.HasResult()) {
		result.catalog = transformer.Transform<string>(opt_catalog.optional_result);
	} else {
		result.catalog = INVALID_CATALOG;
	}
	auto opt_schema = list_pr.Child<OptionalParseResult>(1);
	if (opt_schema.HasResult()) {
		result.schema = transformer.Transform<string>(opt_schema.optional_result);
	} else {
		result.schema = INVALID_SCHEMA;
	}
	result.name = list_pr.Child<IdentifierParseResult>(2).identifier;
	return result;
}

unique_ptr<TableRef> PEGTransformerFactory::TransformTableSubquery(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	// TODO(Dtenwolde)
	auto lateral_opt = list_pr.Child<OptionalParseResult>(0).HasResult();
	auto subquery_reference = transformer.Transform<unique_ptr<TableRef>>(list_pr.Child<ListParseResult>(1));
	auto table_alias_opt = list_pr.Child<OptionalParseResult>(2);
	if (table_alias_opt.HasResult()) {
		auto table_alias = transformer.Transform<TableAlias>(table_alias_opt.optional_result);
		subquery_reference->alias = table_alias.name;
		subquery_reference->column_name_alias = table_alias.column_name_alias;
	}
	return subquery_reference;
}

unique_ptr<TableRef> PEGTransformerFactory::TransformSubqueryReference(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	auto sql_statement = transformer.Transform<unique_ptr<SQLStatement>>(extract_parens);
	if (sql_statement->type != StatementType::SELECT_STATEMENT) {
		throw ParserException("Subquery needs a SELECT statement");
	}
	auto *raw_stmt = sql_statement.release();
	auto select_statement_ptr = static_cast<SelectStatement *>(raw_stmt);
	auto select_statement = unique_ptr<SelectStatement>(select_statement_ptr);

	auto subquery_ref = make_uniq<SubqueryRef>(std::move(select_statement));
	return subquery_ref;
}

unique_ptr<TableRef> PEGTransformerFactory::TransformValuesRef(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto values_select_statement = transformer.Transform<unique_ptr<SelectStatement>>(list_pr.Child<ListParseResult>(0));
	auto subquery_ref = make_uniq<SubqueryRef>(std::move(values_select_statement));
	auto opt_alias = list_pr.Child<OptionalParseResult>(1);
	if (opt_alias.HasResult()) {
		auto table_alias = transformer.Transform<TableAlias>(opt_alias.optional_result);
		subquery_ref->alias = table_alias.name;
		subquery_ref->column_name_alias = table_alias.column_name_alias;
	}
	return subquery_ref;
}

unique_ptr<SelectStatement> PEGTransformerFactory::TransformValuesClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto value_expression_list = ExtractParseResultsFromList(list_pr.Child<ListParseResult>(1));
	vector<vector<unique_ptr<ParsedExpression>>> values_list;
	for (auto value_expression : value_expression_list) {
		values_list.push_back(transformer.Transform<vector<unique_ptr<ParsedExpression>>>(value_expression));
	}
	if (values_list.size() > 1) {
		const auto expected_size = values_list[0].size();
		for (idx_t i = 1; i < values_list.size(); i++) {
			if (values_list[i].size() != expected_size) {
				throw ParserException("VALUES lists must all be the same length");
			}
		}
	}
	auto result = make_uniq<ExpressionListRef>();
	result->alias = "valueslist";
	result->values = std::move(values_list);

	auto select_node = make_uniq<SelectNode>();
	select_node->from_table = std::move(result);
	select_node->select_list.push_back(make_uniq<StarExpression>());
	auto select_statement = make_uniq<SelectStatement>();
	select_statement->node = std::move(select_node);
	return select_statement;
}

vector<unique_ptr<ParsedExpression>> PEGTransformerFactory::TransformValuesExpressions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	vector<unique_ptr<ParsedExpression>> result;
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	auto expression_list = ExtractParseResultsFromList(extract_parens);
	for (auto expression : expression_list) {
		result.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(expression));
	}
	return result;
}

vector<unique_ptr<ParsedExpression>> PEGTransformerFactory::TransformSelectClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	// TODO(Dtenwolde) Do something with opt_distinct
	auto opt_distinct = list_pr.Child<OptionalParseResult>(1);
	auto target_list = transformer.Transform<vector<unique_ptr<ParsedExpression>>>(list_pr.Child<ListParseResult>(2));
	return target_list;
}

vector<unique_ptr<ParsedExpression>> PEGTransformerFactory::TransformTargetList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto target_list = ExtractParseResultsFromList(list_pr.Child<ListParseResult>(0));
	vector<unique_ptr<ParsedExpression>> result;
	for (auto target : target_list) {
		result.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(target));
	}
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformAliasedExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformExpressionAsCollabel(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(0));
	auto collabel_or_string = list_pr.Child<ListParseResult>(2);
	expr->alias = transformer.Transform<string>(collabel_or_string);
	return expr;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformColIdExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto colid = list_pr.Child<ListParseResult>(0);
	auto expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(2));
	expr->alias = transformer.Transform<string>(colid);
	return expr;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformExpressionOptIdentifier(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto expr = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(0));
	auto opt_identifier = list_pr.Child<OptionalParseResult>(1);
	if (opt_identifier.HasResult()) {
		expr->alias = opt_identifier.optional_result->Cast<IdentifierParseResult>().identifier;
	}
	return expr;
}

TableAlias PEGTransformerFactory::TransformTableAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	TableAlias result;
	result.name = transformer.Transform<string>(list_pr.Child<ListParseResult>(1));
	auto opt_column_aliases = list_pr.Child<OptionalParseResult>(2);
	if (opt_column_aliases.HasResult()) {
		result.column_name_alias = transformer.Transform<vector<string>>(opt_column_aliases.optional_result);
	}
	return result;
}

vector<string> PEGTransformerFactory::TransformColumnAliases(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	vector<string> result;
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	auto alias_list = ExtractParseResultsFromList(extract_parens);
	for (auto alias : alias_list) {
		result.push_back(transformer.Transform<string>(alias));
	}
	return result;
}

} // namespace duckdb
