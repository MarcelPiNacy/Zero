#pragma once
#include "Util.hpp"
#include "AST.hpp"
#include "Tokenizer.hpp"



namespace Zero
{
	struct Parser
	{
		struct TokenInfo
		{
			TokenType type;
			TokenData data;
		};

		Tokenizer							tokenizer;
		HashMap<string_view, IdentifierID>	identifiers;
		uintptr								ptr_size;
		TokenInfo							this_token;

		Parser() = default;
		explicit Parser(string_view text);
		Parser(const Parser&) = default;
		Parser& operator=(const Parser&) = default;
		~Parser() = default;

		IdentifierID		GetIdentifierID(string_view name);

		void				Reset();

		TokenInfo			NextToken();

		TokenInfo			ThisToken();

		void				Accept();
		
		void				Expect(TokenType type, string_view message);
		
		template <typename T>
		void				Expect(TokenType type, T data, string_view message)
		{
			using U = std::conditional_t<std::is_same_v<T, const char*>, string_view, T>; // Convert const char* to string_view.

			auto [t, d] = ThisToken();
			Assert(t == type && U(data) == d.Get<U>(), message);
		}

		template <typename... T>
		void				ExpectAndAccept(T... params)
		{
			Expect(params...);
			Accept();
		}

		[[noreturn]] void	Error(string_view message);

		void				Assert(bool condition, string_view message);
		
		void				AcceptSemicolon();
		Expression			ParseGenericRecord();
		Expression			ParseRecord(optional<UnqualifiedIdentifier> name);
		uint64				ParseFundamentalTypeBits(uint64 default_value);
		Use					ParseByUse();
		Namespace			ParseByNamespace();
		Expression			ParseType();
		Declaration			ParseByEnum();
		Expression			ParseByType(Type type);
		Branch				ParseIf();
		Select				ParseSelect();
		While				ParseWhile();
		DoWhile				ParseDoWhile();
		Expression			ParseFor();
		Scope				ParseScope();
		Expression			ParseBracket();
		vector<Expression>	ParseBracketContents();
		Expression			ParseParenthesis();
		Expression			ParseFunction(optional<UnqualifiedIdentifier> name = std::nullopt);
		Expression			ParseByFactors(Expression lhs);
		Expression			Parse();
		vector<Expression>	ParseFile();

		template <typename T>
		bool PopMany(std::tuple<TokenType, T*> first)
		{
			auto token = ThisToken();
			if (token.type != std::get<0>(first))
				return false;
			*std::get<1>(first) = token.data.Get<T>();
			Accept();
			return true;
		}

		template <typename T, typename F>
		bool PopMany(std::tuple<TokenType, T*, F> first)
		{
			auto token = ThisToken();
			if (token.type != std::get<0>(first))
				return false;
			bool r = true;
			token.data.Visit([&](auto& e)
			{
				if constexpr (std::is_same_v<std::remove_reference_t<decltype(e)>, T>)
					r = std::get<2>(first)(e);
				else
					r = false;
			});
			if (!r)
				return false;
			*std::get<1>(first) = token.data.Get<T>();
			Accept();
			return true;
		}

		template <typename T, typename ...U>
		bool PopMany(T&& first, U&&... others)
		{
			if (!PopMany(std::forward<T>(first)))
				return false;
			return PopMany(std::forward<U>(others)...);
		}
	};
}