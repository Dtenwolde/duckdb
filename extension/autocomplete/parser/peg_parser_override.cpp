#include "../include/peg_parser_override.hpp"
#include "../include/tokenizer.hpp"
#include "duckdb/common/exception/binder_exception.hpp"
#include "../include/inlined_grammar.hpp"

namespace duckdb {
struct MatcherToken;

PEGParserOverride::PEGParserOverride() {
/*
	const char grammar[] = {
		"List(D) <- D (',' D)* ','?\n"
		"Statement <- UseStatement / DeleteStatement / SetStatement / ResetStatement\n"
		"UseStatement <- 'USE'i DottedIdentifier\n"
		"DeleteStatement <- 'DELETE'i Identifier\n"
		"UseTarget <- DottedIdentifier\n"
		"SetStatement <- 'SET'i (StandardAssignment / SetTimeZone)\n"
		"StandardAssignment <- (SetVariable / SetSetting) SetAssignment\n"
		"SetTimeZone <- 'TIME'i 'ZONE'i Expression\n"
		"SetSetting <- SettingScope? SettingName\n"
		"SetVariable <- VariableScope SettingName\n"
		"SettingName <- Identifier\n"
		"SettingScope <- LocalScope / SessionScope / GlobalScope\n"
		"LocalScope <- 'LOCAL'i\n"
		"SessionScope <- 'SESSION'i\n"
		"GlobalScope <- 'GLOBAL'i\n"
		"VariableScope <- 'VARIABLE'i\n"
		"SetAssignment <- VariableAssign VariableList\n"
		"VariableAssign <- '=' / 'TO'\n"
		"VariableList <- List(Expression)\n"
		"Expression <- Identifier\n"
		"ResetStatement <- 'RESET'i (SetVariable / SetSetting)\n"
		"DottedIdentifier <- Identifier ('.' Identifier)*\n"
		"Identifier <- [a-z_A-Z]\n"
	};

	*/
	/*const char grammar[] = {
		"List(D) <- D (',' D)* ','?\n"
		"Parens(D) <- '(' D ')'\n"
		"Identifier <- [a-z_A-Z]\n"
		"IdentParen <- Parens(Identifier? Identifier)\n"
		"FilterClause <- Parens('WHERE'i? Identifier)\n"
		"IdentList <- List(Identifier)\n"
		"ExcludeList <- 'EXCLUDE'i (Parens(List(ExcludeName)) / ExcludeName)\n"
		"IdentListParen <- Parens(List(Identifier))\n"
		"DottedIdentifier <- Identifier ('.' Identifier)*\n"
	};
	*/

	auto grammar = const_char_ptr_cast(INLINED_PEG_GRAMMAR);
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