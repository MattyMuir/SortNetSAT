#pragma once
#include "Expression.h"

class VariableFamily
{
public:
	VariableFamily() = default;
	VariableFamily(size_t xWidth, size_t yWidth = 1, size_t zWidth = 1);

	Literal& operator()(size_t x, size_t y = 0, size_t z = 0);
	const Literal& operator()(size_t x, size_t y = 0, size_t z = 0) const;

protected:
	size_t yStride, zStride;
	std::vector<Literal> vars;
};