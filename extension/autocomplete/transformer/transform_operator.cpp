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

ExpressionType PEGTransformerFactory::TransformIsOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	bool is_not = list_pr.Child<OptionalParseResult>(1).HasResult();
	bool is_distinct = list_pr.Child<OptionalParseResult>(2).HasResult();
	if (is_distinct && is_not) {
		return ExpressionType::COMPARE_NOT_DISTINCT_FROM;
	} if (is_distinct) {
		return ExpressionType::COMPARE_DISTINCT_FROM;
	} if (is_not) {
		return ExpressionType::OPERATOR_IS_NOT_NULL;
	}
	return ExpressionType::OPERATOR_IS_NULL;
}

ExpressionType PEGTransformerFactory::TransformLikeOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	bool is_not = list_pr.Child<OptionalParseResult>(0).HasResult();
	ExpressionType result = transformer.Transform<ExpressionType>(list_pr.Child<ListParseResult>(1));
	if (is_not) {
		if (result == ExpressionType::OPERATOR_LIKE) {
			return ExpressionType::OPERATOR_NOT_LIKE;
		} if (result == ExpressionType::OPERATOR_ILIKE) {
			return ExpressionType::OPERATOR_NOT_ILIKE;
		} if (result == ExpressionType::OPERATOR_SIMILAR_TO) {
			return ExpressionType::OPERATOR_NOT_SIMILAR_TO;
		}
		throw ParserException("Unsupported type for LikeOperator");
	}
	return result;
}

ExpressionType PEGTransformerFactory::TransformLikeOrSimilarTo(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	return transformer.TransformEnum<ExpressionType>(list_pr.Child<ChoiceParseResult>(0).result);
}

ExpressionType PEGTransformerFactory::TransformInOperator(PEGTransformer &transformer, optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto is_not = list_pr.Child<OptionalParseResult>(0).HasResult();
	if (is_not) {
		return ExpressionType::COMPARE_NOT_IN;
	}
	return ExpressionType::COMPARE_IN;
}

} // namespace duckdb
