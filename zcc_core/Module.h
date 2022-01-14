#pragma once
#include "AST.hpp"



namespace Zero
{
	struct Module
	{
		HashMap<string_view> dependencies;
		Scope global_scope;
	};
}