#include "WindowMinimizer.h"

#include <print>
#include <numeric>
#include <set>

#include "prefixes.h"

WindowMinimizer::WindowMinimizer(uint8_t n_, bool symmetric_, std::mt19937_64::result_type seed)
	: n(n_), symmetric(symmetric_), gen(seed) {}

Network WindowMinimizer::Optimize(const Network& initialPrefix, size_t runs, size_t populationSize)
{
	AddToPopulation(globalPopulation, initialPrefix);

	for (size_t runIdx = 0; runIdx < runs; runIdx++)
	{
		std::print("Best: {:<10}\r", globalPopulation.begin()->first);

		// Every member of the population has one child
		std::multimap<uint64_t, Permutation> nextGeneration{ globalPopulation };
		for (const auto& [_, perm] : globalPopulation)
			AddToPopulation(nextGeneration, RandomSwap(perm));

		std::swap(globalPopulation, nextGeneration);

		// Keep only 'populationSize' best permutations
		if (globalPopulation.size() <= populationSize) continue;
		auto cutoff = globalPopulation.begin();
		std::advance(cutoff, populationSize);
		globalPopulation.erase(cutoff, globalPopulation.end());
	}
	std::println();

	Network optPrefix{ initialPrefix };
	Permute(optPrefix, globalPopulation.begin()->second.perm);
	return optPrefix;
}

void WindowMinimizer::AddToPopulation(Population& population, const Permutation& perm) const
{
	uint64_t windowWidth = WindowWidth(n, perm.outputs, symmetric);
	population.emplace(windowWidth, perm);
}

void WindowMinimizer::AddToPopulation(Population& population, const Network& prefix) const
{
	Permutation perm{
		std::vector<uint8_t>(n),
		PrefixOutputs(n, prefix, false, false)
	};
	std::iota(perm.perm.begin(), perm.perm.end(), 0);
	AddToPopulation(population, perm);
}

WindowMinimizer::BitswapMask WindowMinimizer::GetBitswapMask(uint8_t ch0, uint8_t ch1) const
{
	if (ch0 > ch1) std::swap(ch0, ch1);

	uint64_t leftMask = 1ULL << ch0;
	uint64_t rightMask = 1ULL << ch1;

	if (symmetric && ch0 + ch1 != n - 1)
	{
		leftMask |= 1ULL << (n - 1 - ch1);
		rightMask |= 1ULL << (n - 1 - ch0);
	}

	uint8_t shift = ch1 - ch0;
	uint64_t stationaryMask = ~(leftMask | rightMask);

	return { stationaryMask, leftMask, rightMask, shift };
}

uint64_t WindowMinimizer::Bitswap(uint64_t x, const BitswapMask& mask)
{
	uint64_t ret = x = (x & mask.stationaryMask)
		| ((x & mask.leftMask) << mask.shift)
		| ((x & mask.rightMask) >> mask.shift);
	return ret;
}

// Distinct random pair in [0, max)
std::pair<uint8_t, uint8_t> WindowMinimizer::RandomPair(uint8_t max)
{
	std::uniform_int_distribution<uint32_t> aDist{ 0, max - 1U };
	std::uniform_int_distribution<uint32_t> bDist{ 0, max - 2U };

	uint8_t a = (uint8_t)aDist(gen);
	uint8_t b = (uint8_t)bDist(gen);

	return { a, (b == a) ? max - 1U : b };
}

WindowMinimizer::Permutation WindowMinimizer::RandomSwap(const Permutation& perm)
{
	// Generate random channels to swap
	auto [ch0, ch1] = RandomPair(n);

	// Swap those channels in the permutation
	Permutation ret{ perm };
	std::swap(ret.perm[ch0], ret.perm[ch1]);
	if (symmetric && ch0 + ch1 != n - 1)
		std::swap(ret.perm[n - 1 - ch1], ret.perm[n - 1 - ch0]);

	BitswapMask swapMask = GetBitswapMask(ch0, ch1);

	// Swap those channels in the outputs
	for (uint64_t& output : ret.outputs)
		output = Bitswap(output, swapMask);

	return ret;
}