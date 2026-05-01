#include "FormulaGenerator.h"

FormulaGenerator::FormulaGenerator(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_),
	comps(d, n, n),
	used(d, n),
	oneDown(d, n, n),
	oneUp(d, n, n) {}

Expression2 FormulaGenerator::Generate(const std::vector<uint64_t>& inputs_)
{
	inputs = inputs_;
	v = VariableFamily{ inputs.size(), d, n };

	InitializeVariables();

	return expr;
}

void FormulaGenerator::InitializeVariables()
{
	// Comp variables
	for (uint8_t k = 0; k < d; k++)
		for (uint8_t i = 0; i + 1 < n; i++)
			for (uint8_t j = i + 1; j < n; j++)
				comps(k, i, j) = expr.NextVar();

	// Used variables
	for (uint8_t k = 0; k < d; k++)
		for (uint8_t i = 0; i < n; i++)
			used(k, i) = expr.NextVar();

	// OneDown variables
	for (uint8_t k = 0; k < d; k++)
		for (uint8_t i = 0; i < n; i++)
			for (uint8_t j = i; j < n; j++)
				oneDown(k, i, j) = expr.NextVar();

	// OneUp variables
	for (uint8_t k = 0; k < d; k++)
		for (uint8_t i = 0; i < n; i++)
			for (uint8_t j = i; j < n; j++)
				oneUp(k, i, j) = expr.NextVar();

	// V variables
	for (size_t inputIdx = 0; inputIdx < inputs.size(); inputIdx++)
		for (uint8_t k = 0; k < d; k++)
			for (uint8_t i = 0; i < n; i++)
				v(inputIdx, k, i) = expr.NextVar();
}

VariableFamily::VariableFamily(size_t xWidth, size_t yWidth, size_t zWidth)
	: yStride(xWidth), zStride(xWidth * yWidth), vars(xWidth * yWidth * zWidth) {}

Var& VariableFamily::operator()(size_t x, size_t y, size_t z)
{
	return vars[z * zStride + y * yStride + x];
}