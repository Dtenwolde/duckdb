#include "parser/peg_parser_override.hpp"
#include "tokenizer.hpp"
#include "duckdb/common/exception/binder_exception.hpp"
#include "inlined_grammar.hpp"

namespace duckdb {
struct MatcherToken;

PEGParserOverride::PEGParserOverride() {
	const char grammar[] = {
		"List(D) <- D (',' D)* ','?\n"
		"UseStatement <- 'USE'i UseTarget\n"
		"UseTarget <- (CatalogName '.' ReservedSchemaName) / SchemaName / CatalogName / { 'Expected \"USE database\" or \"USE database.schema\"' }\n"
		"CatalogName <- Identifier\n"
		"SchemaName <- Identifier\n"
		"ReservedSchemaName <- Identifier\n"
		"Identifier <- [a-z_A-Z]\n"
	};
	// auto grammar = const_char_ptr_cast(INLINED_PEG_GRAMMAR);
	factory = make_uniq<PEGTransformerFactory>(grammar);
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
			auto statement = factory->Transform(tokenizer.statements[0], "Statement");
			result.push_back(std::move(statement));
		}
		return result;
	} catch (const ParserException &) {
		throw;
	}
}

} // namespace duckdb
