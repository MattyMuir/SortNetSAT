#pragma once
#include <minisat.h>

#include "clause.h"
#include "Expression.h"

Minisat::vec<Minisat::Lit> ConvertClause(const Clause& clause);
void LoadExpressionMinisat(Minisat::Solver& solver, const Expression& expr);