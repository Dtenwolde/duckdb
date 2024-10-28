#include <duckdb/parser/peg_parser.hpp>
#include <duckdb/parser/peg_transformer.hpp>
#include "sql_grammar.hpp"
#include <fstream>
#include <chrono>
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

    // Constructor: Initialize the parser using the embedded grammar
    PEGParser::PEGParser() {
        // Start the timer
        auto start_time = std::chrono::high_resolution_clock::now();

        // Use the grammar defined in sql_grammar.hpp
        parser_ = make_uniq<peg::parser>(sql_grammar);
        parser_->enable_ast();
        parser_->enable_packrat_parsing();

        if (!*parser_) {
            throw FatalException("Parser initialization failed");
        }

        // Enable error logging and capture the error message
        parser_->set_logger([this](size_t line, size_t col, const std::string &msg) {
            std::ostringstream oss;
            oss << "Error on line " << line << ":" << col << " -> " << msg;
            parser_error_message = oss.str(); // Store the error message
        });

        // Stop the timer
        auto end_time = std::chrono::high_resolution_clock::now();

        // Calculate the duration
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        // Print or log the duration
        std::cout << "PEGParser initialization took " << duration << " milliseconds." << std::endl;
    }

    // Method to parse a query and throw the actual error message
    void PEGParser::ParseQuery(const string &query) {
        std::cout << query << std::endl;
        if (parser_->parse(query, ast_)) {
            PEGTransformer::Transform(ast_, statements);
            std::cout << "Transform finished" << std::endl;
        }
        else {
            // Throw the stored error message if parsing fails
            throw ParserException("%s: %s", query, parser_error_message);
        }
    }

    vector<unique_ptr<ParsedExpression> > PEGParser::ParseExpressionList(const string &select_list) {
        // construct a mock query prefixed with SELECT
        string mock_query = "SELECT " + select_list;
        // parse the query
        PEGParser parser;
        parser.ParseQuery(mock_query);
        // check the statements
        if (parser.statements.size() != 1 || parser.statements[0]->type != StatementType::SELECT_STATEMENT) {
            throw ParserException("Expected a single SELECT statement");
        }
        auto &select = parser.statements[0]->Cast<SelectStatement>();
        if (select.node->type != QueryNodeType::SELECT_NODE) {
            throw ParserException("Expected a single SELECT node");
        }
        auto &select_node = select.node->Cast<SelectNode>();
        return std::move(select_node.select_list);
    }

    GroupByNode PEGParser::ParseGroupByList(const string &group_by) {
        // construct a mock SELECT query with our group_by expressions
        string mock_query = StringUtil::Format("SELECT 42 GROUP BY %s", group_by);
        // parse the query
        PEGParser parser;
        parser.ParseQuery(mock_query);
        // check the result
        if (parser.statements.size() != 1 || parser.statements[0]->type != StatementType::SELECT_STATEMENT) {
            throw ParserException("Expected a single SELECT statement");
        }
        auto &select = parser.statements[0]->Cast<SelectStatement>();
        D_ASSERT(select.node->type == QueryNodeType::SELECT_NODE);
        auto &select_node = select.node->Cast<SelectNode>();
        return std::move(select_node.groups);
    }

    vector<SimplifiedToken> PEGParser::Tokenize(const string &query) {
        // Create a parser object using the loaded grammar
        static peg::parser parser(sql_grammar);
        if (!parser) {
            throw InternalException("Failed to create PEG parser from the grammar.");
        }

        // Vector to store the resulting tokens
        vector<SimplifiedToken> result;

        // Utility function to add tokens
        auto add_token = [&](const peg::SemanticValues &sv, SimplifiedTokenType type) {
            SimplifiedToken token;
            token.type = type;
            token.start = sv.sv().data() - query.data(); // Calculate the start position
            result.push_back(token);
        };

        // Set up the actions for the different token types defined in the grammar
        parser["Identifier"] = [&](const peg::SemanticValues &sv) {
            add_token(sv, SimplifiedTokenType::SIMPLIFIED_TOKEN_IDENTIFIER);
        };

        parser["NumberLiteral"] = [&](const peg::SemanticValues &sv) {
            add_token(sv, SimplifiedTokenType::SIMPLIFIED_TOKEN_NUMERIC_CONSTANT);
        };

        parser["StringLiteral"] = [&](const peg::SemanticValues &sv) {
            add_token(sv, SimplifiedTokenType::SIMPLIFIED_TOKEN_STRING_CONSTANT);
        };

        parser["Operator"] = [&](const peg::SemanticValues &sv) {
            add_token(sv, SimplifiedTokenType::SIMPLIFIED_TOKEN_OPERATOR);
        };

        parser["ReservedKeyword"] = [&](const peg::SemanticValues &sv) {
            add_token(sv, SimplifiedTokenType::SIMPLIFIED_TOKEN_KEYWORD);
        };

        parser["Comment"] = [&](const peg::SemanticValues &sv) {
            add_token(sv, SimplifiedTokenType::SIMPLIFIED_TOKEN_COMMENT);
        };

        // Parse the input query using the grammar
        if (!parser.parse(query.c_str())) {
            return {};
        }

        return result;
    }

    vector<OrderByNode> PEGParser::ParseOrderList(const string &select_list) {
        // construct a mock query
        string mock_query = "SELECT * FROM tbl ORDER BY " + select_list;
        // parse the query
        PEGParser parser;
        parser.ParseQuery(mock_query);
        // check the statements
        if (parser.statements.size() != 1 || parser.statements[0]->type != StatementType::SELECT_STATEMENT) {
            throw ParserException("Expected a single SELECT statement");
        }
        auto &select = parser.statements[0]->Cast<SelectStatement>();
        D_ASSERT(select.node->type == QueryNodeType::SELECT_NODE);
        auto &select_node = select.node->Cast<SelectNode>();
        if (select_node.modifiers.empty() || select_node.modifiers[0]->type != ResultModifierType::ORDER_MODIFIER ||
            select_node.modifiers.size() != 1) {
            throw ParserException("Expected a single ORDER clause");
        }
        auto &order = select_node.modifiers[0]->Cast<OrderModifier>();
        return std::move(order.orders);
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
        // Process reserved keywords
        for (auto &keyword: reserved_keywords) {
            string pruned_keyword = PruneKeyword(keyword);
            entries.emplace_back(pruned_keyword, KeywordCategory::KEYWORD_RESERVED);
        }

        // Process unreserved keywords
        for (auto &keyword: unreserved_keywords) {
            string pruned_keyword = PruneKeyword(keyword);
            entries.emplace_back(pruned_keyword, KeywordCategory::KEYWORD_UNRESERVED);
        }

        return entries;
    }

    KeywordCategory PEGParser::IsKeyword(const string &text) {
        // Process reserved keywords
        for (auto &keyword: reserved_keywords) {
            string pruned_keyword = PruneKeyword(keyword);
            if (pruned_keyword == text) {
                return KeywordCategory::KEYWORD_RESERVED;
            }
        }

        // Process unreserved keywords
        for (auto &keyword: unreserved_keywords) {
            string pruned_keyword = PruneKeyword(keyword);
            if (pruned_keyword == text) {
                return KeywordCategory::KEYWORD_UNRESERVED;
            }
        }

        return KeywordCategory::KEYWORD_NONE;
    }
} // namespace duckdb
