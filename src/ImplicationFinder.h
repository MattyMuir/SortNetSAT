#pragma once
#include <set>

#include <sortnetutils.h>

#include "FormulaGenerator.h"
#include "minisatutil.h"

class ImplicationFinder
{
public:
	ImplicationFinder(uint8_t n_, uint8_t d_, bool symmetric_, const std::vector<uint64_t>& L_);

	std::vector<uint64_t> FindImplications();

protected:
	uint8_t n, d;
	bool symmetric;
	std::vector<uint64_t> L;

	FormulaGenerator generator;
	Minisat::Solver solver;
	std::vector<bool> implied;
	size_t numImplied;
	std::set<std::vector<bool>> allAssignments;

	std::vector<bool> GetAssignment() const;
	void MarkUnimplied(uint64_t x);
};