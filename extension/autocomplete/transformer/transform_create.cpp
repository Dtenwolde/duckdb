#include "duckdb/parser/parsed_data/create_table_info.hpp"
#include "duckdb/parser/statement/create_statement.hpp"
#include "transformer/peg_transformer.hpp"
#include "duckdb/parser/constraint.hpp"
#include "ast/column_element.hpp"
#include "duckdb/parser/constraints/check_constraint.hpp"
#include "duckdb/parser/constraints/foreign_key_constraint.hpp"
#include "duckdb/parser/constraints/unique_constraint.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformCreateStatement(PEGTransformer &transformer,
                                                                         optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	bool replace = list_pr.Child<OptionalParseResult>(1).HasResult();
	bool temporary = list_pr.Child<OptionalParseResult>(2).HasResult();
	auto result = transformer.Transform<unique_ptr<CreateStatement>>(list_pr.Child<ListParseResult>(3));
	if (result->info->on_conflict == OnCreateConflict::IGNORE_ON_CONFLICT && replace) {
		throw ParserException("Cannot specify both OR REPLACE and IF NOT EXISTS within single create statement");
	}
	result->info->on_conflict = replace ? OnCreateConflict::REPLACE_ON_CONFLICT : OnCreateConflict::ERROR_ON_CONFLICT;
	result->info->temporary = temporary;
	return std::move(result);
}

unique_ptr<CreateStatement>
PEGTransformerFactory::TransformCreateStatementVariation(PEGTransformer &transformer,
                                                         optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto choice_pr = list_pr.Child<ChoiceParseResult>(0);
	return transformer.Transform<unique_ptr<CreateStatement>>(choice_pr.result);
}

unique_ptr<CreateStatement> PEGTransformerFactory::TransformCreateTableStmt(PEGTransformer &transformer,
                                                                         optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();

	auto result = make_uniq<CreateStatement>();
	QualifiedName table_name = transformer.Transform<QualifiedName>(list_pr.Child<ListParseResult>(2));
	// Use appropriate constructor
	auto info = make_uniq<CreateTableInfo>(table_name.catalog, table_name.schema, table_name.name);

	bool if_not_exists = list_pr.Child<OptionalParseResult>(1).HasResult();
	info->on_conflict = if_not_exists ? OnCreateConflict::IGNORE_ON_CONFLICT : OnCreateConflict::ERROR_ON_CONFLICT;
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
	return std::move(result);
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

	// TODO(Dtenwolde) Deal with ColumnConstraint
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

unique_ptr<Constraint> PEGTransformerFactory::TransformTopLevelConstraint(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	// TODO(dtenwolde) figure out what to do with constraint name.
	auto opt_constraint_name = list_pr.Child<OptionalParseResult>(0);
	auto result = transformer.Transform<unique_ptr<Constraint>>(list_pr.Child<ListParseResult>(1));
	return result;
}

unique_ptr<Constraint> PEGTransformerFactory::TransformTopLevelConstraintList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<Constraint>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<Constraint> PEGTransformerFactory::TransformTopPrimaryKeyConstraint(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto column_list = transformer.Transform<vector<string>>(list_pr.Child<ListParseResult>(2));
	auto result = make_uniq<UniqueConstraint>(column_list, true);
	return result;
}

unique_ptr<Constraint> PEGTransformerFactory::TransformTopUniqueConstraint(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto column_list = transformer.Transform<vector<string>>(list_pr.Child<ListParseResult>(2));
	auto result = make_uniq<UniqueConstraint>(column_list, false);
	return result;
}

unique_ptr<Constraint> PEGTransformerFactory::TransformCheckConstraint(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto extract_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	auto check_expr = transformer.Transform<unique_ptr<ParsedExpression>>(extract_parens);
	auto result = make_uniq<CheckConstraint>(std::move(check_expr));
	return result;
}

unique_ptr<Constraint> PEGTransformerFactory::TransformTopForeignKeyConstraint(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto fk_list = transformer.Transform<vector<string>>(list_pr.Child<ListParseResult>(2));

	auto table_name = transformer.Transform<unique_ptr<BaseTableRef>>(list_pr.Child<ListParseResult>(4));
	auto opt_pk_list = list_pr.Child<OptionalParseResult>(5);
	vector<string> pk_list;
	if (opt_pk_list.HasResult()) {
		pk_list = transformer.Transform<vector<string>>(opt_pk_list.optional_result);
	}
	auto key_actions = list_pr.Child<OptionalParseResult>(6);
	// TODO(Dtenwolde) do something with key actions

	ForeignKeyInfo fk_info;
	fk_info.table = table_name->table_name;
	fk_info.schema = table_name->schema_name;
	// TODO(Dtenwolde) unsure about the fk type or how to figure this out.
	fk_info.type = ForeignKeyType::FK_TYPE_FOREIGN_KEY_TABLE;
	return make_uniq<ForeignKeyConstraint>(pk_list, fk_list, fk_info);
}

vector<string> PEGTransformerFactory::TransformColumnIdList(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	vector<string> result;
	auto colid_list = ExtractResultFromParens(list_pr.Child<ListParseResult>(0));
	auto colids = ExtractParseResultsFromList(colid_list);
	for (auto colid : colids) {
		result.push_back(transformer.Transform<string>(colid));
	}
	return result;
}

LogicalType PEGTransformerFactory::TransformBitType(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &opt_varying = list_pr.Child<OptionalParseResult>(1);
	if (opt_varying.HasResult()) {
		throw ParserException("Type with name varbit does not exist.");
	}
	auto &opt_modifiers = list_pr.Child<OptionalParseResult>(2);
	if (opt_modifiers.HasResult()) {
		throw ParserException("Type BIT does not support any modifiers.");
	}
	return LogicalType(LogicalTypeId::BIT);
}

} // namespace duckdb
