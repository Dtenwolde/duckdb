#include "transformer/peg_transformer.hpp"
#include "matcher.hpp"
#include "duckdb/parser/parsed_data/transaction_info.hpp"

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

#define REGISTER_TRANSFORM(FUNCTION) \
Register(string(#FUNCTION).substr(9), &FUNCTION)


PEGTransformerFactory::PEGTransformerFactory() {
    // Registering transform functions using the macro for brevity
    REGISTER_TRANSFORM(TransformStatement);
    REGISTER_TRANSFORM(TransformUseStatement);
    REGISTER_TRANSFORM(TransformDottedIdentifier);
    REGISTER_TRANSFORM(TransformSetStatement);
    REGISTER_TRANSFORM(TransformResetStatement);
    REGISTER_TRANSFORM(TransformDeleteStatement);
    REGISTER_TRANSFORM(TransformPragmaStatement);
    REGISTER_TRANSFORM(TransformPragmaAssign);
    REGISTER_TRANSFORM(TransformPragmaFunction);
    REGISTER_TRANSFORM(TransformPragmaParameters);
    REGISTER_TRANSFORM(TransformUseTarget);
    REGISTER_TRANSFORM(TransformDetachStatement);
    REGISTER_TRANSFORM(TransformAttachStatement);
    REGISTER_TRANSFORM(TransformAttachAlias);
    REGISTER_TRANSFORM(TransformAttachOptions);
    REGISTER_TRANSFORM(TransformGenericCopyOptionList);
    REGISTER_TRANSFORM(TransformGenericCopyOption);
    REGISTER_TRANSFORM(TransformCheckpointStatement);
    REGISTER_TRANSFORM(TransformExportStatement);
    REGISTER_TRANSFORM(TransformImportStatement);
    REGISTER_TRANSFORM(TransformExportSource);
    REGISTER_TRANSFORM(TransformTransactionStatement);
    REGISTER_TRANSFORM(TransformBeginTransaction);
    REGISTER_TRANSFORM(TransformRollbackTransaction);
    REGISTER_TRANSFORM(TransformCommitTransaction);
    REGISTER_TRANSFORM(TransformReadOrWrite);
    REGISTER_TRANSFORM(TransformLoadStatement);
    REGISTER_TRANSFORM(TransformInstallStatement);
    REGISTER_TRANSFORM(TransformFromSource);
    REGISTER_TRANSFORM(TransformVersionNumber);
    REGISTER_TRANSFORM(TransformTruncateStatement);
    REGISTER_TRANSFORM(TransformBaseTableName);
    REGISTER_TRANSFORM(TransformStandardAssignment);
    REGISTER_TRANSFORM(TransformSetAssignment);
    REGISTER_TRANSFORM(TransformSettingOrVariable);
    REGISTER_TRANSFORM(TransformVariableList);
    REGISTER_TRANSFORM(TransformExpression);
    REGISTER_TRANSFORM(TransformBaseExpression);
    REGISTER_TRANSFORM(TransformSingleExpression);
    REGISTER_TRANSFORM(TransformLiteralExpression);
    REGISTER_TRANSFORM(TransformColumnReference);
    REGISTER_TRANSFORM(TransformColIdOrString);
    REGISTER_TRANSFORM(TransformIdentifierOrStringLiteral);
    REGISTER_TRANSFORM(TransformColId);
    REGISTER_TRANSFORM(TransformStringLiteral);
    REGISTER_TRANSFORM(TransformIdentifier);
    REGISTER_TRANSFORM(TransformSetSetting);
    REGISTER_TRANSFORM(TransformSetVariable);

    // Manual registration for mismatched names or special cases
    Register("PragmaName", &TransformIdentifierOrKeyword);
    Register("ColLabel", &TransformIdentifierOrKeyword);
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

    RegisterEnum<TransactionModifierType>("ReadOnly", TransactionModifierType::TRANSACTION_READ_ONLY);
    RegisterEnum<TransactionModifierType>("ReadWrite", TransactionModifierType::TRANSACTION_READ_WRITE);
}

} // namespace duckdb
