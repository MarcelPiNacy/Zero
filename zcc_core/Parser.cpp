#include "Parser.hpp"

namespace Zero
{
    Parser::Parser(string_view text) :
        tokenizer(text),
        identifiers(),
        this_module(),
        ptr_size(sizeof(uintptr) * 8),
        this_token{ TokenType::MaxEnum, {} },
        scopes(),
        this_scope(nullptr)
    {
    }

    IdentifierID Parser::GetIdentifierID(string_view name)
    {
        auto count = identifiers.size();
        auto& e = identifiers[name];
        if (identifiers.size() != count)
            e = (IdentifierID)count;
        return e;
    }

    void Parser::Reset()
    {
        Destruct(identifiers);
        tokenizer.cursor = tokenizer.begin;
        this_token.type = TokenType::MaxEnum;
    }

    void Parser::Accept()
    {
        this_token.type = tokenizer.NextToken(this_token.data);
    }

    bool Parser::Accept(TokenType type)
    {
        bool r = this_token.type == type;
        if (r)
            Accept();
        return r;
    }

    void Parser::Expect(TokenType type, string_view message)
    {
        Assert(this_token.type == type, message);
    }

    void Parser::Error(string_view message)
    {
        fwrite(message.data(), 1, message.size(), stdout);
        abort();
    }

    void Parser::Assert(bool condition, string_view message)
    {
        if (!condition)
            Error(message);
    }

    void Parser::EnterScope(Scope* scope)
    {
        this_scope = scope;
        scopes.push_back(scope);
    }

    void Parser::LeaveScope()
    {
        assert(scopes.size() != 0);
        scopes.pop_back();
        this_scope = !scopes.empty() ? scopes.back() : nullptr;
    }

    void Parser::RegisterDeclaration(Declaration* declaration, bool local)
    {
        auto scope = local ? this_scope : &this_module.global_scope;
        scope->declarations.insert({ declaration->name, declaration });
    }

    vector<Expression> Parser::ParseExpressionsUntil(TokenType terminator)
    {
        vector<Expression> r;
        while (this_token.type != terminator)
        {
            r.push_back(Parse());
            Accept(TokenType::Semicolon);
        }
        Accept();
        return r;
    }

    vector<Expression> Parser::ParseExpressionsUntil(TokenType terminator, Expression::IndexT required_id)
    {
        vector<Expression> r;
        while (this_token.type != terminator)
        {
            auto e = Parse();
            Assert(e.ID() == required_id, "");
            r.push_back(std::move(e));
            Accept(TokenType::Semicolon);
        }
        Accept();
        return r;
    }

    vector<Expression> Parser::ParseTokenSeparatedSequence(TokenType separator)
    {
        vector<Expression> r;
        while (true)
        {
            r.push_back(Parse());
            if (this_token.type != separator)
                break;
            Accept();
        }
        return r;
    }

    vector<Expression> Parser::ParseTokenSeparatedSequence(TokenType separator, TokenType terminator)
    {
        vector<Expression> r;

        if (this_token.type != terminator)
        {
            while (true)
            {
                r.push_back(Parse());
                if (this_token.type == terminator)
                    break;
                ExpectAndAccept(separator, "");
            }
        }
        Accept();
        return r;
    }

    vector<Expression> Parser::ParseTokenSeparatedSequence(TokenType separator, TokenType terminator, Expression::IndexT required_id)
    {
        vector<Expression> r;

        if (this_token.type == terminator)
            return r;

        while (true)
        {
            auto e = Parse();
            Assert(e.ID() == required_id, "");
            r.push_back(std::move(e));
            if (this_token.type == terminator)
            {
                Accept();
                return r;
            }
            ExpectAndAccept(separator, "");
        }
    }

    Expression Parser::ParseControlFlowExpressionBody()
    {
        switch (this_token.type)
        {
        case TokenType::Keyword:
            Assert(this_token.data.Get<Keyword>() == Keyword::Do, "");
            Accept();
            return Parse();
        case TokenType::BraceLeft:
            return Parse();
        default:
            Error("");
        }
    }

    Expression Parser::ParseGenericRecord()
    {
        abort();
    }

    Expression Parser::ParseRecord(optional<Identifier> name)
    {
        Record r = {};

        ExpectAndAccept(TokenType::BraceRight, "");
        r.fields = CastExpressionList<Declaration>(ParseExpressionsUntil(TokenType::BraceRight, Expression::IDOf<Declaration>));

        if (!name.has_value())
            return Type(r.ToPtr());

        Declaration d = {};
        d.type = Expression(Type(MetaType())).ToPtr();
        d.name = name.value();
        d.init = Expression(Type(r.ToPtr())).ToPtr();
        return d;
    }

    uint64 Parser::ParseFundamentalTypeBits(uint64 default_value)
    {
        auto e = this_token;
        if (e.type != TokenType::ParenLeft)
            return default_value;
        Accept();
        auto v = Parse();
        assert(v.Is<LiteralInt>());
        ExpectAndAccept(TokenType::ParenRight, "");
        return v.Get<LiteralInt>().value;
    }

    Use Parser::ParseByUse()
    {
        Use r = {};
        r.modules = ParseTokenSeparatedSequence(TokenType::Comma);
        Accept(TokenType::Semicolon);
        return r;
    }

    Namespace Parser::ParseByNamespace()
    {
        Namespace r = {};
        Expect(TokenType::Identifier, "");
        r.name = GetIdentifierID(this_token.data.Get<string_view>());
        Accept();
        ExpectAndAccept(TokenType::BraceLeft, "");
        r.elements = ParseExpressionsUntil(TokenType::BraceRight);
        return r;
    }

    Expression Parser::ParseType()
    {
        Expression r;
        
        optional<Identifier> name = std::nullopt;

        if (this_token.type == TokenType::Identifier)
        {
            name = GetIdentifierID(this_token.data.Get<string_view>());
            Accept();
        }

        auto [type, data] = this_token;
        switch (type)
        {
        case TokenType::Operator:
            Assert(data.Get<Operator>() == Operator::Assign, "");
            Accept();
            r = Declaration(Expression(Type(MetaType())).ToPtr(), name.value(), Expression(Parse().GetType(*this)).ToPtr());
            Accept(TokenType::Semicolon);
            break;
        case TokenType::BraceLeft:
            Accept();
            r = ParseRecord(name);
            break;
        case TokenType::ParenLeft:
            Accept();
            r = ParseGenericRecord();
            break;
        default:
            Accept(TokenType::Semicolon);
            Assert(name.has_value(), "");
            r = Declaration(Expression(Type()).ToPtr(), name.value());
            break;
        }
        return r;
    }

    Declaration Parser::ParseByEnum()
    {
        Declaration r;
        Enum e;
        ExpectAndAccept(TokenType::Identifier, "");
        r.name = GetIdentifierID(this_token.data.Get<string_view>());
        if (auto token = this_token; token.type == TokenType::Colon)
        {
            Accept();
            e.underlying_type = Parse().ToPtr();
        }
        ExpectAndAccept(TokenType::BraceLeft, "");
        while (this_token.type != TokenType::BraceRight)
        {
            string_view name;
            Operator op = {};

            Assert(PopMany(
                std::make_tuple(TokenType::Identifier, &name),
                std::make_tuple(TokenType::Operator, &op, [](Operator o) { return o == Operator::Assign; })),
                "");

            e.values.insert(std::make_pair(GetIdentifierID(name), Parse()));

            Accept(TokenType::Comma);
        }
        Accept();
        r.init = Expression(Type(e.ToPtr())).ToPtr();
        return r;
    }

    Expression Parser::ParseTypeDecl(Type type)
    {
        Declaration r = {};
        if (!type.IsEmpty())
            r.type = Expression(type).ToPtr();
        auto token = this_token;
        if (token.type != TokenType::Identifier)
            return type;
        r.name = GetIdentifierID(token.data.Get<string_view>());
        Accept();
        if (auto [t, data] = this_token; t == TokenType::Operator && data.Get<Operator>() == Operator::Assign)
        {
            Accept();
            r.init = Parse().ToPtr();
        }
        Accept(TokenType::Semicolon);
        if (r.type == nullptr || r.type->IsEmpty())
            r.type = Expression(r.init->GetType(*this)).ToPtr();
        return r;
    }

    Branch Parser::ParseBranch()
    {
        Branch r = {};

        r.condition = Parse().ToPtr();
        r.on_true = ParseControlFlowExpressionBody().ToPtr();

        auto [type, data] = this_token;
        if (type == TokenType::Keyword)
        {
            switch (data.Get<Keyword>())
            {
            case Keyword::Elif:
                Accept();
                r.on_false = Expression(ParseBranch()).ToPtr();
                break;
            case Keyword::Else:
                Accept();
                r.on_false = Parse().ToPtr();
                break;
            default:
                break;
            }
        }

        return r;
    }

    Select Parser::ParseSelect()
    {
        Select r = {};
        r.key = Parse().ToPtr();
        ExpectAndAccept(TokenType::BraceLeft, "");
        while (this_token.type != TokenType::BraceRight)
        {
            auto [type, data] = this_token;
            Assert(type == TokenType::Keyword, "");
            switch (data.Get<Keyword>())
            {
            case Keyword::If:
            {
                Accept();
                auto k = Parse();
                ExpectAndAccept(TokenType::Colon, "");
                auto v = Parse();
                r.cases.insert({ std::move(k), std::move(v) });
                break;
            }
            case Keyword::Else:
                Assert(r.default_case == nullptr, "");
                Accept();
                ExpectAndAccept(TokenType::Colon, "");
                r.default_case = Parse().ToPtr();
                break;
            default:
                Error("");
            }
        }
        return r;
    }

    While Parser::ParseWhile()
    {
        While r = {};
        r.condition = Parse().ToPtr();
        r.body = ParseControlFlowExpressionBody().ToPtr();
        return r;
    }

    DoWhile Parser::ParseDoWhile()
    {
        DoWhile r = {};
        Expect(TokenType::BraceLeft, "");
        r.body = Parse().ToPtr();
        ExpectAndAccept(TokenType::Keyword, Keyword::While, "");
        r.condition = Parse().ToPtr();
        return r;
    }

    Expression Parser::ParseFor()
    {
        auto first = Parse();
        if (this_token.type == TokenType::Colon)
        {
            Accept();
            ForEach r = {};
            r.iterator = first.ToPtr();
            r.collection = Parse().ToPtr();
            r.body = ParseControlFlowExpressionBody().ToPtr();
            return r;
        }
        else
        {
            For r = {};
            r.init = first.ToPtr();
            r.condition = Parse().ToPtr();
            r.update = Parse().ToPtr();
            r.body = ParseControlFlowExpressionBody().ToPtr();
            return r;
        }
    }

    Scope Parser::ParseScope()
    {
        Scope r = {};
        EnterScope(&r);
        r.expressions = ParseExpressionsUntil(TokenType::BraceRight);
        LeaveScope();
        return r;
    }

    Expression Parser::ParseBracket()
    {
        Expression r;
        bool any_type = false;
        bool any_value = false;

        auto contents = ParseTokenSeparatedSequence(TokenType::Comma, TokenType::BracketRight);

        for (const auto& e : contents)
        {
            any_type = any_type || e.Is<Type>();
            any_value = any_value || !e.Is<Type>();
        }

        ExpectAndAccept(TokenType::BracketRight, "");

        Accept(TokenType::Semicolon);

        return r;
    }

    Expression Parser::ParseParenthesis()
    {
        Function r = {};
        auto expressions = ParseTokenSeparatedSequence(TokenType::Comma, TokenType::ParenRight);

        if (auto type = this_token.type; type != TokenType::Colon && type != TokenType::Arrow)
        {
            Assert(expressions.size() == 1, "");
            return expressions[0];
        }

        r.params = std::move(expressions);

        if (Accept(TokenType::Arrow))
            r.return_type = Parse().ToPtr();

        ExpectAndAccept(TokenType::Colon, "");

        r.body = Parse().ToPtr();

        if (r.return_type == nullptr)
        {
            if (r.body->Is<Scope>())
            {
                auto [success, type] = r.body->InferReturnType(*this);
                r.return_type = Expression(type).ToPtr();
            }
            else
            {
                r.return_type = Expression(r.body->GetType(*this)).ToPtr();
            }
        }

        return r;
    }

    Expression Parser::ParseFunction(optional<Identifier> name)
    {
        Function r = {};

        auto expressions = ParseTokenSeparatedSequence(TokenType::Comma, TokenType::ParenRight);

        r.params = std::move(expressions);

        if (this_token.type == TokenType::Arrow)
        {
            Accept();
            r.return_type = Parse().ToPtr();
        }

        if (this_token.type != TokenType::Colon)
        {
            Assert(name.has_value(), "");
            return FunctionCall(name.value(), std::move(r.params));
        }

        Accept();
        r.body = Parse().ToPtr();

        if (r.return_type == nullptr)
        {
            if (r.body->Is<Scope>())
            {
                auto [success, type] = r.body->InferReturnType(*this);
                r.return_type = Expression(type).ToPtr();
            }
            else
            {
                r.return_type = Expression(r.body->GetType(*this)).ToPtr();
            }
        }

        if (!name.has_value())
            return r;

        Declaration d = {};
        d.type = Expression(r.GetType(*this)).ToPtr();
        d.name = name.value();
        d.init = Expression(std::move(r)).ToPtr();
        return d;
    }

    Expression Parser::ParseFactors(Expression lhs)
    {
        Expression r;
        switch (this_token.type)
        {
        case TokenType::Keyword:
            if (this_token.data.Get<Keyword>() != Keyword::As)
            {
                r = lhs;
            }
            else
            {
                Accept();
                r = Cast{ lhs.ToPtr(), Parse().ToPtr() };
            }
            break;
        case TokenType::Identifier:
        {
            Declaration d = {};
            d.type = lhs.ToPtr();
            d.name = GetIdentifierID(this_token.data.Get<string_view>());
            Accept();
            auto [type, data] = this_token;
            if (type == TokenType::Operator && data.Get<Operator>() == Operator::Assign)
            {
                Accept();
                d.init = Parse().ToPtr();
            }
            else if (type == TokenType::Identifier)
            {
                abort();
            }
            r = std::move(d);
            break;
        }
        case TokenType::Operator:
        {
            const auto op = this_token.data.Get<Operator>();
            Accept();
            r = BinaryExpression(lhs.ToPtr(), Parse().ToPtr(), op);
            break;
        }
        case TokenType::ParenLeft:
            Accept();
            if (lhs.Is<Identifier>())
                r = ParseFunction(lhs.Get<Identifier>()); // Might be a function declaration!
            else
                r = lhs;
            break;
        case TokenType::Semicolon:
            Accept();
            [[fallthrough]];
        default:
            r = lhs;
            break;
        }
        Accept(TokenType::Semicolon);
        return r;
    }

    Expression Parser::Parse()
    {
        Accept(TokenType::MaxEnum);

        auto [type, data] = this_token;
        Accept();
        switch (type)
        {
        case TokenType::Keyword:
            switch (data.Get<Keyword>())
            {
            case Keyword::Use:
                return ParseByUse();
            case Keyword::Namespace:
                return ParseByNamespace();
            case Keyword::Type:
                return ParseType();
            case Keyword::Enum:
                return ParseByEnum();
            case Keyword::True:
                return ParseFactors(LiteralBool(true));
            case Keyword::False:
                return ParseFactors(LiteralBool(false));
            case Keyword::Nil:
                return ParseFactors(LiteralNil());
            case Keyword::Void:
                return ParseTypeDecl(Void());
            case Keyword::Let:
                return ParseTypeDecl(Type());
            case Keyword::Bool:
                return ParseTypeDecl(Bool());
            case Keyword::Int:
                return ParseTypeDecl(Int(ParseFundamentalTypeBits(Int::DefaultBitWidth)));
            case Keyword::UInt:
                return ParseTypeDecl(UInt(ParseFundamentalTypeBits(UInt::DefaultBitWidth)));
            case Keyword::Float:
                return ParseTypeDecl(Float(ParseFundamentalTypeBits(Float::DefaultBitWidth)));
            case Keyword::If:
                return ParseBranch();
            case Keyword::Select:
                return ParseSelect();
            case Keyword::Do:
                return ParseDoWhile();
            case Keyword::While:
                return ParseWhile();
            case Keyword::For:
                return ParseFor();
            case Keyword::Break:
                return Break();
            case Keyword::Continue:
                return Continue();
            case Keyword::Defer:
                return Defer(Parse().ToPtr());
            case Keyword::Return:
                return Return(Parse().ToPtr());
            case Keyword::Yield:
                return Yield(Parse().ToPtr());
            default:
                Error("");
            }
            break;
        case TokenType::Identifier:
            return ParseFactors(Identifier(GetIdentifierID(data.Get<string_view>())));
        case TokenType::LiteralInt:
            return ParseFactors(LiteralInt(data.Get<uint64>()));
        case TokenType::LiteralReal:
            return ParseFactors(LiteralReal(data.Get<double>()));
        case TokenType::Operator:
        {
            auto op = data.Get<Operator>();
            switch (op)
            {
            case Operator::Add:
            case Operator::Sub:
            case Operator::Complement:
            case Operator::Increment:
            case Operator::Decrement:
            case Operator::BoolNot:
                return UnaryExpression{ Parse().ToPtr(), op };
            default:
                Error("");
            }
        }
        case TokenType::Wildcard:
            return Wildcard();
        case TokenType::BraceLeft:
            return ParseScope();
        case TokenType::BracketLeft:
            return ParseBracket();
        case TokenType::ParenLeft:
            return ParseParenthesis();
        case TokenType::Comma:
            abort();
        case TokenType::Colon:
            break;
        case TokenType::Semicolon:
            break;
        case TokenType::TraitsOf:
            break;
        case TokenType::Address:
            break;
        default:
            return Expression();
        }
        Error("");
    }

    Module Parser::ParseFile()
    {
        while (true)
        {
            auto e = Parse();
            if (e.IsEmpty())
                break;
            this_module.global_scope.expressions.push_back(std::move(e));
        }
        return this_module;
    }
}