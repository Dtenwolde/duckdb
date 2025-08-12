#include "duckdb/function/function.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/statement/call_statement.hpp"
#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformCallStatement(PEGTransformer &transformer,
                                                                       optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto table_function_name = list_pr.Child<IdentifierParseResult>(1).identifier;
	auto function_children =
	    transformer.Transform<vector<unique_ptr<ParsedExpression>>>(list_pr.Child<ListParseResult>(2));
	auto result = make_uniq<CallStatement>();
	auto function_expression = make_uniq<FunctionExpression>(table_function_name, std::move(function_children));
	result->function = std::move(function_expression);
	return result;
}

vector<unique_ptr<ParsedExpression>>
PEGTransformerFactory::TransformTableFunctionArguments(PEGTransformer &transformer,
                                                       optional_ptr<ParseResult> parse_result) {
	// TableFunctionArguments <- Parens(List(FunctionArgument)?)
	vector<unique_ptr<ParsedExpression>> result;
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto stripped_parens = ExtractResultFromParens(list_pr.Child<ListParseResult>(0))->Cast<OptionalParseResult>();
	if (stripped_parens.HasResult()) {
		auto argument_list = ExtractParseResultsFromList(stripped_parens.optional_result);
		for (auto &argument : argument_list) {
			result.push_back(transformer.Transform<unique_ptr<ParsedExpression>>(argument));
		}
	}
	return result;
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformFunctionArgument(PEGTransformer &transformer,
                                                                              optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ChoiceParseResult>(0).result);
}

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformNamedParameter(PEGTransformer &transformer,
                                                                            optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto result = transformer.Transform<unique_ptr<ParsedExpression>>(list_pr.Child<ListParseResult>(2));
	result->alias = list_pr.Child<IdentifierParseResult>(0).identifier;
	return result;
}

} // namespace duckdb
