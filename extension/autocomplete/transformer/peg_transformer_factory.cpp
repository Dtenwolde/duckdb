#include "transformer/peg_transformer.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformRoot(PEGTransformer &transformer, ParseResult &parse_result) {
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
	ParseResult *root_parse_result = transformer.MatchRule(root_rule);
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
	// Register all transform functions with their expected return types
	Register("Root", &TransformRoot);
	Register("UseStatement", &TransformUseStatement);
	Register("DottedIdentifier", &TransformDottedIdentifier);
	Register("SetStatement", &TransformSetStatement);
	Register("ResetStatement", &TransformResetStatement);
	Register("DeleteStatement", &TransformDeleteStatement);

	// Intermediate composers
	Register("StandardAssignment", &TransformStandardAssignment);
	Register("SetAssignment", &TransformSetAssignment);

	// Dispatchers that return semantic structs/values
	Register("SettingOrVariable", &TransformSettingOrVariable);
	Register("VariableList", &TransformVariableList);
	Register("Expression", &TransformExpression);

	// Leaf transformers that return semantic structs/values
	Register("SetSetting", &TransformSetSetting);
	Register("SetVariable", &TransformSetVariable);

	// Enum registration
	RegisterEnum<SetScope>("SettingScope",
						   {{"LocalScope", SetScope::LOCAL},
							{"SessionScope", SetScope::SESSION},
							{"GlobalScope", SetScope::GLOBAL}
						   });
	RegisterEnum<SetScope>("VariableScope", {{"VariableScope", SetScope::VARIABLE}});
}

}