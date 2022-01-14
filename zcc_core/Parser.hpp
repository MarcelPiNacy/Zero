#pragma once
#include "Util.hpp"
#include "AST.hpp"
#include "Module.h"
#include "Tokenizer.hpp"



namespace Zero
{
	struct Parser
	{
		struct TokenInfo
		{
			TokenType type;
			TokenData data;

			constexpr bool HasData() const { return HasAssociatedData(type); }
		};

		Tokenizer							tokenizer;
		HashMap<string_view, IdentifierID>	identifiers;
		Module								this_module;
		uintptr								ptr_size;
		TokenInfo							this_token;
		vector<Scope*>						scopes;
		Scope*								this_scope;

		Parser() = default;
		explicit Parser(string_view text);
		Parser(const Parser&) = default;
		Parser& operator=(const Parser&) = default;
		~Parser() = default;

		IdentifierID		GetIdentifierID(string_view name);
		void				Reset();
		void				Accept();
		bool				Accept(TokenType type);
		void				Expect(TokenType type, string_view message);
		
		template <typename T>
		void				Expect(TokenType type, T data, string_view message)
		{
			using U = std::conditional_t<std::is_same_v<T, const char*>, string_view, T>; // Convert const char* to string_view.

			auto [t, d] = this_token;
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

		void				EnterScope(Scope* scope);
		void				LeaveScope();

		void				RegisterDeclaration(Declaration* declaration, bool local);

		vector<Expression>	ParseExpressionsUntil(TokenType terminator);
		vector<Expression>	ParseExpressionsUntil(TokenType terminator, Expression::IndexT required_id);
		vector<Expression>	ParseTokenSeparatedSequence(TokenType separator);
		vector<Expression>	ParseTokenSeparatedSequence(TokenType separator, TokenType terminator);
		vector<Expression>	ParseTokenSeparatedSequence(TokenType separator, TokenType terminator, Expression::IndexT required_id);

		Expression			ParseControlFlowExpressionBody();

		Expression			ParseGenericRecord();
		Expression			ParseRecord(optional<Identifier> name);
		uint64				ParseFundamentalTypeBits(uint64 default_value);
		Use					ParseByUse();
		Namespace			ParseByNamespace();
		Expression			ParseType();
		Declaration			ParseByEnum();
		Expression			ParseTypeDecl(Type type);
		Branch				ParseBranch();
		Select				ParseSelect();
		While				ParseWhile();
		DoWhile				ParseDoWhile();
		Expression			ParseFor();
		Scope				ParseScope();
		Expression			ParseBracket();
		Expression			ParseParenthesis();
		Expression			ParseFunction(optional<Identifier> name = std::nullopt);
		Expression			ParseFactors(Expression lhs);
		Expression			Parse();
		Module				ParseFile();

		template <typename T>
		bool PopMany(std::tuple<TokenType, T*> first)
		{
			auto token = this_token;
			if (token.type != std::get<0>(first))
				return false;
			*std::get<1>(first) = token.data.Get<T>();
			Accept();
			return true;
		}

		template <typename T, typename F>
		bool PopMany(std::tuple<TokenType, T*, F> first)
		{
			auto token = this_token;
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

		template <typename T>
		vector<T> CastExpressionList(Range<Expression> expressions)
		{
			vector<T> r;
			r.resize(expressions.Size());
			for (size_t i = 0; i < expressions.Size(); i++)
				r[i] = std::move(expressions[i].Get<T>());
			return r;
		}
	};
}