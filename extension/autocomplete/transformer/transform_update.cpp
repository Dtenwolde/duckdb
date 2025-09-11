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

}