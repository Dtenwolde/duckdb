#include <string>
#include <fstream>
#include <streambuf>

namespace duckdb {

string FileToString(const std::string& file_name) {
    std::ifstream str(file_name);
    if (!str.is_open()) {
        throw IOException("Cannot open file: " + file_name);
    }
    return string((std::istreambuf_iterator<char>(str)),
                  std::istreambuf_iterator<char>());
}

class PEGParser {
public:
    PEGParser(const string& grammar_file) {
        auto base_grammar = FileToString(grammar_file);

        parser_ = make_uniq<peg::parser>(base_grammar);
        parser_->enable_packrat_parsing();

        if (!*parser_) {
            throw FatalException("Parser construction error");
        }

        // Enable error logging
        parser_->set_logger([](size_t line, size_t col, const string& msg) {
            printf("Error on line %zu:%zu -> %s\n", line, col, msg.c_str());
        });
    }

    void parse(const string &query) {
        auto input = FileToString(query);
        if (!parser_->parse(input)) {
            throw SyntaxException("Parsing failed");
        }
    }
private:
    std::unique_ptr<peg::parser> parser_;
};
}