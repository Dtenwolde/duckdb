#include "transformer/peg_transformer.hpp"

namespace duckdb {

ExpressionType PEGTransformerFactory::TransformOperator(PEGTransformer &transformer,
                                                        optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	if (choice_pr.result->type == ParseResultType::OPERATOR) {
		return OperatorToExpressionType(choice_pr.result->Cast<OperatorParseResult>().operator_token);
	}
	return transformer.Transform<ExpressionType>(choice_pr.result);
}

ExpressionType PEGTransformerFactory::TransformConjunctionOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.TransformEnum<ExpressionType>(list_pr.Child<ChoiceParseResult>(0).result);
}

} // namespace duckdb
