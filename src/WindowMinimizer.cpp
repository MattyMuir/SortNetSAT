#include "WindowMinimizer.h"

#include <print>
#include <random>
#include <numeric>
#include <set>

#include "prefixes.h"

WindowMinimizer::WindowMinimizer(uint8_t n_, bool symmetric_)
	: n(n_), symmetric(symmetric_) {}

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

		// Add children to global population
		globalPopulation.clear();

		// Keep only 'populationSize' best distinct permutations
		std::set<std::vector<uint8_t>> inPopulation;
		for (const auto& [windowWidth, perm] : nextGeneration)
		{
			if (globalPopulation.size() >= populationSize) break;

			if (inPopulation.contains(perm.perm)) continue;
			inPopulation.insert(perm.perm);

			globalPopulation.emplace(windowWidth, perm);
		}
	}

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
	uint8_t shift = ch1 - ch0;
	uint64_t ch0Mask = 1ULL << ch0;
	uint64_t ch1Mask = 1ULL << ch1;

	uint64_t stationaryMask = (1ULL << n) - 1;
	stationaryMask ^= ch0Mask | ch1Mask;
	uint64_t leftMask = ch0Mask;
	uint64_t rightMask = ch1Mask;

	if (symmetric && ch0 + ch1 != n - 1)
	{
		uint8_t ch0Sym = n - 1 - ch1;
		uint8_t ch1Sym = n - 1 - ch0;
		ch0Mask = 1ULL << ch0Sym;
		ch1Mask = 1ULL << ch1Sym;
		stationaryMask ^= ch0Mask | ch1Mask;
		leftMask |= ch0Mask;
		rightMask |= ch1Mask;
	}

	return { stationaryMask, leftMask, rightMask, shift };
}

uint64_t WindowMinimizer::Bitswap(uint64_t x, const BitswapMask& mask)
{
	uint64_t ret = x = (x & mask.stationaryMask)
		| ((x & mask.leftMask) << mask.shift)
		| ((x & mask.rightMask) >> mask.shift);
	return ret;
}

// Distinct random pair in [0, max]
static inline std::pair<uint8_t, uint8_t> RandomPair(uint8_t max)
{
	static std::mt19937_64 gen{ std::random_device{}() };
	std::uniform_int_distribution<uint32_t> aDist{ 0, max };
	std::uniform_int_distribution<uint32_t> bDist{ 0, max - 1U };

	uint8_t a = (uint8_t)aDist(gen);
	uint8_t b = (uint8_t)bDist(gen);

	return { a, (b == a) ? max : b };
}

WindowMinimizer::Permutation WindowMinimizer::RandomSwap(const Permutation& perm)
{
	// Generate random channels to swap
	auto [ch0, ch1] = RandomPair(n - 1);

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