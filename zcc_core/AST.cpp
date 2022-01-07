#include "AST.hpp"
#include "Parser.hpp"

#ifdef _WIN32
#include <Windows.h>
#define SPIN_WAIT YieldProcessor()
#else
#include <sys/mman.h>
#define SPIN_WAIT
#endif



#if 1
template <typename T>
struct ASTSharedAllocator
{
    static constexpr size_t PageSizeHint = 4096;
    static constexpr size_t BlockSize = 1U << 21;
    static constexpr size_t BlockCapacity = BlockSize / sizeof(T);

    struct alignas(T) Block
    {
        Block* next;
        std::atomic_uint32_t bump;
    };

    struct Node
    {
        Node* next;
    };

    template <typename A, typename B = A>
    struct MyPair
    {
        A first;
        B second;
    };

    std::atomic<MyPair<Node*, size_t>> free;
    std::atomic<MyPair<Block*, size_t>> head;

#ifdef _WIN32
    static void* OSMalloc(size_t size)
    {
        return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
#endif

    T* New()
    {
        while (true)
        {
            auto prior = free.load(std::memory_order_acquire);
            if (prior.first == nullptr)
                break;
            decltype(prior) desired = { prior.first->next, prior.second + 1 };
            if (free.compare_exchange_weak(prior, desired, std::memory_order_acquire, std::memory_order_relaxed))
                return (T*)prior.first;
            SPIN_WAIT;
        }

        Block* prior_head = nullptr;
        while (true)
        {
            auto i = head.load(std::memory_order_acquire).first;
            if (i == prior_head)
                break;
            prior_head = i;
            do
            {
                auto n = i->bump.fetch_add(1, std::memory_order_acquire);
                if (n < BlockCapacity)
                    return (T*)i + n;
                i->bump.fetch_sub(1, std::memory_order_relaxed);
                i = i->next;
            } while (i != nullptr);
        }

        auto new_head = (Block*)OSMalloc(BlockSize);
        assert(new_head != nullptr);
        new (&new_head->bump) std::atomic_uint32_t(2);
        while (true)
        {
            auto prior = head.load(std::memory_order_acquire);
            new_head->next = prior.first;
            decltype(prior) desired = { new_head, prior.second + 1 };
            if (head.compare_exchange_weak(prior, desired, std::memory_order_release, std::memory_order_relaxed))
                return (T*)new_head + 1;
            SPIN_WAIT;
        }
    }

    void Delete(T* e)
    {
        auto n = (Node*)e;
        while (true)
        {
            auto prior = free.load(std::memory_order_acquire);
            n->next = prior.first;
            decltype(prior) desired = { n, prior.second + 1 };
            if (free.compare_exchange_weak(prior, desired, std::memory_order_release, std::memory_order_relaxed))
                break;
            SPIN_WAIT;
        }
    }
};
#else
template <typename T>
struct ASTSharedAllocator
{
    T* New()
    {
        return new T();
    }

    void Delete(T* e)
    {
        delete e;
    }
};
#endif



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

        uint64 ExpressionHasher::operator()(const Expression& e) const
        {
            return e.GetHash();
        }
    }



    static ASTSharedAllocator<Expression> expression_allocator;
    static ASTSharedAllocator<Enum> enum_allocator;
    static ASTSharedAllocator<Array> array_allocator;
    static ASTSharedAllocator<Tuple> tuple_allocator;
    static ASTSharedAllocator<Record> record_allocator;
    static ASTSharedAllocator<FunctionType> function_type_allocator;
    static ASTSharedAllocator<Type> type_allocator;



    Expression* ScopedPtrTraits<Expression>::New()
    {
        return expression_allocator.New();
    }

    void ScopedPtrTraits<Expression>::Delete(Expression* e)
    {
        expression_allocator.Delete(e);
    }

    Enum* ScopedPtrTraits<Enum>::New()
    {
        return  enum_allocator.New();
    }

    void ScopedPtrTraits<Enum>::Delete(Enum* e)
    {
        enum_allocator.Delete(e);
    }

    Array* ScopedPtrTraits<Array>::New()
    {
        return  array_allocator.New();
    }

    void ScopedPtrTraits<Array>::Delete(Array* e)
    {
        array_allocator.Delete(e);
    }

    Tuple* ScopedPtrTraits<Tuple>::New()
    {
        return tuple_allocator.New();
    }

    void ScopedPtrTraits<Tuple>::Delete(Tuple* e)
    {
        tuple_allocator.Delete(e);
    }

    Record* ScopedPtrTraits<Record>::New()
    {
        return record_allocator.New();
    }

    void ScopedPtrTraits<Record>::Delete(Record* e)
    {
        record_allocator.Delete(e);
    }

    Type* ScopedPtrTraits<Type>::New()
    {
        return type_allocator.New();
    }

    void ScopedPtrTraits<Type>::Delete(Type* e)
    {
        type_allocator.Delete(e);
    }

    FunctionType* ScopedPtrTraits<FunctionType>::New()
    {
        return function_type_allocator.New();
    }

    void ScopedPtrTraits<FunctionType>::Delete(FunctionType* e)
    {
        function_type_allocator.Delete(e);
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

    uint64 Declaration::GetHash() const
    {
        auto r = type->GetHash();
        r ^= name.GetHash();
        r ^= init->GetHash();
        return r;
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
        auto ptr = expression_allocator.New();
        new (ptr) Expression(std::move(*this));
        return ScopedPtr<Expression>(ptr);
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

    uint64 Expression::GetHash() const
    {
        uint64 r = 0;
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

    uint64 Function::GetHash() const
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

    uint64 Scope::GetHash() const
    {
        uint64 r = 0;
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

    uint64 Branch::GetHash() const
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

    uint64 Select::GetHash() const
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

    uint64 While::GetHash() const
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

    uint64 DoWhile::GetHash() const
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

    uint64 For::GetHash() const
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

    uint64 ForEach::GetHash() const
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

    uint64 UnaryExpression::GetHash() const
    {
        return other->GetHash() ^ WellonsMix((uint64)op);
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

    uint64 BinaryExpression::GetHash() const
    {
        return lhs->GetHash() ^ rhs->GetHash() ^ WellonsMix((uint64)op);
    }

    bool Defer::operator==(const Defer& other) const
    {
        return *body == *other.body;
    }

    bool Defer::IsConst() const
    {
        return false;
    }

    uint64 Defer::GetHash() const
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

    uint64 Return::GetHash() const
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

    uint64 Yield::GetHash() const
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

    uint64 Cast::GetHash() const
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
        auto r = type_allocator.New();
        new (r) Type(std::move(*this));
        return ScopedPtr<Type>(r);
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

    uint64 Type::GetHash() const
    {
        uint64 r = 0;
        Visit([&](auto& e)
        {
            if constexpr (IsScopedPtr<std::remove_reference_t<decltype(e)>>::Value)
                r = e->GetHash();
            else
                r = e.GetHash();
        });
        return r;
    }

    uint64 NoOp::GetHash() const
    {
        constexpr char key[] = __DATE__ "$" __TIME__;
        static const auto h = XXHash64(key, sizeof(key) - 1);
        return h;
    }

    Type UnqualifiedIdentifier::GetType(Parser& parser) const
    {
        abort();
    }

    uint64 UnqualifiedIdentifier::GetHash() const
    {
        return WellonsMix((uint64)id);
    }

    bool UnqualifiedIdentifier::operator==(const UnqualifiedIdentifier& other) const
    {
        return id == other.id;
    }

    Type QualifiedIdentifier::GetType(Parser& parser) const
    {
        abort();
    }

    bool QualifiedIdentifier::operator==(const QualifiedIdentifier& other) const
    {
        if (names.size() != other.names.size())
            return false;
        for (uintptr i = 0; i != names.size(); ++i)
            if (names[i] != other.names[i])
                return false;
        return true;
    }

    uint64 QualifiedIdentifier::GetHash() const
    {
        return XXHash64(names.data(), names.size() * sizeof(IdentifierID));
    }

    uint64 Use::GetHash() const
    {
        uint64 r = 0;
        for (auto& e : modules)
            r ^= e.GetHash();
        return r;
    }

    uint64 Namespace::GetHash() const
    {
        abort();
    }

    Type LiteralBool::GetType(Parser& parser) const
    {
        return Bool();
    }

    uint64 LiteralBool::GetHash() const
    {
        return WellonsMix((uint64)value);
    }

    Type LiteralNil::GetType(Parser& parser) const
    {
        return Nil();
    }

    uint64 LiteralNil::GetHash() const
    {
        const auto h = WellonsMix(WellonsMix((uint64)(__LINE__)) ^ WellonsMix((uint64)(__COUNTER__)));
        return h;
    }

    Type LiteralInt::GetType(Parser& parser) const
    {
        return Int();
    }

    uint64 LiteralInt::GetHash() const
    {
        return WellonsMix((uint64)value);
    }

    Type LiteralUint::GetType(Parser& parser) const
    {
        return UInt();
    }

    uint64 LiteralUint::GetHash() const
    {
        return WellonsMix((uint64)value);
    }

    Type LiteralReal::GetType(Parser& parser) const
    {
        return Float();
    }

    uint64 LiteralReal::GetHash() const
    {
        uint64 x = 0;
        (void)memcpy(&x, &value, sizeof(x));
        return WellonsMix(x);
    }

    uint64 Break::GetHash() const
    {
        const auto h = WellonsMix(WellonsMix((uint64)(__LINE__)) ^ WellonsMix((uint64)(__COUNTER__)));
        return h;
    }

    uint64 Continue::GetHash() const
    {
        return uint64();
    }

    uint64 Wildcard::GetHash() const
    {
        const auto h = WellonsMix(WellonsMix((uint64)(__LINE__)) ^ WellonsMix((uint64)(__COUNTER__)));
        return h;
    }

    bool TraitsOf::operator==(const TraitsOf& other) const
    {
        return *value == *other.value;
    }

    uint64 TraitsOf::GetHash() const
    {
        return WellonsMix(value->GetHash());
    }

    bool Array::operator==(const Array& other) const
    {
        return size == other.size && type == other.type;
    }

    uint64 Array::GetHash() const
    {
        return WellonsMix(size) ^ type->GetHash();
    }

    ScopedPtr<Array> Array::ToPtr()
    {
        auto p = array_allocator.New();
        new (p) Array(std::move(*this));
        return ScopedPtr(p);
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

    uint64 Tuple::GetHash() const
    {
        uint64 r = 0;
        for (auto& e : types)
            r ^= e.GetHash();
        return r;
    }

    ScopedPtr<Tuple> Tuple::ToPtr()
    {
        auto p = tuple_allocator.New();
        new (p) Tuple(std::move(*this));
        return ScopedPtr(p);
    }

    bool Record::operator==(const Record& other) const
    {
        if (fields.size() != other.fields.size())
            return false;
        for (size_t i = 0; i != fields.size(); ++i)
        {
            auto& lhs = fields[i];
            auto& rhs = other.fields[i];
            if (lhs.name.id != rhs.name.id)
                return false;
            if (lhs.type != rhs.type)
                return false;
            if (lhs.init != rhs.init)
                return false;
        }
        return true;
    }

    uint64 Record::GetHash() const
    {
        uint64 r = 0;
        for (auto& e : fields)
            r ^= e.GetHash();
        return r;
    }

    ScopedPtr<Record> Record::ToPtr()
    {
        auto p = record_allocator.New();
        new (p) Record(std::move(*this));
        return ScopedPtr(p);
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

    uint64 Enum::GetHash() const
    {
        uint64 r = 0;
        r ^= underlying_type->GetHash();
        for (auto& e : values)
            r ^= WellonsMix((uint64)e.first) ^ e.second.GetHash();
        return r;
    }

    ScopedPtr<Enum> Enum::ToPtr()
    {
        auto p = enum_allocator.New();
        new (p) Enum(std::move(*this));
        return ScopedPtr(p);
    }

    uint64 Void::GetHash()
    {
        const auto h = WellonsMix(WellonsMix((uint64)(__LINE__)) ^ WellonsMix((uint64)(__COUNTER__)));
        return h;
    }

    uint64 Nil::GetHash()
    {
        const auto h = WellonsMix(WellonsMix((uint64)(__LINE__)) ^ WellonsMix((uint64)(__COUNTER__)));
        return h;
    }

    uint64 Bool::GetHash()
    {
        const auto h = WellonsMix(WellonsMix((uint64)(__LINE__)) ^ WellonsMix((uint64)(__COUNTER__)));
        return h;
    }

    uint64 Int::GetHash() const
    {
        const auto h = WellonsMix(WellonsMix((uint64)(__LINE__)) ^ WellonsMix((uint64)(__COUNTER__)));
        const auto h2 = WellonsMix(DefaultBitWidth);
        return h ^ (bits == DefaultBitWidth ? h2 : WellonsMix(bits));
    }

    uint64 UInt::GetHash() const
    {
        const auto h = WellonsMix(WellonsMix((uint64)(__LINE__)) ^ WellonsMix((uint64)(__COUNTER__)));
        const auto h2 = WellonsMix(DefaultBitWidth);
        return h ^ (bits == DefaultBitWidth ? h2 : WellonsMix(bits));
    }

    uint64 Float::GetHash() const
    {
        const auto h = WellonsMix((uint64)(__LINE__)) ^ WellonsMix((uint64)(__COUNTER__));
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

    uint64 ConstructorCall::GetHash() const
    {
        uint64 r = 0;
        for (auto& e : parameters)
            r ^= e.GetHash();
        return r;
    }

    bool DestructorCall::operator==(const DestructorCall& other) const
    {
        return *object == *other.object;
    }

    uint64 DestructorCall::GetHash() const
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

    uint64 FunctionCall::GetHash() const
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

    uint64 FunctionType::GetHash() const
    {
        auto r = return_type->GetHash();
        for (auto& e : param_types)
            r ^= e.GetHash();
        return r;
    }

    ScopedPtr<FunctionType> FunctionType::ToPtr()
    {
        auto r = function_type_allocator.New();
        new (r) FunctionType(std::move(*this));
        return ScopedPtr<FunctionType>(r);
    }
    
    uint64 MetaType::GetHash()
    {
        const auto h = WellonsMix(WellonsMix((uint64)(__LINE__)) ^ WellonsMix((uint64)(__COUNTER__)));
        return h;
    }
}