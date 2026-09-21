#include "UnextendableSetBuilder2.h"

#include <print>
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <numeric>

#include "Prefixes/prefixes.h"
#include "Timer.h"

// Determines how an element is scored
// 0: Scored by how many witnesses it preserves
// 1: Scored by how many prefixes it preserves
#define COUNT_PERMS 0

UnextendableSetBuilder2::UnextendableSetBuilder2(uint8_t n_, uint8_t d_, size_t maxWitnesses_, bool symmetric_, const std::vector<Network>& prefixes_)
	: n(n_), d(d_), maxWitnesses(maxWitnesses_), symmetric(symmetric_), prefixes(prefixes_),
	generator(n, d, symmetric), subSolver(n, symmetric)
{
	// Compute prefix outputs
	for (const Network& prefix : prefixes)
	{
		prefixOutputs.emplace_back(FactoredOutputSet{ prefix, n }.ToVector());
		std::ranges::sort(prefixOutputs.back());
	}

	// Initialize witnesses
	size_t numPrefixes = prefixes.size();
	witnessPerms.assign(numPrefixes, {});
	isComplete.assign(numPrefixes, false);
	InitializeWitnesses();
}

std::vector<uint64_t> UnextendableSetBuilder2::Build()
{
	// Initialize formula generator and solver
	generator.Generate();
	LoadExpressionMinisat(satSolver, generator.GetExpression());

	for (;;)
	{
		// Solve the CNF formula
		if (!IsSAT())
		{
			std::println("UNSAT: |X| = {} solve took {}s", X.size(), lastSatTime);
			return X;
		}
		std::println("SAT: |X| = {} solve took {}s", X.size(), lastSatTime);

		// Score every element for inclusion
		Timer timer;
		auto scores = ScoreElements();
		timer.Stop();
		totalScoreTime += timer.GetSeconds();

		// Choose the highest scoring unsorted element
		auto newInput = ChooseNewInput(scores);
		if (!newInput) return {};

		// Add element to X and the formula
		AddNewInput(*newInput);

		// Filter witness perms
		size_t numSubsumed = 0;
		for (size_t prefixIdx = 0; prefixIdx < prefixes.size(); prefixIdx++)
		{
			FilterWitnesses(prefixIdx, *newInput);
			if (!witnessPerms[prefixIdx].empty()) numSubsumed++;
		}
		std::println("|X| = {} subsumes {}", X.size(), numSubsumed);
	}

	return X;
}

Permutation UnextendableSetBuilder2::RandomPerm(std::mt19937_64& gen) const
{
	if (!symmetric)
	{
		Permutation perm(n);
		std::iota(perm.begin(), perm.end(), 0);
		std::shuffle(perm.begin(), perm.end(), gen);
		return perm;
	}

	// Generate a random half-permutation using [0 ... n/2)
	uint8_t m = n / 2;
	Permutation halfperm(m);
	std::iota(halfperm.begin(), halfperm.end(), 0);
	std::shuffle(halfperm.begin(), halfperm.end(), gen);

	// Randomly swap some entries with their symmetric complement
	std::uniform_int_distribution<uint64_t> sigmaDist{ 0, (1ULL << m) - 1 };
	uint64_t sigma = sigmaDist(gen);
	for (uint8_t i = 0; i < m; i++)
		if ((sigma >> i) & 1ULL)
			halfperm[i] = n - 1 - halfperm[i];

	// Reconstruct the full permutation
	Permutation perm{ halfperm };
	perm.resize(n);
	for (uint8_t i = m; i < n; i++)
		perm[i] = n - 1 - perm[n - 1 - i];

	return perm;
}

void UnextendableSetBuilder2::InitializeWitnesses()
{
	// When X is empty, any permutation is a subsumption witness
	// Use the subSolver to choose initial perms, these will be 'close' to the identity perm
	// And help incentivise low window-width elements
	for (size_t prefixIdx = 0; prefixIdx < prefixes.size(); prefixIdx++)
	{
		subSolver.Solve({}, prefixOutputs[prefixIdx], maxWitnesses);
		auto [newIsComplete, newWitnesses] = subSolver.GetPerms();
		witnessPerms[prefixIdx] = newWitnesses;
	}
}

bool UnextendableSetBuilder2::IsSAT()
{
	// Run SAT solver
	Timer timer;
	Minisat::vec<Minisat::Lit> dummy;
	Minisat::lbool ret = satSolver.solveLimited(dummy);
	timer.Stop();

	// Update stats
	lastSatTime = timer.GetSeconds();
	totalSatTime += lastSatTime;

	return ret == Minisat::l_True;
}

void UnextendableSetBuilder2::RebuildWitnesses(bool forceUntangled)
{
	size_t numSubsumed = 0;
	size_t totalWitnesses = 0;
	for (size_t prefixIdx = 0; prefixIdx < prefixes.size(); prefixIdx++)
	{
		std::print("Rebuilding {:.3f}%...     \r", (double)prefixIdx / prefixes.size() * 100.0);

		// Skip non-subsumed prefixes
		if (witnessPerms[prefixIdx].empty()) continue;

		// Run the solver
		std::optional<Network> bNetwork = std::nullopt;
		if (forceUntangled) bNetwork = prefixes[prefixIdx];
		subSolver.Solve(X, prefixOutputs[prefixIdx], maxWitnesses, bNetwork);

		// Update global witnesses
		auto [newIsComplete, newWitnesses] = subSolver.GetPerms();
		witnessPerms[prefixIdx] = newWitnesses;
		isComplete[prefixIdx] = newIsComplete;

		// Update stats
		numSubsumed++;
		totalWitnesses += newWitnesses.size();
	}
	std::println("Rebuilding done	 average witnesses: {:.3f}", (double)totalWitnesses / numSubsumed);
}

void UnextendableSetBuilder2::FilterWitnesses(size_t prefixIdx, uint64_t lastAdded)
{
	const std::vector<uint64_t>& outputs = prefixOutputs[prefixIdx];

	// Remove permutations which are broken by the added element
	std::erase_if(witnessPerms[prefixIdx], [&](const Permutation& perm) {
		uint64_t permuted = perm(lastAdded);
		bool stillValid = std::binary_search(outputs.begin(), outputs.end(), permuted);
		return !stillValid;
		});

	// If the list is now empty, and it was known to be incomplete, find more entries
	if (witnessPerms[prefixIdx].empty() && !isComplete[prefixIdx])
	{
		subSolver.Solve(X, outputs, maxWitnesses);
		auto [newIsComplete, newWitnesses] = subSolver.GetPerms();
		witnessPerms[prefixIdx] = newWitnesses;
		isComplete[prefixIdx] = newIsComplete;
	}
}

std::vector<size_t> UnextendableSetBuilder2::ScoreElements()
{
#if COUNT_PERMS
	std::vector<size_t> scores(1ULL << n, 0);
	for (size_t prefixIdx = 0; prefixIdx < prefixes.size(); prefixIdx++)
	{
		if (witnessPerms[prefixIdx].empty()) continue;

		// Update counters
		const std::vector<uint64_t>& outputs = prefixOutputs[prefixIdx];
		for (const Permutation& witness : witnessPerms[prefixIdx])
		{
			Permutation invPerm{ witness };
			invPerm.Invert();
			for (uint64_t bx : outputs)
				scores[invPerm(bx)]++;
		}
	}
	return scores;
#else
	std::vector<size_t> scores(1ULL << n, 0);
	std::vector<size_t> lastIncrementedBy(1ULL << n, UINT64_MAX);
	for (size_t prefixIdx = 0; prefixIdx < prefixes.size(); prefixIdx++)
	{
		if (witnessPerms[prefixIdx].empty()) continue;

		// Update counters
		const std::vector<uint64_t>& outputs = prefixOutputs[prefixIdx];
		for (const Permutation& witness : witnessPerms[prefixIdx])
		{
			Permutation invPerm{ witness };
			invPerm.Invert();
			for (uint64_t bx : outputs)
			{
				uint64_t elem = invPerm(bx);
				if (lastIncrementedBy[elem] == prefixIdx) continue;
				scores[elem]++;
				lastIncrementedBy[elem] = prefixIdx;
			}
				
		}
	}
	return scores;
#endif
}

Network UnextendableSetBuilder2::ReconstructPostfix() const
{
	std::vector<bool> assignment(satSolver.nVars() + 1);
	for (int i = 0; i < satSolver.nVars(); i++)
		assignment[i + 1] = (satSolver.model[i] == Minisat::l_True);
	return generator.ParseAssignment(assignment);
}

std::optional<uint64_t> UnextendableSetBuilder2::ChooseNewInput(const std::vector<size_t>& scores) const
{
	Network postfix = ReconstructPostfix();

	std::optional<uint64_t> bestElement = std::nullopt;
	size_t bestScore = 0;
	for (uint64_t x = 0; x < (1ULL << n); x++)
	{
		if (scores[x] <= bestScore) continue;
		if (IsSorted(n, postfix(x))) continue;

		bestElement = x;
		bestScore = scores[x];
	}

	return bestElement;
}

void UnextendableSetBuilder2::AddNewInput(uint64_t x)
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