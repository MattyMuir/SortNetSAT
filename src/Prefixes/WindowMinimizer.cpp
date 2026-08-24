#include "WindowMinimizer.h"

#include <numeric>
#include <print>

#include "prefixes.h"

WindowMinimizer::WindowMinimizer(uint8_t n_, bool symmetric_, std::mt19937_64::result_type seed)
	: n(n_), symmetric(symmetric_), gen(seed)
{}

Network WindowMinimizer::Optimize(const Network& initialPrefix_, size_t runs, size_t populationSize)
{
	initialPrefix = initialPrefix_;
	InitializePopulation(populationSize);

	std::vector<size_t> idxs(populationSize);
	std::ranges::iota(idxs, 0);
	for (size_t run = 0; run < runs; run++)
	{
		// Sort population by window width
		std::ranges::sort(idxs, {}, [this](size_t idx) { return allWindowWidths[idx]; });

		// The top 50% of prefixes have children, overwriting the lower half
		size_t halfSize = populationSize / 2;
		for (size_t i = 0; i < halfSize; i++)
			CreateChild(idxs[i + halfSize], idxs[i]);
	}

	// Extract best permutation
	auto bestPermSpan = GetPerm(idxs[0]);
	Permutation bestPerm{ bestPermSpan.begin(), bestPermSpan.end() };

	// Apply this permutation to the initial prefix
	Network bestPrefix{ initialPrefix };
	bestPrefix.Permute(bestPerm);
	bestPrefix.Untangle();
	return bestPrefix;
}

std::span<uint64_t> WindowMinimizer::GetOutputs(size_t idx)
{
	uint64_t* start = allOutputs.data() + idx * numOutputs;
	return std::span<uint64_t>(start, numOutputs);
}

std::span<uint8_t> WindowMinimizer::GetPerm(size_t idx)
{
	uint8_t* start = allPerms.data() + idx * n;
	return std::span<uint8_t>(start, n);
}

void WindowMinimizer::InitializePopulation(size_t populationSize)
{
	// Get original outputs
	initialOutputs = FactoredOutputSet{ initialPrefix, n }.ToVector();
	numOutputs = initialOutputs.size();

	// Allocate memory
	allOutputs.resize(numOutputs * populationSize);
	allPerms.resize(n * populationSize);
	allWindowWidths.resize(populationSize);

	// Insert the initial outputs into the population
	std::ranges::copy(initialOutputs, GetOutputs(0).begin());
	std::ranges::iota(GetPerm(0), 0);
	allWindowWidths[0] = WindowWidth(n, initialOutputs, symmetric);

	for (size_t i = 1; i < populationSize; i++)
		CreateChild(i, i - 1);
}

std::pair<uint8_t, uint8_t> WindowMinimizer::RandomPair()
{
	std::uniform_int_distribution<uint32_t> aDist{ 0, n - 1U };
	std::uniform_int_distribution<uint32_t> bDist{ 0, n - 2U };

	uint8_t a = (uint8_t)aDist(gen);
	uint8_t b = (uint8_t)bDist(gen);

	return { a, (b == a) ? n - 1U : b };
}

void WindowMinimizer::CreateChild(size_t dstIdx, size_t srcIdx)
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
	Permutation outputPerm = initialPrefix.GetOutputPermutation(Permutation{ dstPerm.begin(), dstPerm.end() });
	auto dstOutputs = GetOutputs(dstIdx);
	for (size_t i = 0; i < initialOutputs.size(); i++)
		dstOutputs[i] = outputPerm(initialOutputs[i]);

	// Compute window width
	allWindowWidths[dstIdx] = WindowWidth(n, dstOutputs, symmetric);
}