#include "duckdb/parser/statement/delete_statement.hpp"
#include "duckdb/parser/tableref/basetableref.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformTruncateStatement(PEGTransformer &transformer,
																		 optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = make_uniq<DeleteStatement>();
	result->table = transformer.Transform<unique_ptr<BaseTableRef>>(list_pr.Child<ListParseResult>(2));
	return result;
}

} // namespace duckdb
