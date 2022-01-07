#include "Keyword.hpp"

namespace Zero
{
	static const auto symbol_lookup = []
	{
		HashMap<string_view, Keyword> r = {};
		for (uint8 i = 0; i != (uint8)Keyword::MaxEnum; ++i)
			r.insert({ KEYWORD_STRINGS[i], (Keyword)i });
		return r;
	}();

	static constexpr auto MAX_KEYWORD_SIZE = []()
	{
		uintptr r = 0;
		for (auto& e : KEYWORD_STRINGS)
			if (e.size() > r)
				r = e.size();
		return r;
	}();

	static constexpr auto MIN_KEYWORD_SIZE = []()
	{
		uintptr r = SIZE_MAX;
		for (auto& e : KEYWORD_STRINGS)
			if (e.size() < r)
				r = e.size();
		return r;
	}();

    Keyword IsKeyword(string_view token)
    {
		if (token.size() < MIN_KEYWORD_SIZE || token.size() > MAX_KEYWORD_SIZE)
			return Keyword::MaxEnum;
		auto it = symbol_lookup.find(token);
		if (it != symbol_lookup.end())
			return it->second;
		return Keyword::MaxEnum;
    }
}