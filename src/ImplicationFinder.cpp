#include "ImplicationFinder.h"

#include <print>

ImplicationFinder::ImplicationFinder(uint8_t n_, uint8_t d_, bool symmetric_, const std::vector<uint64_t>& L_)
	: n(n_), d(d_), symmetric(symmetric_), L(L_), generator(n_, d_, symmetric_), implied(1ULL << n, true), numImplied(1ULL << n) {}

std::vector<uint64_t> ImplicationFinder::FindImplications()
{
	// Erase trivial implications
	for (uint64_t x : L)
	{
		MarkUnimplied(x);
		if (symmetric) MarkUnimplied(Mirror(n, x));
	}
	for (uint8_t k = 0; k <= n; k++)
	{
		uint64_t sorted = ((1ULL << n) - 1) ^ ((1ULL << k) - 1);
		MarkUnimplied(sorted);
	}

	// Initialize formula generator
	generator.Generate();

	// Add inputs
	generator.AddInputs(L);

	// Load into SAT solver
	LoadExpressionMinisat(solver, generator.GetExpression());

	for (size_t netIdx = 1;; netIdx++)
	{
		// Generate a network that sorts everything in L
		Minisat::vec<Minisat::Lit> dummy;
		Minisat::lbool ret = solver.solveLimited(dummy);
		if (ret != Minisat::l_True) { /*std::println("UNSAT!");*/ break; };

		// Get assignment
		auto assignment = GetAssignment();
		if (allAssignments.contains(assignment)) std::println("Duplicate assignment!");
		allAssignments.insert(assignment);

		// Ban from future solves
		generator.BanAssignment(assignment);
		solver.addClause(ConvertClause(generator.GetExpression().GetClauses().back()));

		// Update implications
		Network postfix = generator.ParseAssignment(assignment);
		for (uint64_t r = 0; r < (1ULL << n); r++)
		{
			if (!implied[r]) continue;
			if (!IsSorted(n, postfix(r))) MarkUnimplied(r);
		}

		//std::println("Found network {}, non-trivial implications remaining: {}", netIdx, numImplied);
	}

	std::vector<uint64_t> impliedElements;
	for (uint64_t r = 0; r < (1ULL << n); r++)
		if (implied[r])
			impliedElements.push_back(r);
	return impliedElements;
}

std::vector<bool> ImplicationFinder::GetAssignment() const
{
	auto [compMin, compMax] = generator.CompVarRange();
	std::vector<bool> assignment(compMax - compMin + 1);
	for (Var v = compMin; v <= compMax; v++)
		assignment[v - compMin] = (solver.model[v - 1] == Minisat::l_True);
	return assignment;
}

void ImplicationFinder::MarkUnimplied(uint64_t x)
{
	if (implied[x]) numImplied--;
	implied[x] = false;
}