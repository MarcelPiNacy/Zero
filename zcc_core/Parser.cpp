#include "Parser.hpp"

namespace Zero
{
    Parser::Parser(string_view text) :
        tokenizer(text),
        identifiers(),
        ptr_size(sizeof(uintptr) * 8),
        this_token{ TokenType::MaxEnum, {} }
    {
    }

    IdentifierID Parser::GetIdentifierID(string_view name)
    {
        auto count = identifiers.size();
        auto& e = identifiers[name];
        if (identifiers.size() == count)
            return e;
        e = (IdentifierID)count;
        return e;
    }

    void Parser::Reset()
    {
        Destruct(identifiers);
        tokenizer.cursor = tokenizer.begin;
        this_token.type = TokenType::MaxEnum;
    }

    Parser::TokenInfo Parser::NextToken()
    {
        auto r = ThisToken();
        Accept();
        return r;
    }

    Parser::TokenInfo Parser::ThisToken()
    {
        if (this_token.type == TokenType::MaxEnum)
            Accept();
        return this_token;
    }

    void Parser::Accept()
    {
        this_token.type = tokenizer.NextToken(this_token.data);
    }

    void Parser::Expect(TokenType type, string_view message)
    {
        Assert(ThisToken().type == type, message);
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

    void Parser::AcceptSemicolon()
    {
        if (ThisToken().type == TokenType::Semicolon)
            Accept();
    }

    Expression Parser::ParseGenericRecord()
    {
        vector<Expression> params;
        while (ThisToken().type != TokenType::ParenRight)
        {
            params.push_back(Parse());
            if (ThisToken().type == TokenType::Comma)
                Accept();
        }
        abort();
    }

    Expression Parser::ParseRecord(optional<UnqualifiedIdentifier> name)
    {
        Record r = {};

        while (ThisToken().type != TokenType::BraceRight)
        {
            auto e = Parse();
            Assert(e.Is<Declaration>(), "");
            r.fields.push_back(e.Get<Declaration>());
        }

        Accept();

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
        auto e = ThisToken();
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
        do
        {
            r.modules.push_back(Parse());
        } while (ThisToken().type == TokenType::Comma);
        Accept();
        return r;
    }

    Namespace Parser::ParseByNamespace()
    {
        Namespace r = {};
        while (ThisToken().type != TokenType::BraceRight)
            r.elements.push_back(Parse());
        return r;
    }

    Expression Parser::ParseType()
    {
        Expression r;
        optional<UnqualifiedIdentifier> name = std::nullopt;
        if (ThisToken().type == TokenType::Identifier)
        {
            name = GetIdentifierID(ThisToken().data.Get<string_view>());
            Accept();
        }

        auto [type, data] = ThisToken();
        switch (type)
        {
        case TokenType::Operator:
            Assert(data.Get<Operator>() == Operator::Assign, "");
            Accept();
            r = Declaration(Expression(Type(MetaType())).ToPtr(), name.value(), Expression(Parse().GetType(*this)).ToPtr());
            AcceptSemicolon();
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
            if (type == TokenType::Semicolon)
                Accept();
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
        Expect(TokenType::Identifier, "");
        auto id = GetIdentifierID(NextToken().data.Get<string_view>());
        r.name = id;
        if (auto token = ThisToken(); token.type == TokenType::Colon)
        {
            Accept();
            e.underlying_type = Parse().ToPtr();
        }
        ExpectAndAccept(TokenType::BraceLeft, "");
        while (ThisToken().type != TokenType::BraceRight)
        {
            string_view name;
            Operator op = {};

            Assert(PopMany(
                std::make_tuple(TokenType::Identifier, &name),
                std::make_tuple(TokenType::Operator, &op, [](Operator o) { return o == Operator::Assign; })),
                "");

            id = GetIdentifierID(name);
            e.values.insert(std::make_pair(id, Parse()));

            if (ThisToken().type == TokenType::Comma)
                Accept();
        }
        Accept();
        r.init = Expression(Type(e.ToPtr())).ToPtr();
        return r;
    }

    Expression Parser::ParseByType(Type type)
    {
        Declaration r = {};
        if (!type.IsEmpty())
            r.type = Expression(type).ToPtr();
        auto token = ThisToken();
        if (token.type != TokenType::Identifier)
            return type;
        auto id = GetIdentifierID(token.data.Get<string_view>());
        r.name = id;
        Accept();
        if (auto [t, data] = ThisToken(); t == TokenType::Operator && data.Get<Operator>() == Operator::Assign)
        {
            Accept();
            r.init = Parse().ToPtr();
        }
        AcceptSemicolon();
        if (r.type == nullptr || r.type->IsEmpty())
            r.type = Expression(r.init->GetType(*this)).ToPtr();
        return r;
    }

    Branch Parser::ParseIf()
    {
        Branch r = {};

        r.condition = Parse().ToPtr();
        
        {
            auto [type, data] = ThisToken();
            switch (type)
            {
            case TokenType::BraceLeft:
                break;
            case TokenType::Keyword:
                Assert(data.Get<Keyword>() == Keyword::Do, "");
                Accept();
                break;
            default:
                Error("");
                break;
            }
        }

        r.on_true = Parse().ToPtr();

        {
            auto [type, data] = ThisToken();
            if (type == TokenType::Keyword)
            {
                switch (data.Get<Keyword>())
                {
                case Keyword::Elif:
                    Accept();
                    r.on_false = Expression(ParseIf()).ToPtr();
                    break;
                case Keyword::Else:
                    Accept();
                    r.on_false = Parse().ToPtr();
                    break;
                default:
                    break;
                }
            }
        }

        return r;
    }

    Select Parser::ParseSelect()
    {
        Select r = {};
        r.key = Parse().ToPtr();
        ExpectAndAccept(TokenType::BraceLeft, "");
        while (ThisToken().type != TokenType::BraceRight)
        {
            auto [type, data] = ThisToken();
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
        auto [type, data] = ThisToken();
        switch (type)
        {
        case TokenType::BraceLeft:
            break;
        case TokenType::Keyword:
            Assert(data.Get<Keyword>() == Keyword::Do, "");
            Accept();
            break;
        default:
            Error("");
            break;
        }
        r.body = Parse().ToPtr();
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
        if (ThisToken().type == TokenType::Colon)
        {
            Accept();
            ForEach r = {};
            r.iterator = first.ToPtr();
            r.collection = Parse().ToPtr();
            auto [type, data] = ThisToken();
            switch (type)
            {
            case TokenType::BraceLeft:
                break;
            case TokenType::Keyword:
                Assert(data.Get<Keyword>() == Keyword::Do, "");
                Accept();
                break;
            default:
                Error("");
                break;
            }
            r.body = Parse().ToPtr();
            return r;
        }
        else
        {
            For r = {};
            r.init = first.ToPtr();
            AcceptSemicolon();
            r.condition = Parse().ToPtr();
            r.update = Parse().ToPtr();
            auto [type, data] = ThisToken();
            switch (type)
            {
            case TokenType::BraceLeft:
                break;
            case TokenType::Keyword:
                Assert(data.Get<Keyword>() == Keyword::Do, "");
                Accept();
                break;
            default:
                Error("");
                break;
            }
            r.body = Parse().ToPtr();
            return r;
        }
    }

    Scope Parser::ParseScope()
    {
        Scope r = {};
        while (ThisToken().type != TokenType::BraceRight)
        {
            Assert(ThisToken().type != TokenType::MaxEnum, "");
            r.expressions.push_back(Parse());
        }
        Accept();
        return r;
    }

    Expression Parser::ParseBracket()
    {
        Expression r;
        bool any_type = false;
        bool any_value = false;

        auto contents = ParseBracketContents();
        for (const auto& e : contents)
        {
            any_type = any_type || e.Is<Type>();
            any_value = any_value || !e.Is<Type>();
        }

        ExpectAndAccept(TokenType::BracketRight, "");

        if (!any_value)
        {
            Tuple t;
            for (size_t i = 0; i < contents.size(); i++)
            {

            }
            Declaration d;
            d.type = Expression(Type(t.ToPtr())).ToPtr();
            auto token = ThisToken();
            d.name = UnqualifiedIdentifier(GetIdentifierID(token.data.Get<string_view>()));
            Accept();
            if (ThisToken().type == TokenType::Operator && ThisToken().data.Get<Operator>() == Operator::Assign)
            {
                Accept();
                d.init = Parse().ToPtr();
            }
            r = d;
        }
        else
        {
            r = Type(t.ToPtr());
        }
        
        AcceptSemicolon();

        return r;
    }

    vector<Expression> Parser::ParseBracketContents()
    {
        vector<Expression> r;
        if (ThisToken().type != TokenType::BracketRight)
        {
            while (true)
            {
                auto e = Parse();
                r.push_back(e.GetType(*this));
                if (ThisToken().type == TokenType::BracketRight)
                    break;
                ExpectAndAccept(TokenType::Comma, "");
            }
        }
        return r;
    }

    Expression Parser::ParseParenthesis()
    {
        Function r = {};
        vector<Expression> expressions;

        if (ThisToken().type != TokenType::ParenRight)
        {
            while (true)
            {
                expressions.push_back(Parse());
                if (ThisToken().type == TokenType::ParenRight)
                    break;
                ExpectAndAccept(TokenType::Comma, "");
            }
        }

        Accept();

        const auto type = ThisToken().type;
        if (type != TokenType::Colon && type != TokenType::Arrow)
        {
            Assert(expressions.size() == 1, "");
            return expressions[0];
        }

        r.params = std::move(expressions);

        if (ThisToken().type == TokenType::Arrow)
        {
            Accept();
            r.return_type = Parse().ToPtr();
        }

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

    Expression Parser::ParseFunction(optional<UnqualifiedIdentifier> name)
    {
        Function r = {};
        vector<Expression> expressions;
        if (ThisToken().type != TokenType::ParenRight)
        {
            while (true)
            {
                expressions.push_back(Parse());
                if (ThisToken().type == TokenType::ParenRight)
                    break;
                ExpectAndAccept(TokenType::Comma, "");
            }
        }
        Accept();

        r.params = std::move(expressions);

        if (ThisToken().type == TokenType::Arrow)
        {
            Accept();
            r.return_type = Parse().ToPtr();
        }

        if (ThisToken().type != TokenType::Colon)
        {
            Accept();
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

    Expression Parser::ParseByFactors(Expression lhs)
    {
        Expression r;
        switch (ThisToken().type)
        {
        case TokenType::Keyword:
            if (ThisToken().data.Get<Keyword>() != Keyword::As)
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
            d.name = GetIdentifierID(ThisToken().data.Get<string_view>());
            Accept();
            if (auto [type, data] = ThisToken(); type == TokenType::Operator && data.Get<Operator>() == Operator::Assign)
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
            const auto op = ThisToken().data.Get<Operator>();
            Accept();
            r = BinaryExpression(lhs.ToPtr(), Parse().ToPtr(), op);
            break;
        }
        case TokenType::ParenLeft:
            Accept();
            if (lhs.Is<UnqualifiedIdentifier>())
                r = ParseFunction(lhs.Get<UnqualifiedIdentifier>()); // Might be a function declaration!
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
        AcceptSemicolon();
        return r;
    }

    Expression Parser::Parse()
    {
        auto [type, data] = NextToken();
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
                return ParseByFactors(LiteralBool(true));
            case Keyword::False:
                return ParseByFactors(LiteralBool(false));
            case Keyword::Nil:
                return ParseByFactors(LiteralNil());
            case Keyword::Void:
                return ParseByType(Void());
            case Keyword::Let:
                return ParseByType(Type());
            case Keyword::Bool:
                return ParseByType(Bool());
            case Keyword::Int:
                return ParseByType(Int(ParseFundamentalTypeBits(Int::DefaultBitWidth)));
            case Keyword::UInt:
                return ParseByType(UInt(ParseFundamentalTypeBits(UInt::DefaultBitWidth)));
            case Keyword::Float:
                return ParseByType(Float(ParseFundamentalTypeBits(Float::DefaultBitWidth)));
            case Keyword::If:
                return ParseIf();
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
            return ParseByFactors(UnqualifiedIdentifier(GetIdentifierID(data.Get<string_view>())));
        case TokenType::LiteralInt:
            return ParseByFactors(LiteralInt(data.Get<int64>()));
        case TokenType::LiteralReal:
            return ParseByFactors(LiteralReal(data.Get<double>()));
        case TokenType::Operator:
        {
            const auto op = data.Get<Operator>();
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
        case TokenType::Dot:
            break;
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

    vector<Expression> Parser::ParseFile()
    {
        vector<Expression> r;
        while (true)
        {
            auto e = Parse();
            if (e.IsEmpty())
                break;
            r.push_back(std::move(e));
        }
        return r;
    }
}