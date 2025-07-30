#include "transformer/peg_transformer.hpp"
#include "matcher.hpp"

namespace duckdb {

unique_ptr<SQLStatement> PEGTransformerFactory::TransformStatement(PEGTransformer &transformer,
                                                                   optional_ptr<ParseResult> parse_result) {
	auto &list_pr = parse_result->Cast<ListParseResult>();
	auto &choice_pr = list_pr.Child<ChoiceParseResult>(0);
	return transformer.Transform<unique_ptr<SQLStatement>>(choice_pr.result);
}

unique_ptr<SQLStatement> PEGTransformerFactory::Transform(vector<MatcherToken> &tokens, const char *root_rule) {
	string token_stream;
	for (auto &token : tokens) {
		token_stream += token.text + " ";
	}

	vector<MatcherSuggestion> suggestions;
	ParseResultAllocator parse_result_allocator;
	MatchState state(tokens, suggestions, parse_result_allocator);
	MatcherAllocator allocator;
	auto &matcher = Matcher::RootMatcher(allocator);
	auto match_result = matcher.MatchParseResult(state);
	if (match_result == nullptr || state.token_index < state.tokens.size()) {
		// TODO(dtenwolde) add error handling
		throw ParserException("Not all tokens were matched");
	}
	match_result->name = "Statement";
	ArenaAllocator transformer_allocator(Allocator::DefaultAllocator());
	PEGTransformerState transformer_state(tokens);
	PEGTransformer transformer(transformer_allocator, transformer_state, sql_transform_functions, parser.rules, enum_mappings);
	return transformer.Transform<unique_ptr<SQLStatement>>(match_result);
}

PEGTransformerFactory::PEGTransformerFactory() {
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
	Register("UseTarget", &TransformUseTarget);

	Register("DetachStatement", &TransformDetachStatement);
	Register("AttachStatement", &TransformAttachStatement);
	Register("AttachAlias", &TransformAttachAlias);

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
	RegisterEnum<SetScope>("LocalScope", SetScope::LOCAL);
	RegisterEnum<SetScope>("GlobalScope", SetScope::GLOBAL);
	RegisterEnum<SetScope>("SessionScope", SetScope::SESSION);
	RegisterEnum<SetScope>("VariableScope", SetScope::VARIABLE);

	RegisterEnum<Value>("FalseLiteral", Value(false));
	RegisterEnum<Value>("TrueLiteral", Value(true));
	RegisterEnum<Value>("NullLiteral", Value());
}

} // namespace duckdb
