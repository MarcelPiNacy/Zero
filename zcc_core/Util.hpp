#pragma once
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cctype>
#include <cassert>
#include <cfloat>
#include <vector>
#include <string>
#include <atomic>
#include <memory>
#include <tuple>
#include <variant>
#include <optional>
#include <algorithm>
#include <string_view>
#include <stdexcept>

#include "dependencies/flat_hash_map/bytell_hash_map.hpp"



namespace Zero
{
	struct Parser;

	using uint8 = uint8_t;
	using uint16 = uint16_t;
	using uint32 = uint32_t;
	using uint64 = uint64_t;
	using uintptr = size_t;

	using int8 = int8_t;
	using int16 = int16_t;
	using int32 = int32_t;
	using int64 = int64_t;
	using intptr = ptrdiff_t;

	using std::pair;
	using std::atomic;
	using std::vector;
	using std::string;
	using std::unique_ptr;
	using std::shared_ptr;
	using std::string_view;
	using std::optional;

	enum class IdentifierID : uint64 {};

	namespace Build
	{
#ifdef _DEBUG
		constexpr bool IsDebug = true;
#else
		constexpr bool IsDebug = false;
#endif
	}

	namespace Detail
	{
		constexpr auto wellons_ctrl32 = std::make_tuple<uint8, uint32, uint8, uint32, uint8>(15, 0xd168aaad, 15, 0xaf723597, 15);
		constexpr auto wellons_ctrl64 = std::make_tuple<uint8, uint64, uint8, uint64, uint8>(32, 0xd6e8feb86659fd93, 32, 0xd6e8feb86659fd93, 32);
	}

	constexpr uint32 WellonsMix(uint32 x)
	{
		constexpr auto c = Detail::wellons_ctrl32;
		x ^= x >> std::get<0>(c);
		x *= std::get<1>(c);
		x ^= x >> std::get<2>(c);
		x *= std::get<3>(c);
		x ^= x >> std::get<4>(c);
		return x;
	}

	constexpr uint64 WellonsMix(uint64 x)
	{
		constexpr auto c = Detail::wellons_ctrl64;
		x ^= x >> std::get<0>(c);
		x *= std::get<1>(c);
		x ^= x >> std::get<2>(c);
		x *= std::get<3>(c);
		x ^= x >> std::get<4>(c);
		return x;
	}

	uint64						XXHash64(const void* data, uintptr size);
	std::pair<uint64, uint64>	XXHash128(const void* data, uintptr size);

	// Random constant that is XORed into every hash when built in debug mode.
	constexpr uintptr HashMagic = []()
	{
		if constexpr (Build::IsDebug)
		{
			uint64 r = 0xcbf29ce484222325;

			constexpr char date[] = __DATE__ "-" __TIME__;

			for (auto c : date)
			{
				auto n = (r ^ c) * 0x00000100000001B3;
				auto d = c & 63;
				r ^= (n << d) | (n >> d);
			}

			if constexpr (sizeof(uintptr) == 4)
				r ^= (r >> 32);

			return (uintptr)r;
		}
		else
		{
			return 0;
		}
	}();

	template <typename T>
	struct CustomHasher
	{
		auto operator()(const T& key) const
		{
			return XXHash64(&key, sizeof(T)) ^ HashMagic;
		}
	};

	template <>
	struct CustomHasher<string>
	{
		auto operator()(const string& key) const
		{
			return XXHash64(key.data(), key.size()) ^ HashMagic;
		}
	};

	template <>
	struct CustomHasher<string_view>
	{
		auto operator()(const string_view& key) const
		{
			return XXHash64(key.data(), key.size()) ^ HashMagic;
		}
	};

	template <typename T>
	struct CustomHasher<vector<T>>
	{
		auto operator()(const vector<T>& key) const
		{
			if constexpr (std::is_trivial_v<T>)
			{
				return XXHash64(key.data(), key.size() * sizeof(T)) ^ HashMagic;
			}
			else
			{
				uintptr r = 0;
				for (auto& e : key)
					r ^= CustomHasher<T>()(e);
				return r ^ HashMagic;
			}
		}
	};

	template <typename K, typename V = void, typename H = CustomHasher<K>>
	using HashMap = std::conditional_t<std::is_void_v<V>, ska::bytell_hash_set<K, H>, ska::bytell_hash_map<K, V, H>>;
	//using HashMap = std::conditional_t<std::is_void_v<V>, std::unordered_set<K, H>, std::unordered_map<K, V, H>>;

	template <uintmax_t K>
	using MinUint =
		std::conditional_t<K <= UINT8_MAX, uint8,
		std::conditional_t<K <= UINT16_MAX, uint16,
		std::conditional_t<K <= UINT32_MAX, uint32, uint64>>>;

	namespace Detail
	{
		template <typename K, uintptr N, typename...T>
		struct VariantTypeFinder;

		template <typename K, uintptr N>
		struct VariantTypeFinder<K, N> { static constexpr uintptr Index = N; };

		template <typename K, uintptr N, typename T>
		struct VariantTypeFinder<K, N, T>
		{
			static constexpr uintptr Index = std::is_same_v<K, T> ? N : N + 1;
		};

		template <typename K, uintptr N, typename T, typename ...U>
		struct VariantTypeFinder<K, N, T, U...>
		{
			static constexpr uintptr Index = std::is_same_v<K, T> ? N : VariantTypeFinder<K, N + 1, U...>::Index;
		};
	}

	template <typename T>
	constexpr void Destruct(T& value)
	{
		value.~T();
	}

	template <typename... T>
	struct Variant :
		private std::variant<std::monostate, T...>
	{
		using IndexT = MinUint<sizeof...(T)>;
		using Base = std::variant<std::monostate, T...>;

		template <typename U>
		static constexpr IndexT IDOf = Detail::VariantTypeFinder<U, 0, T...>::Index;

		constexpr Variant() :
			Base()
		{
		}
		
		Variant(const Variant& other) :
			Base(*(const Base*)&other)
		{
		}

		Variant& operator=(const Variant& other)
		{
			this->~Variant();
			new (this) Variant(other);
			return *this;
		}

		Variant(Variant&& other) noexcept :
			Base(std::move(*(Base*)&other))
		{
		}

		Variant& operator=(Variant&& other) noexcept
		{
			this->~Variant();
			new (this) Variant(std::move(other));
			return *this;
		}

		template <
			typename U,
			typename = std::enable_if_t<Detail::VariantTypeFinder<std::remove_reference_t<U>, 0, T...>::Index != sizeof...(T)>>
		constexpr Variant(U&& value) :
			Base(std::forward<U>(value))
		{
		}

		template <
			typename U,
			typename = std::enable_if_t<Detail::VariantTypeFinder<std::remove_reference_t<U>, 0, T...>::Index != sizeof...(T)>>
		Variant& operator=(U&& value)
		{
			this->~Variant();
			new (this) Variant(std::forward<U>(value));
			return *this;
		}

		~Variant() = default;

		template <typename F>
		void Visit(F&& fn)
		{
			std::visit([&](auto& e)
			{
				if constexpr (!std::is_same_v<std::remove_reference_t<decltype(e)>, std::monostate>)
				{
					fn(e);
				}
			}, *(Base*)this);
		}

		template <uintptr N = 0, typename F>
		void Visit(F&& fn) const
		{
			std::visit([&](auto& e)
			{
				if constexpr (!std::is_same_v<std::remove_reference_t<decltype(e)>, std::monostate>)
				{
					fn(e);
				}
			}, *(Base*)this);
		}

		template <
			typename U,
			typename = std::enable_if_t<Detail::VariantTypeFinder<std::remove_reference_t<U>, 0, T...>::Index != sizeof...(T)>>
		void Set(U&& value)
		{
			*(Base*)this = std::forward<U>(value);
		}

		template <typename U>
		auto& Get()
		{
			return std::get<U>(*(Base*)this);
		}

		template <typename U>
		const auto& Get() const
		{
			return std::get<U>(*(Base*)this);
		}

		template <uintptr N>
		auto& Get()
		{
			static_assert(N < sizeof...(T));
			return std::get<N>(*(Base*)this);
		}

		template <uintptr N>
		auto& Get() const
		{
			static_assert(N < sizeof...(T));
			return std::get<N>(*(Base*)this);
		}

		constexpr auto ID() const
		{
			return Base::index();
		}

		constexpr bool IsEmpty() const
		{
			return Base::index() == 0;
		}

		template <typename U>
		constexpr bool Is() const
		{
			return ID() == Detail::VariantTypeFinder<U, 0, std::monostate, T...>::Index; // Mind the std::monostate!
		}
	};



	struct Expression;
	struct Type;
	struct Enum;
	struct Array;
	struct Tuple;
	struct Record;
	struct FunctionType;

	template <typename T>
	struct ScopedPtrTraits;

	template <>
	struct ScopedPtrTraits<Expression>
	{
		static Expression* New();
		static void Delete(Expression* ptr);
	};

	template <>
	struct ScopedPtrTraits<Enum>
	{
		static Enum* New();
		static void Delete(Enum* ptr);
	};

	template <>
	struct ScopedPtrTraits<Array>
	{
		static Array* New();
		static void Delete(Array* ptr);
	};

	template <>
	struct ScopedPtrTraits<Tuple>
	{
		static Tuple* New();
		static void Delete(Tuple* ptr);
	};

	template <>
	struct ScopedPtrTraits<Record>
	{
		static Record* New();
		static void Delete(Record* ptr);
	};

	template <>
	struct ScopedPtrTraits<FunctionType>
	{
		static FunctionType* New();
		static void Delete(FunctionType* ptr);
	};

	template <>
	struct ScopedPtrTraits<Type>
	{
		static Type* New();
		static void Delete(Type* ptr);
	};



	template <typename T, typename Traits = ScopedPtrTraits<T>>
	struct ScopedPtr
	{
		T* ptr;

		constexpr ScopedPtr() noexcept :
			ptr()
		{
		}
		
		constexpr ScopedPtr(T* ptr) noexcept :
			ptr(ptr)
		{
		}

		ScopedPtr(const ScopedPtr& other) noexcept :
			ptr(other.ptr != nullptr ? Traits::New() : nullptr)
		{
			if (other.ptr != nullptr)
				new (ptr) T(*other.ptr);
		}

		ScopedPtr& operator=(const ScopedPtr& other) noexcept
		{
			this->~ScopedPtr();
			new (this) ScopedPtr(other);
			return *this;
		}

		constexpr ScopedPtr(ScopedPtr&& other) noexcept :
			ptr(other.ptr)
		{
			other.ptr = nullptr;
		}

		ScopedPtr& operator=(ScopedPtr&& other) noexcept
		{
			this->~ScopedPtr();
			new (this) ScopedPtr(std::move(other));
			return *this;
		}

		~ScopedPtr() noexcept
		{
			if (ptr != nullptr)
			{
				Traits::Delete(ptr);
				ptr = nullptr;
			}
		}

		constexpr auto operator==(T* other) const
		{
			return ptr == other;
		}

		constexpr auto operator==(const ScopedPtr& other) const
		{
			return ptr == other.ptr;
		}

		constexpr auto operator!=(T* other) const
		{
			return !operator==(other);
		}

		constexpr auto operator!=(const ScopedPtr& other) const
		{
			return !operator==(other);
		}

		constexpr auto operator->()
		{
			return ptr;
		}

		constexpr auto operator->() const
		{
			return ptr;
		}

		constexpr auto& operator*()
		{
			return *ptr;
		}

		constexpr auto& operator*() const
		{
			return *ptr;
		}
	};



	template <typename T, typename I = T*>
	struct Range
	{
		I begin_it, end_it;

		constexpr Range() :
			begin_it(), end_it()
		{
		}

		Range(const Range&) = default;
		Range& operator=(const Range&) = default;
		~Range() = default;

		constexpr Range(I begin, I end) :
			begin_it(begin), end_it(end)
		{
		}

		constexpr Range(I elements, uintptr count) :
			begin_it(elements), end_it(elements + count)
		{
		}

		constexpr Range(std::initializer_list<T>& elements)
			: begin_it(elements.begin()), end_it(elements.end())
		{
		}

		constexpr Range(vector<T>& elements)
			: begin_it(elements.data()), end_it(elements.data() + elements.size())
		{
		}

		constexpr auto& operator[](ptrdiff_t index)
		{
			return *std::next(Begin(), index);
		}

		constexpr auto& operator[](ptrdiff_t index) const
		{
			return *std::next(Begin(), index);
		}

		constexpr T& First()
		{
			return *Begin();
		}

		constexpr T& Last()
		{
			return End()[-1];
		}

		constexpr T& PopFront()
		{
			T r = *begin_it;
			++begin_it;
			return r;
		}

		constexpr T& PopBack()
		{
			--end_it;
			return *end_it;
		}

		constexpr void ExpandFront()
		{
			--begin_it;
		}

		constexpr void ExpandBack()
		{
			++end_it;
		}

		constexpr I begin() { return begin_it; }
		constexpr I end() { return end_it; }
		constexpr I begin() const { return begin_it; }
		constexpr I end() const { return end_it; }
		constexpr I cbegin() const { return begin_it; }
		constexpr I cend() const { return end_it; }

		constexpr I Begin() { return begin_it; }
		constexpr I End() { return end_it; }
		constexpr I Begin() const { return begin_it; }
		constexpr I End() const { return end_it; }

		constexpr uintptr Size() const { return std::distance(begin(), end()); }
	};

	template <typename F>
	struct ScopedCallback
	{
		F fn;

		constexpr ScopedCallback(F&& fn)
			: fn(std::forward<F>(fn))
		{
		}

		~ScopedCallback()
		{
			fn();
		}
	};
}