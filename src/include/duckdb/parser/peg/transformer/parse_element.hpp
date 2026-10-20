//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/peg/transformer/parse_element.hpp
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/parser/peg/transformer/parse_result.hpp"

namespace duckdb {

class ParseElement;

//! Indexes a container of parse elements by position
template <class CONTAINER>
class ParseElementIterator {
public:
	ParseElementIterator(const CONTAINER &container_p, idx_t index_p) : container(container_p), index(index_p) {
	}
	ParseElement operator*() const;
	ParseElementIterator &operator++() {
		index++;
		return *this;
	}
	bool operator!=(const ParseElementIterator &other) const {
		return index != other.index;
	}

private:
	const CONTAINER &container;
	idx_t index;
};

//! The repeated elements of a repetition
class ParseElementArray {
public:
	explicit ParseElementArray(unsafe_array_ptr<reference<ParseResult>> children_p) : children(children_p) {
	}
	idx_t size() const { // NOLINT: match stl API
		return children.size();
	}
	ParseElement operator[](idx_t index) const;
	ParseElementIterator<ParseElementArray> begin() const { // NOLINT: match stl API
		return ParseElementIterator<ParseElementArray>(*this, 0);
	}
	ParseElementIterator<ParseElementArray> end() const { // NOLINT: match stl API
		return ParseElementIterator<ParseElementArray>(*this, size());
	}

private:
	unsafe_array_ptr<reference<ParseResult>> children;
};

//! The items of List(D) <- D (',' D)* ','?
class ParseElementItems {
public:
	explicit ParseElementItems(ParseResult &list) : items(list) {
		// validate the shape of every item before any of them is consumed
		for (idx_t i = 0; i < items.size(); i++) {
			items[i];
		}
	}
	idx_t size() const { // NOLINT: match stl API
		return items.size();
	}
	ParseElement operator[](idx_t index) const;
	ParseElementIterator<ParseElementItems> begin() const { // NOLINT: match stl API
		return ParseElementIterator<ParseElementItems>(*this, 0);
	}
	ParseElementIterator<ParseElementItems> end() const { // NOLINT: match stl API
		return ParseElementIterator<ParseElementItems>(*this, size());
	}

private:
	ParseResultListItems items;
};

//! One matched element of a rule as the generated transforms read it: a child position of a sequence, the content
//! of an optional or a choice, a repetition, or a token. Wraps the parse result tree.
class ParseElement {
public:
	explicit ParseElement(ParseResult &result_p) : result(result_p) {
	}

public:
	ParseResult &Result() const {
		return result;
	}
	ParseResultType Type() const {
		return result.type;
	}
	optional_ptr<const CompiledGrammarRule> Rule() const {
		return result.GetRule();
	}
	const string &Name() const {
		return result.Name();
	}
	optional_idx Offset() const {
		return result.GetOffset();
	}

	//! Child of a sequence by position
	ParseElement Child(idx_t index) const {
		return ParseElement(result.Cast<ListParseResult>().GetChild(index));
	}
	//! The content of Parens(D) <- '(' D ')'
	ParseElement Unwrap() const {
		return Child(1);
	}
	//! Whether an optional matched
	bool HasResult() const {
		return result.Cast<OptionalParseResult>().HasResult();
	}
	//! The content of an optional that matched
	ParseElement GetResult() const {
		return ParseElement(result.Cast<OptionalParseResult>().GetResult());
	}
	//! The alternative a choice selected
	ParseElement Choice() const {
		return ParseElement(result.Cast<ChoiceParseResult>().GetResult());
	}
	ParseElementArray Repeat() const {
		return ParseElementArray(result.Cast<RepeatParseResult>().GetChildren());
	}
	ParseElementItems ListItems() const {
		return ParseElementItems(result);
	}

	Identifier &GetIdentifier() const {
		return result.Cast<IdentifierParseResult>().identifier;
	}
	const string &Keyword() const {
		return result.Cast<KeywordParseResult>().keyword;
	}
	const string &StringLiteral() const {
		return result.Cast<StringLiteralParseResult>().result;
	}
	const string &Operator() const {
		return result.Cast<OperatorParseResult>().operator_token;
	}

private:
	ParseResult &result;
};

template <class CONTAINER>
ParseElement ParseElementIterator<CONTAINER>::operator*() const {
	return container[index];
}

inline ParseElement ParseElementArray::operator[](idx_t index) const {
	return ParseElement(children[index].get());
}

inline ParseElement ParseElementItems::operator[](idx_t index) const {
	return ParseElement(items[index].get());
}

} // namespace duckdb
