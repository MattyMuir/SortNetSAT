#include "PrefixGenerator.h"

#include <print>
#include <numeric>
#include <algorithm>

#include "prefixes.h"

PrefixGenerator::NetworkOutputs::NetworkOutputs(const Network& network_, uint8_t n)
	: network(network_)
{
	auto outputsVec = GetOutputs(network, n);
	outputs.insert_range(outputsVec);
}

PrefixGenerator::PrefixGenerator(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_)
{
	// Initialize alphabet
	for (uint8_t i = 0; i + 1 < n; i++)
		for (uint8_t j = i + 1; j < n; j++)
			if (!symmetric || i <= n - 1 - j)
				alphabet.push_back({ i, j });

	// Generate all layers
	allLayers.push_back(Network{});
	for (CE ce : alphabet)
	{
		size_t numPartial = allLayers.size();
		for (size_t li = 0; li < numPartial; li++)
			if (CanAddCE(allLayers[li], ce))
				allLayers.emplace_back(AddCE(allLayers[li], ce));
	}
}

std::vector<Network> PrefixGenerator::GeneratePrefixes()
{
	R.emplace_back(PrefixPar(n), n);
	for (uint8_t k = 1; k < d; k++)
	{
		Generate();
		Prune();
	}
	
	std::vector<Network> allPrefixes;
	for (const NetworkOutputs& network : R) allPrefixes.push_back(network.network);
	return allPrefixes;
}

void PrefixGenerator::Generate()
{
	N.clear();
	for (const NetworkOutputs& partialPrefix : R)
		for (const Network& layer : allLayers)
			N.emplace_back(AddLayer(partialPrefix, layer));
}

void PrefixGenerator::Prune()
{
	R.clear();
	for (const NetworkOutputs& prefix : N)
	{
		// Skip if 'prefix' is subsumed by another prefix already in R
		bool subsumed = false;
		for (const NetworkOutputs& other : R)
			if (Subsumes(other, prefix)) { subsumed = true; break; }
		if (subsumed) continue;

		// Remove prefixes in R subsumed by 'prefix'
		std::erase_if(R, [this, &prefix](const NetworkOutputs& other) { return Subsumes(prefix, other); });

		// Add to R
		R.push_back(prefix);
	}
}

bool PrefixGenerator::CanAddCE(const Network& layer, CE ce) const
{
	uint64_t usedMask = 0;
	for (auto [i, j] : layer)
		usedMask |= (1ULL << i) | (1ULL << j);

	uint64_t ceMask = (1ULL << ce.lo) | (1ULL << ce.hi);
	if (symmetric) ceMask |= (1ULL << (n - 1 - ce.hi)) | (1ULL << (n - 1 - ce.lo));

	return (usedMask & ceMask) == 0;
}

void PrefixGenerator::AddCEInplace(Network& layer, CE ce) const
{
	layer.push_back(ce);
	if (symmetric && ce.lo != n - 1 - ce.hi)
		layer.push_back({ (uint8_t)(n - 1 - ce.hi), (uint8_t)(n - 1 - ce.lo) });
}

Network PrefixGenerator::AddCE(const Network& layer, CE ce) const
{
	Network ret{ layer };
	AddCEInplace(ret, ce);
	return ret;
}

void PrefixGenerator::ReduceOutputs(std::unordered_set<uint64_t>& outputs, CE ce) const
{
	std::unordered_set<uint64_t> newOutputs;
	uint64_t ceMask = (1ULL << ce.lo) | (1ULL << ce.hi);
	uint64_t inversionMask = (1ULL << ce.lo);
	for (uint64_t output : outputs)
	{
		if ((output & ceMask) == inversionMask)
			output ^= ceMask;
		newOutputs.insert(output);
	}
	std::swap(outputs, newOutputs);
}

PrefixGenerator::NetworkOutputs PrefixGenerator::AddLayer(const NetworkOutputs& network, const Network& layer) const
{
	NetworkOutputs newNetwork;
	newNetwork.network = Concatenate(network.network, layer);

	newNetwork.outputs = network.outputs;
	for (CE ce : layer) ReduceOutputs(newNetwork.outputs, ce);

	return newNetwork;
}

bool PrefixGenerator::CannotSubsume1(const NetworkOutputs& a, const NetworkOutputs& b) const
{
	std::vector<size_t> aNum1s(n + 1), bNum1s(n + 1);
	for (uint64_t output : a.outputs) aNum1s[std::popcount(output)]++;
	for (uint64_t output : b.outputs) bNum1s[std::popcount(output)]++;

	for (uint8_t i = 0; i <= n; i++)
		if (aNum1s[i] > bNum1s[i]) return true;

	return false;
}

bool PrefixGenerator::IsPermutedSubset(const std::unordered_set<uint64_t>& a, const std::unordered_set<uint64_t>& b) const
{
	std::vector<uint8_t> perm(n);
	std::iota(perm.begin(), perm.end(), 0);

	do
	{
		bool isSubset = true;
		for (uint64_t ax : a)
		{
			// Permute the output
			uint64_t permutedOutput = 0;
			for (uint8_t i = 0; i < n; i++)
			{
				permutedOutput <<= 1;
				permutedOutput |= (ax >> perm[n - 1 - i]) & 1;
			}

			// Check if the permuted output is contained in b
			if (!b.contains(permutedOutput)) { isSubset = false; break; }
		}

		if (isSubset) return true;
	} while (std::next_permutation(perm.begin(), perm.end()));

	return false;
}

bool PrefixGenerator::Subsumes(const NetworkOutputs& a, const NetworkOutputs& b) const
{
	if (a.outputs.size() > b.outputs.size()) return false;
	if (CannotSubsume1(a, b)) return false;

	return IsPermutedSubset(a.outputs, b.outputs);
}