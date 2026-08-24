#pragma once
#include <span>
#include <random>

#include <sortnetutils.h>

class IntersectionMaximizer
{
protected:
	struct BitswapMask
	{
		uint64_t stationaryMask, leftMask, rightMask;
		uint8_t shift;
	};

public:
	IntersectionMaximizer(uint8_t n_, bool symmetric_, std::mt19937_64::result_type seed = std::random_device{}());

	Permutation Optimize(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b, size_t runs, size_t populationSize);

protected:
	uint8_t n;
	bool symmetric;
	std::mt19937_64 gen;

	size_t aSize;
	std::vector<bool> bContains;
	std::vector<uint64_t> allAs;
	std::vector<uint8_t> allPerms;
	std::vector<uint64_t> allOverlaps;

	std::span<uint64_t> GetA(size_t idx);
	std::span<uint8_t> GetPerm(size_t idx);
	size_t Overlap(std::span<const uint64_t> a);

	std::pair<uint8_t, uint8_t> RandomPair();
	BitswapMask GetBitswapMask(uint8_t i, uint8_t j) const;
	static uint64_t Bitswap(uint64_t x, const BitswapMask& mask);
	void SwapBits(std::span<uint64_t> dst, std::span<uint64_t> src, uint8_t i, uint8_t j);
	void CreateChild(size_t dstIdx, size_t srcIdx);

	void InitializePopulation(const std::vector<uint64_t>& a, size_t populationSize);
};