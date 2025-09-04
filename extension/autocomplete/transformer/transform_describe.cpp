#include "duckdb/parser/tableref/showref.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<SelectStatement> PEGTransformerFactory::TransformDescribeStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto select_statement = make_unique<SelectStatement>();
	select_statement->node = transformer.Transform<unique_ptr<QueryNode>>(list_pr.Child<ChoiceParseResult>(0).result);
	return select_statement;
}

unique_ptr<QueryNode> PEGTransformerFactory::TransformShowSelect(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = make_uniq<ShowRef>();
	result->show_type = transformer.Transform<ShowType>(list_pr.Child<ListParseResult>(0));
	auto sql_statement = transformer.Transform<unique_ptr<SQLStatement>>(list_pr.Child<ListParseResult>(1));
	if (sql_statement->type != StatementType::SELECT_STATEMENT) {
		throw ParserException("Subquery needs a SELECT statement");
	}
	// TODO(Dtenwolde)
	auto *raw_stmt = sql_statement.release();
	auto select_statement_ptr = static_cast<SelectStatement *>(raw_stmt);
	auto select_statement = unique_ptr<SelectStatement>(select_statement_ptr);
	result->query = std::move(select_statement->node);
	auto select_node = make_uniq<SelectNode>();
	select_node->select_list.push_back(make_uniq<StarExpression>());
	select_node->from_table = std::move(result);
	return select_node;
}

ShowType PEGTransformerFactory::TransformShowOrDescribeOrSummarize(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<ShowType>(list_pr.Child<ChoiceParseResult>(0).result);
}

ShowType PEGTransformerFactory::TransformShowOrDescribe(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.TransformEnum<ShowType>(list_pr.Child<ChoiceParseResult>(0).result);
}


}