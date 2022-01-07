#pragma once
#include "Util.hpp"



namespace Zero
{
	constexpr string_view KEYWORD_STRINGS[] =
	{
		"pragma",
		"use",
		"namespace",
		"type", "enum",
		"void",
		"nil", "true", "false",
		"let",
		"bool", "int", "uint", "float",
		"if", "elif", "else", "select",
		"do", "while", "for",
		"as",
		"break", "continue", "defer",
		"return", "yield"
	};

	enum class Keyword : uint8_t
	{
		Pragma,
		Use,
		Namespace,
		Type, Enum,
		True, False,
		Nil,
		Void, Let, Bool, Int, UInt, Float,
		If, Elif, Else, Select,
		Do, While, For,
		As,
		Break, Continue, Defer,
		Return, Yield,
		MaxEnum,
	};

	Keyword IsKeyword(string_view token);
}