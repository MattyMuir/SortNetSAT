#include "FormulaGenerator.h"

#include <bit>

FormulaGenerator::FormulaGenerator(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_),
	comps(d, n, n),
	used(d, n),
	oneDown(d, n, n),
	oneUp(d, n, n) {}

Expression2 FormulaGenerator::Generate(const std::vector<uint64_t>& inputs_)
{
	inputs = inputs_;

	InitializeVariables();
	AddValid();
	AddUsedDefinitions();
	AddUpDownDefinitions();

	for (size_t i = 0; i < inputs.size(); i++)
		AddInput(i);

	AddPhi1(d - 1);
	AddPhi2();
	AddPhi3();
	AddPhi4();
	AddPsi1();
	AddPsi2a();
	AddPsi2b();
	AddPsi2c();
	AddPsi3a();
	AddPsi3b();

	return expr;
}

void FormulaGenerator::CreateTrueFalse()
{
	trueVar = expr.NextVar();
	falseVar = expr.NextVar();
	expr.AddClause({ trueVar });
	expr.AddClause({ -falseVar });
}

void FormulaGenerator::InitializeVariables()
{
	// Create fixed true and false variables
	CreateTrueFalse();

	// Comp variables
	for (uint8_t k = 0; k < d; k++)
		for (uint8_t i = 0, jSym = n - 1; i + 1 < n; i++, jSym--)
			for (uint8_t j = i + 1, iSym = n - 2 - i; j < n; j++, iSym--)
				comps(k, i, j) = (symmetric && i > iSym) ? comps(k, iSym, jSym) : expr.NextVar();

	// Used variables
	for (uint8_t k = 0; k < d; k++)
		for (uint8_t i = 0; i < n; i++)
			used(k, i) = (symmetric && i >= n / 2) ? used(k, n - 1 - i) : expr.NextVar();

	// OneDown variables
	for (uint8_t k = 0; k < d; k++)
		for (uint8_t i = 0; i < n; i++)
			for (uint8_t j = i; j < n; j++)
				oneDown(k, i, j) = (i == j) ? falseVar : expr.NextVar();

	// OneUp variables
	for (uint8_t k = 0; k < d; k++)
		for (uint8_t i = 0; i < n; i++)
			for (uint8_t j = i; j < n; j++)
				oneUp(k, i, j) = (symmetric || i == j) ? oneDown(k, n - 1 - j, n - 1 - i) : expr.NextVar();

	// V variables
	v = VariableFamily{ inputs.size(), d + 1U, n };
	for (size_t inputIdx = 0; inputIdx < inputs.size(); inputIdx++)
	{
		uint64_t input = inputs[inputIdx];
		uint64_t leadingZeros = LeadingZeros(input);
		uint64_t tailingOnes = TailingOnes(input);

		// Ensure that the zero-th layer values equal the inputs
		for (uint8_t i = 0; i < n; i++)
			v(inputIdx, 0, i) = (input & (1ULL << i)) ? trueVar : falseVar;

		for (uint8_t k = 1; k < d; k++)
		{
			for (uint8_t i = 0; i < leadingZeros; i++)
				v(inputIdx, k, i) = falseVar;
			for (uint8_t i = leadingZeros; i < n - tailingOnes; i++)
				v(inputIdx, k, i) = expr.NextVar();
			for (uint8_t i = n - tailingOnes; i < n; i++)
				v(inputIdx, k, i) = trueVar;
		}

		// Ensure that the output layer is sorted
		uint64_t numZeros = n - std::popcount(input);
		for (uint8_t i = 0; i < n; i++)
			v(inputIdx, d, i) = (i < numZeros) ? falseVar : trueVar;
	}
}

void FormulaGenerator::AddValid()
{
	for (uint8_t k = 0; k < d; k++)
		for (uint8_t i = 0; i < n; i++)
			AddOnce(k, i);
}

void FormulaGenerator::AddOnce(uint8_t k, uint8_t i)
{
	for (uint8_t j = 0; j < n; j++)
	{
		if (j == i) continue;
		for (uint8_t l = j + 1; l < n; l++)
		{
			if (l == i) continue;
			
			uint8_t lo0 = std::min(i, j);
			uint8_t hi0 = std::max(i, j);
			if (symmetric && lo0 > n - 1 - hi0) continue;

			Var first = comps(k, std::min(i, j), std::max(i, j));
			Var second = comps(k, std::min(i, l), std::max(i, l));
			expr.AddClause({ -first, -second });
		}
	}
}

void FormulaGenerator::AddUsedDefinitions()
{
	for (uint8_t k = 0; k < d; k++)
	{
		for (uint8_t i = 0; i < n; i++)
		{
			Clause def;
			for (uint8_t j = 0; j < i; j++)		def.push_back(comps(k, j, i));
			for (uint8_t j = i + 1; j < n; j++)	def.push_back(comps(k, i, j));
			expr.AddEquals(used(k, i), def);
		}
	}
}

void FormulaGenerator::AddUpDownDefinitions()
{
	for (uint8_t k = 0; k < d; k++)
	{
		for (uint8_t i = 0; i < n; i++)
		{
			for (uint8_t j = i + 1; j < n; j++)
			{
				AddOneDownDefinition(k, i, j);
				if (!symmetric) AddOneUpDefinition(k, i, j);
			}
		}
	}
}

void FormulaGenerator::AddOneDownDefinition(uint8_t k, uint8_t i, uint8_t j)
{
	Clause def;
	for (uint8_t l = i + 1; l <= j; l++)
		def.push_back(comps(k, i, l));
	expr.AddEquals(oneDown(k, i, j), def);
}

void FormulaGenerator::AddOneUpDefinition(uint8_t k, uint8_t i, uint8_t j)
{
	Clause def;
	for (uint8_t l = i; l < j; l++)
		def.push_back(comps(k, l, j));
	expr.AddEquals(oneUp(k, i, j), def);
}

void FormulaGenerator::AddInput(size_t inputIdx)
{
	uint64_t inputWord = inputs[inputIdx];

	uint64_t leadingZeros = LeadingZeros(inputWord);
	uint64_t tailingOnes = TailingOnes(inputWord);

	// Ensure that intermediate layer results are updated according to the comparators
	for (uint8_t k = 1; k <= d; k++)
	{
		for (uint8_t i = leadingZeros; i < (n - tailingOnes); i++)
		{
			// Let us assume that the value on this channel is "1". 
			// Thus, this will stay unless there is a comparator down inside the window,
			// and the other input is "0". 
			expr.AddClause({ oneDown(k - 1, i, n - tailingOnes - 1), -v(inputIdx, k - 1, i), v(inputIdx, k, i) });

			// Not let us assume there is a comparator to a higher index. 
			// Then, this "1" will disappear if the other input is a "0"
			for (uint8_t j = i + 1; j < n - tailingOnes; j++)
			{
				// If the other input is "0"...
				expr.AddClause({ -comps(k - 1, i, j), v(inputIdx, k - 1, j), -v(inputIdx, k, i) });

				// If both inputs equal "1"
				expr.AddClause({ -comps(k - 1, i, j), -v(inputIdx, k - 1, j), -v(inputIdx, k - 1, i), v(inputIdx, k, i) });
			}
			// What if the input is "0"? 
			// No comparator to lower indices -> output will be zero
			expr.AddClause({ oneUp(k - 1, leadingZeros, i), v(inputIdx, k - 1, i), -v(inputIdx, k, i) });

			for (uint8_t j = leadingZeros; j < i; j++)
			{
				// Comparator chosen, and other value="1" -> swap
				expr.AddClause({ -comps(k - 1, j, i), -v(inputIdx, k - 1, j), v(inputIdx, k, i) });

				// Comparator chosen, and other value="0" -> still "0"
				expr.AddClause({ -comps(k - 1, j, i), v(inputIdx, k - 1, j), v(inputIdx, k - 1, i), -v(inputIdx, k, i) });
			}
		}
	}
}

void FormulaGenerator::AddPhi1(uint8_t l)
{
	for (uint8_t i = 0; i < n - 2; i++)
	{
		for (uint8_t j = i + 2; j < n; j++)
		{
			Clause clause{ -comps(l, i, j) };
			for (uint8_t k = l + 1; k < d; k++)
			{
				clause.push_back(used(k, i));
				clause.push_back(used(k, j));
			}
			expr.AddClause(clause);
		}
	}
}

void FormulaGenerator::AddPhi2()
{
	for (uint8_t i = 0; i < n - 4; i++)
		for (uint8_t j = i + 4; j < n; j++)
			expr.AddClause({ -comps(d - 2, i, j) });
}

void FormulaGenerator::AddPhi3()
{
	for (uint8_t i = 0; i < n - 3; i++)
	{
		expr.AddClause({
			-comps(d - 2, i, i + 3),
			comps(d - 1, i, i + 1)
			});

		expr.AddClause({
			-comps(d - 2, i, i + 3),
			comps(d - 1, i + 2, i + 3)
			});
	}
}

void FormulaGenerator::AddPhi4()
{
	for (uint8_t i = 0; i < n - 2; i++)
	{
		expr.AddClause({
			-comps(d - 2, i, i + 2),
			comps(d - 1, i, i + 1),
			comps(d - 1, i + 1, i + 2),
			});
	}
}

void FormulaGenerator::AddPsi1()
{
	for (uint8_t i = 0; i < n - 1; i++)
	{
		expr.AddClause({
			used(d - 1, i),
			used(d - 1, i + 1),
			});
	}
}

void FormulaGenerator::AddPsi2a()
{
	for (uint8_t i = 0; i < n - 3; i++)
	{
		expr.AddClause({
			-comps(d - 1, i, i + 1),
			-comps(d - 1, i + 2, i + 3),
			used(d - 2, i),
			used(d - 2, i + 2)
			});

		expr.AddClause({
			-comps(d - 1, i, i + 1),
			-comps(d - 1, i + 2, i + 3),
			used(d - 2, i),
			used(d - 2, i + 3)
			});

		expr.AddClause({
			-comps(d - 1, i, i + 1),
			-comps(d - 1, i + 2, i + 3),
			used(d - 2, i + 1),
			used(d - 2, i + 2)
			});

		expr.AddClause({
			-comps(d - 1, i, i + 1),
			-comps(d - 1, i + 2, i + 3),
			used(d - 2, i + 1),
			used(d - 2, i + 3)
			});
	}
}

void FormulaGenerator::AddPsi2b()
{
	for (uint8_t i = 0; i < n - 2; i++)
	{
		expr.AddClause({
			-comps(d - 1, i, i + 1),
			used(d - 1, i + 2),
			used(d - 2, i),
			used(d - 2, i + 2)
			});

		expr.AddClause({
			-comps(d - 1, i, i + 1),
			used(d - 1, i + 2),
			used(d - 2, i + 1),
			used(d - 2, i + 2)
			});
	}
}

void FormulaGenerator::AddPsi2c()
{
	for (uint8_t i = 0; i < n - 2; i++)
	{
		expr.AddClause({
			-comps(d - 1, i + 1, i + 2),
			used(d - 1, i),
			used(d - 2, i),
			used(d - 2, i + 1)
			});

		expr.AddClause({
			-comps(d - 1, i + 1, i + 2),
			used(d - 1, i),
			used(d - 2, i),
			used(d - 2, i + 2)
			});
	}
}

void FormulaGenerator::AddPsi3a()
{
	for (uint8_t i = 0; i < n - 2; i++)
	{
		expr.AddClause({
			-comps(d - 1, i, i + 1),
			used(d - 1, i + 2),
			used(d - 2, i),
			used(d - 2, i + 1),
			});
	}
}

void FormulaGenerator::AddPsi3b()
{
	for (uint8_t i = 1; i < n - 1; i++)
	{
		expr.AddClause({
			-comps(d - 1, i, i + 1),
			used(d - 1, i - 1),
			used(d - 2, i),
			used(d - 2, i + 1)
			});
	}
}

uint64_t FormulaGenerator::LeadingZeros(uint64_t input)
{
	return std::min<uint64_t>(n, std::countr_zero(input));
}

uint64_t FormulaGenerator::TailingOnes(uint64_t input)
{
	return std::countl_one(input << (64 - n));
}

VariableFamily::VariableFamily(size_t xWidth, size_t yWidth, size_t zWidth)
	: yStride(xWidth), zStride(xWidth * yWidth), vars(xWidth * yWidth * zWidth) {}

Var& VariableFamily::operator()(size_t x, size_t y, size_t z)
{
	return vars[z * zStride + y * yStride + x];
}