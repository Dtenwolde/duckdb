//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/peg/tokenizer/tokenizer.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/array.hpp"
#include "duckdb/common/once_flag.hpp"
#include "duckdb/parser/peg/keyword_helper.hpp"
#include "duckdb/parser/peg/token_type.hpp"
#include "duckdb/parser/peg/matcher_token.hpp"

namespace duckdb {

enum class TokenizeState {
	STANDARD = 0,
	SINGLE_LINE_COMMENT,
	MULTI_LINE_COMMENT,
	QUOTED_IDENTIFIER,
	STRING_LITERAL,
	KEYWORD,
	NUMERIC,
	OPERATOR,
	DOLLAR_QUOTED_STRING
};

class Tokenizer;

class TokenizerBehavior {
public:
	TokenizerBehavior(const string &sql, vector<MatcherToken> &tokens);
	virtual ~TokenizerBehavior() = default;

public:
	virtual void PushToken(idx_t start, idx_t end, TokenType type, bool unterminated = false);
	virtual void OnStatementEnd(idx_t pos);
	virtual void OnLastToken(const Tokenizer &tokenizer, TokenizeState state, string last_word, idx_t last_pos);

	//! Sentinel appended at the end of the token vector on a clean exit. Override to return
	//! `END_OF_INPUT_AUTOCOMPLETE` for autocomplete behavior. Dirty exits (unterminated comment /
	//! dollar-quote) always append `END_OF_INPUT` regardless of this hook.
	virtual TokenType GetTerminator() const {
		return TokenType::END_OF_INPUT;
	}

public:
	const string &sql;
	vector<MatcherToken> &tokens;
	bool has_block_comment = false;
	idx_t last_block_comment_position = 0;
};

class Tokenizer {
public:
	virtual ~Tokenizer() = default;
	explicit Tokenizer(const PEGKeywordHelper &keyword_helper);

public:
	//! Tokenize the behavior's input and return whether autocomplete can be offered.
	virtual bool TokenizeInput(TokenizerBehavior &behavior) const;

protected:
	virtual bool BackslashEscapesStringLiterals() const;
	virtual bool IsQuotedIdentifierDelimiter(char character) const;
	virtual void PushOperatorToken(TokenizerBehavior &behavior, idx_t start, idx_t end) const;
	void PushWordToken(TokenizerBehavior &behavior, idx_t start, idx_t end) const;
	virtual void HandleLastToken(TokenizerBehavior &behavior, TokenizeState state, const string &sql,
	                             idx_t last_pos) const;

private:
	//! Core tokenization loop. Returns true on a clean exit, false if the input ended inside an
	//! unterminated comment / dollar-quoted string. Does NOT append the trailing sentinel —
	//! `TokenizeInput()` is the one that appends `GetTerminator()` (clean) or `END_OF_INPUT`
	//! (dirty) based on the return value.
	bool TokenizeInputInternal(TokenizerBehavior &behavior) const;
	bool IsCompoundColonToken(const string &sql, idx_t pos, idx_t &token_length) const;
	//! Scan the word starting at position i and push it, or enter the KEYWORD state when the input ends in it
	void TokenizeWord(TokenizerBehavior &behavior, idx_t &i, idx_t &last_pos, TokenizeState &state) const;

	//! The first check of the STANDARD state that applies to a byte, in the order the checks are made
	enum class StandardByteClass : uint8_t {
		STRING_QUOTE,
		QUOTED_IDENTIFIER,
		SEMICOLON,
		DOLLAR,
		MINUS,
		SLASH,
		SPACE,
		COLON,
		SINGLE_BYTE_OPERATOR,
		NUMBER,
		SPECIAL_STRING_PREFIX,
		OPERATOR,
		WORD
	};
	//! Built on first use, since the quoted identifier delimiters are a virtual property of the tokenizer
	const array<StandardByteClass, 256> &StandardByteClasses() const;

public:
	static bool IsSingleByteOperator(char c);
	static bool CharacterIsInitialNumber(char c);
	static bool CharacterIsNumber(char c);
	static bool CharacterIsScientific(char c);
	static bool CharacterIsControlFlow(char c);
	static bool CharacterIsKeyword(char c);
	static bool CharacterIsOperator(char c);
	static bool CharacterIsSpecialStringCharacter(char c);
	static bool IsValidDollarTagCharacter(char c);
	static TokenType TokenizeStateToType(TokenizeState state);
	static bool IsUnterminatedState(TokenizeState state);

public:
	const PEGKeywordHelper &keyword_helper;

private:
	mutable OnceFlag standard_byte_classes_once;
	mutable array<StandardByteClass, 256> standard_byte_classes;
};

} // namespace duckdb
