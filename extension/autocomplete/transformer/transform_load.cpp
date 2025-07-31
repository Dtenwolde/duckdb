#include "duckdb/parser/statement/load_statement.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformLoadStatement(PEGTransformer &transformer,
																		 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = make_uniq<LoadStatement>();
	auto info = make_uniq<LoadInfo>();
	info->load_type = LoadType::LOAD;
	info->filename = transformer.Transform<string>(list_pr.Child<ListParseResult>(1));
	result->info = std::move(info);
	return result;
}

} // namespace duckdb
