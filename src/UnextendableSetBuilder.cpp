#include "UnextendableSetBuilder.h"

#include <print>
#include <iostream>
#include <random>
#include <algorithm>
#include <unordered_set>
#include <numeric>

#include "Prefixes/prefixes.h"
#include "Timer.h"

UnextendableSetBuilder::UnextendableSetBuilder(uint8_t n_, uint8_t d_, bool symmetric_, const std::vector<Network>& prefixes_)
	: n(n_), d(d_), symmetric(symmetric_), prefixes(prefixes_),
	generator(n, d, symmetric), subSolver(n, symmetric)
{
	// Compute prefix outputs
	for (const Network& prefix : prefixes)
	{
		prefixOutputs.emplace_back(FactoredOutputSet{ prefix, n }.ToVector());
		std::ranges::sort(prefixOutputs.back());
	}

	size_t numPrefixes = prefixes.size();
	subsumed.assign(numPrefixes, true);
	witnessPerms.assign(numPrefixes, {});
}

std::vector<uint64_t> UnextendableSetBuilder::Build()
{
	// Initialize formula generator and solver
	generator.Generate();
	LoadExpressionMinisat(satSolver, generator.GetExpression());

	std::optional<uint64_t> lastAdded = std::nullopt;
	for (;;)
	{
		// Solve the CNF formula
		if (!IsSAT())
		{
			//std::println("UNSAT: |X| = {} solve took {}s", X.size(), satTime);
			return X;
		}

		// Score every element for inclusion
		std::vector<size_t> scores(1ULL << n, 0);
		Timer timer;
		size_t numSubsumed = ScoreElements(scores, lastAdded);
		timer.Stop();
		totalScoreTime += timer.GetSeconds();
		//std::println("SAT: |X| = {} subsumes {} solve took {}s", X.size(), numSubsumed, satTime);

		// Choose the highest scoring unsorted element
		uint64_t newInput = ChooseNewInput(scores);

		// Add element to X and the formula
		lastAdded = newInput;
		AddNewInput(newInput);
	}

	return X;
}

double UnextendableSetBuilder::GetTotalSATTime() const
{
	return totalSatTime;
}

double UnextendableSetBuilder::GetTotalScoreTime() const
{
	return totalScoreTime;
}

bool UnextendableSetBuilder::IsSAT()
{
	Minisat::vec<Minisat::Lit> dummy;
	Timer timer;
	Minisat::lbool ret = satSolver.solveLimited(dummy);
	timer.Stop();
	lastSatTime = timer.GetSeconds();
	totalSatTime += lastSatTime;

	return ret == Minisat::l_True;
}

bool UnextendableSetBuilder::SubsumedTrivially(size_t prefixIdx, uint64_t lastAdded)
{
	uint64_t permuted = witnessPerms[prefixIdx](lastAdded);
	return std::ranges::binary_search(prefixOutputs[prefixIdx], permuted);
}

size_t UnextendableSetBuilder::ScoreElements(std::vector<size_t>& scores, std::optional<uint64_t> lastAdded)
{
	size_t numSubsumed = 0;
	for (size_t prefixIdx = 0; prefixIdx < prefixes.size(); prefixIdx++)
	{
		if (!subsumed[prefixIdx]) continue;
		const std::vector<uint64_t>& outputs = prefixOutputs[prefixIdx];

		// Check if the previous witness perm is still valid after the last addition
		if (!lastAdded || !SubsumedTrivially(prefixIdx, *lastAdded))
		{
			// Run a full subsumption test and update the witness perm
			if (subSolver.Solve(X, outputs) != DoesSubsume)
			{
				subsumed[prefixIdx] = false;
				continue;
			}
			witnessPerms[prefixIdx] = subSolver.GetPerm();
		}

		// Update counters
		numSubsumed++;
		Permutation invPerm{ witnessPerms[prefixIdx] };
		invPerm.Invert();
		for (uint64_t bx : outputs)
			scores[invPerm(bx)]++;
	}

	return numSubsumed;
}

Network UnextendableSetBuilder::ReconstructPostfix() const
{
	std::vector<bool> assignment(satSolver.nVars() + 1);
	for (int i = 0; i < satSolver.nVars(); i++)
		assignment[i + 1] = (satSolver.model[i] == Minisat::l_True);
	return generator.ParseAssignment(assignment);
}

static inline uint64_t NumInversions(uint64_t x, uint8_t n)
{
	uint64_t zeroMask = ~x & ((1ULL << n) - 1);
	uint64_t popcount = std::popcount(x);
	uint64_t inversions = 0;
	for (uint8_t _ = 0; _ < popcount; _++)
	{
		uint8_t i = std::countr_zero(x);
		x &= x - 1;
		inversions += std::popcount(zeroMask >> (i + 1));
	}

	return inversions;
}

uint64_t UnextendableSetBuilder::ChooseNewInput(const std::vector<size_t>& scores) const
{
	Network postfix = ReconstructPostfix();

#if 1
	uint64_t bestElement;
	size_t bestScore = 0;
	for (uint64_t x = 0; x < (1ULL << n); x++)
	{
		if (scores[x] <= bestScore) continue;
		if (IsSorted(n, postfix(x))) continue;

		bestElement = x;
		bestScore = scores[x];
	}

	return bestElement;
#else
	size_t bestScore = 0;
	for (uint64_t x = 0; x < (1ULL << n); x++)
		if (scores[x] > bestScore && !IsSorted(n, postfix(x)))
			bestScore = scores[x];

	std::vector<uint64_t> candidates;
	for (uint64_t x = 0; x < (1ULL << n); x++)
		if ((double)scores[x] / bestScore > 0.9 && !IsSorted(n, postfix(x)))
			candidates.push_back(x);

	return std::ranges::min(candidates, {}, [this](uint64_t x) { return NumInversions(x, n); });
#endif
}

void UnextendableSetBuilder::AddNewInput(uint64_t x)
{
	X.push_back(x);

	// Add new input to the generator
	const Expression& expr = generator.GetExpression();
	size_t numClausesBefore = expr.NumClauses();
	Var varsBefore = expr.NumVars();
	generator.AddInput(x);

	// Add new variables to the solver
	size_t numVarsAdded = expr.NumVars() - varsBefore;
	for (size_t i = 0; i < numVarsAdded; i++)
		satSolver.newVar();

	// Add new clauses to the solver
	const auto& allClauses = expr.GetClauses();
	for (size_t i = numClausesBefore; i < allClauses.size(); i++)
		satSolver.addClause(ConvertClause(allClauses[i]));
}