#include "parser/peg_parser_override.hpp"
#include "tokenizer.hpp"
#include "duckdb/common/exception/binder_exception.hpp"
#include "inlined_grammar.hpp"

namespace duckdb {
struct MatcherToken;

PEGParserOverride::PEGParserOverride(unique_ptr<ParserOverrideOptions> options, ClientContext &context)
    : ParserOverride(std::move(options), context) {
	factory = make_uniq<PEGTransformerFactory>();
}

vector<unique_ptr<SQLStatement>> PEGParserOverride::Parse(const string &query) {
	vector<MatcherToken> root_tokens;
	string clean_sql;

	ParserTokenizer tokenizer(query, root_tokens);
	tokenizer.TokenizeInput();
	tokenizer.statements.push_back(std::move(root_tokens));

	vector<unique_ptr<SQLStatement>> result;
	try {
		for (auto tokenized_statement : tokenizer.statements) {
			if (tokenized_statement.empty()) {
				continue;
			}
			// Printer::PrintF("Trying to transform: %s", query);
			auto statement = factory->Transform(tokenizer.statements[0], "Statement");
			// Printer::PrintF("Successfully transformed: %s", statement->ToString());
			result.push_back(std::move(statement));
		}
		return result;
	} catch (const ParserException &) {
		throw;
	}
}

} // namespace duckdb
