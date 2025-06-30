#include "include/peg_parser_override.hpp"
#include "include/tokenizer.hpp"
#include "duckdb/common/exception/binder_exception.hpp"

namespace duckdb {
struct MatcherToken;

PEGParserOverride::PEGParserOverride() {
	const char grammar[] = {
		"Root <- UseStatement / DeleteStatement\n"
		"UseStatement <- 'USE'i UseTarget\n"
		"DeleteStatement <- 'DELETE'i Identifier\n"
		"UseTarget <- (CatalogName '.' SchemaName) / SchemaName / CatalogName\n"
		"CatalogName <- Identifier\n"
		"SchemaName <- Identifier\n"
		"Identifier <- [a-z_A-Z]\n"
	};
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
			auto statement = factory->Transform(tokenizer.statements[0], "Root");
			Printer::PrintF("Successfully transformed statement: %s", statement->ToString());
			result.push_back(std::move(statement));
		}
		return result;
	} catch (const ParserException &) {
		throw;
	}
}

} // namespace duckdb