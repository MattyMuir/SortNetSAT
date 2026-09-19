#pragma once
#include <sortnetutils.h>

#include "NetworkSignature.h"
#include "DomainRefiner.h"

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
	class SolutionLimitReached {};

public:
	SubsumptionSolver(uint8_t n_, bool symmetric_, size_t maxSearches_ = 0);

	SubsumptionResult Solve(const std::vector<uint64_t>& a_, const std::vector<uint64_t>& b_,
		size_t maxSolutions_ = 1, std::optional<Network> bNetwork_ = std::nullopt);
	size_t GetNumSearches() const;
	Permutation GetPerm() const;
	std::pair<bool, std::vector<Permutation>> GetPerms() const;

protected:
	// === Parameters ===
	uint8_t n;
	bool symmetric;
	size_t maxSearches;
	const std::vector<uint64_t>* a, * b;
	size_t maxSolutions;
	std::optional<Network> bNetwork;

	// === Domains ===
	NetworkSignature aSig, bSig;
	std::vector<uint64_t> initialDomains;				// domains[src] stores a mask of destinations that 'src' can be mapped to
	DomainRefiner refiner;

	// === Search State ===
	Permutation perm;									// Working permutation using the 'gather' convention
	FastPermutation fastperm;
	std::vector<std::vector<uint8_t>> patternCounts;	// Counts the number of each pattern, in unary (to avoid overflow)
	std::vector<std::vector<uint64_t>> patternSources;	// Stores a representative element in b for each pattern
	size_t numSearches = 0;
	bool isComplete;									// Specifies whether subPerms contains every possible valid perm
	std::vector<Permutation> subPerms;					// List of permutations which result in a valid subsumption

	bool SourceUsed(uint8_t src) const;
	bool DestUsed(uint8_t dst) const;
	void Assign(uint8_t src, uint8_t dst);
	void Unassign(uint8_t dst);

	uint8_t PickBranchDest(const std::vector<uint64_t>& domains);
	uint64_t ReverseBits(uint64_t x) const;
	void FilterDomains(std::vector<uint64_t>& domains, uint64_t ax, uint64_t bx) const;
	uint64_t GetDestMask() const;
	void BuildLUT(uint64_t dstMask);
	void ResetLUT(uint64_t dstMask);
	bool IsValidPermutation(std::vector<uint64_t>& domains, uint64_t dstMask);
	void Search(const std::vector<uint64_t>& domains);
	void ResetSearchState();
};