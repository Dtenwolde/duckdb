
#include "duckdb/parser/statement/pragma_statement.hpp"
namespace duckdb {
unique_ptr<SQLStatement> PEGTransformerFactory::TransformPragmaStatement(PEGTransformer &transformer,
                                                                         ParseResult &parse_result) {
	// Rule: PragmaStatement <- 'PRAGMA'i (PragmaAssign / PragmaFunction)
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto &matched_child = list_pr.Child<ChoiceParseResult>(1);
	return transformer.Transform<unique_ptr<SQLStatement>>(matched_child.result);
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformPragmaAssign(PEGTransformer &transformer,
																		ParseResult &parse_result) {
	// Rule: PragmaAssign <- SettingName '=' Expression
	auto &list_pr = parse_result.Cast<ListParseResult>();
	auto name = transformer.Transform<string>(list_pr.children[0]);
	auto value = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.children[2]);
	auto result = make_uniq<SetVariableStatement>(std::move(name), std::move(value), SetScope::AUTOMATIC);
	return result;
}

unique_ptr<SQLStatement> PEGTransformerFactory::TransformPragmaFunction(PEGTransformer &transformer,
																		ParseResult &parse_result) {
	// Rule: PragmaFunction <- PragmaName PragmaParameters?
	auto result = make_uniq<PragmaStatement>();
	auto &list_pr = parse_result.Cast<ListParseResult>();
	result->info->name = transformer.Transform<string>(list_pr.children[0]);
	auto &optional_parameters_pr = list_pr.Child<OptionalParseResult>(1);
	if (optional_parameters_pr.optional_result) {
		result->info->parameters = transformer.Transform<vector<unique_ptr<ParsedExpression>>>(*optional_parameters_pr.optional_result);
	}
	return result;
}

vector<unique_ptr<ParsedExpression>> PEGTransformerFactory::TransformPragmaParameters(PEGTransformer &transformer,
																						ParseResult &parse_result) {
	// PragmaParameters <- List(Expression)

	throw NotImplementedException("TransformPragmaParameters not implemented");
}

} // namespace duckdb
