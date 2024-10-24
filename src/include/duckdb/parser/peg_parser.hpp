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
#include "peglib.h"
#include "sql_statement.hpp"
#include "simplified_token.hpp"
#include <duckdb/parser/group_by_node.hpp>
#include <duckdb/parser/result_modifier.hpp>


namespace duckdb {

    string FileToString(const string& file_name);

    class PEGParser {
    public:
        PEGParser();

        void ParseQuery(const string &query);

        string ToString() {
            return peg::ast_to_s(ast_);
        }
        static string PruneKeyword(const string &keyword);
        static vector<string> ReadKeywordsFromFile(const string &file_path);
        static vector<ParserKeyword> KeywordList();
        static KeywordCategory IsKeyword(const string &text);
        static vector<unique_ptr<ParsedExpression>> ParseExpressionList(const string &select_list);
        static vector<OrderByNode> ParseOrderList(const string &select_list);
        static GroupByNode ParseGroupByList(const string &group_by);
        static vector<SimplifiedToken> Tokenize(const string &query);

    public:
        vector<ParserKeyword> keywords; // List of keywords
        vector<unique_ptr<SQLStatement>> statements;
        std::shared_ptr<peg::Ast> ast_;
        std::string parser_error_message; // Member to store the error message

    private:
        unique_ptr<peg::parser> parser_;
    };

} // namespace duckdb
