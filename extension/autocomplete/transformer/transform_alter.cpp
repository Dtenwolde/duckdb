#include "transformer/peg_transformer.hpp"
#include "duckdb/parser/statement/alter_statement.hpp"
#include "duckdb/parser/parsed_data/alter_info.hpp"
#include "duckdb/parser/parsed_data/alter_table_info.hpp"

namespace duckdb {
unique_ptr<SQLStatement> PEGTransformerFactory::TransformAlterStatement(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = make_uniq<AlterStatement>();
	result->info = transformer.Transform<unique_ptr<AlterInfo>>(list_pr.Child<ListParseResult>(1));
	return result;
}

unique_ptr<AlterInfo> PEGTransformerFactory::TransformAlterOptions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<AlterInfo>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<AlterInfo> PEGTransformerFactory::TransformAlterTableStmt(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto if_exists = list_pr.Child<OptionalParseResult>(1).HasResult();
	auto table = transformer.Transform<unique_ptr<BaseTableRef>>(list_pr.Child<ListParseResult>(2));

	auto result = transformer.Transform<unique_ptr<AlterInfo>>(list_pr.Child<ListParseResult>(3));
	result->if_not_found = if_exists ? OnEntryNotFound::RETURN_NULL : OnEntryNotFound::THROW_EXCEPTION;
	result->catalog = table->catalog_name;
	result->schema = table->schema_name;
	result->name = table->table_name;

	return result;
}

unique_ptr<AlterInfo> PEGTransformerFactory::TransformAlterTableOptions(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<AlterInfo>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<AlterInfo> PEGTransformerFactory::TransformAddColumn(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	ColumnDefinition new_column = transformer.Transform<ColumnDefinition>(list_pr.Child<ListParseResult>(3));
	bool if_not_exists = list_pr.Child<OptionalParseResult>(1).HasResult();
	auto result = make_uniq<AddColumnInfo>(AlterEntryData(), std::move(new_column), if_not_exists);
	return result;
}

unique_ptr<AlterInfo> PEGTransformerFactory::TransformDropColumn(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	bool cascade = false;
	transformer.TransformOptional<bool>(list_pr, 4, cascade);
	bool if_exists = list_pr.Child<OptionalParseResult>(1).HasResult();
	auto nested_column = transformer.Transform<vector<string>>(list_pr.Child<ListParseResult>(3));
	if (nested_column.size() == 1) {
		auto result = make_uniq<RemoveColumnInfo>(AlterEntryData(), nested_column[0], if_exists, cascade);
		return result;
	} else {
		auto result = make_uniq<RemoveFieldInfo>(AlterEntryData(), nested_column, if_exists, cascade);
		return result;
	}
}

vector<string> PEGTransformerFactory::TransformNestedColumnName(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto opt_identifier = list_pr.Child<OptionalParseResult>(0);
	vector<string> result;
	if (opt_identifier.HasResult()) {
		auto repeat_pr = opt_identifier.optional_result->Cast<RepeatParseResult>();
		for (auto child : repeat_pr.children) {
			result.push_back(child->Cast<IdentifierParseResult>().identifier + ".");
		}
	}
	result.push_back(list_pr.Child<IdentifierParseResult>(1).identifier);
	return result;
}

unique_ptr<AlterInfo> PEGTransformerFactory::TransformAlterColumn(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	throw NotImplementedException("AlterColumn not implemented");
}

unique_ptr<AlterInfo> PEGTransformerFactory::TransformRenameColumn(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto nested_column = transformer.Transform<vector<string>>(list_pr.Child<ListParseResult>(2));
	auto new_column_name = list_pr.Child<IdentifierParseResult>(4).identifier;
	if (nested_column.size() == 1) {
		auto result = make_uniq<RenameColumnInfo>(AlterEntryData(), nested_column[0], new_column_name);
		return result;
	} else {
		auto result = make_uniq<RenameFieldInfo>(AlterEntryData(), nested_column, new_column_name);
		return result;
	}
}

unique_ptr<AlterInfo> PEGTransformerFactory::TransformRenameAlter(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto new_table_name = list_pr.Child<IdentifierParseResult>(2).identifier;
	auto result = make_uniq<RenameTableInfo>(AlterEntryData(), new_table_name);
	return result;
}


}