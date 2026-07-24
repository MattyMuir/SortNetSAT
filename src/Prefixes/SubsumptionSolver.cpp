#include "SubsumptionSolver.h"

#include <algorithm>
#include <bit>

#include <immintrin.h>

SubsumptionSolver::SubsumptionSolver(uint8_t n_, bool symmetric_, size_t maxSearches_)
	: n(n_), symmetric(symmetric_), maxSearches(maxSearches_ ? maxSearches_ : UINT64_MAX),
	initialDomains(n, (uint64_t)-1), perm(n, Unassigned), patternCounts(n + 1), patternSources(n + 1)
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

	// Compute column sums
	std::vector<uint32_t> aColumnSum(n), bColumnSum(n);
	for (uint64_t ax : *a)
		for (uint8_t bi = 0; bi < n; bi++)
			aColumnSum[bi] += (ax >> bi) & 1;
	for (uint64_t bx : *b)
		for (uint8_t bi = 0; bi < n; bi++)
			bColumnSum[bi] += (bx >> bi) & 1;

	// Compute domains using the column sums
	for (uint8_t src = 0; src < n; src++)
	{
		for (uint8_t dst = 0; dst < n; dst++)
		{
			if (aColumnSum[src] > bColumnSum[dst]
				|| a->size() - aColumnSum[src] > b->size() - bColumnSum[dst])
			{
				initialDomains[src] &= ~(1ULL << dst);
				if (symmetric) initialDomains[n - 1 - src] &= ~(1ULL << (n - 1 - dst));
			}
		}
	}

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

uint8_t SubsumptionSolver::PickBranchPosition(const std::vector<uint64_t>& domains)
{
	// Pick the source location with the fewest remaining destinations in its domain
	uint8_t bestSrc = 0;
	size_t fewestDest = n + 1;
	for (uint8_t src = 0; src < n; src++)
	{
		if (SourceUsed(src)) continue;

		// Count unused destinations for this src
		size_t numDest = 0;
		for (uint8_t dst = 0; dst < n; dst++)
			if (((domains[src] >> dst) & 1) && !DestUsed(dst))
				numDest++;

		// Update best
		if (numDest < fewestDest)
		{
			bestSrc = src;
			fewestDest = numDest;
		}
	}

	return bestSrc;
}

bool SubsumptionSolver::SourceUsed(uint8_t src)
{
	return std::ranges::contains(perm, src);
}

bool SubsumptionSolver::DestUsed(uint8_t dst)
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

uint64_t SubsumptionSolver::ReverseBits(uint64_t x) const
{
	x = __builtin_bswap64(x);
	x = ((x >> 1) & 0x5555555555555555ULL) | ((x & 0x5555555555555555ULL) << 1);
	x = ((x >> 2) & 0x3333333333333333ULL) | ((x & 0x3333333333333333ULL) << 2);
	x = ((x >> 4) & 0x0F0F0F0F0F0F0F0FULL) | ((x & 0x0F0F0F0F0F0F0F0FULL) << 4);
	x >>= (64 - n);
	return x;
}

void SubsumptionSolver::FilterDomains(std::vector<uint64_t>& domains, uint64_t ax, uint64_t bx)
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

bool SubsumptionSolver::IsValidPermutation(std::vector<uint64_t>& domains)
{
	// Get a mask of all destination bits assigned so far
	uint64_t dstMask = 0;
	for (uint8_t i = 0; i < n; i++)
		if (DestUsed(i))
			dstMask |= 1ULL << i;

	// Insert elements of b into the LUTs
	for (uint64_t bx : *b)
	{
		uint64_t bitCount = std::popcount(bx);
		uint64_t pattern = _pext_u64(bx, dstMask);
		uint8_t& patternCount = patternCounts[bitCount][pattern];
		patternCount = (patternCount << 1) | 1; // Unary increment to avoid overflow
		patternSources[bitCount][pattern] = bx;
	}

	// Check if every element of 'a' has a matching pattern
	bool isValid = true;
	for (uint64_t ax : *a)
	{
		uint64_t bitCount = std::popcount(ax);
		uint64_t pattern = _pext_u64(perm(ax), dstMask);
		uint8_t patternCount = patternCounts[bitCount][pattern];

		if (!patternCount)
		{
			isValid = false;
			break;
		}

		// If this element in 'a' has a unique remaining element in 'b' that it can map to, filter the domains
		if (patternCount == 1)
			FilterDomains(domains, ax, patternSources[bitCount][pattern]);
	}

	// Exploit symmetry in domains
	if (symmetric)
		for (uint8_t dst = 0; dst < n; dst++)
			domains[dst] &= ReverseBits(domains[n - 1 - dst]);

	// Reset the LUTs
	for (uint64_t bx : *b)
	{
		uint64_t bitCount = std::popcount(bx);
		patternCounts[bitCount][_pext_u64(bx, dstMask)] = 0;
	}

	return isValid;
}

bool SubsumptionSolver::Search(const std::vector<uint64_t>& domains)
{
	// Base case: all positions assigned
	if (!std::ranges::contains(perm, Unassigned)) return true;

	// Check if search limit has been reached
	if (++numSearches >= maxSearches)
		throw SearchLimitReached{};

	uint8_t src = PickBranchPosition(domains);

	for (uint8_t dst = 0; dst < n; dst++)
	{
		if (~domains[src] & (1ULL << dst)) continue;
		if (DestUsed(dst)) continue;

		// Make the assignment
		Assign(src, dst);

		// Check if the assignment is valid
		std::vector<uint64_t> newDomains{ domains };
		bool isValid = IsValidPermutation(newDomains);
		if (!isValid) { Unassign(dst); continue; }

		// Recurse
		if (Search(newDomains))
			return true;

		// This assignment failed, undo it
		Unassign(dst);
	}

	return false;
}

void SubsumptionSolver::ResetSearchState()
{
	std::fill(initialDomains.begin(), initialDomains.end(), (uint64_t)-1);
	std::fill(perm.begin(), perm.end(), Unassigned);
	numSearches = 0;
}