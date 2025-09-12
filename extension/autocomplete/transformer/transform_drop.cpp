#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformDropStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto drop_entry = transformer.Transform<unique_ptr<DropStatement>>(list_pr.Child<ListParseResult>(1));
	transformer.TransformOptional<bool>(list_pr, 2, drop_entry->info->cascade);
	return drop_entry;
}

unique_ptr<DropStatement> PEGTransformerFactory::TransformDropEntries(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<DropStatement>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<DropStatement> PEGTransformerFactory::TransformDropTable(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = make_uniq<DropStatement>();
	auto info = make_uniq<DropInfo>();
	auto catalog_type = transformer.Transform<CatalogType>(list_pr.Child<ListParseResult>(0));
	bool if_exists = list_pr.Child<OptionalParseResult>(1).HasResult();
	auto base_table_list = ExtractParseResultsFromList(list_pr.Child<ListParseResult>(2));
	if (base_table_list.size() > 1) {
		throw NotImplementedException("Can only drop one object at a time");
	}
	auto base_table = transformer.Transform<unique_ptr<BaseTableRef>>(base_table_list[0]);
	info->catalog = base_table->catalog_name;
	info->schema = base_table->schema_name;
	info->name = base_table->table_name;
	info->type = catalog_type;
	info->if_not_found = if_exists ? OnEntryNotFound::RETURN_NULL : OnEntryNotFound::THROW_EXCEPTION;
	result->info = std::move(info);
	return result;
}

CatalogType PEGTransformerFactory::TransformTableOrView(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.TransformEnum<CatalogType>(list_pr.Child<ChoiceParseResult>(0).result);
}


}