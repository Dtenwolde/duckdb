#include "transformer/peg_transformer.hpp"
#include "duckdb/parser/parsed_data/create_view_info.hpp"

namespace duckdb {

unique_ptr<CreateStatement> PEGTransformerFactory::TransformCreateViewStmt(PEGTransformer &transformer,
                                                                         optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	// TODO(Dtenwolde) handle recursive views
	auto recursive = list_pr.Child<OptionalParseResult>(0).HasResult();
	auto if_not_exists = list_pr.Child<OptionalParseResult>(2).HasResult();
	auto qualified_name = transformer.Transform<QualifiedName>(list_pr.Child<ListParseResult>(3));
	auto insert_column_list_pr = list_pr.Child<OptionalParseResult>(4);
	vector<string> column_list;
	if (insert_column_list_pr.HasResult()) {
		column_list = transformer.Transform<vector<string>>(insert_column_list_pr.optional_result);
	}
	auto result = make_uniq<CreateStatement>();
	auto info = make_uniq<CreateViewInfo>();
	info->on_conflict = if_not_exists ? OnCreateConflict::IGNORE_ON_CONFLICT : OnCreateConflict::ERROR_ON_CONFLICT;
	info->catalog = qualified_name.catalog;
	info->schema = qualified_name.schema;
	info->view_name = qualified_name.name;
	info->aliases = column_list;
	auto sql_statement = transformer.Transform<unique_ptr<SQLStatement>>(list_pr.Child<ListParseResult>(6));
	if (sql_statement->type != StatementType::SELECT_STATEMENT) {
		throw ParserException("Expected a select statement");
	}
	info->query = unique_ptr_cast<SQLStatement, SelectStatement>(std::move(sql_statement));
	result->info = std::move(info);
	return result;
}

} // namespace duckdb
