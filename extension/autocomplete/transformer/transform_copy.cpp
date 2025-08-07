#include "duckdb/parser/statement/copy_statement.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformCopyStatement(PEGTransformer &transformer,
																		 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &copy_mode = list_pr.Child<ListParseResult>(1);
	return transformer.Transform<unique_ptr<SQLStatement>>(copy_mode.Child<ChoiceParseResult>(0).result);
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformCopySelect(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	throw NotImplementedException("TransformCopySelect");
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformCopyTable(PEGTransformer &transformer,
																		 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto result = make_uniq<CopyStatement>();
	auto info = make_uniq<CopyInfo>();

	auto base_table = transformer.Transform<unique_ptr<BaseTableRef>>(list_pr.Child<ListParseResult>(0));
	info->table = base_table->table_name;
	info->schema = base_table->schema_name;
	info->catalog = base_table->catalog_name;
	auto insert_column_list = list_pr.Child<OptionalParseResult>(1);
	if (insert_column_list.HasResult()) {
		info->select_list = transformer.Transform<vector<string>>(insert_column_list.optional_result);
	}
	info->is_from = transformer.Transform<bool>(list_pr.Child<ListParseResult>(2));

	info->file_path = transformer.Transform<string>(list_pr.Child<ListParseResult>(3));

	auto &copy_options_pr = list_pr.Child<OptionalParseResult>(4);
	if (copy_options_pr.HasResult()) {
		// TODO(dtenwolde) deal with format option here which is a special case.
		info->options = transformer.Transform<case_insensitive_map_t<vector<Value>>>(copy_options_pr.optional_result);
	}

	result->info = std::move(info);
	return result;
}

bool PEGTransformerFactory::TransformFromOrTo(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto from_or_to = list_pr.Child<ChoiceParseResult>(0).result;
	auto keyword = from_or_to->Cast<KeywordParseResult>();
	return StringUtil::Lower(keyword.keyword) == "from";
}

string PEGTransformerFactory::TransformCopyFileName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	// TODO(dtenwolde) support stdin and stdout
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<string>(list_pr.Child<ChoiceParseResult>(0).result);
}

string PEGTransformerFactory::TransformIdentifierColId(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	string result;
	result += list_pr.Child<IdentifierParseResult>(0).name;
	result += ".";
	result += transformer.Transform<string>(list_pr.Child<ListParseResult>(2));
	return result;
}

case_insensitive_map_t<vector<Value>> PEGTransformerFactory::TransformCopyOptions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	throw NotImplementedException("CopyOptions not implemented");
}

} // namespace duckdb
