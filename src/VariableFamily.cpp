#include "VariableFamily.h"

VariableFamily::VariableFamily(size_t xWidth, size_t yWidth, size_t zWidth)
	: yStride(xWidth), zStride(xWidth* yWidth), vars(xWidth* yWidth* zWidth) {}

Literal& VariableFamily::operator()(size_t x, size_t y, size_t z)
{
	return vars[z * zStride + y * yStride + x];
}

const Literal& VariableFamily::operator()(size_t x, size_t y, size_t z) const
{
	return vars[z * zStride + y * yStride + x];
}