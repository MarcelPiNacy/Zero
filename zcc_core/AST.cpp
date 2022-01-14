#include "AST.hpp"
#include "Parser.hpp"



namespace Zero
{
    namespace Detail
    {
        std::pair<bool, Type> NoReturnType::InferReturnType(Parser& parser)
        {
            return std::make_pair(false, Type());
        }

        Type Untyped::GetType(Parser& parser)
        {
            abort();
        }

        HashT ExpressionHasher::operator()(const Expression& e) const
        {
            return e.GetHash();
        }

        template <bool V>
        struct Result
        {
            static constexpr bool Value = V;
        };

        template <typename T>
        struct IsScopedPtr :                    Result<false> { };

        template <typename T, typename U>
        struct IsScopedPtr<ScopedPtr<T, U>> :   Result<true> { };
        
        template <typename T>
        struct IsVector :                       Result<false> { };

        template <typename T, typename U>
        struct IsVector<vector<T, U>> :         Result<true> { };

        // Please don't ask me what this does, I don't want to know:

        template<typename T, typename F>
        constexpr auto HasMember(F&& f) -> decltype(f(std::declval<T>()), true)
        {
            return true;
        }

        template <typename>
        constexpr bool HasMember(...)
        {
            return false;
        }
    }



    template <typename T>
    static HashT HashAll(T&& value)
    {
        using V = std::remove_const_t<std::remove_reference_t<T>>;

        if constexpr (Detail::IsScopedPtr<V>::Value)
        {
            return value->GetHash();
        }
        else if constexpr (std::is_pointer_v<V>)
        {
            return value->GetHash();
        }
        else if constexpr (Detail::IsVector<V>::Value)
        {
            HashT r = 0;
            for (auto& e : value)
                r ^= HashAll(e);
            return r;
        }
        else
        {
            return value.GetHash();
        }
    }

    template <typename T, typename... U>
    static HashT HashAll(T&& first, U&&... rest)
    {
        return HashAll(first) ^ HashAll(std::forward<U>(rest)...);
    }



    bool Declaration::operator==(const Declaration& other) const
    {
        return type == other.type && name == other.name && init == other.init;
    }

    Type Declaration::GetType(Parser& parser) const
    {
        return type->GetType(parser);
    }

    bool Declaration::IsConst() const
    {
        return type->IsConst() && name.IsConst() && init->IsConst();
    }

    HashT Declaration::GetHash() const
    {
        return HashAll(type, name, init);
    }

    bool Expression::IsConst() const
    {
        bool r = false;
        this->Visit([&](auto& e)
        {
            r = e.IsConst();
        });
        return r;
    }

    Type Expression::GetType(Parser& parser) const
    {
        Type r = {};
        this->Visit([&](auto& e)
        {
            r = e.GetType(parser);
        });
        return r;
    }

    std::pair<bool, Type> Expression::InferReturnType(Parser& parser) const
    {
        std::pair<bool, Type> r = {};
        this->Visit([&](auto& e)
        {
            r = e.InferReturnType(parser);
        });
        return r;
    }

    ScopedPtr<Expression> Expression::ToPtr()
    {
        return ScopedPtr<std::remove_reference_t<decltype(*this)>>::New(std::move(*this));
    }

    bool Expression::operator==(const Expression& other) const
    {
        if (ID() != other.ID())
            return false;
        bool r = false;
        Visit([&](auto& lhs)
        {
            other.Visit([&](auto& rhs)
            {
                if constexpr (std::is_same_v<std::remove_reference_t<decltype(lhs)>, std::remove_reference_t<decltype(rhs)>>)
                    r = lhs == rhs;
            });
        });
        return r;
    }

    HashT Expression::GetHash() const
    {
        HashT r = 0;
        Visit([&](auto& e) { r = e.GetHash(); });
        return r;
    }

    bool Function::operator==(const Function& other) const
    {
        if (params != other.params)
            return false;
        if (*body != *other.body || *return_type != *other.return_type)
            return false;
        for (size_t i = 0; i < params.size(); i++)
            if (params[i] != other.params[i])
                return false;
        return true;
    }

    bool Function::IsConst() const
    {
        if (!return_type->IsConst() || !body->IsConst())
            return false;
        for (auto& e : params)
            if (!e.IsConst())
                return false;
        return true;
    }

    Type Function::GetType(Parser& parser) const
    {
        FunctionType r = {};
        r.return_type = r.return_type;
        r.param_types.reserve(params.size());
        for (auto& e : params)
            r.param_types.push_back(e.GetType(parser));
        return r.ToPtr();
    }

    HashT Function::GetHash() const
    {
        auto r = body->GetHash() ^ return_type->GetHash();
        for (auto& e : params)
            r ^= e.GetHash();
        return r;
    }

    bool Scope::operator==(const Scope& other) const
    {
        if (expressions.size() != other.expressions.size())
            return false;
        for (size_t i = 0; i < expressions.size(); i++)
            if (expressions[i] != other.expressions[i])
                return false;
        return true;
    }

    bool Scope::IsConst() const
    {
        for (auto& e : expressions)
            if (!e.IsConst())
                return false;
        return true;
    }

    std::pair<bool, Type> Scope::InferReturnType(Parser& parser) const
    {
        std::vector<Type> candidates;
        candidates.reserve(expressions.size());
        for (auto& e : expressions)
        {
            std::pair<bool, Type> v = {};
            if (e.Is<Return>() || e.Is<Yield>() || e.Is<Scope>())
                v = e.InferReturnType(parser);

            if (v.first)
                candidates.push_back(v.second);
        }

        if (candidates.size() == 0)
            return { false, Void() };

        auto& first = candidates.front();
        for (auto i = candidates.begin() + 1; i != candidates.end(); ++i)
            if (i->GetType(parser) != first.GetType(parser))
                parser.Error("Ambiguous function return type.");
        return std::make_pair<bool, Type>(true, std::move(first));
    }

    HashT Scope::GetHash() const
    {
        HashT r = 0;
        for (auto& e : expressions)
            r ^= e.GetHash();
        return r;
    }

    bool Branch::operator==(const Branch& other) const
    {
        if (((on_false == nullptr || other.on_false == nullptr) && (on_false != other.on_false)) || // Check if one branch has on_false and the other one doesn't.
            on_false != nullptr && *on_false != *other.on_false)                                    // Check for on_false equality.
            return false;
        return *condition == *other.condition && *on_true == *other.on_true;
    }

    bool Branch::IsConst() const
    {
        if (on_false != nullptr && !on_false->IsConst())
            return false;
        return condition->IsConst() && on_true->IsConst();
    }

    std::pair<bool, Type> Branch::InferReturnType(Parser& parser) const
    {
        auto tr = on_true->InferReturnType(parser);
        auto fr = on_false->InferReturnType(parser);
        if (tr.first && fr.first)
            if (tr.second != fr.second)
                parser.Error("Ambiguous function return type.");
        return tr.first ? std::move(tr) : std::move(fr);
    }

    HashT Branch::GetHash() const
    {
        return condition->GetHash() ^ on_true->GetHash() ^ on_false->GetHash();
    }

    bool Select::operator==(const Select& other) const
    {
        if (cases.size() != other.cases.size())
            return false;
        for (auto& e : cases)
        {
            auto i = other.cases.find(e.first);
            if (i == other.cases.cend())
                return false;
            if (i->second != e.second)
                return false;
        }
        return *key == *other.key && *default_case == *other.default_case;
    }

    bool Select::IsConst() const
    {
        if (!key->IsConst() || !default_case->IsConst())
            return false;
        for (auto& e : cases)
            if (!e.first.IsConst() || ! e.second.IsConst())
                return false;
        return true;
    }

    std::pair<bool, Type> Select::InferReturnType(Parser& parser) const
    {
        std::vector<Type> candidates;
        candidates.reserve(cases.size());
        for (auto& e : cases)
        {
            std::pair<bool, Type> v = {};
            if (e.second.Is<Return>() || e.second.Is<Yield>() || e.second.Is<Scope>())
                v = e.second.InferReturnType(parser);

            if (v.first)
                candidates.push_back(v.second);
        }

        if (default_case != nullptr)
        {
            auto& e = *default_case;
            std::pair<bool, Type> v = {};
            if (e.Is<Return>() || e.Is<Yield>() || e.Is<Scope>())
                v = e.InferReturnType(parser);

            if (v.first)
                candidates.push_back(v.second);

            if (candidates.size() == 0)
                return { false, Void() };
        }

        auto& first = candidates.front();
        for (auto i = candidates.begin() + 1; i != candidates.end(); ++i)
            if (i->GetType(parser) != first.GetType(parser))
                parser.Error("Ambiguous function return type.");
        return std::make_pair<bool, Type>(true, std::move(first));
    }

    HashT Select::GetHash() const
    {
        auto r = key->GetHash() ^ default_case->GetHash();
        for (auto& e : cases)
            r ^= e.first.GetHash() ^ e.second.GetHash();
        return r;
    }

    bool While::operator==(const While& other) const
    {
        return *condition == *other.condition && *body == *other.body;
    }

    bool While::IsConst() const
    {
        return condition->IsConst() && body->IsConst();
    }

    std::pair<bool, Type> While::InferReturnType(Parser& parser) const
    {
        return body->InferReturnType(parser);
    }

    HashT While::GetHash() const
    {
        return condition->GetHash() ^ body->GetHash();
    }

    bool DoWhile::operator==(const DoWhile& other) const
    {
        return *condition == *other.condition && *body == *other.body;
    }

    bool DoWhile::IsConst() const
    {
        return condition->IsConst() && body->IsConst();
    }

    std::pair<bool, Type> DoWhile::InferReturnType(Parser& parser) const
    {
        return body->InferReturnType(parser);
    }

    HashT DoWhile::GetHash() const
    {
        return condition->GetHash() ^ body->GetHash();
    }

    bool For::operator==(const For& other) const
    {
        return false;
    }

    bool For::IsConst() const
    {
        return init->IsConst() && condition->IsConst() && update->IsConst() && body->IsConst();
    }

    std::pair<bool, Type> For::InferReturnType(Parser& parser) const
    {
        return body->InferReturnType(parser);
    }

    HashT For::GetHash() const
    {
        return
            init->GetHash() ^
            condition->GetHash() ^
            update->GetHash() ^
            body->GetHash();
    }

    bool ForEach::operator==(const ForEach& other) const
    {
        return *iterator == *other.iterator && *collection == *other.collection && *body == *other.body;
    }

    bool ForEach::IsConst() const
    {
        return iterator->IsConst() && collection->IsConst() && body->IsConst();
    }

    std::pair<bool, Type> ForEach::InferReturnType(Parser& parser) const
    {
        return body->InferReturnType(parser);
    }

    HashT ForEach::GetHash() const
    {
        return iterator->GetHash() ^ collection->GetHash() ^ body->GetHash();
    }

    bool UnaryExpression::operator==(const UnaryExpression& other) const
    {
        return op == other.op && *this->other == *other.other;
    }

    bool UnaryExpression::IsConst() const
    {
        return other->IsConst();
    }

    Type UnaryExpression::GetType(Parser& parser) const
    {
        abort();
    }

    HashT UnaryExpression::GetHash() const
    {
        return other->GetHash() ^ WellonsMix((HashT)op);
    }

    bool BinaryExpression::operator==(const BinaryExpression& other) const
    {
        return op == other.op && *lhs == *other.lhs && *rhs == *other.rhs;
    }

    bool BinaryExpression::IsConst() const
    {
        return lhs->IsConst() && rhs->IsConst();
    }

    Type BinaryExpression::GetType(Parser& parser) const
    {
        abort();
    }

    HashT BinaryExpression::GetHash() const
    {
        return lhs->GetHash() ^ rhs->GetHash() ^ WellonsMix((HashT)op);
    }

    bool Defer::operator==(const Defer& other) const
    {
        return *body == *other.body;
    }

    bool Defer::IsConst() const
    {
        return false;
    }

    HashT Defer::GetHash() const
    {
        return WellonsMix(body->GetHash());
    }

    bool Return::operator==(const Return& other) const
    {
        return *value == *other.value;
    }

    bool Return::IsConst() const
    {
        return false;
    }

    std::pair<bool, Type> Return::InferReturnType(Parser& parser) const
    {
        return
            value != nullptr ?
            std::make_pair(true, value->GetType(parser)) :
            std::make_pair(true, Type(Void()));
    }

    HashT Return::GetHash() const
    {
        return WellonsMix(value->GetHash());
    }

    bool Yield::operator==(const Yield& other) const
    {
        return *value == *other.value;
    }

    bool Yield::IsConst() const
    {
        return false;
    }

    std::pair<bool, Type> Yield::InferReturnType(Parser& parser) const
    {
        return
            value != nullptr ?
            std::make_pair(true, value->GetType(parser)) :
            std::make_pair(true, Type(Void()));
    }

    HashT Yield::GetHash() const
    {
        return WellonsMix(value->GetHash());
    }

    bool Cast::operator==(const Cast& other) const
    {
        return value == other.value;
    }

    bool Cast::IsConst() const
    {
        return false;
    }

    HashT Cast::GetHash() const
    {
        return value->GetHash() ^ new_type->GetHash();
    }

    bool Type::operator==(const Type& other) const
    {
        if (ID() != other.ID())
            return false;
        bool r = false;
        Visit([&](auto& lhs)
        {
            other.Visit([&](auto& rhs)
            {
                if constexpr (std::is_same_v<std::remove_reference_t<decltype(lhs)>, std::remove_reference_t<decltype(rhs)>>)
                    r = lhs == rhs;
            });
        });
        return r;
    }

    Type Type::GetType(Parser& parser) const
    {
        return *this;
    }

    ScopedPtr<Type> Type::ToPtr()
    {
        return ScopedPtr<std::remove_reference_t<decltype(*this)>>::New(std::move(*this));
    }

    template <typename T>
    struct IsScopedPtr
    {
        static constexpr bool Value = false;
    };

    template <typename T>
    struct IsScopedPtr<ScopedPtr<T>>
    {
        static constexpr bool Value = true;
    };

    HashT Type::GetHash() const
    {
        HashT r = 0;
        Visit([&](auto& e)
        {
            if constexpr (IsScopedPtr<std::remove_reference_t<decltype(e)>>::Value)
                r = e->GetHash();
            else
                r = e.GetHash();
        });
        return r;
    }

    HashT NoOp::GetHash() const
    {
        constexpr char key[] = __DATE__ "$" __TIME__;
        static const auto h = XXHash64(key, sizeof(key) - 1);
        return h;
    }

    Type Identifier::GetType(Parser& parser) const
    {
        abort();
    }

    HashT Identifier::GetHash() const
    {
        return WellonsMix((HashT)id);
    }

    bool Identifier::operator==(const Identifier& other) const
    {
        return id == other.id;
    }

    HashT Use::GetHash() const
    {
        HashT r = 0;
        for (auto& e : modules)
            r ^= e.GetHash();
        return r;
    }

    HashT Namespace::GetHash() const
    {
        abort();
    }

    Type LiteralBool::GetType(Parser& parser) const
    {
        return Bool();
    }

    HashT LiteralBool::GetHash() const
    {
        return WellonsMix((HashT)value);
    }

    Type LiteralNil::GetType(Parser& parser) const
    {
        return Nil();
    }

    HashT LiteralNil::GetHash() const
    {
        const auto h = WellonsMix(WellonsMix((HashT)(__LINE__)) ^ WellonsMix((HashT)(__COUNTER__)));
        return h;
    }

    Type LiteralInt::GetType(Parser& parser) const
    {
        return Int();
    }

    HashT LiteralInt::GetHash() const
    {
        return WellonsMix((HashT)value);
    }

    Type LiteralUint::GetType(Parser& parser) const
    {
        return UInt();
    }

    HashT LiteralUint::GetHash() const
    {
        return WellonsMix((HashT)value);
    }

    Type LiteralReal::GetType(Parser& parser) const
    {
        return Float();
    }

    HashT LiteralReal::GetHash() const
    {
        HashT x = 0;
        (void)memcpy(&x, &value, sizeof(x));
        return WellonsMix(x);
    }

    HashT Break::GetHash() const
    {
        const auto h = WellonsMix(WellonsMix((HashT)(__LINE__)) ^ WellonsMix((HashT)(__COUNTER__)));
        return h;
    }

    HashT Continue::GetHash() const
    {
        return HashT();
    }

    HashT Wildcard::GetHash() const
    {
        const auto h = WellonsMix(WellonsMix((HashT)(__LINE__)) ^ WellonsMix((HashT)(__COUNTER__)));
        return h;
    }

    bool TraitsOf::operator==(const TraitsOf& other) const
    {
        return *value == *other.value;
    }

    HashT TraitsOf::GetHash() const
    {
        return WellonsMix(value->GetHash());
    }

    bool Array::operator==(const Array& other) const
    {
        return size == other.size && type == other.type;
    }

    HashT Array::GetHash() const
    {
        return WellonsMix(size) ^ type->GetHash();
    }

    ScopedPtr<Array> Array::ToPtr()
    {
        return ScopedPtr<std::remove_reference_t<decltype(*this)>>::New(std::move(*this));
    }

    bool Tuple::operator==(const Tuple& other) const
    {
        if (types.size() != other.types.size())
            return false;
        for (size_t i = 0; i != types.size(); ++i)
            if (types[i] != other.types[i])
                return false;
        return true;
    }

    HashT Tuple::GetHash() const
    {
        HashT r = 0;
        for (auto& e : types)
            r ^= e.GetHash();
        return r;
    }

    ScopedPtr<Tuple> Tuple::ToPtr()
    {
        return ScopedPtr<std::remove_reference_t<decltype(*this)>>::New(std::move(*this));
    }

    bool Record::operator==(const Record& other) const
    {
        if (fields.size() != other.fields.size())
            return false;

        for (size_t i = 0; i < fields.size(); i++)
            if (fields[i] != other.fields[i])
                return false;
        return
            operators == other.operators &&
            variables == other.variables &&
            variables_static == other.variables_static &&
            functions == other.functions &&
            functions_static == other.functions_static;
    }

    HashT Record::GetHash() const
    {
        HashT r = 0;
        for (auto& e : fields)
            r ^= e.GetHash();
        return r;
    }

    ScopedPtr<Record> Record::ToPtr()
    {
        return ScopedPtr<std::remove_reference_t<decltype(*this)>>::New(std::move(*this));
    }

    bool Enum::operator==(const Enum& other) const
    {
        if (*underlying_type != *other.underlying_type)
            return false;
        for (auto& e : values)
        {
            auto it = other.values.find(e.first);
            if (it == other.values.cend())
                return false;
            if (it->second != e.second)
                return false;
        }
        return true;
    }

    HashT Enum::GetHash() const
    {
        HashT r = 0;
        r ^= underlying_type->GetHash();
        for (auto& e : values)
            r ^= WellonsMix((HashT)e.first) ^ e.second.GetHash();
        return r;
    }

    ScopedPtr<Enum> Enum::ToPtr()
    {
        return ScopedPtr<std::remove_reference_t<decltype(*this)>>::New(std::move(*this));
    }

    HashT Void::GetHash()
    {
        const auto h = WellonsMix(WellonsMix((HashT)(__LINE__)) ^ WellonsMix((HashT)(__COUNTER__)));
        return h;
    }

    HashT Nil::GetHash()
    {
        const auto h = WellonsMix(WellonsMix((HashT)(__LINE__)) ^ WellonsMix((HashT)(__COUNTER__)));
        return h;
    }

    HashT Bool::GetHash()
    {
        const auto h = WellonsMix(WellonsMix((HashT)(__LINE__)) ^ WellonsMix((HashT)(__COUNTER__)));
        return h;
    }

    HashT Int::GetHash() const
    {
        const auto h = WellonsMix(WellonsMix((HashT)(__LINE__)) ^ WellonsMix((HashT)(__COUNTER__)));
        const auto h2 = WellonsMix(DefaultBitWidth);
        return h ^ (bits == DefaultBitWidth ? h2 : WellonsMix(bits));
    }

    HashT UInt::GetHash() const
    {
        const auto h = WellonsMix(WellonsMix((HashT)(__LINE__)) ^ WellonsMix((HashT)(__COUNTER__)));
        const auto h2 = WellonsMix(DefaultBitWidth);
        return h ^ (bits == DefaultBitWidth ? h2 : WellonsMix(bits));
    }

    HashT Float::GetHash() const
    {
        const auto h = WellonsMix((HashT)(__LINE__)) ^ WellonsMix((HashT)(__COUNTER__));
        const auto h2 = WellonsMix(DefaultBitWidth);
        return h ^ (bits == DefaultBitWidth ? h2 : WellonsMix(bits));
    }

    bool ConstructorCall::operator==(const ConstructorCall& other) const
    {
        if (parameters.size() != other.parameters.size() || *object != *other.object)
            return false;
        for (size_t i = 0; i < parameters.size(); i++)
            if (parameters[i] != other.parameters[i])
                return false;
        return true;
    }

    Type ConstructorCall::GetType(Parser& parser) const
    {
        return object->GetType(parser);
    }

    HashT ConstructorCall::GetHash() const
    {
        HashT r = 0;
        for (auto& e : parameters)
            r ^= e.GetHash();
        return r;
    }

    bool DestructorCall::operator==(const DestructorCall& other) const
    {
        return *object == *other.object;
    }

    HashT DestructorCall::GetHash() const
    {
        return WellonsMix(object->GetHash());
    }

    bool FunctionCall::operator==(const FunctionCall& other) const
    {
        if (params.size() != other.params.size())
            return false;
        if (*callable != *other.callable)
            return false;
        for (size_t i = 0; i != params.size(); ++i)
            if (params[i] != other.params[i])
                return false;
        return true;
    }

    bool FunctionCall::IsConst() const
    {
        return false;
    }

    Type FunctionCall::GetType(Parser& parser) const
    {
        return callable->InferReturnType(parser).second;
    }

    std::pair<bool, Type> FunctionCall::InferReturnType(Parser& parser) const
    {
        abort();
    }

    HashT FunctionCall::GetHash() const
    {
        auto r = callable->GetHash();
        for (auto& e : params)
            r ^= e.GetHash();
        return r;
    }

    bool FunctionType::operator==(const FunctionType& other) const
    {
        if (param_types.size() != other.param_types.size() || *return_type != *other.return_type)
            return false;
        for (size_t i = 0; i != param_types.size(); ++i)
            if (param_types[i] != other.param_types[i])
                return false;
        return true;
    }

    HashT FunctionType::GetHash() const
    {
        auto r = return_type->GetHash();
        for (auto& e : param_types)
            r ^= e.GetHash();
        return r;
    }

    ScopedPtr<FunctionType> FunctionType::ToPtr()
    {
        return ScopedPtr<std::remove_reference_t<decltype(*this)>>::New(std::move(*this));
    }
    
    HashT MetaType::GetHash()
    {
        const auto h = WellonsMix(WellonsMix((HashT)(__LINE__)) ^ WellonsMix((HashT)(__COUNTER__)));
        return h;
    }

    bool Union::operator==(const Union& other) const
    {
        if (fields.size() != other.fields.size())
            return false;
        for (size_t i = 0; i < fields.size(); i++)
            if (fields[i] != other.fields[i])
                return false;
        return
            operators == other.operators &&
            variables == other.variables &&
            variables_static == other.variables_static &&
            functions == other.functions &&
            functions_static == other.functions_static;
    }

    HashT Union::GetHash() const
    {
        HashT h = 0;
        for (auto& e : fields)
            h ^= e.GetHash();
        return h;
    }

    ScopedPtr<Union> Union::ToPtr()
    {
        return ScopedPtr<std::remove_reference_t<decltype(*this)>>::New(std::move(*this));
    }

    bool Variant::operator==(const Variant& other) const
    {
        if (types.size() != other.types.size())
            return false;

        for (size_t i = 0; i < types.size(); i++)
            if (types[i] != other.types[i])
                return false;

        return key == other.key;
    }
    
    HashT Variant::GetHash() const
    {
        return 0;
    }

    ScopedPtr<Variant> Variant::ToPtr()
    {
        return ScopedPtr<std::remove_reference_t<decltype(*this)>>::New(std::move(*this));
    }
}