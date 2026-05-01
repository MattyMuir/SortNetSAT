#pragma once
#include <cstdint>

#include "Network.h"
#include "Expression2.h"

class VariableFamily
{
public:
	VariableFamily() = default;
	VariableFamily(size_t xWidth, size_t yWidth = 1, size_t zWidth = 1);

	Var& operator()(size_t x, size_t y = 0, size_t z = 0);

protected:
	size_t yStride, zStride;
	std::vector<Var> vars;
};

class FormulaGenerator
{
public:
	FormulaGenerator(uint8_t n_, uint8_t d_, bool symmetric_);

	Expression2 Generate(const std::vector<uint64_t>& inputs_);

protected:
	uint8_t n, d;
	bool symmetric;
	std::vector<uint64_t> inputs;

	Expression2 expr;
	VariableFamily comps, used, oneDown, oneUp, v;

	void InitializeVariables();
};