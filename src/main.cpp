#include <print>
#include <optional>

#include <KissatLib.h>

#include "prefixes.h"
#include "WindowMinimizer.h"
#include "FormulaGenerator.h"

std::optional<Network> ExtendPrefix(uint8_t n, uint8_t d, const Network& prefix, uint8_t prefixDepth, bool symmetric)
{
	// Optimize prefix
	WindowMinimizer minimizer{ n, symmetric };
	Network prefixOpt = minimizer.Optimize(prefix, 300, 300);
	auto prefixOutputs = PrefixOutputs(n, prefixOpt, true, symmetric);

	// Build CNF formula
	FormulaGenerator generator{ n, (uint8_t)(d - prefixDepth), symmetric };
	Expression expr = generator.Generate(prefixOutputs);

	// Solve formula
	KissatSolver solver;
	solver.LoadExpression(expr.GetMaxVar(), expr.GetClauses());
	KissatSolver::Result res = solver.Solve();
	if (res != KissatSolver::Satisfiable) return std::nullopt;

	// Convert witness to postfix
	std::vector<int> witness = solver.GetWitness();
	std::vector<bool> assignment(expr.GetMaxVar() + 1);
	for (int literal : witness)
		assignment[std::abs(literal)] = literal > 0;
	Network postfix = generator.ParseAssignment(assignment);

	// Append postfix to prefix and untangle
	Network network = Append(prefixOpt, postfix);
	Untangle(n, network);
	return network;
}

int main()
{
	// === Parameters ===
	uint8_t n = 10;
	uint8_t d = 7;
	bool symmetric = true;

	Network prefix = PrefixPar(n);
	uint8_t prefixDepth = 1;
	// ==================

	auto network = ExtendPrefix(n, d, prefix, prefixDepth, symmetric);
	if (network.has_value()) PrintNetwork(network.value());
	else std::println("Unextendable!");
}