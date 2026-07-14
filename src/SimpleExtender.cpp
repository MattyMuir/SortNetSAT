#include "SimpleExtender.h"

#include <algorithm>

#include "Prefixes/prefixes.h"
#include "Prefixes/WindowMinimizer.h"
#include "FormulaGenerator.h"

SimpleExtender::SimpleExtender(uint8_t n_, uint8_t d_, bool symmetric_, const Network& prefix)
	: Extender(n_, d_, symmetric_, prefix), generator(n, dPost, symmetric)
{
	solver.verbosity = 0;
}

void SimpleExtender::SetParameters(InputOrder inputOrder_, double inputFraction_)
{
	inputOrder = inputOrder_;
	inputFraction = inputFraction_;
}

bool SimpleExtender::Extend()
{
	// Order and trim the prefix outputs
	OrderInputs();
	size_t numOutputs = prefixOutputs.size();
	prefixOutputs.resize(numOutputs * inputFraction);

	// Build CNF formula
	generator.Generate(prefixOutputs);

	// Load expression into solver
	LoadExpressionMinisat(solver, generator.GetExpression());

	// Simplify
	if (!solver.simplify()) return false;
	solver.failedLiteralCheck();

	// Solve
	Minisat::vec<Minisat::Lit> dummy;
	Minisat::lbool ret = solver.solveLimited(dummy);
	return (ret == Minisat::l_True);
}

Network SimpleExtender::GetNetwork() const
{
	// Convert witness to postfix
	std::vector<bool> assignment(solver.nVars() + 1);
	for (int i = 0; i < solver.nVars(); i++)
		assignment[i + 1] = (solver.model[i] == Minisat::l_True);
	Network postfix = generator.ParseAssignment(assignment);

	// Append postfix to prefix and untangle
	Network network = optimizedPrefix + postfix;
	network.Untangle();
	return network;
}

void SimpleExtender::OrderInputs()
{
	switch (inputOrder)
	{
	case OrderDefault: return;
	case OrderWindowWidth:
		SortByWindowWidth(n, prefixOutputs);
		return;
	case OrderRandom:
	{
		static std::mt19937_64 gen{ std::random_device{}() };
		std::shuffle(prefixOutputs.begin(), prefixOutputs.end(), gen);
	}
	}
}