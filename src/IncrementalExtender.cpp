#include "IncrementalExtender.h"

#include <print>
#include <algorithm>
#include <ranges>

#include "prefixes.h"

IncrementalExtender::IncrementalExtender(uint8_t n_, uint8_t d_, bool symmetric_, const Network& prefix)
	: Extender(n_, d_, symmetric_, prefix), generator(n, dPost, symmetric)
{
	solver.verbosity = 0;
}

void IncrementalExtender::SetParameters(size_t maxAddNum_)
{
	maxAddNum = maxAddNum_;
}

bool IncrementalExtender::Extend()
{
	std::println("Total Outputs: {}", prefixOutputs.size());

	// Generate core network structural requirements and definitions
	generator.Generate();
	LoadExpressionMinisat(solver, generator.GetExpression());

	// Run main incremental loop
	excludedInputs = prefixOutputs;
	for (;;)
	{
		// Solve the CNF formula
		Minisat::vec<Minisat::Lit> dummy;
		Minisat::lbool ret = solver.solveLimited(dummy);
		if (ret != Minisat::l_True) return false;

		// Get the indices of all failing inputs
		std::vector<size_t> failing = GetFailingInputs();
		LogProgress(failing.size());

		// Check if all inputs were sorted
		if (failing.empty()) return true;

		// Choose a subet of failing inputs to include
		auto newTestors = ChooseNewTestors(failing);

		// Add the new inputs to the solver and move from excluded -> included
		IncludeNewInputs(newTestors);
	}
	std::println();
}

Network IncrementalExtender::GetNetwork() const
{
	// Append postfix to prefix and untangle
	Network postfix = ReconstructPostfix();
	Network network = Concatenate(optimizedPrefix, postfix);
	Untangle(network, n);
	return network;
}

Network IncrementalExtender::ReconstructPostfix() const
{
	std::vector<bool> assignment(solver.nVars());
	for (int i = 0; i < solver.nVars(); i++)
		assignment[i + 1] = (solver.model[i] == Minisat::l_True);
	return generator.ParseAssignment(assignment);
}

std::vector<size_t> IncrementalExtender::GetFailingInputs() const
{
	// Reconstruct the postfix
	Network postfix = ReconstructPostfix();

	// Collect all failing inputs
	std::vector<size_t> failing;
	for (size_t i = 0; i < excludedInputs.size(); i++)
	{
		uint64_t input = excludedInputs[i];
		uint64_t output = RunNetwork(postfix, input);
		if (!IsSorted(n, output)) failing.push_back(i);
	}

	return failing;
}

void IncrementalExtender::LogProgress(size_t numFailing) const
{
	double passing = 1.0 - (double)numFailing / prefixOutputs.size();
	std::print("{} inputs, width {}, {:.3f}% passing\r", includedInputs.size(), WindowWidth(n, includedInputs, symmetric), passing * 100.0);
}

size_t IncrementalExtender::ComputeSimilarity(uint64_t input) const
{
	uint64_t similarity = 0;
	for (uint64_t other : includedInputs)
	{
		uint64_t matching = ~(input ^ other);
		similarity += std::popcount(matching);
	}
	return similarity;
}

std::vector<size_t> IncrementalExtender::ChooseNewTestors(const std::vector<size_t>& failing) const
{
	struct InputCost
	{
		uint64_t windowWidth, similarity;
		auto operator<=>(const InputCost& other) const = default;
	};

	// Score all failing outputs based on window size and similarity
	std::vector<std::pair<size_t, InputCost>> scoredFailing;
	for (size_t failingIdx : failing)
	{
		uint64_t failingInput = excludedInputs[failingIdx];
		uint64_t windowWidth = WindowWidth(n, failingInput);
		uint64_t similarity = ComputeSimilarity(failingInput);
		scoredFailing.emplace_back(failingIdx, InputCost{ windowWidth, similarity });
	}

	// Sort to find failing inputs with the lowest cost to add
	std::sort(scoredFailing.begin(), scoredFailing.end(), [](auto a, auto b) { return a.second < b.second; });

	// Remove costs and limit to 'maxAddNum' new testors
	std::vector<size_t> newTestors;
	for (auto [failingIdx, cost] : scoredFailing | std::views::take(maxAddNum))
		newTestors.push_back(failingIdx);

	// Sort the indices (necessary for removal)
	std::sort(newTestors.begin(), newTestors.end());
	return newTestors;
}

void IncrementalExtender::IncludeNewInputs(const std::vector<size_t>& newTestors)
{
	const Expression& expr = generator.GetExpression();

	// Add new inputs to the formula
	size_t numClausesBefore = expr.NumClauses();
	Var varsBefore = expr.NumVars();
	for (size_t newInputIdx : newTestors)
		generator.AddInput(excludedInputs[newInputIdx]);

	// Add new variables to the solver
	size_t numVarsAdded = expr.NumVars() - varsBefore;
	for (size_t i = 0; i < numVarsAdded; i++)
		solver.newVar();

	// Add new clauses to the solver
	const auto& allClauses = expr.GetClauses();
	for (size_t i = numClausesBefore; i < allClauses.size(); i++)
		solver.addClause(ConvertClause(allClauses[i]));

	// Include the new inputs and remove from excludedInputs
	for (size_t i : newTestors)
		includedInputs.push_back(excludedInputs[i]);
	for (size_t i : newTestors | std::views::reverse)
		excludedInputs.erase(excludedInputs.begin() + i);
}