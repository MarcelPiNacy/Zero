#include "Util.hpp"
#define XXH_INLINE_ALL
#include <dependencies/xxHash/xxhash.h>



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
}