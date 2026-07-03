#include "PrefixGenerator.h"

#include <print>
#include <numeric>
#include <algorithm>
#include <ranges>
#include <random>

#include "prefixes.h"
#include "SubsumptionSolver.h"
#include "../Timer.h"

PrefixGenerator::PrefixGenerator(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_),
	allLayers(GetAllLayers(n, symmetric)),
	solver(n, symmetric)
{}

size_t solvedByPrecheck = 0;
size_t solvedByRestricted = 0;
size_t solvedByFull = 0;
std::vector<Network> PrefixGenerator::GeneratePrefixes()
{
	// Add the empty layer to the initial set of prefixes
	prefixes.emplace_back(NetworkMeta{ Network{} });

	for (uint8_t k = 0; k < d; k++)
	{
		Generate(k == 0);
		
		Timer t;
		Prune(100);
		Prune();
		t.Stop();
		timings.push_back(t.GetSeconds());

		std::println("Number of {}-layer prefixes: {}", k + 1, prefixes.size());
	}

	std::println("Precheck:   {}", solvedByPrecheck);
	std::println("Restricted: {}", solvedByRestricted);
	std::println("Full:       {}", solvedByFull);

	// Strip the metadata from prefixes
	std::vector<Network> noMeta;
	for (auto& networkMeta : prefixes)
		noMeta.emplace_back(std::move(networkMeta.prefix));
	return noMeta;
}

std::vector<double> PrefixGenerator::GetTimings()
{
	return timings;
}

void PrefixGenerator::Generate(bool isFirst)
{
	size_t numPartial = prefixes.size();
	for (size_t i = 0; i < numPartial; i++)
	{
		for (const Network& layer : allLayers)
		{
			// For the first layer, skip unfull layers
			if (isFirst && layer.size() != n / 2)
				continue;

			// Compute the new extended prefix and its outputs
			Network extended = Concatenate(prefixes[i].prefix, layer);
			FactoredOutputSet outputs{ extended, n };

			// Skip redundant networks
			if (outputs.IsRedundant()) continue;

			// Compute the signature and add this prefix
			prefixes.emplace_back(extended);
		}
	}

	SortByOutputs();
}

void PrefixGenerator::SortByOutputs()
{
	static std::mt19937_64 gen{ std::random_device{}() };
	std::shuffle(prefixes.begin(), prefixes.end(), gen);

	std::vector<std::pair<NetworkMeta, size_t>> sizedPrefixes;
	for (const NetworkMeta& network : prefixes)
	{
		size_t numOutputs = FactoredOutputSet{ network.prefix, n }.Size();
		sizedPrefixes.emplace_back(network, numOutputs);
	}

	std::sort(sizedPrefixes.begin(), sizedPrefixes.end(), [](const auto& a, const auto& b) {
		return a.second > b.second;
		});

	std::vector<NetworkMeta> newPrefixes;
	for (const auto& [prefix, _] : sizedPrefixes)
		newPrefixes.push_back(prefix);

	std::swap(prefixes, newPrefixes);
}

bool PrefixGenerator::IsAlreadyRepresented(const NetworkMeta& prefix, const std::vector<size_t>& representatives, size_t maxSearches)
{
	for (size_t repIdx : representatives)
		if (Subsumes(prefixes[repIdx], prefix, maxSearches))
			return true;
	return false;
}

void PrefixGenerator::CacheOutputs(NetworkMeta& network)
{
	network.outputs = FactoredOutputSet{ network.prefix, n }.ToVector();
	network.signature = ComputeSignature(*network.outputs);
}

void PrefixGenerator::ClearCache(NetworkMeta& network)
{
	network.outputs.reset();
	network.signature.reset();
}

void PrefixGenerator::Prune(size_t maxSearches)
{
	std::vector<size_t> representatives;

	for (size_t prefixIdx = 0; prefixIdx < prefixes.size(); prefixIdx++)
	{
		// Log progress
		if (prefixIdx % 75 == 0)
			std::print("Progress: {:>6.3f}% R: {}    \r", (double)prefixIdx / prefixes.size() * 100.0, representatives.size());

		// Get the current prefix and cache its outputs
		NetworkMeta& prefix = prefixes[prefixIdx];
		CacheOutputs(prefix);
		
		// Skip this prefix if its already represented
		if (IsAlreadyRepresented(prefix, representatives, maxSearches))
		{
			ClearCache(prefix);
			continue;
		}

		// Remove representatives that are subsumed by the new prefix (and clear their caches)
		std::erase_if(representatives, [&, this](size_t repIdx)
			{
				bool subsumes = Subsumes(prefix, prefixes[repIdx], maxSearches);
				if (subsumes) ClearCache(prefixes[repIdx]);
				return subsumes;
			});

		// Add the new prefix as a representative
		representatives.push_back(prefixIdx);
	}

	// Clear all caches
	for (NetworkMeta& network : prefixes)
		ClearCache(network);

	// Update global prefixes variable
	std::vector<NetworkMeta> newPrefixes;
	for (size_t repIdx : representatives)
		newPrefixes.emplace_back(std::move(prefixes[repIdx]));
	std::swap(prefixes, newPrefixes);
}

std::vector<std::vector<uint64_t>> PrefixGenerator::SplitIntoClusters(const std::vector<uint64_t>& outputs) const
{
	std::vector<std::vector<uint64_t>> clusters(n + 1);
	for (uint64_t output : outputs)
		clusters[std::popcount(output)].push_back(output);
	return clusters;
}

std::vector<size_t> PrefixGenerator::T5Signature(const std::vector<uint64_t>& outputs) const
{
	std::vector<size_t> t5(n);
	for (uint64_t output : outputs)
		for (uint8_t bi = 0; bi < n; bi++)
			t5[bi] += (output >> bi) & 1ULL;
	std::sort(t5.begin(), t5.end());
	return t5;
}

std::vector<size_t> PrefixGenerator::T6Signature(const std::vector<uint64_t>& outputs) const
{
	std::vector<size_t> t6(n);
	for (uint64_t output : outputs)
	{
		uint64_t popcount = std::popcount(output);
		for (uint8_t bi = 0; bi < n; bi++)
			if ((output >> bi) & 1)
				t6[bi] += popcount;
	}
	std::sort(t6.begin(), t6.end());
	return t6;
}

PrefixGenerator::NetworkSignature PrefixGenerator::ComputeSignature(const std::vector<uint64_t>& outputs) const
{
	// === T1 Signature ===
	size_t numOutputs = outputs.size();
	size_t popcountSum = 0;
	for (uint64_t output : outputs)
		popcountSum += std::popcount(output);

	// === T2 Signature ===
	auto clusters = SplitIntoClusters(outputs);
	std::vector<size_t> t2(n + 1);
	for (uint8_t popcount = 0; popcount <= n; popcount++)
		t2[popcount] = clusters[popcount].size();

	// === T3 Signature ===
	std::vector<uint64_t> zeroPos(n + 1), onePos(n + 1);
	for (uint64_t output : outputs)
	{
		uint64_t popcount = std::popcount(output);
		zeroPos[popcount] |= (~output) & ((1ULL << n) - 1);
		onePos[popcount] |= output;
	}
	std::vector<size_t> t3z(n + 1), t3o(n + 1);
	for (uint8_t popcount = 0; popcount <= n; popcount++)
	{
		t3z[popcount] = std::popcount(zeroPos[popcount]);
		t3o[popcount] = std::popcount(onePos[popcount]);
	}

	// === T5 Signature ===
	std::vector<size_t> t5 = T5Signature(outputs);
	std::vector<std::vector<size_t>> t5c;
	for (const auto& cluster : clusters)
		t5c.push_back(T5Signature(cluster));

	// === T6 Signature ===
	std::vector<size_t> t6 = T6Signature(outputs);

	return NetworkSignature{ numOutputs, popcountSum, t2, t3z, t3o, t5, t6, t5c };
}

bool PrefixGenerator::TPrecheck(const std::vector<size_t>& at, const std::vector<size_t>& bt, size_t aSize, size_t bSize) const
{
	for (uint8_t i = 0; i < n; i++)
		if (at[i] > bt[i] || bt[i] - at[i] > bSize - aSize)
			return true;
	return false;
}

SubsumptionResult PrefixGenerator::SignaturePrecheck(const NetworkSignature& aSig, const NetworkSignature& bSig) const
{
	// === T1 Signature ===
	if (aSig.numOutputs > bSig.numOutputs)
		return DoesntSubsume;

	// === T3 Signature ===
	for (uint8_t popcount = 0; popcount <= n; popcount++)
		if (aSig.t3z[popcount] > bSig.t3z[popcount] || aSig.t3o[popcount] > bSig.t3o[popcount])
			return DoesntSubsume;

	// === T5 Signature ===
	if (TPrecheck(aSig.t5, bSig.t5, aSig.numOutputs, bSig.numOutputs))
		return DoesntSubsume;
	for (uint8_t popcount = 0; popcount <= n; popcount++)
		if (TPrecheck(aSig.t5c[popcount], bSig.t5c[popcount], aSig.t2[popcount], bSig.t2[popcount]))
			return DoesntSubsume;

	// === T6 Signature ===
	if (TPrecheck(aSig.t6, bSig.t6, aSig.popcountSum, bSig.popcountSum))
		return DoesntSubsume;

	return Unknown;
}

bool PrefixGenerator::Subsumes(const NetworkMeta& a, const NetworkMeta& b, size_t maxSearches)
{
	// Run the signature-based prechecks
	SubsumptionResult precheck = SignaturePrecheck(*a.signature, *b.signature);
	if (precheck != Unknown)
	{
		solvedByPrecheck++;
		return (precheck == DoesSubsume);
	}

	// Run a full backtracking subsumption test
	(maxSearches ? solvedByRestricted : solvedByFull)++;
	SubsumptionResult result = solver.Solve(*a.outputs, *b.outputs, maxSearches);
	return result == DoesSubsume;
}