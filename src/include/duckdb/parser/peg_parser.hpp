//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/peg_parser.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include <string>
#include <memory>
#include "../../../../third_party/peg_parser/peglib.h"
#include "sql_statement.hpp"
#include "simplified_token.hpp"

namespace duckdb {

    string FileToString(const string& file_name);

    class PEGParser {
    public:
        PEGParser(const string& grammar_file);

        void ParseQuery(const string &query);

        string ToString() {
            return peg::ast_to_s(ast_);
        }
        static string PruneKeyword(const string &keyword);
        static vector<string> ReadKeywordsFromFile(const string &file_path);
        static vector<ParserKeyword> KeywordList();
        static KeywordCategory IsKeyword(const string &text);

    public:
        vector<ParserKeyword> keywords; // List of keywords
        vector<unique_ptr<SQLStatement>> statements;
        std::shared_ptr<peg::Ast> ast_;
        std::string parser_error_message; // Member to store the error message

    private:
        unique_ptr<peg::parser> parser_;
    };

} // namespace duckdb
