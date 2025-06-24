#pragma once

#include "duckdb/common/arena_linked_list.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/optional_ptr.hpp"
#include "duckdb/common/types.hpp"
#include <string>

namespace duckdb {

enum class ParseResultType : uint8_t { LIST, OPTIONAL, CHOICE, EXPRESSION, IDENTIFIER, KEYWORD, EXTENSION };

struct ParseResult {
	virtual ~ParseResult();

	ParseResultType type;

	template <class TARGET>
	TARGET &Cast() {
		if (type != TARGET::TYPE) {
			throw InternalException("Failed to cast ParseResult %s", ToString());
		}
		return reinterpret_cast<TARGET &>(*this);
	}

	template <class TARGET>
	const TARGET &Cast() const {
		if (type != TARGET::TYPE) {
			throw InternalException("Failed to cast ParseResult");
		}
		return reinterpret_cast<const TARGET &>(*this);
	}

	virtual string ToString();
};

struct ExpressionParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::EXPRESSION;
	unique_ptr<Expression> expression;

	string ToString() override {
		return "ExpressionParseResult";
	}
};

struct IdentifierParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::IDENTIFIER;
	string identifier;

	string ToString() override {
		return "IdentifierParseResult: " + identifier;
	}
};

struct KeywordParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::KEYWORD;
	string keyword;

	string ToString() override {
		return "KeywordParseResult: " + keyword;
	}
};

struct ListParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::LIST;
	ArenaLinkedList<reference<ParseResult>> parse_result;

	template <class T>
	T &GetChild(idx_t index) {
		auto &child_ref = parse_result[index];
		auto &child = child_ref.get();
		if (child.type != T::TYPE) {
			throw InternalException("Expected child of type %s", ToString());
		}
		return static_cast<T &>(child);
	}

	string ToString() override {
		return "ListParseResult";
	}
};

struct OptionalParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::OPTIONAL;
	optional_ptr<ParseResult> optional_result;

	string ToString() override {
		return "OptionalParseResult";
	}
};

struct ChoiceParseResult : ParseResult {
	static constexpr ParseResultType TYPE = ParseResultType::CHOICE;
	ParseResult &result;
	idx_t selected_idx;

	string ToString() override {
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