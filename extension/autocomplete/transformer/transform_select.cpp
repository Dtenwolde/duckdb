#include "transformer/peg_transformer.hpp"
#include "duckdb/parser/query_node/select_node.hpp"

namespace duckdb {

unique_ptr<SelectStatement> PEGTransformerFactory::TransformSelectStatement(PEGTransformer &transformer,
																		 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto select_statement = transformer.Transform<unique_ptr<SelectStatement>>(list_pr.Child<ListParseResult>(0));
	// SetOperationNode
	auto repeat_setop_select = transformer.Transform<unique_ptr<SelectStatement>>(list_pr.Child<RepeatParseResult>(1));
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
	auto modifiers = transformer.Transform<vector<unique_ptr<ResultModifier>>>(list_pr.Child<ListParseResult>(2));
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
	}
	return select_node;
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

} // namespace duckdb
