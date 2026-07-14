#pragma once
#include <cstdint>
#include <map>
#include <random>

#include <sortnetutils.h>

class WindowMinimizer
{
protected:
	struct PermOutputs
	{
		Permutation perm;
		std::vector<uint64_t> outputs;
	};
	using Population = std::multimap<uint64_t, PermOutputs>;

	struct BitswapMask
	{
		uint64_t stationaryMask, leftMask, rightMask;
		uint8_t shift;
	};

public:
	WindowMinimizer(uint8_t n_, bool symmetric_, std::mt19937_64::result_type seed = std::random_device{}());

	Network Optimize(const Network& initialPrefix, size_t runs, size_t populationSize);

protected:
	uint8_t n;
	bool symmetric;
	Population globalPopulation;
	std::mt19937_64 gen;

	void AddToPopulation(Population& population, const PermOutputs& perm) const;
	void AddToPopulation(Population& population, const Network& prefix) const;
	BitswapMask GetBitswapMask(uint8_t ch0, uint8_t ch1) const;
	static uint64_t Bitswap(uint64_t x, const BitswapMask& mask);
	std::pair<uint8_t, uint8_t> RandomPair(uint8_t max);
	PermOutputs RandomSwap(const PermOutputs& perm);
};