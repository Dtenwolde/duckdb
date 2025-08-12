#include "duckdb/parser/tableref/basetableref.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<BaseTableRef> PEGTransformerFactory::TransformBaseTableName(PEGTransformer &transformer,
                                                                       optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	if (choice_pr.result->type == ParseResultType::IDENTIFIER) {
		auto table_name = choice_pr.result->Cast<IdentifierParseResult>().identifier;
		const auto description = TableDescription(INVALID_CATALOG, INVALID_SCHEMA, table_name);
		return make_uniq<BaseTableRef>(description);
	}
	return transformer.Transform<unique_ptr<BaseTableRef>>(choice_pr.result);
}

unique_ptr<BaseTableRef> PEGTransformerFactory::TransformSchemaReservedTable(PEGTransformer &transformer,
                                                                             optional_ptr<ParseResult> parse_result) {
	// SchemaReservedTable <- SchemaQualification ReservedTableName
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto schema = transformer.Transform<string>(list_pr.Child<ListParseResult>(0));
	auto table_name = list_pr.Child<IdentifierParseResult>(1).identifier;

	const auto description = TableDescription(INVALID_CATALOG, schema, table_name);
	return make_uniq<BaseTableRef>(description);
}

unique_ptr<BaseTableRef>
PEGTransformerFactory::TransformCatalogReservedSchemaTable(PEGTransformer &transformer,
                                                           optional_ptr<ParseResult> parse_result) {
	// CatalogReservedSchemaTable <- CatalogQualification ReservedSchemaQualification ReservedTableName
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto catalog = transformer.Transform<string>(list_pr.Child<ListParseResult>(0));
	auto schema = transformer.Transform<string>(list_pr.Child<ListParseResult>(1));
	auto table_name = list_pr.Child<IdentifierParseResult>(2).identifier;
	const auto description = TableDescription(catalog, catalog, table_name);
	return make_uniq<BaseTableRef>(description);
}

string PEGTransformerFactory::TransformSchemaQualification(PEGTransformer &transformer,
                                                           optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return list_pr.Child<IdentifierParseResult>(0).identifier;
}

string PEGTransformerFactory::TransformCatalogQualification(PEGTransformer &transformer,
                                                            optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return list_pr.Child<IdentifierParseResult>(0).identifier;
}

} // namespace duckdb
