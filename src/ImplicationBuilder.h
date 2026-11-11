#pragma once
#include <sortnetutils.h>

#include "minisatutil.h"

class ImplicationBuilder
{
public:
	ImplicationBuilder(uint8_t n_, uint8_t d_, bool symmetric_);

	std::vector<uint64_t> Build(size_t size);

protected:
	uint8_t n, d;
	bool symmetric;

	std::vector<uint64_t> L;

	std::vector<bool> GetAssignment(const Minisat::Solver& solver, size_t compMin, size_t compMax) const;
	std::vector<Network> GetSATNetworks(size_t maxNum);
	uint64_t ChooseNewElement(const std::vector<size_t>& elemCounts) const;
};