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

	using HashT = uintptr;



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



	namespace OS
	{
		void*	Malloc(size_t size);
		void	Free(void* ptr, size_t size);
	}



	namespace Detail
	{
		template <typename T>
		using XS3M2Tuple = std::tuple<uint8, uint8, uint8, T, T>;

		template <typename T>
		constexpr auto WellonsMixCtrl = std::is_same_v<T, uint32> ?
			XS3M2Tuple<T>(15, 15, 15, 0xd168aaad, 0xaf723597) :
			XS3M2Tuple<T>(32, 32, 32, 0xd6e8feb86659fd93, 0xd6e8feb86659fd93);
	}



	constexpr uint32 WellonsMix(uint32 x)
	{
		const auto [s0, s1, s2, k0, k1] = Detail::WellonsMixCtrl<uint32>;

		x ^= x >> s0;
		x *= k0;
		x ^= x >> s1;
		x *= k1;
		x ^= x >> s2;

		return x;
	}

	constexpr uint64 WellonsMix(uint64 x)
	{
		const auto [s0, s1, s2, k0, k1] = Detail::WellonsMixCtrl<uint64>;

		x ^= x >> s0;
		x *= k0;
		x ^= x >> s1;
		x *= k1;
		x ^= x >> s2;

		return x;
	}



	uint64						XXHash64(const void* data, uintptr size);
	std::pair<uint64, uint64>	XXHash128(const void* data, uintptr size);

	HashT						XXHash(const void* data, uintptr size);



	template <typename T>
	struct CustomHasher
	{
		auto operator()(const T& key) const
		{
			return XXHash(&key, sizeof(T));
		}
	};

	template <>
	struct CustomHasher<string>
	{
		auto operator()(const string& key) const
		{
			return XXHash(key.data(), key.size());
		}
	};

	template <>
	struct CustomHasher<string_view>
	{
		auto operator()(const string_view& key) const
		{
			return XXHash(key.data(), key.size());
		}
	};

	template <typename T>
	struct CustomHasher<vector<T>>
	{
		auto operator()(const vector<T>& key) const
		{
			if constexpr (std::is_trivial_v<T>)
			{
				return XXHash(key.data(), key.size() * sizeof(T));
			}
			else
			{
				uintptr r = 0;
				for (auto& e : key)
					r ^= CustomHasher<T>()(e);
				return r;
			}
		}
	};



	template <typename K, typename V = void, typename H = CustomHasher<K>>
#if 1
	using HashMap = std::conditional_t<std::is_void_v<V>, ska::bytell_hash_set<K, H>, ska::bytell_hash_map<K, V, H>>;
#else
	using HashMap = std::conditional_t<std::is_void_v<V>, std::unordered_set<K, H>, std::unordered_map<K, V, H>>;
#endif



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
	struct TaggedUnion :
		private std::variant<std::monostate, T...>
	{
		using IndexT = MinUint<sizeof...(T)>;
		using Base = std::variant<std::monostate, T...>;

		template <typename U>
		static constexpr IndexT IDOf = Detail::VariantTypeFinder<U, 0, T...>::Index;

		constexpr TaggedUnion() :
			Base()
		{
		}
		
		TaggedUnion(const TaggedUnion& other) :
			Base(*(const Base*)&other)
		{
		}

		TaggedUnion& operator=(const TaggedUnion& other)
		{
			this->~TaggedUnion();
			new (this) TaggedUnion(other);
			return *this;
		}

		TaggedUnion(TaggedUnion&& other) noexcept :
			Base(std::move(*(Base*)&other))
		{
		}

		TaggedUnion& operator=(TaggedUnion&& other) noexcept
		{
			this->~TaggedUnion();
			new (this) TaggedUnion(std::move(other));
			return *this;
		}

		template <
			typename U,
			typename = std::enable_if_t<Detail::VariantTypeFinder<std::remove_reference_t<U>, 0, T...>::Index != sizeof...(T)>>
		constexpr TaggedUnion(U&& value) :
			Base(std::forward<U>(value))
		{
		}

		template <
			typename U,
			typename = std::enable_if_t<Detail::VariantTypeFinder<std::remove_reference_t<U>, 0, T...>::Index != sizeof...(T)>>
		TaggedUnion& operator=(U&& value)
		{
			this->~TaggedUnion();
			new (this) TaggedUnion(std::forward<U>(value));
			return *this;
		}

		~TaggedUnion() = default;

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



	template <typename T>
	struct SimpleObjectPool
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

		T* Acquire()
		{
			while (true)
			{
				auto prior = free.load(std::memory_order_acquire);
				if (prior.first == nullptr)
					break;
				decltype(prior) desired = { prior.first->next, prior.second + 1 };
				if (free.compare_exchange_weak(prior, desired, std::memory_order_acquire, std::memory_order_relaxed))
					return (T*)prior.first;
				//SPIN_WAIT
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

			auto new_head = (Block*)OS::Malloc(BlockSize);
			assert(new_head != nullptr);
			new (&new_head->bump) std::atomic_uint32_t(2);
			while (true)
			{
				auto prior = head.load(std::memory_order_acquire);
				new_head->next = prior.first;
				decltype(prior) desired = { new_head, prior.second + 1 };
				if (head.compare_exchange_weak(prior, desired, std::memory_order_release, std::memory_order_relaxed))
					return (T*)new_head + 1;
				//SPIN_WAIT
			}
		}

		void Release(T* e)
		{
			auto n = (Node*)e;
			while (true)
			{
				auto prior = free.load(std::memory_order_acquire);
				n->next = prior.first;
				decltype(prior) desired = { n, prior.second + 1 };
				if (free.compare_exchange_weak(prior, desired, std::memory_order_release, std::memory_order_relaxed))
					break;
				//SPIN_WAIT
			}
		}
	};



	template <typename T>
	struct ScopedPtrTraits
	{
		inline static SimpleObjectPool<T> allocator;

		static T* New()
		{
			return allocator.Acquire();
		}

		static void Release(T* ptr)
		{
			allocator.Release(ptr);
		}
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
				Traits::Release(ptr);
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

		template <typename... U>
		static auto New(U&&... params)
		{
			auto r = Traits::New();
			new (r) T(std::forward<U>(params)...);
			return ScopedPtr<T>(r);
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

		constexpr Range(vector<T>&& elements)
			: begin_it(elements.data()), end_it(elements.data() + elements.size())
		{
		}

		constexpr Range(const vector<T>& elements)
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