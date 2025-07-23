#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformStatement(PEGTransformer &transformer,
                                                                   ParseResult &parse_result) {
	auto &choice_pr = parse_result.Cast<ChoiceParseResult>();
	return transformer.Transform<unique_ptr<SQLStatement>>(choice_pr.result.get());
}

unique_ptr<SQLStatement> PEGTransformerFactory::Transform(vector<MatcherToken> &tokens, const char *root_rule) {
	ArenaAllocator allocator(Allocator::DefaultAllocator());
	PEGTransformerState state(tokens);
	string token_stream;
	for (auto &token : tokens) {
		token_stream += token.text + " ";
	}
	Printer::PrintF("Tokens: %s", token_stream.c_str());
	PEGTransformer transformer(allocator, state, sql_transform_functions, parser.rules, enum_mappings);
	auto root_parse_result = transformer.MatchRule(root_rule);
	if (!root_parse_result) {
		throw ParserException("Failed to parse string: No match found for root rule '%s'.", root_rule);
	}
	Printer::Print("Successfully parsed string");
	if (state.token_index < tokens.size()) {
		throw ParserException("Failed to parse string: Unconsumed tokens remaining.");
	}
	root_parse_result->name = root_rule;
	return transformer.Transform<unique_ptr<SQLStatement>>(*root_parse_result);
}

PEGTransformerFactory::PEGTransformerFactory(const char *grammar) : parser(grammar) {
	Register("Statement", &TransformStatement);
	Register("UseStatement", &TransformUseStatement);
	Register("DottedIdentifier", &TransformDottedIdentifier);
	Register("SetStatement", &TransformSetStatement);
	Register("ResetStatement", &TransformResetStatement);
	Register("DeleteStatement", &TransformDeleteStatement);
	Register("PragmaStatement", &TransformPragmaStatement);
	Register("PragmaAssign", &TransformPragmaAssign);
	Register("PragmaFunction", &TransformPragmaFunction);
	Register("PragmaParameters", &TransformPragmaParameters);
	Register("PragmaName", &TransformIdentifierOrKeyword);

	Register("StandardAssignment", &TransformStandardAssignment);
	Register("SetAssignment", &TransformSetAssignment);

	Register("SettingOrVariable", &TransformSettingOrVariable);
	Register("VariableList", &TransformVariableList);

	Register("Expression", &TransformExpression);
	Register("BaseExpression", &TransformBaseExpression);
	Register("SingleExpression", &TransformSingleExpression);

	Register("LiteralExpression", &TransformLiteralExpression);
	Register("ColumnReference", &TransformColumnReference);

	Register("SetSetting", &TransformSetSetting);
	Register("SetVariable", &TransformSetVariable);

	Register("DottedIdentifier", &TransformDottedIdentifier);
	Register("ColId", &TransformIdentifierOrKeyword);
	Register("ColLabel", &TransformIdentifierOrKeyword);
	Register("Identifier", &TransformIdentifierOrKeyword);
	Register("PlainIdentifier", &TransformIdentifierOrKeyword);
	Register("QuotedIdentifier", &TransformIdentifierOrKeyword);
	Register("ReservedKeyword", &TransformIdentifierOrKeyword);
	Register("UnreservedKeyword", &TransformIdentifierOrKeyword);
	Register("ColumnNameKeyword", &TransformIdentifierOrKeyword);
	Register("FuncNameKeyword", &TransformIdentifierOrKeyword);
	Register("TypeNameKeyword", &TransformIdentifierOrKeyword);
	Register("SettingName", &TransformIdentifierOrKeyword);

	// Enum registration
	RegisterEnum<SetScope>(
	    "SettingScope",
	    {{"LocalScope", SetScope::LOCAL}, {"SessionScope", SetScope::SESSION}, {"GlobalScope", SetScope::GLOBAL}});
	RegisterEnum<SetScope>("VariableScope", {{"VariableScope", SetScope::VARIABLE}});
	RegisterEnum<Value>("ConstantLiteral",
	                    {{"NullLiteral", Value()}, {"TrueLiteral", Value(true)}, {"FalseLiteral", Value(false)}});
}

} // namespace duckdb
