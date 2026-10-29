#include "ImplicationFinder.h"

#include <print>
#include <random>

ImplicationFinder::ImplicationFinder(uint8_t n_, uint8_t d_, bool symmetric_, const std::vector<uint64_t>& L_)
	: n(n_), d(d_), symmetric(symmetric_), L(L_), implied(1ULL << n, true), numImplied(1ULL << n) {}

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

	// Pruning
	bool complete = PruneImplications(100);

	// Checking
	if (!complete) CheckImplications();

	std::vector<uint64_t> impliedElements;
	for (uint64_t r = 0; r < (1ULL << n); r++)
		if (implied[r])
			impliedElements.push_back(r);
	return impliedElements;
}

std::vector<bool> ImplicationFinder::GetAssignment(const Minisat::Solver& solver, size_t compMin, size_t compMax) const
{
	std::vector<bool> assignment(compMax - compMin + 1);
	for (Literal v = compMin; v <= compMax; v++)
		assignment[v - compMin] = (solver.model[v - 1] == Minisat::l_True);
	return assignment;
}

void ImplicationFinder::MarkUnimplied(uint64_t x)
{
	if (implied[x]) numImplied--;
	implied[x] = false;
}

bool ImplicationFinder::PruneImplications(size_t iter)
{
	// Initialize formula generator
	FormulaGenerator2 generator{ n, d, symmetric };
	generator.Initialize();
	for (uint64_t lx : L)
		generator.AddInput(lx);

	// Random number source
	std::mt19937_64 gen{};
	std::uniform_int_distribution<uint32_t> seedDist{ 1, 2147483646 };

	for (size_t i = 0; i < iter; i++)
	{
		// Load into SAT solver
		Minisat::Solver solver;
		solver.random_seed = seedDist(gen);
		solver.random_var_freq = 0.01;
		solver.rnd_init_act = true;
		LoadExpressionMinisat(solver, generator.GetExpression());

		// Generate a network that sorts everything in L
		Minisat::vec<Minisat::Lit> dummy;
		Minisat::lbool ret = solver.solveLimited(dummy);
		if (ret != Minisat::l_True) { std::println("UNSAT!"); return true; };

		// Get assignment
		auto [compMin, compMax] = generator.CompVarRange();
		auto assignment = GetAssignment(solver, compMin, compMax);
		if (allAssignments.contains(assignment)) std::println("Duplicate assignment!");
		allAssignments.insert(assignment);

		// Ban from future solves
		generator.BanAssignment(assignment);

		// Update implications
		Network postfix = generator.ParseAssignment(assignment);
		for (uint64_t r = 0; r < (1ULL << n); r++)
		{
			if (!implied[r]) continue;
			if (!IsSorted(n, postfix(r))) MarkUnimplied(r);
			if (!numImplied) return true;
		}

		std::println("Found network {}, non-trivial implications remaining: {}", i + 1, numImplied);
	}

	return false;
}

void ImplicationFinder::CheckImplications()
{
	for (uint64_t r = 0; r < (1ULL << n); r++)
	{
		if (!implied[r]) continue;

		std::print("Checking {:0>{}b}... ", r, n);

		// Initialize formula generator
		FormulaGenerator2 generator{ n, d, symmetric };
		generator.Initialize();
		for (uint64_t lx : L)
			generator.AddInput(lx);
		generator.AddInput(r, RequireUnsorted);

		// Load into SAT solver
		Minisat::Solver solver;
		LoadExpressionMinisat(solver, generator.GetExpression());

		// Run the SAT solve
		Minisat::vec<Minisat::Lit> dummy;
		bool isSat = (solver.solveLimited(dummy) == Minisat::l_True);
		std::println("{}", isSat ? "DISPUTED" : "VERIFIED");
		if (isSat) implied[r] = false;
	}
}