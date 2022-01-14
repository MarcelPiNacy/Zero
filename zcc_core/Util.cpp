#include "Util.hpp"

#define XXH_INLINE_ALL
#include <dependencies/xxHash/xxhash.h>

#ifdef _WIN32
#include <Windows.h>
#endif



namespace Zero
{
	uint64 XXHash64(const void* data, uintptr size)
	{
		return XXH3_64bits(data, size);
	}

	std::pair<uint64, uint64> XXHash128(const void* data, uintptr size)
	{
		const auto v = XXH3_128bits(data, size);
		return std::make_pair(v.low64, v.high64);
	}

	HashT XXHash(const void* data, uintptr size)
	{
		auto h = XXHash64(data, size);
		if constexpr (sizeof(HashT) == sizeof(uint32))
			h ^= h >> 32;
		return (HashT)h;
	}

	namespace OS
	{
		void* Malloc(size_t size)
		{
			return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		}

		void Free(void* ptr, size_t size)
		{
			VirtualFree(ptr, 0, MEM_RELEASE);
		}
	}
}
