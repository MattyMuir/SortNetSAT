#include <print>
#include <optional>

#include <KissatLib.h>
#include <minisat.h>

#include "prefixes.h"
#include "WindowMinimizer.h"
#include "FormulaGenerator.h"

std::optional<Network> ExtendPrefixKissat(uint8_t n, uint8_t d, const Network& prefix, uint8_t prefixDepth, bool symmetric)
{
	// Optimize prefix
	WindowMinimizer minimizer{ n, symmetric };
	Network prefixOpt = minimizer.Optimize(prefix, 300, 300);
	auto prefixOutputs = PrefixOutputs(n, prefixOpt, true, symmetric);

	// Build CNF formula
	FormulaGenerator generator{ n, (uint8_t)(d - prefixDepth), symmetric };
	Expression expr = generator.Generate(prefixOutputs);
	expr.SanityCheck();

	// Solve formula
	KissatSolver solver;
	solver.LoadExpression(expr.NumVars(), expr.GetClauses());
	KissatSolver::Result res = solver.Solve();
	if (res != KissatSolver::Satisfiable) return std::nullopt;

	// Convert witness to postfix
	std::vector<int> witness = solver.GetWitness();
	std::vector<bool> assignment(expr.NumVars() + 1);
	for (int literal : witness)
		assignment[std::abs(literal)] = literal > 0;
	Network postfix = generator.ParseAssignment(assignment);

	// Append postfix to prefix and untangle
	Network network = Append(prefixOpt, postfix);
	Untangle(n, network);
	return network;
}

void LoadExpressionMinisat(Minisat::Solver& solver, const Expression& expr)
{
	// Initialize variables
	for (int64_t i = 0; i < expr.NumVars(); i++)
		solver.newVar();

	const auto& clauses = expr.GetClauses();
	for (const Clause& clause : clauses)
	{
		// Convert clause to minisat clause
		Minisat::vec<Minisat::Lit> mClause;
		for (Literal l : clause)
		{
			int var = std::abs(l) - 1;
			mClause.push((l > 0) ? Minisat::mkLit(var) : ~Minisat::mkLit(var));
		}

		// Add clause to solver
		solver.addClause(mClause);
	}
}

std::optional<Network> ExtendPrefixMinisat(uint8_t n, uint8_t d, const Network& prefix, uint8_t prefixDepth, bool symmetric)
{
	// Optimize prefix
	WindowMinimizer minimizer{ n, symmetric, 1234 };
	Network prefixOpt = minimizer.Optimize(prefix, 300, 300);
	PrintNetwork(prefixOpt);
	auto prefixOutputs = PrefixOutputs(n, prefixOpt, true, symmetric);

	// Build CNF formula
	FormulaGenerator generator{ n, (uint8_t)(d - prefixDepth), symmetric };
	Expression expr = generator.Generate(prefixOutputs);
	expr.SaveToFile("net18.cnf");

	// Load expression into solver
	Minisat::Solver solver;
	solver.verbosity = 1;
	LoadExpressionMinisat(solver, expr);

	// Simplify
	if (!solver.simplify())
		return std::nullopt;

	// Failed literal check
	bool fCheckDone = false;
	while (!fCheckDone)
	{
		int freeBefore = solver.nFreeVars();
		std::println("Free: {}", freeBefore);
		solver.failedLiteralCheck();
		fCheckDone = solver.nFreeVars() == freeBefore;
	}
	std::println("Free: {}", solver.nFreeVars());

	// Solve
	Minisat::vec<Minisat::Lit> dummy;
	Minisat::lbool ret = solver.solveLimited(dummy);
	solver.printStats();
	if (ret != Minisat::l_True) return std::nullopt;

	// Convert witness to postfix
	std::vector<bool> assignment(expr.NumVars() + 1);
	for (int i = 0; i < solver.nVars(); i++)
		assignment[i + 1] = (solver.model[i] == Minisat::l_True);
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
	Network prefix = {{
			{0,6},{1,10},{2,15},{3,5},{4,9},{7,16},{8,13},{11,17},{12,14},
			{0,12},{1,4},{3,11},{5,17},{6,14},{7,8},{9,10},{13,16}
		}};
	uint8_t prefixDepth = 2;
	// ==================

	auto networkOpt = ExtendPrefixMinisat(n, d, prefix, prefixDepth, symmetric);

	if (networkOpt.has_value()) PrintNetwork(networkOpt.value());
	else std::println("Unextendable!");
}