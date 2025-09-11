#include "ast/update_set_element.hpp"
#include "duckdb/parser/statement/update_statement.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformUpdateStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = make_uniq<UpdateStatement>();
	auto with_opt = list_pr.Child<OptionalParseResult>(0);
	if (with_opt.HasResult()) {
		throw NotImplementedException("With clause inside update statement is not yet supported");
	}
	result->table = transformer.Transform<unique_ptr<BaseTableRef>>(list_pr.Child<ListParseResult>(2));
	result->set_info = transformer.Transform<unique_ptr<UpdateSetInfo>>(list_pr.Child<ListParseResult>(3));
	transformer.TransformOptional<unique_ptr<TableRef>>(list_pr, 4, result->from_table);
	transformer.TransformOptional<unique_ptr<ParsedExpression>>(list_pr, 5, result->set_info->condition);
	transformer.TransformOptional<vector<unique_ptr<ParsedExpression>>>(list_pr, 6, result->returning_list);
	return result;
}

unique_ptr<BaseTableRef> PEGTransformerFactory::TransformUpdateTarget(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<BaseTableRef>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<BaseTableRef> PEGTransformerFactory::TransformUpdateTargetNoAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<BaseTableRef>>(list_pr.Child<ListParseResult>(0));
}

unique_ptr<BaseTableRef> PEGTransformerFactory::TransformUpdateTargetAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = transformer.Transform<unique_ptr<BaseTableRef>>(list_pr.Child<ListParseResult>(0));
	transformer.TransformOptional<string>(list_pr, 1, result->alias);
	return result;
}

string PEGTransformerFactory::TransformUpdateAlias(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<string>(list_pr.Child<ListParseResult>(1));
}

unique_ptr<UpdateSetInfo> PEGTransformerFactory::TransformUpdateSetClause(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<UpdateSetInfo>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<UpdateSetInfo> PEGTransformerFactory::TransformUpdateSetElementList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto set_element_list = ExtractParseResultsFromList(list_pr.Child<ListParseResult>(0));
	auto result = make_uniq<UpdateSetInfo>();
	for (auto element : set_element_list) {
		auto update_set_element = transformer.Transform<UpdateSetElement>(element);
		result->columns.push_back(update_set_element.column_name);
		result->expressions.push_back(std::move(update_set_element.expression));
	}
	return result;
}

unique_ptr<UpdateSetInfo> PEGTransformerFactory::TransformUpdateSetColumnListExpression(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = make_uniq<UpdateSetInfo>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	auto column_name_list = ExtractParseResultsFromList(extract_parens);
	for (auto column : column_name_list) {
		result->columns.push_back(column->Cast<IdentifierParseResult>().identifier);
	}
	result->expressions.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(2)));
	return result;
}

UpdateSetElement PEGTransformerFactory::TransformUpdateSetElement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	UpdateSetElement result;
	result.column_name = list_pr.Child<IdentifierParseResult>(0).identifier;
	result.expression = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(2));
	return result;
}

}