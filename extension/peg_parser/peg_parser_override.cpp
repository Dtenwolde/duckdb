#include "peg_parser_override.hpp"
#include "duckdb/main/client_context.hpp"
#include "tokenizer.hpp" // The tokenizer you've been using

namespace duckdb {
struct MatcherToken;

PEGParserOverride::PEGParserOverride() {
	// The grammar is now owned by the factory, which is owned by the override instance.
	const char grammar[] = {
		"Root <- UseStatement \n" // Can be extended with / OtherStatement
		"UseStatement <- 'USE'i Identifier\n"
		"Identifier <- [a-z_A-Z][a-z_A-Z0-9]*\n"
	};
	factory = make_uniq<PEGTransformerFactory>(grammar);
}

vector<unique_ptr<SQLStatement>> PEGParserOverride::Parse(ClientContext &context, const string &query) {
	// 1. Tokenize the input string.
	vector<MatcherToken> root_tokens;
	string clean_sql;
	// NOTE: StripUnicodeSpaces is not a public DuckDB helper, so you may need to
	// copy its implementation into your extension if it's not accessible.
	// For now, we assume it's available or we just use the raw query.
	ParserTokenizer tokenizer(query, root_tokens);
	tokenizer.TokenizeInput();
	tokenizer.statements.push_back(std::move(root_tokens));

	// For now, we only handle single-statement queries with this parser.
	if (tokenizer.statements.size() != 1 || tokenizer.statements[0].empty()) {
		// Not a single, simple statement. Let the default parser handle it.
		return {};
	}

	try {
		// 2. Attempt to transform the tokens using our PEG parser.
		auto statement = factory->Transform(tokenizer.statements[0], "Root");
		vector<unique_ptr<SQLStatement>> result;
		result.push_back(std::move(statement));
		return result;
	} catch (const BinderException &) {
		// The PEG parser failed to match. This isn't an error; it just means
		// this query is not for us. Return an empty vector to fall back.
		Printer::PrintF("Failed to bind: %s", query);
		return {};
	}
}

} // namespace duckdb