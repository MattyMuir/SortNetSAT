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
	expr.SaveToFile("tmp.cnf");

	// Solve formula
	KissatSolver solver;
	solver.LoadDimacs("tmp.cnf");
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
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;

	Network prefix = { {
			{0,6},{1,10},{2,15},{3,5},{4,9},{7,16},{8,13},{11,17},{12,14},
			{0,12},{1,4},{3,11},{5,17},{6,14},{7,8},{9,10},{13,16},
			{1,13},{2,7},{4,16},{6,9},{8,11},{10,15},
			{0,1},{2,3},{4,12},{5,13},{7,9},{8,10},{14,15},{16,17}
		} };
	uint8_t prefixDepth = 4;
	// ==================

	auto network = ExtendPrefix(n, d, prefix, prefixDepth, symmetric);
	if (network.has_value()) PrintNetwork(network.value());
	else std::println("Unextendable!");
}