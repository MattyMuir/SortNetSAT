#pragma once
#include <random>
#include <span>

#include <sortnetutils.h>

class WindowMinimizer
{
protected:
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
	std::mt19937_64 gen;

	size_t numOutputs;
	std::vector<uint64_t> allOutputs;
	std::vector<uint8_t> allPerms;
	std::vector<uint64_t> allWindowWidths;

	std::span<uint64_t> GetOutputs(size_t idx);
	std::span<uint8_t> GetPerm(size_t idx);

	void InitializePopulation(const Network& initialPrefix, size_t populationSize);

	std::pair<uint8_t, uint8_t> RandomPair();
	BitswapMask GetBitswapMask(uint8_t i, uint8_t j) const;
	static uint64_t Bitswap(uint64_t x, const BitswapMask& mask);
	void SwapBits(std::span<uint64_t> dst, std::span<uint64_t> src, uint8_t i, uint8_t j);
	void CreateChild(size_t dstIdx, size_t srcIdx);
};