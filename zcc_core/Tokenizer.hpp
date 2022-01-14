#pragma once
#include "Util.hpp"
#include "Keyword.hpp"
#include "Operator.hpp"



namespace Zero
{
	enum class TokenType : uint8
	{
		Keyword,
		Identifier,
		LiteralInt,
		LiteralReal,
		LiteralChar,
		LiteralString,
		Operator,

		BraceLeft, BraceRight,
		BracketLeft, BracketRight,
		ParenLeft, ParenRight,
		Comma, Colon, Semicolon,
		TraitsOf,
		Address,
		Arrow,
		Wildcard,
		Hash,

		MaxEnum
	};

	constexpr bool HasAssociatedData(TokenType type) noexcept
	{
		return (uint8)type <= (uint8)TokenType::Operator;
	}



	using TokenData = TaggedUnion<
		Keyword, Operator,
		bool, uint64, double, char32_t,
		string_view>;

	struct Tokenizer
	{
		const char* cursor;
		const char* end;
		uintptr tab_count;
		uintptr line_count;
		const char* begin;

		Tokenizer() = default;
		Tokenizer(const Tokenizer&) = default;
		Tokenizer& operator=(const Tokenizer&) = default;
		~Tokenizer() = default;

		constexpr Tokenizer(string_view text) :
			cursor(text.data()), end(text.data() + text.size()), tab_count(), line_count(), begin(cursor)
		{
		}

		bool TryGet(char c);
		void SkipComment();
		void SkipWhitespace();
		void SkipCommentsAndWhitespace();
		void SkipIdentifier();
		TokenType TokenizeSign(TokenData& out);
		TokenType TokenizeNonDecimal(char key, TokenData& out);
		TokenType TokenizeNumeric(TokenData& out);
		TokenType TokenizeKeywordOrIdentifier(TokenData& out);
		TokenType NextToken(TokenData& out);
	};
}