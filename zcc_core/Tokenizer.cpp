#include "Tokenizer.hpp"



namespace Zero
{
	bool Tokenizer::TryGet(char c)
	{
		if (cursor == end || *cursor != c)
			return false;
		++cursor;
		return true;
	}

	void Tokenizer::SkipComment()
	{
		++cursor;
		char terminator = '`';
		if (cursor < end && *cursor == '`')
			terminator = '\n';
		++cursor;
		while (cursor < end && *cursor != terminator)
			++cursor;
		if (cursor < end)
			++cursor;
	}

	void Tokenizer::SkipWhitespace()
    {
		while (cursor < end && isspace(*cursor))
		{
			if (*cursor == '\t')
				++tab_count;
			else if (*cursor == '\n')
			{
				++line_count;
				tab_count = 0;
			}
			++cursor;
		}
    }

	void Tokenizer::SkipCommentsAndWhitespace()
	{
		while (true)
		{
			SkipWhitespace();
			if (cursor == end)
				break;
			if (*cursor != '`')
				break;
			SkipComment();
		}
	}

	void Tokenizer::SkipIdentifier()
	{
		while (cursor < end && (isalnum(*cursor) || *cursor == '_' || *cursor == '.'))
			++cursor;
	}

	TokenType Tokenizer::TokenizeSign(TokenData& out)
	{
		char a = *cursor;
		++cursor;
		switch (a)
		{
		case '=':
			if (TryGet('='))
				out = Operator::CompareEQ;
			else if (TryGet('>'))
				return TokenType::Arrow;
			else
				out = Operator::Assign;
			return TokenType::Operator;
		case '+':
			if (TryGet('='))
				out = Operator::AddAssign;
			else if (TryGet('+'))
				out = Operator::Increment;
			else
				out = Operator::Add;
			return TokenType::Operator;
		case '-':
			if (TryGet('='))
				out = Operator::SubAssign;
			else if (TryGet('-'))
				out = Operator::Decrement;
			else
				out = Operator::Sub;
			return TokenType::Operator;
		case '*':
			if (TryGet('='))
				out = Operator::MulAssign;
			else
				out = Operator::Mul;
			return TokenType::Operator;
		case '/':
			if (TryGet('='))
				out = Operator::DivAssign;
			else
				out = Operator::Div;
			return TokenType::Operator;
		case '%':
			if (TryGet('='))
				out = Operator::ModAssign;
			else
				out = Operator::Mod;
			return TokenType::Operator;
		case '&':
			if (TryGet('='))
				out = Operator::AndAssign;
			else if (TryGet('&'))
				out = Operator::BoolAnd;
			else
				out = Operator::And;
			return TokenType::Operator;
		case '|':
			if (TryGet('='))
				out = Operator::OrAssign;
			else if (TryGet('|'))
				out = Operator::BoolOr;
			else
				out = Operator::Or;
			return TokenType::Operator;
		case '^':
			if (TryGet('='))
				out = Operator::XorAssign;
			else
				out = Operator::Xor;
			return TokenType::Operator;
		case '~':
			out = Operator::Complement;
			return TokenType::Operator;
		case '<':
			if (TryGet('<'))
			{
				if (TryGet('<'))
				{
					if (TryGet('='))
						out = Operator::RotateLeftAssign;
					else
						out = Operator::RotateLeft;
				}
				else if (TryGet('='))
					out = Operator::ShiftLeftAssign;
				else
					out = Operator::ShiftLeft;
			}
			else if (TryGet('='))
			{
				if (TryGet('>'))
					out = Operator::CompareTW;
				else
					out = Operator::CompareLE;
			}
			else
				out = Operator::CompareLT;
			return TokenType::Operator;
		case '>':
			if (TryGet('>'))
			{
				if (TryGet('>'))
				{
					if (TryGet('='))
						out = Operator::RotateRightAssign;
					else
						out = Operator::RotateRight;
				}
				else if (TryGet('='))
					out = Operator::ShiftRightAssign;
				else
					out = Operator::ShiftRight;
			}
			else if (TryGet('='))
				out = Operator::CompareGE;
			else
				out = Operator::CompareGT;
			return TokenType::Operator;
		case '?':
			return TokenType::TraitsOf;
		case '$':
			return TokenType::Wildcard;
		case '@':
			return TokenType::Address;
		case '\'':
		{
			auto b = cursor;
			while (cursor < end)
			{
				if (*cursor == '\\')
				{
					++cursor;
					if (cursor == end)
						exit(EXIT_FAILURE);
					if (*cursor == '\'')
						continue;
				}
				else if (*cursor == '\'')
					break;
			}

			if (cursor - b != 1)
			{
				return TokenType::MaxEnum;
			}

			out = (char32_t)*b;
			return TokenType::LiteralChar;
		}
		case '\"':
		{
			auto b = cursor;
			while (cursor < end)
			{
				if (*cursor == '\\')
				{
					++cursor;
					if (cursor == end)
						exit(EXIT_FAILURE);
					if (*cursor == '\"')
						continue;
				}
				else if (*cursor == '\"')
					break;
			}
			out = string_view(b, cursor - b);
			return TokenType::LiteralString;
		}
		case '(':
			return TokenType::ParenLeft;
		case ')':
			return TokenType::ParenRight;
		case '[':
			return TokenType::BracketLeft;
		case ']':
			return TokenType::BracketRight;
		case '{':
			return TokenType::BraceLeft;
		case '}':
			return TokenType::BraceRight;
		case '.':
			return TokenType::Dot;
		case ':':
			return TokenType::Colon;
		case ',':
			return TokenType::Comma;
		case ';':
			if (TryGet(';'))
				return TokenType::NoOp;
			else
				return TokenType::Semicolon;
		default:
			return TokenType::MaxEnum;
		}
	}

	TokenType Tokenizer::TokenizeNonDecimal(char key, TokenData& out)
	{
		char buffer[256] = {};
		int radix;
		auto a = cursor;
		switch (key)
		{
		case 'b': case 'B':
			radix = 2;
			while (cursor != end && (*cursor == '0' || *cursor == '1'))
				++cursor;
			break;
		case 'x': case 'X':
			radix = 16;
			while (cursor != end && isxdigit(*cursor))
				++cursor;
			break;
		case 'r': case 'R':
			while (cursor != end && *cursor != ':')
				++cursor;
			memcpy(buffer, a, cursor - a);
			++cursor;
			radix = strtoul(buffer, nullptr, 10);
			a = cursor;
			while (cursor != end && isalnum(*cursor))
				++cursor;
			break;
		default:
			return TokenType::MaxEnum;
		}
		auto token = string_view(a, cursor - a);
		if (token.size() >= 256)
			abort();
		(void)memcpy(buffer, token.data(), token.size());
		out = (int64)strtoull(buffer, nullptr, radix);
		return TokenType::LiteralInt;
	}

	TokenType Tokenizer::TokenizeNumeric(TokenData& out)
	{
		if (*cursor == '0' && isalpha(cursor[1]))
		{
			auto c = cursor[1];
			cursor += 2;
			return TokenizeNonDecimal(c, out);
		}
		else
		{
			auto a = cursor;
			bool dot = false;
			for (; cursor != end; ++cursor)
			{
				if (*cursor == '.')
				{
					if (dot)
						break;
					dot = true;
					continue;
				}
				if (!isdigit(*cursor))
					break;
			}
			auto token = string_view(a, cursor - a);
			if (token.size() >= 256)
			{
				abort();
			}
			char buffer[256];
			(void)memcpy(buffer, token.data(), token.size());
			buffer[token.size()] = '\0';
			if (dot)
			{
				out = strtod(buffer, nullptr);
				return TokenType::LiteralReal;
			}
			else
			{
				out = (int64)strtoull(buffer, nullptr, 10);
				return TokenType::LiteralInt;
			}
		}
	}

	TokenType Tokenizer::TokenizeKeywordOrIdentifier(TokenData& out)
	{
		auto start = cursor;
		SkipIdentifier();
		auto token = string_view(start, cursor - start);
		auto kw = IsKeyword(token);
		if (kw != Keyword::MaxEnum)
		{
			out = kw;
			return TokenType::Keyword;
		}
		else
		{
			out = token;
			return TokenType::Identifier;
		}
	}

    TokenType Tokenizer::NextToken(TokenData& out)
    {
		SkipCommentsAndWhitespace();
		if (cursor == end)
			return TokenType::MaxEnum;

		if (isalpha(*cursor) || *cursor == '_')
			return TokenizeKeywordOrIdentifier(out);

		if (isdigit(*cursor))
			return TokenizeNumeric(out);
		else
			return TokenizeSign(out);
    }
}