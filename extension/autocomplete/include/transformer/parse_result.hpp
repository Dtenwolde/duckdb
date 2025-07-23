#pragma once
#include "duckdb/common/arena_linked_list.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/optional_ptr.hpp"
#include "duckdb/common/string.hpp"
#include "duckdb/common/types/string_type.hpp"
#include "duckdb/parser/sql_statement.hpp"
#include "duckdb/parser/parsed_expression.hpp"

namespace duckdb {

class PEGTransformer; // Forward declaration

enum class ParseResultType : uint8_t {
	LIST,
	OPTIONAL,
	REPEAT,
	CHOICE,
	EXPRESSION,
	IDENTIFIER,
	KEYWORD,
	OPERATOR,
	STATEMENT,
	EXTENSION,
	NUMBER,
	STRING,
	INVALID
};

class ParseResult {
public:
	explicit ParseResult(ParseResultType type) : type(type) {
	}
	virtual ~ParseResult() = default;

	template <class TARGET>
	TARGET &Cast() {
		if (TARGET::TYPE != ParseResultType::INVALID && type != TARGET::TYPE) {
			throw InternalException("Failed to cast parse result to type - mismatch");
		}
		return reinterpret_cast<TARGET &>(*this);
	}

	ParseResultType type;
	string name;
};

struct IdentifierParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::IDENTIFIER;
	string identifier;

	explicit IdentifierParseResult(string identifier_p) : ParseResult(TYPE), identifier(std::move(identifier_p)) {
	}
};

struct KeywordParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::KEYWORD;
	string keyword;

	explicit KeywordParseResult(string keyword_p) : ParseResult(TYPE), keyword(std::move(keyword_p)) {
	}
};

struct ListParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::LIST;
	vector<reference<ParseResult>> children;

	explicit ListParseResult(vector<reference<ParseResult>> results_p)
	    : ParseResult(TYPE), children(std::move(results_p)) {
	}

	template <class T>
	T &Child(idx_t index) {
		if (index >= children.size()) {
			throw InternalException("Child index out of bounds");
		}
		return children[index].get().Cast<T>();
	}

};

struct RepeatParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::REPEAT;
	vector<reference<ParseResult>> children;

	explicit RepeatParseResult(vector<reference<ParseResult>> results_p)
		: ParseResult(TYPE), children(std::move(results_p)) {
	}

	template <class T>
	T &Child(idx_t index) {
		if (index >= children.size()) {
			throw InternalException("Child index out of bounds");
		}
		return children[index].get().Cast<T>();
	}
};

struct OptionalParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::OPTIONAL;
	optional_ptr<ParseResult> optional_result;

	explicit OptionalParseResult(ParseResult *result_p) : ParseResult(TYPE), optional_result(result_p) {
	}
};

class ChoiceParseResult : public ParseResult {
public:
	static constexpr ParseResultType TYPE = ParseResultType::CHOICE;

	explicit ChoiceParseResult(reference<ParseResult> parse_result_p, idx_t selected_idx_p)
	    : ParseResult(TYPE), result(parse_result_p), selected_idx(selected_idx_p) {
	}

	reference<ParseResult> result;
	idx_t selected_idx;
};

class NumberParseResult : public ParseResult {
public:
	static constexpr ParseResultType TYPE = ParseResultType::NUMBER;

	explicit NumberParseResult(string number_p) : ParseResult(TYPE), number(std::move(number_p)) {
	}
	// TODO(dtenwolde): Should probably be stored as a size_t, int32_t or float_t depending on what number is.
	string number;
};

class StringLiteralParseResult : public ParseResult {
public:
	static constexpr ParseResultType TYPE = ParseResultType::STRING;

	explicit StringLiteralParseResult(string string_p) : ParseResult(TYPE), result(std::move(string_p)) {
	}
	string result;
};


class OperatorParseResult : public ParseResult {
public:
	static constexpr ParseResultType TYPE = ParseResultType::OPERATOR;

	explicit OperatorParseResult(string operator_p) : ParseResult(TYPE), operator_token(std::move(operator_p)) {
	}
	string operator_token;
};
} // namespace duckdb
