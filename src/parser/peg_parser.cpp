#include <duckdb/parser/peg_parser.hpp>
#include <duckdb/parser/peg_transformer.hpp>
#include <fstream>
#include <streambuf>
#include <iostream>

namespace duckdb {

    std::string FileToString(const string &file_name) {
        std::ifstream str(file_name);
        if (!str.is_open()) {
            throw IOException("Failed to open file " + file_name);
        }
        return std::string((std::istreambuf_iterator<char>(str)),
                           std::istreambuf_iterator<char>());
    }

// Constructor
    PEGParser::PEGParser(const string &grammar_file) {
        auto base_grammar = FileToString(grammar_file);

        parser_ = make_uniq<peg::parser>(base_grammar);
        parser_->enable_ast();
        parser_->enable_packrat_parsing();

        if (!*parser_) {
            throw FatalException("Parser initialization failed");
        }

        // Enable error logging and capture the error message
        parser_->set_logger([this](size_t line, size_t col, const std::string &msg) {
            std::ostringstream oss;
            oss << "Error on line " << line << ":" << col << " -> " << msg;
            parser_error_message = oss.str();  // Store the error message
        });
    }

// Method to parse a query and throw the actual error message
    void PEGParser::ParseQuery(const string &query) {
        if (parser_->parse(query, ast_)) {
            PEGTransformer::Transform(ast_->nodes[0], statements);
        } else {
            // Throw the stored error message if parsing fails
            throw ParserException("%s: %s", query, parser_error_message);
        }
    }

} // namespace duckdb