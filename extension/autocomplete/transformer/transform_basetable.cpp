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

} // namespace duckdb
