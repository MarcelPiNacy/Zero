#pragma once
#include "Util.hpp"
#include "Operator.hpp"



#define TYPE_HEADER(TYPE) using this_type = TYPE;																\
	TYPE() = default;																							\
	TYPE(const TYPE&) = default;																				\
	TYPE& operator=(const TYPE&) = default;																		\
	~TYPE() = default

#define ALWAYS_EQUAL															\
	constexpr bool operator==(const this_type& other) const { return true; }	\
	constexpr bool operator!=(const this_type& other) const { return false; }

#define DEFAULT_EQUALITY	inline bool operator==(const this_type& other) const { return memcmp(this, &other, sizeof(this_type)) == 0; }
#define DEFAULT_INEQUALITY	inline bool operator!=(const this_type& other) const { return !operator==(other); }



namespace Zero
{
	struct Parser;
	struct Expression;
	struct Type;
	struct Declaration;
	struct Enum;



	enum class ExpressionCategory : uint64
	{
		ImplCounterBegin = __COUNTER__,

#define GET_NEXT_POW2 (UINT64_C(1) << (__COUNTER__ - (ImplCounterBegin + 1))) // Yields the next power of two.

		Undefined	= 0,
		Type		= GET_NEXT_POW2,
		Identifier	= GET_NEXT_POW2,
		Literal		= GET_NEXT_POW2,
		Scope		= GET_NEXT_POW2,
		Block		= GET_NEXT_POW2,
		Branch		= GET_NEXT_POW2,
		Loop		= GET_NEXT_POW2,
		Jump		= GET_NEXT_POW2,
		Return		= GET_NEXT_POW2,
		Defer		= GET_NEXT_POW2,
		Traits		= GET_NEXT_POW2,
		Misc		= GET_NEXT_POW2,

#undef GET_NEXT_POW2
	};



	constexpr auto& operator&=(ExpressionCategory& lhs, ExpressionCategory rhs)
	{
		using T = std::underlying_type_t<std::remove_reference_t<decltype(lhs)>>;
		lhs = (std::remove_reference_t<decltype(lhs)>)((T)lhs & (T)rhs);
		return lhs;
	}

	constexpr auto& operator|=(ExpressionCategory& lhs, ExpressionCategory rhs)
	{
		using T = std::underlying_type_t<std::remove_reference_t<decltype(lhs)>>;
		lhs = (std::remove_reference_t<decltype(lhs)>)((T)lhs | (T)rhs);
		return lhs;
	}

	constexpr auto& operator^=(ExpressionCategory& lhs, ExpressionCategory rhs)
	{
		using T = std::underlying_type_t<std::remove_reference_t<decltype(lhs)>>;
		lhs = (std::remove_reference_t<decltype(lhs)>)((T)lhs ^ (T)rhs);
		return lhs;
	}

	constexpr auto operator&(ExpressionCategory lhs, ExpressionCategory rhs)
	{
		using T = std::underlying_type_t<std::remove_reference_t<decltype(lhs)>>;
		return (std::remove_reference_t<decltype(lhs)>)((T)lhs & (T)rhs);
	}

	constexpr auto operator|(ExpressionCategory lhs, ExpressionCategory rhs)
	{
		using T = std::underlying_type_t<std::remove_reference_t<decltype(lhs)>>;
		return (std::remove_reference_t<decltype(lhs)>)((T)lhs | (T)rhs);
	}

	constexpr auto operator^(ExpressionCategory lhs, ExpressionCategory rhs)
	{
		using T = std::underlying_type_t<std::remove_reference_t<decltype(lhs)>>;
		return (std::remove_reference_t<decltype(lhs)>)((T)lhs ^ (T)rhs);
	}



	namespace Detail
	{
		struct Untyped
		{
			static Type GetType(Parser& parser);
		};

		struct NoReturnType
		{
			static std::pair<bool, Type> InferReturnType(Parser& parser);
		};

		template <ExpressionCategory C = ExpressionCategory::Undefined>
		struct CategoryWrapper
		{
			static constexpr ExpressionCategory Category = C;
		};

		struct NoFields
		{
			template <typename F> static constexpr void ForEachField(F&& fn) { }
		};

		struct AlwaysConst
		{
			static constexpr bool IsConst() { return true; }
		};

		struct ExpressionHasher
		{
			uint64 operator()(const Expression& e) const;
		};
	}



	struct UnqualifiedIdentifier :
		Detail::CategoryWrapper<ExpressionCategory::Identifier>,
		Detail::NoReturnType,
		Detail::AlwaysConst
	{
		TYPE_HEADER(UnqualifiedIdentifier);

		constexpr UnqualifiedIdentifier(IdentifierID id) :
			id(id)
		{
		}

		IdentifierID id;

		Type GetType(Parser& parser) const;
		uint64 GetHash() const;

		bool operator==(const UnqualifiedIdentifier& other) const;
		DEFAULT_INEQUALITY

		template <typename F>
		void ForEachField(F&& fn)
		{
			fn(id);
		}

		template <typename F>
		void ForEachField(F&& fn) const
		{
			fn(id);
		}

	};



	struct QualifiedIdentifier :
		Detail::CategoryWrapper<ExpressionCategory::Identifier>,
		Detail::NoReturnType,
		Detail::AlwaysConst
	{
		TYPE_HEADER(QualifiedIdentifier);

		inline QualifiedIdentifier(IdentifierID id) :
			names({ id })
		{
		}

		vector<IdentifierID> names;

		Type	GetType(Parser& parser) const;
		uint64	GetHash() const;

		bool operator==(const QualifiedIdentifier& other) const;
		DEFAULT_INEQUALITY

		template <typename F>
		void ForEachField(F&& fn)
		{
			fn(names);
		}

		template <typename F>
		void ForEachField(F&& fn) const
		{
			fn(names);
		}

	};



	struct MetaType :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType,
		Detail::NoFields
	{
		TYPE_HEADER(MetaType);
		ALWAYS_EQUAL

		static uint64 GetHash();
	};



	struct Void :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType,
		Detail::NoFields
	{
		TYPE_HEADER(Void);
		ALWAYS_EQUAL

		static uint64 GetHash();
	};



	struct Nil :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType,
		Detail::NoFields
	{
		TYPE_HEADER(Nil);
		ALWAYS_EQUAL

		static uint64 GetHash();
	};



	struct Bool :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType,
		Detail::NoFields
	{
		TYPE_HEADER(Bool);
		ALWAYS_EQUAL

		static uint64 GetHash();
	};



	struct Int :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType
	{
		TYPE_HEADER(Int);
		DEFAULT_EQUALITY
		DEFAULT_INEQUALITY

		static constexpr uint64 DefaultBitWidth = 32;

		constexpr Int(uint64 bits) :
			bits(bits)
		{
		}

		uint64 bits = DefaultBitWidth;

		uint64 GetHash() const;
	};



	struct UInt :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType
	{
		TYPE_HEADER(UInt);
		DEFAULT_EQUALITY
		DEFAULT_INEQUALITY

		static constexpr uint64 DefaultBitWidth = 32;

		uint64 bits = DefaultBitWidth;

		constexpr UInt(uint64 bits) :
			bits(bits)
		{
		}

		uint64 GetHash() const;
	};



	struct Float :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType
	{
		TYPE_HEADER(Float);
		DEFAULT_EQUALITY
		DEFAULT_INEQUALITY

		static constexpr uint64 DefaultBitWidth = 32;

		uint64 bits = DefaultBitWidth;

		constexpr Float(uint64 bits) :
			bits(bits)
		{
		}

		uint64 GetHash() const;
	};



	struct Enum :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType
	{
		TYPE_HEADER(Enum);

		HashMap<IdentifierID, Expression> values;
		ScopedPtr<Expression> underlying_type;

		bool operator==(const Enum& other) const;
		DEFAULT_INEQUALITY

		uint64 GetHash() const;
		ScopedPtr<Enum> ToPtr();
	};



	struct Array :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType
	{
		TYPE_HEADER(Array);

		ScopedPtr<Type> type;
		uint64 size;

		bool operator==(const Array& other) const;
		DEFAULT_INEQUALITY

		uint64 GetHash() const;
		ScopedPtr<Array> ToPtr();
	};



	struct Tuple :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType
	{
		TYPE_HEADER(Tuple);

		vector<Type> types;

		bool operator==(const Tuple& other) const;
		DEFAULT_INEQUALITY

		uint64 GetHash() const;
		ScopedPtr<Tuple> ToPtr();
	};



	struct Record :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType
	{
		TYPE_HEADER(Record);

		vector<Declaration> fields;
		HashMap<Operator, Declaration*> operators;
		HashMap<IdentifierID, Declaration*> variables;
		HashMap<IdentifierID, Declaration*> variables_static;
		HashMap<IdentifierID, Declaration*> functions;
		HashMap<IdentifierID, Declaration*> functions_static;

		bool operator==(const Record& other) const;
		DEFAULT_INEQUALITY

		uint64 GetHash() const;
		ScopedPtr<Record> ToPtr();
	};



	struct FunctionType :
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType
	{
		TYPE_HEADER(FunctionType);

		ScopedPtr<Expression> return_type;
		vector<Expression> param_types;

		bool operator==(const FunctionType& other) const;
		DEFAULT_INEQUALITY

		uint64 GetHash() const;
		ScopedPtr<FunctionType> ToPtr();
	};



	enum class TypeCategory
	{
		MetaType, Void, Nil, Bool, Int, UInt, Float, Enum, Array, Tuple, Record, Type, Function
	};

	namespace Detail
	{
		using TypeBase = Variant<
			MetaType,
			Void, Nil, Bool, Int, UInt, Float,
			ScopedPtr<Enum>,
			ScopedPtr<Array>,
			ScopedPtr<Tuple>,
			ScopedPtr<Record>,
			ScopedPtr<FunctionType>>;
	}



	struct Type :
		Detail::TypeBase,
		Detail::CategoryWrapper<ExpressionCategory::Type>,
		Detail::NoReturnType,
		Detail::AlwaysConst
	{
		TYPE_HEADER(Type);

		using Base = Detail::TypeBase;

		template <typename T, typename = std::enable_if_t<!std::is_same_v<std::remove_reference_t<T>, Type>>>
		Type(T&& value)
		{
			Set<T>(std::forward<T>(value));
		}

		bool operator==(const Type& other) const;
		DEFAULT_INEQUALITY

		constexpr bool IsMetaType() const { return Is<MetaType>(); }
		constexpr TypeCategory GetCategory() const { return (TypeCategory)this->ID(); }

		Type					GetType(Parser& parser) const;
		ScopedPtr<Type>			ToPtr();
		uint64					GetHash() const;
	};



	struct NoOp :
		Detail::CategoryWrapper<ExpressionCategory::Misc>,
		Detail::NoReturnType,
		Detail::NoFields,
		Detail::AlwaysConst,
		Detail::Untyped
	{
		TYPE_HEADER(NoOp);
		ALWAYS_EQUAL

		uint64					GetHash() const;
	};



	struct Use :
		Detail::CategoryWrapper<>,
		Detail::NoReturnType,
		Detail::AlwaysConst,
		Detail::Untyped
	{
		TYPE_HEADER(Use);
		ALWAYS_EQUAL

		vector<Expression> modules;

		uint64					GetHash() const;
	};



	struct Namespace:
		Detail::CategoryWrapper<>,
		Detail::NoReturnType,
		Detail::AlwaysConst,
		Detail::Untyped
	{
		vector<Expression> elements;

		TYPE_HEADER(Namespace);
		ALWAYS_EQUAL

		uint64					GetHash() const;
	};



	struct Declaration :
		Detail::CategoryWrapper<>,
		Detail::NoReturnType
	{
		TYPE_HEADER(Declaration);

		ScopedPtr<Expression> type;
		UnqualifiedIdentifier name;
		ScopedPtr<Expression> init;

		inline Declaration(ScopedPtr<Expression> type, UnqualifiedIdentifier name, ScopedPtr<Expression> init = nullptr) :
			type(type), name(name), init(init)
		{
		}

		bool operator==(const Declaration& other) const;
		DEFAULT_INEQUALITY

		Type					GetType(Parser& parser) const;
		bool					IsConst() const;
		uint64					GetHash() const;
	};



	struct Cast :
		Detail::CategoryWrapper<ExpressionCategory::Literal>,
		Detail::NoReturnType,
		Detail::Untyped
	{
		TYPE_HEADER(Cast);

		ScopedPtr<Expression> value;
		ScopedPtr<Expression> new_type;

		inline Cast(ScopedPtr<Expression> value, ScopedPtr<Expression> new_type) :
			value(value), new_type(new_type)
		{
		}

		bool operator==(const Cast& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		uint64					GetHash() const;
	};



	struct LiteralBool :
		Detail::CategoryWrapper<ExpressionCategory::Literal>,
		Detail::NoReturnType,
		Detail::AlwaysConst
	{
		TYPE_HEADER(LiteralBool);

		bool value;

		constexpr LiteralBool(bool value) :
			value(value)
		{
		}

		DEFAULT_EQUALITY
		DEFAULT_INEQUALITY

		Type					GetType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct LiteralNil :
		Detail::CategoryWrapper<ExpressionCategory::Literal>,
		Detail::NoReturnType,
		Detail::NoFields,
		Detail::AlwaysConst
	{
		TYPE_HEADER(LiteralNil);
		
		DEFAULT_EQUALITY
		DEFAULT_INEQUALITY

		Type					GetType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct LiteralInt :
		Detail::CategoryWrapper<ExpressionCategory::Literal>,
		Detail::NoReturnType,
		Detail::AlwaysConst
	{
		TYPE_HEADER(LiteralInt);
		
		DEFAULT_EQUALITY
		DEFAULT_INEQUALITY

		int64 value;

		constexpr LiteralInt(int64 value) :
			value(value)
		{
		}

		Type					GetType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct LiteralUint :
		Detail::CategoryWrapper<ExpressionCategory::Literal>,
		Detail::NoReturnType,
		Detail::AlwaysConst
	{
		TYPE_HEADER(LiteralUint);
		
		DEFAULT_EQUALITY
		DEFAULT_INEQUALITY

		uint64 value;

		constexpr LiteralUint(uint64 value) :
			value(value)
		{
		}

		Type					GetType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct LiteralReal :
		Detail::CategoryWrapper<ExpressionCategory::Literal>,
		Detail::NoReturnType,
		Detail::AlwaysConst
	{
		TYPE_HEADER(LiteralReal);
		
		DEFAULT_EQUALITY
		DEFAULT_INEQUALITY

		double value;

		constexpr LiteralReal(double value) :
			value(value)
		{
		}

		Type					GetType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct Function :
		Detail::CategoryWrapper<>,
		Detail::NoReturnType // A function has no inferrable return type, its body does.
	{
		TYPE_HEADER(Function);

		ScopedPtr<Expression>	body;
		ScopedPtr<Expression>	return_type;
		vector<Expression>		params;

		bool operator==(const Function& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		Type					GetType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct FunctionCall :
		Detail::CategoryWrapper<>
	{
		TYPE_HEADER(FunctionCall);

		ScopedPtr<Expression>	callable;
		vector<Expression>		params;

		template <typename F>
		FunctionCall(F&& callable, vector<Expression> params) :
			callable(Expression(std::forward<F>(callable)).ToPtr()), params(params)
		{
		}

		bool operator==(const FunctionCall& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		Type					GetType(Parser& parser) const;
		std::pair<bool, Type>	InferReturnType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct Scope :
		Detail::CategoryWrapper<ExpressionCategory::Block>,
		Detail::Untyped
	{
		TYPE_HEADER(Scope);

		vector<Expression> expressions;

		bool operator==(const Scope& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		std::pair<bool, Type>	InferReturnType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct Branch :
		Detail::CategoryWrapper<ExpressionCategory::Branch>,
		Detail::Untyped
	{
		TYPE_HEADER(Branch);

		ScopedPtr<Expression> condition;
		ScopedPtr<Expression> on_true;
		ScopedPtr<Expression> on_false;

		bool operator==(const Branch& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		std::pair<bool, Type>	InferReturnType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct Select :
		Detail::CategoryWrapper<ExpressionCategory::Branch>,
		Detail::Untyped
	{
		TYPE_HEADER(Select);

		ScopedPtr<Expression>	key;
		HashMap<Expression, Expression, Detail::ExpressionHasher> cases;
		ScopedPtr<Expression>	default_case;

		bool operator==(const Select& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		std::pair<bool, Type>	InferReturnType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct While :
		Detail::CategoryWrapper<ExpressionCategory::Loop>,
		Detail::Untyped
	{
		TYPE_HEADER(While);

		ScopedPtr<Expression> condition;
		ScopedPtr<Expression> body;

		bool operator==(const While& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		std::pair<bool, Type>	InferReturnType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct DoWhile :
		Detail::CategoryWrapper<ExpressionCategory::Loop>,
		Detail::Untyped
	{
		TYPE_HEADER(DoWhile);

		ScopedPtr<Expression> condition;
		ScopedPtr<Expression> body;

		bool operator==(const DoWhile& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		std::pair<bool, Type>	InferReturnType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct For :
		Detail::CategoryWrapper<ExpressionCategory::Loop>,
		Detail::Untyped
	{
		TYPE_HEADER(For);

		ScopedPtr<Expression> init;
		ScopedPtr<Expression> condition;
		ScopedPtr<Expression> update;
		ScopedPtr<Expression> body;

		bool operator==(const For& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		std::pair<bool, Type>	InferReturnType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct ForEach :
		Detail::CategoryWrapper<ExpressionCategory::Loop>,
		Detail::Untyped
	{
		TYPE_HEADER(ForEach);

		ScopedPtr<Expression> iterator;
		ScopedPtr<Expression> collection;
		ScopedPtr<Expression> body;

		bool operator==(const ForEach& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		std::pair<bool, Type>	InferReturnType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct UnaryExpression :
		Detail::CategoryWrapper<>,
		Detail::NoReturnType
	{
		TYPE_HEADER(UnaryExpression);

		ScopedPtr<Expression> other;
		Operator op;

		inline UnaryExpression(ScopedPtr<Expression> other, Operator op) :
			other(other), op(op)
		{
		}

		bool operator==(const UnaryExpression& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		Type					GetType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct BinaryExpression :
		Detail::CategoryWrapper<>,
		Detail::NoReturnType
	{
		TYPE_HEADER(BinaryExpression);

		ScopedPtr<Expression> lhs;
		ScopedPtr<Expression> rhs;
		Operator op;

		inline BinaryExpression(ScopedPtr<Expression> lhs, ScopedPtr<Expression> rhs, Operator op) :
			lhs(lhs), rhs(rhs), op(op)
		{
		}

		bool operator==(const BinaryExpression& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		Type					GetType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct Break :
		Detail::CategoryWrapper<ExpressionCategory::Jump>,
		Detail::NoReturnType,
		Detail::NoFields,
		Detail::AlwaysConst,
		Detail::Untyped
	{
		TYPE_HEADER(Break);
		ALWAYS_EQUAL

		uint64					GetHash() const;
	};



	struct Continue :
		Detail::CategoryWrapper<ExpressionCategory::Jump>,
		Detail::NoReturnType,
		Detail::NoFields,
		Detail::AlwaysConst,
		Detail::Untyped
	{
		TYPE_HEADER(Continue);
		ALWAYS_EQUAL

		uint64					GetHash() const;
	};



	struct Defer :
		Detail::CategoryWrapper<ExpressionCategory::Defer>,
		Detail::NoReturnType,
		Detail::Untyped
	{
		TYPE_HEADER(Defer);

		ScopedPtr<Expression> body;

		inline Defer(ScopedPtr<Expression> body) :
			body(body)
		{
		}

		bool operator==(const Defer& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		uint64					GetHash() const;
	};



	struct Return :
		Detail::CategoryWrapper<ExpressionCategory::Jump>,
		Detail::Untyped
	{
		TYPE_HEADER(Return);

		ScopedPtr<Expression> value;

		inline Return(ScopedPtr<Expression> value) :
			value(value)
		{
		}

		bool operator==(const Return& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		std::pair<bool, Type>	InferReturnType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct Yield :
		Detail::CategoryWrapper<ExpressionCategory::Jump>,
		Detail::Untyped
	{
		TYPE_HEADER(Yield);

		ScopedPtr<Expression> value;

		inline Yield(ScopedPtr<Expression> value) :
			value(value)
		{
		}

		bool operator==(const Yield& other) const;
		DEFAULT_INEQUALITY

		bool					IsConst() const;
		std::pair<bool, Type>	InferReturnType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct Wildcard :
		Detail::CategoryWrapper<ExpressionCategory::Misc>,
		Detail::NoReturnType,
		Detail::NoFields,
		Detail::AlwaysConst,
		Detail::Untyped
	{
		TYPE_HEADER(Wildcard);
		ALWAYS_EQUAL

		uint64					GetHash() const;
	};



	struct TraitsOf :
		Detail::CategoryWrapper<ExpressionCategory::Traits>,
		Detail::NoReturnType,
		Detail::AlwaysConst,
		Detail::Untyped
	{
		TYPE_HEADER(TraitsOf);

		ScopedPtr<Expression> value;

		inline TraitsOf(ScopedPtr<Expression> value) :
			value(value)
		{
		}

		bool operator==(const TraitsOf& other) const;
		DEFAULT_INEQUALITY

		uint64					GetHash() const;
	};



	struct ConstructorCall :
		Detail::CategoryWrapper<ExpressionCategory::Traits>,
		Detail::NoReturnType,
		Detail::AlwaysConst
	{
		TYPE_HEADER(ConstructorCall);

		ScopedPtr<Expression> object;
		vector<Expression> parameters;

		bool operator==(const ConstructorCall& other) const;
		DEFAULT_INEQUALITY

		Type					GetType(Parser& parser) const;
		uint64					GetHash() const;
	};



	struct DestructorCall :
		Detail::CategoryWrapper<ExpressionCategory::Traits>,
		Detail::NoReturnType,
		Detail::AlwaysConst,
		Detail::Untyped
	{
		TYPE_HEADER(DestructorCall);

		ScopedPtr<Expression> object;

		bool operator==(const DestructorCall& other) const;
		DEFAULT_INEQUALITY

		uint64					GetHash() const;
	};



	namespace Detail
	{
		using ExpressionBase = Variant<
			Use,
			Namespace,
			NoOp,
			UnqualifiedIdentifier,
			QualifiedIdentifier,
			Type,
			Cast,
			Function,
			FunctionCall,
			Scope,
			Branch,
			Select,
			While,
			DoWhile,
			For,
			ForEach,
			UnaryExpression,
			BinaryExpression,
			Declaration,
			LiteralNil, LiteralBool, LiteralInt, LiteralUint, LiteralReal,
			Break, Continue, Defer, Return, Yield,
			Wildcard,
			TraitsOf,
			ConstructorCall,
			DestructorCall>;
	}



	struct Expression :
		Detail::ExpressionBase
	{
		using Base = Detail::ExpressionBase;

		TYPE_HEADER(Expression);

		template <typename T, typename = std::enable_if_t<!std::is_same_v<std::remove_reference_t<T>, Expression>>>
		Expression(T&& value) :
			Base()
		{
			Set<T>(std::forward<T>(value));
		}

		bool					IsConst() const;
		Type					GetType(Parser& parser) const;
		std::pair<bool, Type>	InferReturnType(Parser& parser) const;
		uint64					GetHash() const;
		ScopedPtr<Expression>	ToPtr();

		bool operator==(const Expression& other) const;
		DEFAULT_INEQUALITY
	};
}