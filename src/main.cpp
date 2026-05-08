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
	WindowMinimizer minimizer{ n, symmetric };
	Network prefixOpt = minimizer.Optimize(prefix, 300, 300);
	auto prefixOutputs = PrefixOutputs(n, prefixOpt, true, symmetric);

	// Build CNF formula
	FormulaGenerator generator{ n, (uint8_t)(d - prefixDepth), symmetric };
	Expression expr = generator.Generate(prefixOutputs);

	// Load expression into solver
	Minisat::Solver solver;
	solver.verbosity = 1;
	LoadExpressionMinisat(solver, expr);

	// Simplify
	if (!solver.simplify())
		return std::nullopt;

	// Solve
	Minisat::vec<Minisat::Lit> dummy;
	Minisat::lbool ret = solver.solveLimited(dummy);
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
	uint8_t n = 17;
	uint8_t d = 10;
	bool symmetric = false;
	Network prefix = {{
			{1,2},{3,4},{5,6},{7,8},{9,10},{11,12},{13,14},{15,16},
			{1,3},{2,4},{5,7},{6,8},{9,11},{10,12},{13,15},{14,16},
			{1,5},{2,6},{3,7},{4,8},{9,13},{10,14},{11,15},{12,16}
		}};
	uint8_t prefixDepth = 3;
	// ==================

	auto networkOpt = ExtendPrefixMinisat(n, d, prefix, prefixDepth, symmetric);

	if (networkOpt.has_value()) PrintNetwork(networkOpt.value());
	else std::println("Unextendable!");
}