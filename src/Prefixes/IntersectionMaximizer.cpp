#include "IntersectionMaximizer.h"

#include <numeric>
#include <print>
#include <algorithm>

IntersectionMaximizer* poop;

IntersectionMaximizer::IntersectionMaximizer(uint8_t n_, bool symmetric_, std::mt19937_64::result_type seed)
	: n(n_), symmetric(symmetric_), gen(seed), bContains(1ULL << n) { poop = this; }

Permutation IntersectionMaximizer::Optimize(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b, size_t runs, size_t populationSize)
{
	for (uint64_t bx : b)
		bContains[bx] = true;

	InitializePopulation(a, populationSize);

	std::vector<size_t> idxs(populationSize);
	std::iota(idxs.begin(), idxs.end(), 0);
	for (size_t run = 0; run < runs; run++)
	{
		// Sort population by overlap
		std::ranges::sort(idxs, std::greater{}, [this](size_t idx) { return allOverlaps[idx]; });

		std::println("Best overlap: {}", allOverlaps[idxs[0]]);

		// The top 50% of prefixes have children, overwriting the lower half
		size_t halfSize = populationSize / 2;
		for (size_t i = 0; i < halfSize; i++)
			CreateChild(idxs[i + halfSize], idxs[i]);
	}

	// Extract best permutation
	auto bestPermSpan = GetPerm(idxs[0]);
	Permutation bestPerm{ bestPermSpan.begin(), bestPermSpan.end() };
	return bestPerm;
}

std::span<uint64_t> IntersectionMaximizer::GetA(size_t idx)
{
	uint64_t* start = allAs.data() + idx * aSize;
	return std::span<uint64_t>(start, aSize);
}

std::span<uint8_t> IntersectionMaximizer::GetPerm(size_t idx)
{
	uint8_t* start = allPerms.data() + idx * n;
	return std::span<uint8_t>(start, n);
}

size_t IntersectionMaximizer::Overlap(std::span<const uint64_t> a)
{
	size_t overlap = 0;
	for (uint64_t ax : a)
		if (bContains[ax])
			overlap++;
	return overlap;
}

std::pair<uint8_t, uint8_t> IntersectionMaximizer::RandomPair()
{
	std::uniform_int_distribution<uint32_t> aDist{ 0, n - 1U };
	std::uniform_int_distribution<uint32_t> bDist{ 0, n - 2U };

	uint8_t a = (uint8_t)aDist(gen);
	uint8_t b = (uint8_t)bDist(gen);

	return { a, (b == a) ? n - 1U : b };
}

IntersectionMaximizer::BitswapMask IntersectionMaximizer::GetBitswapMask(uint8_t i, uint8_t j) const
{
	if (i > j) std::swap(i, j);

	uint64_t leftMask = 1ULL << i;
	uint64_t rightMask = 1ULL << j;

	if (symmetric && i + j != n - 1)
	{
		leftMask |= 1ULL << (n - 1 - j);
		rightMask |= 1ULL << (n - 1 - i);
	}

	uint8_t shift = j - i;
	uint64_t stationaryMask = ~(leftMask | rightMask);

	return { stationaryMask, leftMask, rightMask, shift };
}

uint64_t IntersectionMaximizer::Bitswap(uint64_t x, const BitswapMask& mask)
{
	uint64_t ret = x = (x & mask.stationaryMask)
		| ((x & mask.leftMask) << mask.shift)
		| ((x & mask.rightMask) >> mask.shift);
	return ret;
}

void IntersectionMaximizer::SwapBits(std::span<uint64_t> dst, std::span<uint64_t> src, uint8_t i, uint8_t j)
{
	BitswapMask swapMask = GetBitswapMask(i, j);
	for (size_t writeIdx = 0; writeIdx < src.size(); writeIdx++)
		dst[writeIdx] = Bitswap(src[writeIdx], swapMask);
}

void IntersectionMaximizer::CreateChild(size_t dstIdx, size_t srcIdx)
{
	// Generate random channels to swap
	auto [i, j] = RandomPair();

	// Swap those channels in the permutation
	auto srcPerm = GetPerm(srcIdx);
	auto dstPerm = GetPerm(dstIdx);
	std::ranges::copy(srcPerm, dstPerm.begin());
	std::swap(dstPerm[i], dstPerm[j]);
	if (symmetric && i + j != n - 1)
		std::swap(dstPerm[n - 1 - j], dstPerm[n - 1 - i]);

	// Create output set with bits i and j swapped
	auto srcA = GetA(srcIdx);
	auto dstA = GetA(dstIdx);
	SwapBits(dstA, srcA, i, j);

	// Compute overlap
	allOverlaps[dstIdx] = Overlap(dstA);
}

void IntersectionMaximizer::InitializePopulation(const std::vector<uint64_t>& a, size_t populationSize)
{
	// Allocate memory
	aSize = a.size();
	allAs.resize(aSize * populationSize);
	allPerms.resize(n * populationSize);
	allOverlaps.resize(populationSize);

	// Insert initial a into the population
	std::copy(a.begin(), a.end(), GetA(0).begin());
	auto perm = GetPerm(0);
	std::iota(perm.begin(), perm.end(), 0);
	allOverlaps[0] = Overlap(a);

	for (size_t i = 1; i < populationSize; i++)
		CreateChild(i, i - 1);
}