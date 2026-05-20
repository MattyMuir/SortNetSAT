#include "minisatutil.h"

Minisat::vec<Minisat::Lit> ConvertClause(const Clause& clause)
{
	Minisat::vec<Minisat::Lit> mClause;
	for (Literal l : clause)
	{
		int var = std::abs(l) - 1;
		mClause.push((l > 0) ? Minisat::mkLit(var) : ~Minisat::mkLit(var));
	}
	return mClause;
}

void LoadExpressionMinisat(Minisat::Solver& solver, const Expression& expr)
{
	// Initialize variables
	for (int64_t i = 0; i < expr.NumVars(); i++)
		solver.newVar();

	// Add clauses
	for (const Clause& clause : expr.GetClauses())
		solver.addClause(ConvertClause(clause));
}