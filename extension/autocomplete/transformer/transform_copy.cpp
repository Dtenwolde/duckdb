#include "duckdb/parser/statement/copy_statement.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformCopyStatement(PEGTransformer &transformer,
																		 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &copy_mode = list_pr.Child<ListParseResult>(1);
	return transformer.Transform<unique_ptr<SQLStatement>>(copy_mode.Child<ChoiceParseResult>(0).result);
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformCopyTable(PEGTransformer &transformer,
																		 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto result = make_uniq<CopyStatement>();
	auto info = make_uniq<CopyInfo>();

	auto base_table = transformer.Transform<unique_ptr<BaseTableRef>>(list_pr.Child<ListParseResult>(0));
	auto insert_column_list = list_pr.Child<OptionalParseResult>(1);
	if (insert_column_list.HasResult()) {
		throw NotImplementedException("Insert columns not implemented");
	}
	info->is_from = transformer.Transform<bool>(list_pr.Child<ListParseResult>(2));

	auto copy_file_name = transformer.Transform<QualifiedName>(list_pr.Child<ListParseResult>(3));

	auto copy_options = transformer.Transform<unordered_map<string, Value>>(list_pr.Child<ListParseResult>(4));

	result->info = std::move(info);
	return result;
}

bool PEGTransformerFactory::TransformFromOrTo(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto from_or_to = list_pr.Child<ChoiceParseResult>(0).result;
	auto keyword = from_or_to->Cast<KeywordParseResult>();
	return StringUtil::Lower(keyword.keyword) == "from";
}

} // namespace duckdb
