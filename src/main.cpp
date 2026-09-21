#include <iostream>
#include <print>
#include <ranges>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <set>

#include "Timer.h"
#include "Prefixes/prefixes.h"
#include "Prefixes/PrefixGenerator.h"
#include "IncrementalExtender.h"
#include "BulkChecker.h"
#include "Prefixes/WindowMinimizer.h"
#include "Prefixes/IntersectionMaximizer.h"
#include "minisatutil.h"
#include "UnextendableSetBuilder.h"
#include "UnextendableSetBuilder2.h"
#include "SetBuilderChecker.h"

std::vector<uint64_t> GetUnextendableSubset()
{
	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;
	Network prefix = ParseNetwork("[(0,13),(4,17),(1,2),(15,16),(3,14),(5,6),(11,12),(7,10),(8,9),(0,11),(6,17),(1,16),(2,7),(10,15),(3,8),(9,14),(4,5),(12,13),(0,4),(13,17),(1,2),(15,16),(3,7),(10,14),(5,12),(6,11),(8,9)]");
	// ==================

	// Run the IncrementalExtender
	IncrementalExtender extender{ n, d, symmetric, prefix, true };
	extender.SetParameters(1);
	TIMER(t);
	bool extendable = extender.Extend();
	std::println();
	STOP_LOG(t);

	// Log result
	std::println("{}", extendable ? "SAT" : "UNSAT");
	if (extendable) return {};

	return extender.GetIncludedInputs();
}

std::vector<uint64_t> LoadOutputSet(const std::string& filepath)
{
	std::ifstream file{ filepath };

	std::string line;
	std::vector<uint64_t> outputs;
	while (std::getline(file, line))
		outputs.push_back(std::stoull(line));

	return outputs;
}

void SaveOutputSet(const std::string& filepath, const std::vector<uint64_t>& set)
{
	std::ofstream file{ filepath };
	for (uint64_t x : set)
		file << x << '\n';
}

void UnextendableSubsetPruning(const std::vector<uint64_t>& unextendableSet)
{
	// === Parameters ===
	uint8_t n = 18;
	bool symmetric = true;
	// ==================

	// Load all prefixes
	auto allPrefixes = ParsePrefixFile("C:\\Users\\matty\\source\\repos\\SortNetSAT\\prefixes\\18_3_sym.txt");

	TIMER(t);
	SubsumptionSolver solver{ n, symmetric };
	size_t numSubsumed = 0;
	for (size_t prefixIdx = 1; prefixIdx < allPrefixes.size(); prefixIdx++)
	{
		const Network& otherPrefix = allPrefixes[prefixIdx];
		std::vector<uint64_t> otherOutputs = FactoredOutputSet{ otherPrefix, n }.ToVector();

		auto result = solver.Solve(unextendableSet, otherOutputs, false, otherPrefix);
		if (result == DoesSubsume)
			numSubsumed++;

		// Log progress
		if (prefixIdx % 100 == 0)
			std::print("{}/{}   \r", numSubsumed, prefixIdx);
	}
	std::println();
	STOP_LOG(t);
}

std::vector<std::pair<uint64_t, size_t>> GetScores(const std::vector<Network>& prefixes, uint8_t n, bool symmetric)
{
	std::vector<std::pair<uint64_t, size_t>> scores(1ULL << n);
	for (uint64_t x = 0; x < (1ULL << n); x++)
		scores[x] = { x, 0 };

	for (size_t prefixIdx = 0; prefixIdx < prefixes.size(); prefixIdx++)
	{
		const Network& prefix = prefixes[prefixIdx];
		std::vector<uint64_t> outputs = FactoredOutputSet{ prefix, n }.ToVector();
		for (uint64_t output : outputs)
			scores[output].second++;

		if (prefixIdx % 1000) std::print("{:.3f}%    \r", (double)prefixIdx / prefixes.size() * 100.0);
	}

	// Sort elements by score
	std::ranges::sort(scores, std::greater{}, [](auto elem) { return elem.second; });

	// Remove sorted vectors and those with smaller mirrors
	std::erase_if(scores, [n](auto elem) { return IsSorted(n, elem.first); });
	if (symmetric)
		std::erase_if(scores, [n](auto elem) { return HasSmallerMirror(n, elem.first); });

	return scores;
}

std::vector<uint64_t> NaiveScoring()
{
	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 7;
	bool symmetric = true;
	auto allPrefixes = ParsePrefixFile("C:\\Users\\matty\\source\\repos\\SortNetSAT\\prefixes\\18_3_sym.txt");
	// ==================

	// Get element scores
	auto scores = GetScores(allPrefixes, n, symmetric);

	// Initialize formula generator and solver
	FormulaGenerator generator{ n, d, symmetric };
	Minisat::Solver satSolver;
	generator.Generate();
	LoadExpressionMinisat(satSolver, generator.GetExpression());

	std::vector<uint64_t> X;
	for (;;)
	{
		// Solve SAT
		Minisat::vec<Minisat::Lit> dummy;
		Timer timer;
		Minisat::lbool ret = satSolver.solveLimited(dummy);
		timer.Stop();
		bool isSat = (ret == Minisat::l_True);
		std::println("{}: |X| = {} took {}s", isSat ? "SAT" : "UNSAT", X.size(), timer.GetSeconds());
		if (!isSat) return X;
		if (X.size() > 50) return X;

		// Reconstruct postfix
		std::vector<bool> assignment(satSolver.nVars() + 1);
		for (int i = 0; i < satSolver.nVars(); i++)
			assignment[i + 1] = (satSolver.model[i] == Minisat::l_True);
		Network postfix = generator.ParseAssignment(assignment);

		// Choose highest scoring element
		for (auto [x, _] : scores)
		{
			if (IsSorted(n, postfix(x))) continue;

			// Add to X
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

			break;
		}
	}
}

int main()
{	
	auto unextendable = NaiveScoring();
	SaveOutputSet("naive.txt", unextendable);
	UnextendableSubsetPruning(unextendable);
}