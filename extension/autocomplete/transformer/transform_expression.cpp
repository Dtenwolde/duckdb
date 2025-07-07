

namespace duckdb {

unique_ptr<ParsedExpression> PEGTransformerFactory::TransformExpression(PEGTransformer &transformer, ParseResult &parse_result) {
	// Leaf: Identifier
	auto &identifier_pr = parse_result.Cast<IdentifierParseResult>();
	return make_uniq<ConstantExpression>(Value(identifier_pr.identifier));
}

}