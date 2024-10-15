#include "duckdb/parser/peg_parser.hpp"
#include <fstream>
#include <streambuf>
#include <stdexcept>
#include <iostream>

namespace duckdb {

    std::string FileToString(const string& file_name) {
        std::ifstream str(file_name);
        if (!str.is_open()) {
            throw std::runtime_error("Cannot open file: " + file_name);
        }
        return std::string((std::istreambuf_iterator<char>(str)),
                           std::istreambuf_iterator<char>());
    }

    PEGParser::PEGParser(const string& grammar_file) {
        auto base_grammar = FileToString(grammar_file);

        parser_ = make_uniq<peg::parser>(base_grammar);
        parser_->enable_packrat_parsing();

        if (!*parser_) {
            throw std::runtime_error("Parser construction error");
        }

        // Enable error logging
        parser_->set_logger([](size_t line, size_t col, const std::string& msg) {
            std::cout << "Error on line " << line << ":" << col << " -> " << msg << '\n';
        });
    }

    void PEGParser::ParseQuery(const string &query) {
        auto input = FileToString(query);
        if (!parser_->parse(input)) {
            throw std::runtime_error("Parsing failed");
        }
    }

} // namespace duckdb