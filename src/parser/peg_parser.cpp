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

    vector<string> PEGParser::ReadKeywordsFromFile(const string &file_path) {
        vector<string> keywords;
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw IOException("Failed to open file " + file_path);
        }

        string line;
        while (getline(file, line)) {
            if (!line.empty()) {
                keywords.push_back(line);
            }
        }
        file.close();
        return keywords;
    }

    string PEGParser::PruneKeyword(const string &keyword) {
        // If the keyword ends with _P, remove the _P suffix
        if (keyword.size() > 2 && keyword.substr(keyword.size() - 2) == "_P") {
            return keyword.substr(0, keyword.size() - 2);
        }
        return keyword;
    }


    vector<ParserKeyword> PEGParser::KeywordList() {
        vector<ParserKeyword> entries;
        // Read keywords from the files
        auto reserved_keywords = ReadKeywordsFromFile("third_party/peg_parser/keywords/reserved_keywords.list");
        auto unreserved_keywords = ReadKeywordsFromFile("third_party/peg_parser/keywords/unreserved_keywords.list");

        // Process reserved keywords
        for (auto &keyword : reserved_keywords) {
            string pruned_keyword = PruneKeyword(keyword);
            entries.emplace_back(pruned_keyword, KeywordCategory::KEYWORD_RESERVED);
        }

        // Process unreserved keywords
        for (auto &keyword : unreserved_keywords) {
            string pruned_keyword = PruneKeyword(keyword);
            entries.emplace_back(pruned_keyword, KeywordCategory::KEYWORD_UNRESERVED);
        }

        return entries;
    }

    KeywordCategory PEGParser::IsKeyword(const string &text) {
        auto reserved_keywords = ReadKeywordsFromFile("third_party/peg_parser/keywords/reserved_keywords.list");
        auto unreserved_keywords = ReadKeywordsFromFile("third_party/peg_parser/keywords/unreserved_keywords.list");

        // Process reserved keywords
        for (auto &keyword : reserved_keywords) {
            string pruned_keyword = PruneKeyword(keyword);
            if (pruned_keyword == text) {
                return KeywordCategory::KEYWORD_RESERVED;
            }
        }

        // Process unreserved keywords
        for (auto &keyword : unreserved_keywords) {
            string pruned_keyword = PruneKeyword(keyword);
            if (pruned_keyword == text) {
                return KeywordCategory::KEYWORD_UNRESERVED;
            }
        }

        return KeywordCategory::KEYWORD_NONE;

    }
} // namespace duckdb