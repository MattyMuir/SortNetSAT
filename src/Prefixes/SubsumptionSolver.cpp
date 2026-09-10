#include "SubsumptionSolver.h"

#include <algorithm>
#include <bit>
#include <ranges>

#include <immintrin.h>

SubsumptionSolver::SubsumptionSolver(uint8_t n_, bool symmetric_, size_t maxSearches_)
	: n(n_), symmetric(symmetric_), maxSearches(maxSearches_ ? maxSearches_ : UINT64_MAX),
	aSig(n), bSig(n), initialDomains(n, (1ULL << n) - 1), refiner(n),
	perm(n, Unassigned), fastperm(n), patternCounts(n + 1), patternSources(n + 1)
{
	// Initialize pattern LUTs
	for (uint64_t bitCount = 0; bitCount <= n; bitCount++)
	{
		patternCounts[bitCount].resize(1ULL << n);
		patternSources[bitCount].resize(1ULL << n);
	}
}

SubsumptionResult SubsumptionSolver::Solve(const std::vector<uint64_t>& a_, const std::vector<uint64_t>& b_)
{
	// Reset all state
	ResetSearchState();

	// Assign parameters
	a = &a_;
	b = &b_;

	// Compute domains using signatures
	aSig.Construct(*a, false);
	bSig.Construct(*b, false);
	initialDomains = NetworkSignature::GetDomains(aSig, bSig);

	// Exploit symmetry in domains
	if (symmetric)
		for (uint8_t dst = 0; dst < n; dst++)
			initialDomains[dst] &= ReverseBits(initialDomains[n - 1 - dst]);

	// Refine initial domains
	bool validDomains = refiner.Refine(initialDomains);
	if (!validDomains) return DoesntSubsume;

	// Run the search
	try
	{
		bool result = Search(initialDomains);
		return result ? DoesSubsume : DoesntSubsume;
	}
	catch (const SearchLimitReached&)
	{
		return Unknown;
	}
}

size_t SubsumptionSolver::GetNumSearches() const
{
	return numSearches;
}

bool SubsumptionSolver::SourceUsed(uint8_t src) const
{
	return std::ranges::contains(perm, src);
}

bool SubsumptionSolver::DestUsed(uint8_t dst) const
{
	return perm[dst] != Unassigned;
}

void SubsumptionSolver::Assign(uint8_t src, uint8_t dst)
{
	perm[dst] = src;
	if (symmetric) perm[n - 1 - dst] = n - 1 - src;
}

void SubsumptionSolver::Unassign(uint8_t dst)
{
	perm[dst] = Unassigned;
	if (symmetric) perm[n - 1 - dst] = Unassigned;
}

uint8_t SubsumptionSolver::PickBranchDest(const std::vector<uint64_t>& domains)
{
	// Pick the destination location with the fewest remaining sources in its domain
	uint8_t bestDst = 0;
	size_t fewestSrc = n + 1;
	for (uint8_t dst = 0; dst < n; dst++)
	{
		if (DestUsed(dst)) continue;

		// Count unused sources for this dst
		size_t numSrc = 0;
		for (uint8_t src = 0; src < n; src++)
			if (((domains[src] >> dst) & 1) && !SourceUsed(src))
				numSrc++;

		// Update best
		if (numSrc < fewestSrc)
		{
			bestDst = dst;
			fewestSrc = numSrc;
		}
	}

	return bestDst;
}

uint64_t SubsumptionSolver::ReverseBits(uint64_t x) const
{
	x = __builtin_bswap64(x);
	x = ((x >> 1) & 0x5555555555555555ULL) | ((x & 0x5555555555555555ULL) << 1);
	x = ((x >> 2) & 0x3333333333333333ULL) | ((x & 0x3333333333333333ULL) << 2);
	x = ((x >> 4) & 0x0F0F0F0F0F0F0F0FULL) | ((x & 0x0F0F0F0F0F0F0F0FULL) << 4);
	x >>= (64 - n);
	return x;
}

void SubsumptionSolver::FilterDomains(std::vector<uint64_t>& domains, uint64_t ax, uint64_t bx) const
{
	for (uint8_t src = 0; src < n; src++)
	{
		// Takes the bit at position 'src' and broadcasts it to every bit
		uint64_t srcBitmask = ((~ax >> src) & 1ULL) - 1;

		// Clears the domain mask where the bits differ
		uint64_t equalMask = ~(srcBitmask ^ bx);
		domains[src] &= equalMask;
	}
}

uint64_t SubsumptionSolver::GetDestMask() const
{
	uint64_t dstMask = 0;
	for (uint8_t i = 0; i < n; i++)
		if (DestUsed(i))
			dstMask |= 1ULL << i;
	return dstMask;
}

void SubsumptionSolver::BuildLUT(uint64_t dstMask)
{
	// Insert elements of b into the LUTs
	for (uint64_t bx : *b)
	{
		uint64_t bitCount = std::popcount(bx);
		uint64_t pattern = _pext_u64(bx, dstMask);
		uint8_t& patternCount = patternCounts[bitCount][pattern];
		patternCount = (patternCount << 1) | 1; // Unary increment to avoid overflow
		patternSources[bitCount][pattern] = bx;
	}
}

void SubsumptionSolver::ResetLUT(uint64_t dstMask)
{
	// Reset the LUTs
	for (uint64_t bx : *b)
	{
		uint64_t bitCount = std::popcount(bx);
		patternCounts[bitCount][_pext_u64(bx, dstMask)] = 0;
	}
}

bool SubsumptionSolver::IsValidPermutation(std::vector<uint64_t>& domains, uint64_t dstMask)
{
	// Check if every element of 'a' has a matching pattern
	for (uint64_t ax : *a)
	{
		uint64_t bitCount = std::popcount(ax);
		uint64_t pattern = _pext_u64(fastperm(ax), dstMask);
		uint8_t patternCount = patternCounts[bitCount][pattern];

		if (!patternCount) return false;

		// If this element in 'a' has a unique remaining element in 'b' that it can map to, filter the domains
		if (patternCount == 1)
			FilterDomains(domains, ax, patternSources[bitCount][pattern]);
	}

	// Exploit symmetry in domains
	if (symmetric)
		for (uint8_t dst = 0; dst < n; dst++)
			domains[dst] &= ReverseBits(domains[n - 1 - dst]);

	// Refine domains
	return refiner.Refine(domains);
}

bool SubsumptionSolver::Search(const std::vector<uint64_t>& domains)
{
	// Base case: all destinations assigned
	if (!std::ranges::contains(perm, Unassigned)) return true;

	// Check if search limit has been reached
	if (++numSearches >= maxSearches)
		throw SearchLimitReached{};

	// Choose a branching destination
	uint8_t dst = PickBranchDest(domains);

	// Build the LUT
	uint64_t newDstMask = 1ULL << dst;
	if (symmetric) newDstMask |= 1ULL << (n - 1 - dst);
	uint64_t dstMask = GetDestMask() | newDstMask;
	BuildLUT(dstMask);

	// Determine which choices of src produce valid partial permutations
	std::vector<uint8_t> validSrcs;
	std::vector<std::vector<uint64_t>> allNewDomains;
	for (uint8_t src = 0; src < n; src++)
	{
		if (~domains[src] & (1ULL << dst)) continue;
		if (SourceUsed(src)) continue;

		// Make the assignment
		Assign(src, dst);
		fastperm.Assign(perm);

		// Prepare new domains for this assignment
		std::vector<uint64_t> newDomains{ domains };
		for (uint8_t otherSrc = 0; otherSrc < n; otherSrc++)
			newDomains[otherSrc] &= ~newDstMask;
		newDomains[src] = 1ULL << dst;
		if (symmetric) newDomains[n - 1 - src] = 1ULL << (n - 1 - dst);

		// Check if the assignment is valid
		if (IsValidPermutation(newDomains, dstMask))
		{
			validSrcs.push_back(src);
			allNewDomains.emplace_back(std::move(newDomains));
		}

		// Undo the assignment
		Unassign(dst);
	}

	// Reset the LUT
	ResetLUT(dstMask);

	// Recurse into valid assignments
	for (size_t assignIdx = 0; assignIdx < validSrcs.size(); assignIdx++)
	{
		// Make the assignment
		Assign(validSrcs[assignIdx], dst);

		if (Search(allNewDomains[assignIdx]))
			return true;

		// Undo the assignment
		Unassign(dst);
	}

	return false;
}

void SubsumptionSolver::ResetSearchState()
{
	std::fill(initialDomains.begin(), initialDomains.end(), (1ULL << n) - 1);
	std::fill(perm.begin(), perm.end(), Unassigned);
	numSearches = 0;
}