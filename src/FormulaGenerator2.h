#pragma once
#include <unordered_map>

#include <sortnetutils.h>

#include "Expression.h"
#include "VariableFamily.h"
#include "clause.h"

enum InputRequirement
{
	RequireSorted,
	RequireUnsorted
};

class FormulaGenerator2
{
public:
	FormulaGenerator2(uint8_t n_, uint8_t d_, bool symmetric_);

	void Initialize();
	VariableFamily AddInput(uint64_t input, InputRequirement requirement = RequireSorted);

	const Expression& GetExpression() const;
	std::pair<Literal, Literal> CompVarRange() const; // Returns the range [gMin, gMax] of comparator variables
	Network ParseAssignment(const std::vector<bool>& assignment) const; // Expects the comparator variables in the range [gMin, gMax]

protected:
	uint8_t n, d;
	bool symmetric;
	std::vector<CE> allCEs, allSymCEs;

	Expression expr;
	Literal trueVar, compMin, compMax;
	VariableFamily comps, used, oneDown, oneUp;
	std::unordered_map<Clause, Literal, ClauseHasher> clauseVars;

	bool ShareChannel(CE aCE, CE bCE) const;
	Literal GetClauseVar(const Clause& clause);
};