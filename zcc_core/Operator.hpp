#pragma once
#include "Util.hpp"



namespace Zero
{
	enum class Operator : uint8
	{
		Assign,
		Add, AddAssign,
		Sub, SubAssign,
		Mul, MulAssign,
		Div, DivAssign,
		Mod, ModAssign,
		And, AndAssign,
		Or, OrAssign,
		Xor, XorAssign,
		ShiftLeft, ShiftLeftAssign,
		ShiftRight, ShiftRightAssign,
		RotateLeft, RotateLeftAssign,
		RotateRight, RotateRightAssign,
		Complement,
		Increment, Decrement,
		CompareEQ, CompareNE,
		CompareLT, CompareLE,
		CompareGT, CompareGE,
		CompareTW,
		BoolNot, BoolAnd, BoolOr,
		MemberAccess,
		TraitsAccess,
		MaxEnum
	};
}