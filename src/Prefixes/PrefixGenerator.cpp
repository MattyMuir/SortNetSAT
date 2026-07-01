#include "PrefixGenerator.h"

#include <print>
#include <numeric>
#include <algorithm>
#include <ranges>

#include "prefixes.h"
#include "SubsumptionSolver.h"
#include "../Timer.h"

PrefixGenerator::PrefixGenerator(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_),
	allLayers(GetAllLayers(n, symmetric)),
	solver(n, symmetric)
{}

std::vector<Network> PrefixGenerator::GeneratePrefixes()
{
	// Add the empty layer to the initial set of prefixes
	prefixes.emplace_back(NetworkMeta{ Network{} });

	for (uint8_t k = 0; k < d; k++)
	{
		Generate(k == 0);
		Prune(10);
		Prune();
		std::println("Number of {}-layer prefixes: {}", k + 1, prefixes.size());
	}

	// Strip the metadata from prefixes
	std::vector<Network> noMeta;
	for (auto& networkMeta : prefixes)
		noMeta.emplace_back(std::move(networkMeta.prefix));
	return noMeta;
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
}

bool PrefixGenerator::IsAlreadyRepresented(const NetworkMeta& prefix, const std::vector<size_t> representatives, size_t maxSearches)
{
	for (size_t repIdx : representatives)
		if (Subsumes(prefixes[repIdx], prefix, maxSearches))
			return true;
	return false;
}

void PrefixGenerator::Prune(size_t maxSearches)
{
	std::vector<size_t> representatives;
	for (size_t prefixIdx = 0; prefixIdx < prefixes.size(); prefixIdx++)
	{
		// Log progress
		if (prefixIdx % 75 == 0)
			std::print("Progress: {:>6.3f}% R: {}\r", (double)prefixIdx / prefixes.size() * 100.0, representatives.size());

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

SubsumptionResult PrefixGenerator::SignaturePrecheck(const NetworkSignature& aSig, const NetworkSignature& bSig) const
{
	// === T1 Signature ===
	if (aSig.numOutputs > bSig.numOutputs)
		return DoesntSubsume;

	// === T2 Signature ===
	for (uint8_t popcount = 0; popcount <= n; popcount++)
		if (aSig.t2[popcount] > bSig.t2[popcount])
			return DoesntSubsume;

	// === T3 Signature ===
	for (uint8_t popcount = 0; popcount <= n; popcount++)
		if (aSig.t3z[popcount] > bSig.t3z[popcount] || aSig.t3o[popcount] > bSig.t3o[popcount])
			return DoesntSubsume;

	return Unknown;
}

PrefixGenerator::NetworkSignature PrefixGenerator::ComputeSignature(const std::vector<uint64_t>& outputs) const
{
	// === T1 Signature ===
	size_t numOutputs = outputs.size();

	// === T2 Signature ===
	std::vector<size_t> t2(n + 1);
	for (uint64_t output : outputs)
		t2[std::popcount(output)]++;

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

	return NetworkSignature{ numOutputs, t2, t3z, t3o };
}

void PrefixGenerator::CacheOutputs(NetworkMeta& network)
{
	network.outputs = FactoredOutputSet{ network.prefix, n }.ToVector();
	network.signature = ComputeSignature(*network.outputs);
}

void PrefixGenerator::ClearCache(NetworkMeta & network)
{
	network.outputs.reset();
	network.signature.reset();
}

bool PrefixGenerator::Subsumes(const NetworkMeta& a, const NetworkMeta& b, size_t maxSearches)
{
	// Run the signature-based prechecks
	SubsumptionResult precheck = SignaturePrecheck(*a.signature, *b.signature);
	if (precheck != Unknown) return (precheck == DoesSubsume);

	// Run a full backtracking subsumption test
	SubsumptionResult result = solver.Solve(*a.outputs, *b.outputs, maxSearches);
	return result == DoesSubsume;
}