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
//#include "peglib.h"  // Assuming you are using peglib
#include "../../../../third_party/peg_parser/peglib.h"
#include "sql_statement.hpp"

namespace duckdb {

    string FileToString(const string& file_name);

    class PEGParser {
    public:
        PEGParser(const string& grammar_file);

        void ParseQuery(const string &query);

    public:
        vector<unique_ptr<SQLStatement>> statements;

    private:
        unique_ptr<peg::parser> parser_;
    };

} // namespace duckdb
