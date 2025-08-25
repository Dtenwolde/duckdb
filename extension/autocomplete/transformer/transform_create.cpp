#include "ast/column_element.hpp"
#include "duckdb/parser/parsed_data/create_table_info.hpp"
#include "duckdb/parser/statement/create_statement.hpp"
#include "transformer/peg_transformer.hpp"
#include "duckdb/parser/constraint.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformCreateStatement(PEGTransformer &transformer,
                                                                         optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	bool replace = list_pr.Child<OptionalParseResult>(1).HasResult();
	bool temporary = list_pr.Child<OptionalParseResult>(2).HasResult();
	auto result = transformer.Transform<unique_ptr<SQLStatement>>(list_pr.Child<ListParseResult>(3));

	// todo(dtenwolde) do some more with result
	return result;
}

unique_ptr<SQLStatement>
PEGTransformerFactory::TransformCreateStatementVariation(PEGTransformer &transformer,
                                                         optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto choice_pr = list_pr.Child<ChoiceParseResult>(0);
	return transformer.Transform<unique_ptr<SQLStatement>>(choice_pr.result);
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformCreateTableStmt(PEGTransformer &transformer,
                                                                         optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto result = make_uniq<CreateStatement>();
	QualifiedName table_name = transformer.Transform<QualifiedName>(list_pr.Child<ListParseResult>(2));
	// Use appropriate constructor
	auto info = make_uniq<CreateTableInfo>(table_name.catalog, table_name.schema, table_name.name);

	bool if_not_exists = list_pr.Child<OptionalParseResult>(1).HasResult();
	auto &table_as_or_column_list = list_pr.Child<ListParseResult>(3).Child<ChoiceParseResult>(0);
	if (table_as_or_column_list.name == "CreateTableAs") {
		throw NotImplementedException("CreateTableAs");
		// info->query =
		// transformer.Transform<unique_ptr<SQLStatement>>(table_as_or_column_list.result)->Cast<SelectStatement>();
		// info->query = std::move(sql_statement);
	} else {
		auto column_list = transformer.Transform<ColumnElements>(table_as_or_column_list.result);
		info->columns = std::move(column_list.columns);
		info->constraints = std::move(column_list.constraints);
	}
	// TODO(dtenwolde) Figure out how to deal with commit action
	auto commit_action = list_pr.Child<OptionalParseResult>(4).HasResult();

	result->info = std::move(info);
	return result;
}

ColumnElements PEGTransformerFactory::TransformCreateColumnList(PEGTransformer &transformer,
                                                                optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto create_table_column_list = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	return transformer.Transform<ColumnElements>(create_table_column_list);
}

ColumnElements PEGTransformerFactory::TransformCreateTableColumnList(PEGTransformer &transformer,
                                                                     optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto column_elements = ExtractParseResultsFromList(list_pr.Child<ListParseResult>(0));
	ColumnElements result;
	for (auto column_element : column_elements) {
		auto column_element_child = column_element->Cast<ListParseResult>().Child<ChoiceParseResult>(0).result;
		if (column_element_child->name == "ColumnDefinition") {
			result.columns.AddColumn(transformer.Transform<ColumnDefinition>(column_element_child));
		} else if (column_element_child->name == "TopLevelConstraint") {
			result.constraints.push_back(transformer.Transform<unique_ptr<Constraint>>(column_element_child));
		} else {
			throw NotImplementedException("Unknown column type encountered: %s", column_element_child->name);
		}
	}
	return result;
}

ColumnDefinition PEGTransformerFactory::TransformColumnDefinition(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto column_name = transformer.Transform<QualifiedName>(list_pr.Child<ListParseResult>(0));
	auto type = transformer.Transform<LogicalType>(list_pr.Child<ListParseResult>(1));
	auto result = ColumnDefinition(column_name.name, type);
	return result;
}

LogicalType PEGTransformerFactory::TransformTypeOrGenerated(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto type_pr = list_pr.Child<OptionalParseResult>(0);
	auto generated_column_pr = list_pr.Child<OptionalParseResult>(1);
	LogicalType type = LogicalType::INVALID;
	if (type_pr.HasResult()) {
		type = transformer.Transform<LogicalType>(type_pr.optional_result);
	}
	// TODO(Dtenwolde) deal with generated columns
	return type;
}

} // namespace duckdb
