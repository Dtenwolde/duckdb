#pragma once

#include "duckdb/common/arena_linked_list.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/optional_ptr.hpp"
#include "duckdb/common/string.hpp"

namespace duckdb {
class PEGTransformer;

enum class ParseResultType : uint8_t { LIST, OPTIONAL, CHOICE, EXPRESSION, IDENTIFIER, KEYWORD, EXTENSION, INVALID };

class ParseResult {
public:
	explicit ParseResult(ParseResultType type) : type(type) {
	}

	virtual ~ParseResult();


	template <class TARGET>
	TARGET &Cast() {
		if (TARGET::TYPE != ParseResultType::INVALID && type != TARGET::TYPE) {
			throw InternalException("Failed to cast parse result to type - parse result type mismatch");
		}
		return reinterpret_cast<TARGET &>(*this);
	}

	template <class TARGET>
	const TARGET &Cast() const {
		if (TARGET::TYPE != ParseResultType::INVALID && type != TARGET::TYPE) {
			throw InternalException("Failed to cast parse result to type - parse result type mismatch");
		}
		return reinterpret_cast<const TARGET &>(*this);
	}

	virtual string ToString() const = 0;

	ParseResultType Type() const {
		return type;
	}

	void SetName(string name_p) {
		name = std::move(name_p);
	}

	string GetName() const {
		if (name.empty()) {
			return ToString();
		}
		return name;
	}

protected:
	ParseResultType type;
	string name;
};

struct ExpressionParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::EXPRESSION;
	unique_ptr<Expression> expression;

	explicit ExpressionParseResult(PEGTransformer &, unique_ptr<Expression> &expression) :
		ParseResult(TYPE), expression(std::move(expression)) {}

	string ToString() const override {
		return "ExpressionParseResult";
	}
};

struct IdentifierParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::IDENTIFIER;
	string identifier;

	IdentifierParseResult(PEGTransformer &, string identifier) :
		ParseResult(TYPE), identifier(std::move(identifier)) {}

	string ToString() const override {
		return "IdentifierParseResult: " + identifier;
	}
};

struct KeywordParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::KEYWORD;
	string keyword;

	explicit KeywordParseResult(PEGTransformer &, const string &keyword) :
		ParseResult(TYPE), keyword(keyword) {}
	explicit KeywordParseResult(PEGTransformer &, KeywordParseResult &result) :
		ParseResult(TYPE), keyword(result.keyword) {}

	string ToString() const override {
		return "KeywordParseResult: " + keyword;
	}
};

struct ListParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::LIST;
	vector<reference<ParseResult>> children;

	ListParseResult(PEGTransformer &, vector<reference<ParseResult>> &results) :
		ParseResult(TYPE), children(std::move(results)) {};

	template <class T>
	T &Child(idx_t index) {
		if (index >= children.size()) {
			throw InternalException("Child index out of bounds");
		}
		return children[index].get().Cast<T>;
	}


	string ToString() const override {
		return "ListParseResult";
	}
};

struct OptionalParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::OPTIONAL;
	optional_ptr<ParseResult> optional_result;

	OptionalParseResult(PEGTransformer &, optional_ptr<ParseResult> &) :
		ParseResult(TYPE), optional_result(nullptr) {};

	string ToString() const override {
		return "OptionalParseResult";
	}
};

class ChoiceParseResult : public ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::CHOICE;

	ChoiceParseResult(PEGTransformer &, reference<ParseResult> parse_result_p, idx_t selected_idx_p) :
		ParseResult(TYPE), result(parse_result_p), selected_idx(selected_idx_p) {}

	reference<ParseResult> result;
	idx_t selected_idx;

	string ToString() const override {
		return "ChoiceParseResult";
	}
};

// Future extension point
// struct ExtensionParseResult : ParseResult {
// 	static constexpr ParseResultType TYPE = ParseResultType::EXTENSION;
// 	std::unique_ptr<ExtraData> extra_data;
//
// 	string ToString() override {
// 		return "ExtensionParseResult";
// 	}
// };

} // namespace duckdb