#include "FormulaGenerator2.h"

#include <set>

FormulaGenerator2::FormulaGenerator2(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_),
	comps(d, n, n),
	used(d, n),
	oneDown(d, n, n),
	oneUp(d, n, n)
{
	for (uint8_t i = 0; i < n; i++)
	{
		for (uint8_t j = i + 1; j < n; j++)
		{
			allCEs.emplace_back(i, j);
			if (!symmetric || i <= n - 1 - j) allSymCEs.emplace_back(i, j);
		}
	}
}

void FormulaGenerator2::Initialize()
{
	// Create 'true' variable
	trueVar = expr.NextVar();
	expr.AddClause({ trueVar });

	// Create comp variables
	compMin = expr.NumVars() + 1;
	for (uint8_t k = 0; k < d; k++)
		for (auto [i, j] : allCEs)
			comps(k, i, j) = (symmetric && i > n - 1 - j) ? comps(k, n - 1 - j, n - 1 - i) : expr.NextVar();
	compMax = expr.NumVars();

	// Ban overlapping comparators
	for (uint8_t k = 0; k < d; k++)
		for (CE aCE : allSymCEs)
			for (CE bCE : allSymCEs)
				if (aCE < bCE && ShareChannel(aCE, bCE))
					expr.AddClause({ -comps(k, aCE.lo, aCE.hi), -comps(k, bCE.lo, bCE.hi) });

	// Initialize 'used' variables
	for (uint8_t k = 0; k < d; k++)
	{
		for (uint8_t i = 0; i < n; i++)
		{
			Clause c;
			for (uint8_t j = 0; j < i; j++)		c.push_back(comps(k, j, i));
			for (uint8_t j = i + 1; j < n; j++)	c.push_back(comps(k, i, j));
			used(k, i) = GetClauseVar(c);
		}
	}

	// Initialize 'oneUp' and 'oneDown' variables
	for (uint8_t k = 1; k < d; k++)
	{
		for (uint8_t i = 0; i < n; i++)
			oneDown(k, i, i) = oneUp(k, i, i) = -trueVar;

		for (auto [i, j] : allCEs)
		{
			// oneDown
			Clause downComps;
			for (uint8_t l = i + 1; l <= j; l++)
				downComps.push_back(comps(k, i, l));
			oneDown(k, i, j) = GetClauseVar(downComps);

			// oneUp
			Clause upComps;
			for (uint8_t l = i; l < j; l++)
				upComps.push_back(comps(k, l, j));
			oneUp(k, i, j) = GetClauseVar(upComps);
		}
	}
}

VariableFamily FormulaGenerator2::AddInput(uint64_t input, InputRequirement requirement)
{
	if (IsSorted(n, input))
	{
		if (requirement == RequireUnsorted) expr.AddClause({ -trueVar });
		return VariableFamily{};
	}

	uint64_t leadingZeros = std::min<uint64_t>(n, std::countr_zero(input));
	uint64_t tailingOnes = std::countl_one(input << (64 - n));
	uint64_t numZeros = n - std::popcount(input);

	// Create variables holding the values on each wire at each layer
	VariableFamily v{ d + 1U, n };
	for (uint8_t k = 2; k <= d; k++)
		for (uint8_t i = leadingZeros; i < n - tailingOnes; i++)
			v(k, i) = expr.NextVar();

	// Special case for first-layer outputs
	for (uint8_t i = leadingZeros; i < n - tailingOnes; i++)
	{
		bool val = input & (1ULL << i);

		if (val)
		{
			// Value will only be affected by zeros in higher indices
			Clause affectingComps;
			for (uint8_t j = i + 1; j < (n - tailingOnes); j++)
				if (~input & (1ULL << j))
					affectingComps.push_back(comps(0, i, j));

			v(1, i) = -GetClauseVar(affectingComps);
		}
		else
		{
			// Value will only be affected by ones in lower indices
			Clause affectingComps;
			for (uint8_t j = leadingZeros; j < i; j++)
				if (input & (1ULL << j))
					affectingComps.push_back(comps(0, j, i));

			v(1, i) = GetClauseVar(affectingComps);
		}
	}

	// Ensure that intermediate layer results are updated according to the comparators
	for (uint8_t k = 2; k <= d; k++)
	{
		for (uint8_t i = leadingZeros; i < (n - tailingOnes); i++)
		{
			// Let us assume that the value on this channel is "1". 
			// Thus, this will stay unless there is a comparator down inside the window,
			// and the other input is "0". 
			expr.AddClause({ oneDown(k - 1, i, n - tailingOnes - 1), -v(k - 1, i), v(k, i) });

			// Not let us assume there is a comparator to a higher index. 
			// Then, this "1" will disappear if the other input is a "0"
			for (uint8_t j = i + 1; j < n - tailingOnes; j++)
			{
				// If the other input is "0"...
				expr.AddClause({ -comps(k - 1, i, j), v(k - 1, j), -v(k, i) });

				// If both inputs equal "1"
				expr.AddClause({ -comps(k - 1, i, j), -v(k - 1, j), -v(k - 1, i), v(k, i) });
			}
			// What if the input is "0"? 
			// No comparator to lower indices -> output will be zero
			expr.AddClause({ oneUp(k - 1, leadingZeros, i), v(k - 1, i), -v(k, i) });

			for (uint8_t j = leadingZeros; j < i; j++)
			{
				// Comparator chosen, and other value="1" -> swap
				expr.AddClause({ -comps(k - 1, j, i), -v(k - 1, j), v(k, i) });

				// Comparator chosen, and other value="0" -> still "0"
				expr.AddClause({ -comps(k - 1, j, i), v(k - 1, j), v(k - 1, i), -v(k, i) });
			}
		}
	}

	if (requirement == RequireSorted)
	{
		for (uint8_t i = leadingZeros; i < n - tailingOnes; i++)
			expr.AddClause({ (i < numZeros) ? -v(d, i) : v(d, i) });
	}
	else
	{
		Clause banSorted;
		for (uint8_t i = leadingZeros; i < n - tailingOnes; i++)
			banSorted.push_back((i < numZeros) ? v(d, i) : -v(d, i));
		expr.AddClause(banSorted);
	}

	return v;
}

void FormulaGenerator2::BanAssignment(const std::vector<bool>& assignment)
{
	Clause banClause;
	for (size_t i = 0; i < assignment.size(); i++)
	{
		Literal compVar = compMin + i;
		banClause.push_back(assignment[i] ? -compVar : compVar);
	}
	expr.AddClause(banClause);
}

const Expression& FormulaGenerator2::GetExpression() const
{
	return expr;
}

std::pair<Literal, Literal> FormulaGenerator2::CompVarRange() const
{
	return { compMin, compMax };
}

Network FormulaGenerator2::ParseAssignment(const std::vector<bool>& assignment) const
{
	Network network;
	for (uint8_t k = 0; k < d; k++)
		for (auto ce : allCEs)
			if (assignment[comps(k, ce.lo, ce.hi) - compMin])
				network.push_back(ce);

	return network;
}

static inline bool DoShareChannel(CE aCE, CE bCE)
{
	return aCE.lo == bCE.lo
		|| aCE.lo == bCE.hi
		|| aCE.hi == bCE.lo
		|| aCE.hi == bCE.hi;
}

bool FormulaGenerator2::ShareChannel(CE aCE, CE bCE) const
{
	if (DoShareChannel(aCE, bCE)) return true;
	if (!symmetric) return false;
	CE aSym{ (uint8_t)(n - 1 - aCE.hi), (uint8_t)(n - 1 - aCE.lo) };
	return DoShareChannel(aSym, bCE);
}

Literal FormulaGenerator2::GetClauseVar(const Clause& clause)
{
	// Convert clause to an ordered set
	std::set<Literal> literalSet{ clause.begin(), clause.end() };
	if (literalSet.contains(trueVar)) return trueVar;
	literalSet.erase(-trueVar);
	if (literalSet.size() == 1) return *literalSet.begin();

	// Convert set back to a clause and insert
	Clause prunedClause(literalSet.begin(), literalSet.end());
	auto [it, inserted] = clauseVars.try_emplace(prunedClause, 0);

	// If the clause is new, create a new variable for it
	if (inserted)
	{
		Literal v = expr.NextVar();
		it->second = v;
		expr.AddEquals(v, clause);
	}

	return it->second;
}