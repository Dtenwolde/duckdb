#include "ast/insert_values.hpp"
#include "transformer/peg_transformer.hpp"
#include "duckdb/parser/statement/insert_statement.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformInsertStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = make_uniq<InsertStatement>();
	auto with_opt = list_pr.Child<OptionalParseResult>(0);
	if (with_opt.HasResult()) {
		throw NotImplementedException("WITH clause in INSERT statement is not yet supported.");
	}
	auto or_action_opt = list_pr.Child<OptionalParseResult>(2);
	auto insert_target = transformer.Transform<unique_ptr<BaseTableRef>>(list_pr.Child<ListParseResult>(4));
	// TODO(Dtenwolde) What about the insert alias?
	result->catalog = insert_target->catalog_name;
	result->schema = insert_target->schema_name;
	result->table = insert_target->table_name;
	transformer.TransformOptional<InsertColumnOrder>(list_pr, 5, result->column_order);
	transformer.TransformOptional<vector<string>>(list_pr, 6, result->columns);
	auto insert_values = transformer.Transform<InsertValues>(list_pr.Child<ListParseResult>(7));
	result->default_values = insert_values.default_values;
	if (insert_values.sql_statement) {
		result->select_statement = unique_ptr_cast<SQLStatement, SelectStatement>(std::move(insert_values.sql_statement));
	}
	auto on_conflict_info = transformer.Transform<unique_ptr<OnConflictInfo>>(list_pr.Child<ListParseResult>(8));
	transformer.TransformOptional(list_pr, 2, on_conflict_info->action_type);
	result->on_conflict_info = std::move(on_conflict_info);
	transformer.TransformOptional<vector<unique_ptr<ParsedExpression>>>(list_pr, 9, result->returning_list);

	return result;
}

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
