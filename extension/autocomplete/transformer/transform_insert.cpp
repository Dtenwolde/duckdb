#include "transformer/peg_transformer.hpp"

namespace duckdb {

vector<string> PEGTransformerFactory::TransformInsertColumnList(PEGTransformer &transformer,
																		 optional_ptr<ParseResult> parse_result) {
	// InsertColumnList <- Parens(ColumnList)
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto column_list = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	return transformer.Transform<vector<string>>(column_list);
}

vector<string> PEGTransformerFactory::TransformColumnList(PEGTransformer &transformer,
																		 optional_ptr<ParseResult> parse_result) {
	// ColumnList <- List(ColId)
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto column_list = ExtractParseResultsFromList(list_pr.Child<ListParseResult>(0));
	vector<string> result;
	for (auto &column : column_list) {
		result.push_back(transformer.Transform<string>(column));
	}
	return result;
}


} // namespace duckdb
