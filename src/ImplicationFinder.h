#pragma once
#include <set>

#include <sortnetutils.h>

#include "FormulaGenerator2.h"
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

	std::vector<bool> implied;
	size_t numImplied;
	std::set<std::vector<bool>> allAssignments;

	std::vector<bool> GetAssignment(const Minisat::Solver& solver, size_t compMin, size_t compMax) const;
	void MarkUnimplied(uint64_t x);

	// Returns 'true' if pruning is exact
	bool PruneImplications(size_t iter);
	void DoPrune(const Network& postfix);
	void CheckImplications();
};