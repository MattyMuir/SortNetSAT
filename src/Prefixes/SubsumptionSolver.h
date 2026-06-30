#pragma once
#include <sortnetutils.h>

enum SubsumptionResult
{
	DoesntSubsume,
	DoesSubsume,
	Unknown
};

class SubsumptionSolver
{
protected:
	// Use '63' as the placeholder so that shifting by this value is not UB
	static constexpr uint8_t Unassigned = 63;

	class SearchLimitReached {};

public:
	SubsumptionSolver(uint8_t n_, bool symmetric_);

	SubsumptionResult Solve(const std::vector<uint64_t>& a_, const std::vector<uint64_t>& b_, size_t maxSearches_ = 0);

protected:
	// === Parameters ===
	uint8_t n;
	bool symmetric;
	const std::vector<uint64_t>*a, *b;
	size_t maxSearches;

	// === Search State ===
	// domains[src] stores a mask of destination bits that 'src' can be mapped to under permutation
	std::vector<uint64_t> initialDomains;
	// Working permutation using the 'gather' convention
	std::vector<uint8_t> perm;
	// Counts the number of each pattern, in unary (to avoid overflow)
	std::vector<std::vector<uint8_t>> patternCounts;
	// Stores a representative element in b for each pattern
	std::vector<std::vector<uint64_t>> patternSources;
	size_t numSearches = 0;

	uint8_t PickBranchPosition(const std::vector<uint64_t>& domains);
	bool SourceUsed(uint8_t src);
	bool DestUsed(uint8_t dst);
	void Assign(uint8_t src, uint8_t dst);
	void Unassign(uint8_t dst);
	uint64_t ReverseBits(uint64_t x) const;
	void FilterDomains(std::vector<uint64_t>& domains, uint64_t ax, uint64_t bx);
	bool IsValidPermutation(std::vector<uint64_t>& domains);
	bool Search(const std::vector<uint64_t>& domains);
	void ResetSearchState();
};