#include "FormulaGenerator.h"

#include <print>
#include <bit>
#include <sstream>
#include <algorithm>

FormulaGenerator::FormulaGenerator(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_),
	comps(d, n, n),
	used(d, n),
	oneDown(d, n, n),
	oneUp(d, n, n) {}

Expression FormulaGenerator::Generate(const std::vector<uint64_t>& inputs_, const std::vector<CEd>& fixedComps_, const std::vector<CEd>& bannedComps_)
{
	inputs = inputs_;
	fixedComps = fixedComps_;
	bannedComps = bannedComps_;

	InitializeVariables();
	AddValid();
	AddUsedDefinitions();
	AddUpDownDefinitions();

	for (size_t i = 0; i < inputs.size(); i++)
		AddInput(i);

	AddCompConstraints();

	AddPhi1();
	AddPhi2();
	AddPhi3();
	AddPhi4();
	AddPsi1();
	AddPsi2a();
	AddPsi2b();
	AddPsi2c();
	AddPsi3a();
	AddPsi3b();

	AddSamplingComment();

	return expr;
}

Network FormulaGenerator::ParseAssignment(const std::vector<bool>& assignment)
{
	Network network;
	for (uint8_t k = 0; k < d; k++)
	{
		for (uint8_t i = 0; i < n - 1; i++)
		{
			for (uint8_t j = i + 1; j < n; j++)
			{
				Literal l = comps(k, i, j);
				bool isNeg = l < 0;
				Var v = std::abs(l);

				if (assignment[v] ^ isNeg)
					network.push_back({ i, j });
			}
				
		}
	}
	return network;
}

std::vector<Var> FormulaGenerator::GetSamplingVariables() const
{
	std::vector<Var> sampling;
	for (uint8_t k = 0; k < d; k++)
		for (uint8_t i = 0; i + 1 < n; i++)
			for (uint8_t j = i + 1; j < n; j++)
				if (!symmetric || i <= n - 1 - j)
					sampling.push_back(comps(k, i, j));

	return sampling;
}

void FormulaGenerator::LogVariableInfo(Var var)
{
	if (var == trueVar)
	{
		std::println("{} = True variable", var);
		return;
	}

	if (var == falseVar)
	{
		std::println("{} = False variable", var);
		return;
	}

	for (uint8_t k = 0; k < d; k++)
	{
		for (uint8_t i = 0; i + 1 < n; i++)
		{
			for (uint8_t j = i + 1; j < n; j++)
			{
				if (comps(k, i, j) != var) continue;
				std::println("{} = g^{}_{},{}", var, k, i, j);
				return;
			}
		}
	}

	for (uint8_t k = 0; k < d; k++)
	{
		for (uint8_t i = 0; i < n; i++)
		{
			if (used(k, i) != var) continue;
			std::println("{} = used^{}_{}", var, k, i);
			return;
		}
	}

	for (uint8_t k = 0; k < d; k++)
	{
		for (uint8_t i = 0; i < n; i++)
		{
			if (used(k, i) != var) continue;
			std::println("{} = used^{}_{}", var, k, i);
			return;
		}
	}

	for (uint8_t k = 1; k < d; k++)
	{
		for (uint8_t i = 0; i < n; i++)
		{
			for (uint8_t j = i; j < n; j++)
			{
				if (oneDown(k, i, j) == var)
				{
					std::println("{} = oneDown^{}_{},{}", var, k, i, j);
					return;
				}
				if (oneUp(k, i, j) == var)
				{
					std::println("{} = oneUp^{}_{},{}", var, k, i, j);
					return;
				}
			}
		}
	}

	for (size_t inputIdx = 0; inputIdx < inputs.size(); inputIdx++)
	{
		for (uint8_t k = 0; k <= d; k++)
		{
			for (uint8_t i = 0; i < n; i++)
			{
				if (v(inputIdx, k, i) != var) continue;
				std::println("{} = v^{}_{},{} : {:0>{}b}", var, k, inputIdx, i, inputs[inputIdx], n);
				return;
			}
		}
	}

	for (const auto& [clause, cv] : clauseVars)
	{
		if (cv != var) continue;
		std::println("{} = Clause var: {}", var, clause);
	}

	std::println("Unknown variable");
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

	// OneUpDown variables
	for (uint8_t k = 1; k < d; k++)
		for (uint8_t i = 0; i < n; i++)
			oneDown(k, i, i) = oneUp(k, i, i) = falseVar;

	// V variables
	v = VariableFamily{ inputs.size(), d + 1U, n };
	for (size_t inputIdx = 0; inputIdx < inputs.size(); inputIdx++)
		InitializeVVariables(inputIdx);
}

void FormulaGenerator::InitializeVVariables(size_t inputIdx)
{
	uint64_t input = inputs[inputIdx];
	uint64_t leadingZeros = LeadingZeros(input);
	uint64_t tailingOnes = TailingOnes(input);
	uint64_t numZeros = n - std::popcount(input);

	// Leading zeros and tailing ones
	for (uint8_t k = 0; k <= d; k++)
	{
		for (uint8_t i = 0; i < leadingZeros; i++)
			v(inputIdx, k, i) = falseVar;
		for (uint8_t i = n - tailingOnes; i < n; i++)
			v(inputIdx, k, i) = trueVar;
	}

	// Intermediate layers
	for (uint8_t k = 2; k < d - 2; k++)
		for (uint8_t i = leadingZeros; i < n - tailingOnes; i++)
			v(inputIdx, k, i) = expr.NextVar();

	// Antepenultimate layer
	for (uint8_t i = leadingZeros; i < n - tailingOnes; i++)
	{
		if (i < numZeros && numZeros - i >= 5)
			v(inputIdx, d - 2, i) = falseVar;
		else if (i > numZeros && i - numZeros >= 4)
			v(inputIdx, d - 2, i) = trueVar;
		else
			v(inputIdx, d - 2, i) = expr.NextVar();
	}

	// Since the last layer only contains comparators of height 1,
	// bits in the penultimate set of values can only 'move' by 1 space.
	// Any bits which are more than 1 away from the 0 -> 1 transition
	// must already be correct.
	for (uint8_t i = 0; i < numZeros - 1; i++)
		v(inputIdx, d - 1, i) = falseVar;
	v(inputIdx, d - 1, numZeros - 1) = expr.NextVar();
	v(inputIdx, d - 1, numZeros) = expr.NextVar();
	for (uint8_t i = numZeros + 1; i < n; i++)
		v(inputIdx, d - 1, i) = trueVar;

	// Ensure that the output layer is sorted
	for (uint8_t i = 0; i < n; i++)
		v(inputIdx, d, i) = (i < numZeros) ? falseVar : trueVar;
}

bool FormulaGenerator::ShareChannel(uint8_t i0, uint8_t j0, uint8_t i1, uint8_t j1) const
{
	bool shares =  i0 == i1
				|| i0 == j1
				|| j0 == i1
				|| j0 == j1;

	if (!symmetric) return shares;

	uint8_t i0Sym = n - 1 - j0;
	uint8_t j0Sym = n - 1 - i0;
	bool sharesSym =   i0Sym == i1
					|| i0Sym == j1
					|| j0Sym == i1
					|| j0Sym == j1;

	return shares || sharesSym;
}

void FormulaGenerator::AddValid()
{
	for (uint8_t k = 0; k < d; k++)
	{
		// Loop over all comparators (i0, j0)
		for (uint8_t i0 = 0; i0 < n - 1U; i0++)
		{
			for (uint8_t j0 = i0 + 1; j0 < n; j0++)
			{
				// Comparator (i0, j0) must be lexicographically leq to its reflection
				if (symmetric && i0 > n - 1 - j0) continue;

				// Loop over all comparators (i1, j1)
				for (uint8_t i1 = 0; i1 < n - 1U; i1++)
				{
					for (uint8_t j1 = i1 + 1; j1 < n; j1++)
					{
						// Comparator (i1, j1) must be lexicographically leq to its reflection
						if (symmetric && i1 > n - 1 - j1) continue;
						// Comparator (i0, j0) must be lexicographically < (i1, j1)
						if (i1 < i0 || (i1 == i0 && j1 <= j0)) continue;

						if (ShareChannel(i0, j0, i1, j1))
							expr.AddClause({ -comps(k, i0, j0), -comps(k, i1, j1) });
					}
				}
			}
		}
	}
}

void FormulaGenerator::AddUsedDefinitions()
{
	for (uint8_t k = d - 2; k < d; k++)
	{
		for (uint8_t i = 0; i < n; i++)
		{
			Clause def;
			for (uint8_t j = 0; j < i; j++)		def.push_back(comps(k, j, i));
			for (uint8_t j = i + 1; j < n; j++)	def.push_back(comps(k, i, j));
			used(k, i) = GetClauseVar(def);
		}
	}
}

void FormulaGenerator::AddUpDownDefinitions()
{
	for (uint8_t k = 1; k < d; k++)
	{
		for (uint8_t i = 0; i < n; i++)
		{
			for (uint8_t j = i + 1; j < n; j++)
			{
				AddOneDownDefinition(k, i, j);
				AddOneUpDefinition(k, i, j);
			}
		}
	}
}

void FormulaGenerator::AddOneDownDefinition(uint8_t k, uint8_t i, uint8_t j)
{
	Clause def;
	for (uint8_t l = i + 1; l <= j; l++)
		def.push_back(comps(k, i, l));
	oneDown(k, i, j) = GetClauseVar(def);
}

void FormulaGenerator::AddOneUpDefinition(uint8_t k, uint8_t i, uint8_t j)
{
	Clause def;
	for (uint8_t l = i; l < j; l++)
		def.push_back(comps(k, l, j));
	oneUp(k, i, j) = GetClauseVar(def);
}

void FormulaGenerator::AddInput(size_t inputIdx)
{
	uint64_t inputWord = inputs[inputIdx];

	uint64_t leadingZeros = LeadingZeros(inputWord);
	uint64_t tailingOnes = TailingOnes(inputWord);
	uint64_t numZeros = n - std::popcount(inputWord);

	// Special case for first-layer outputs
	for (uint8_t i = leadingZeros; i < (n - tailingOnes); i++)
	{
		bool val = inputWord & (1ULL << i);

		if (val)
		{
			// Value will only be affected by zeros in higher indices
			Clause affectingComps;
			for (uint8_t j = i + 1; j < (n - tailingOnes); j++)
				if (~inputWord & (1ULL << j))
					affectingComps.push_back(comps(0, i, j));

			v(inputIdx, 1, i) = -GetClauseVar(affectingComps);
		}
		else
		{
			// Value will only be affected by ones in lower indices
			Clause affectingComps;
			for (uint8_t j = leadingZeros; j < i; j++)
				if (inputWord & (1ULL << j))
					affectingComps.push_back(comps(0, j, i));

			v(inputIdx, 1, i) = GetClauseVar(affectingComps);
		}
	}

	// Ensure that intermediate layer results are updated according to the comparators
	for (uint8_t k = 2; k < d; k++)
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

	// Special case for the penultimate set of values
	// Only the two values on the 0 -> 1 transition are not-fixed
	Var v0 = v(inputIdx, d - 1, numZeros - 1);
	Var v1 = v(inputIdx, d - 1, numZeros);

	// Exactly one of v0 and v1 must be a 1
	expr.AddClause({ v0, v1 });
	expr.AddClause({ -v0, -v1 });

	// If they are out-of-order, a comparator must exist between them
	expr.AddClause({ comps(d - 1, numZeros - 1, numZeros), -v0, v1 });
}

void FormulaGenerator::AddCompConstraints()
{
	for (auto [k, i, j] : fixedComps) expr.AddClause({ comps(k, i, j) });
	for (auto [k, i, j] : bannedComps) expr.AddClause({ -comps(k, i, j) });
}

void FormulaGenerator::AddPhi1()
{
	for (uint8_t i = 0; i < n - 2; i++)
		for (uint8_t j = i + 2; j < n; j++)
			if (!symmetric || i <= n - 1 - j)
				expr.AddClause({ -comps(d - 1, i, j) });
}

void FormulaGenerator::AddPhi2()
{
	for (uint8_t i = 0; i < n - 4; i++)
		for (uint8_t j = i + 4; j < n; j++)
			if (!symmetric || i <= n - 1 - j)
				expr.AddClause({ -comps(d - 2, i, j) });
}

void FormulaGenerator::AddPhi3()
{
	uint8_t numChannels = symmetric ? n / 2 - 1 : n - 3;
	for (uint8_t i = 0; i < numChannels; i++)
	{
		expr.AddClause({
			-comps(d - 2, i, i + 3),
			comps(d - 1, i, i + 1)
			});

		if (i < numChannels - 1)
			expr.AddClause({
				-comps(d - 2, i, i + 3),
				comps(d - 1, i + 2, i + 3)
				});
	}
}

void FormulaGenerator::AddPhi4()
{
	uint8_t numChannels = symmetric ? n / 2 - 1 : n - 2;
	for (uint8_t i = 0; i < numChannels; i++)
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
	uint8_t numChannels = symmetric ? n / 2 - 1 : n - 1;
	for (uint8_t i = 0; i < numChannels; i++)
	{
		expr.AddClause({
			used(d - 1, i),
			used(d - 1, i + 1),
			});
	}

	if (symmetric)
		expr.AddClause({ used(d - 1, n / 2 - 1) });
}

void FormulaGenerator::AddPsi2a()
{
	uint8_t numChannels = symmetric ? n / 2 - 1 : n - 3;
	for (uint8_t i = 0; i < numChannels; i++)
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

void FormulaGenerator::AddSamplingComment()
{
	std::stringstream ss;
	ss << "c p show ";

	std::vector<Var> sampleVars = GetSamplingVariables();
	for (Var v : sampleVars) ss << v << ' ';
	
	ss << '0';
	expr.AddComment(ss.str());
}

Var FormulaGenerator::GetClauseVar(const Clause& clause)
{
	// Filter out fixed true/false variables
	Clause sorted;
	for (Literal l : clause)
	{
		if (l == trueVar || l == -falseVar) return trueVar;
		if (l != falseVar && l != -trueVar) sorted.push_back(l);
	}

	// No need to create a new variable equal to a unit clause
	if (sorted.size() == 1) return sorted[0];
	
	// Sort and insert the clause into the hash map
	std::sort(sorted.begin(), sorted.end());
	auto [it, inserted] = clauseVars.try_emplace(sorted, 0);

	// If the clause is new, create a new variable for it
	if (inserted)
	{
		Var v = expr.NextVar();
		it->second = v;
		expr.AddEquals(v, clause);
	}
	
	return it->second;
}

uint64_t FormulaGenerator::LeadingZeros(uint64_t input) const
{
	return std::min<uint64_t>(n, std::countr_zero(input));
}

uint64_t FormulaGenerator::TailingOnes(uint64_t input) const
{
	return std::countl_one(input << (64 - n));
}

VariableFamily::VariableFamily(size_t xWidth, size_t yWidth, size_t zWidth)
	: yStride(xWidth), zStride(xWidth * yWidth), vars(xWidth * yWidth * zWidth) {}

Var& VariableFamily::operator()(size_t x, size_t y, size_t z)
{
	return vars[z * zStride + y * yStride + x];
}

const Var& VariableFamily::operator()(size_t x, size_t y, size_t z) const
{
	return vars[z * zStride + y * yStride + x];
}