#pragma once
#include "AST.hpp"



namespace Zero
{
	struct EvalEngine
	{
		vector<Expression> stack;

		void Eval(const Expression& e);
	};
}