#pragma once
#include <cstdint>
#include <map>

#include "Network.h"

class WindowMinimizer
{
protected:
	struct Permutation
	{
		std::vector<uint8_t> perm;
		std::vector<uint64_t> outputs;
	};
	using Population = std::multimap<uint64_t, Permutation>;

	struct BitswapMask
	{
		uint64_t stationaryMask, leftMask, rightMask;
		uint8_t shift;
	};

public:
	WindowMinimizer(uint8_t n_, bool symmetric_);

	Network Optimize(const Network& initialPrefix, size_t runs, size_t populationSize);

protected:
	uint8_t n;
	bool symmetric;
	Population globalPopulation;

	void AddToPopulation(Population& population, const Permutation& perm) const;
	void AddToPopulation(Population& population, const Network& prefix) const;
	BitswapMask GetBitswapMask(uint8_t ch0, uint8_t ch1) const;
	static uint64_t Bitswap(uint64_t x, const BitswapMask& mask);
	Permutation RandomSwap(const Permutation& perm);
};