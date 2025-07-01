#include "include/peg_parser_override.hpp"
#include "include/tokenizer.hpp"
#include "duckdb/common/exception/binder_exception.hpp"

namespace duckdb {
struct MatcherToken;

PEGParserOverride::PEGParserOverride() {
	const char grammar[] = {
		"Root <- UseStatement / DeleteStatement / SetStatement / ResetStatement\n"
		"UseStatement <- 'USE'i UseTarget\n"
		"DeleteStatement <- 'DELETE'i Identifier\n"
		"UseTarget <- DottedIdentifier\n"
		"SetStatement <- 'SET'i (StandardAssignment / SetTimeZone)\n"
		"StandardAssignment <- (SetVariable / SetSetting) SetAssignment\n"
		"SetTimeZone <- 'TIME'i 'ZONE'i Expression\n"
		"SetSetting <- SettingScope? SettingName\n"
		"SetVariable <- 'VARIABLE'i SettingName\n"
		"SettingScope <- 'LOCAL'i / 'SESSION'i / 'GLOBAL'i\n"
		"SetAssignment <- VariableAssign VariableList\n"
		"VariableAssign <- '=' / 'TO'\n"
		"VariableList <- List(Expression)\n"
		"ResetStatement <- 'RESET'i (SetVariable / SetSetting)\n"
		"DottedIdentifier <- Identifier ('.' Identifier)*\n"
		"Identifier <- [a-z_A-Z]\n"
		"List(D) <- D (',' D)* ','?\n"
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
			result.push_back(std::move(statement));
		}
		return result;
	} catch (const ParserException &) {
		throw;
	}
}

} // namespace duckdb