#include "PrefixGenerator.h"

#include <print>
#include <numeric>
#include <algorithm>

#include "prefixes.h"
#include "SubsumptionSolver.h"
#include "../Timer.h"

void PrefixGenerator::NetworkMeta::CacheOutputs(uint8_t n)
{
	outputs = FactoredOutputSet{ prefix, n }.ToVector();
}

PrefixGenerator::PrefixGenerator(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_),
	allLayers(GetAllLayers(n, symmetric)),
	solver(n, symmetric)
{}

std::vector<Network> PrefixGenerator::GeneratePrefixes()
{
	// Add the empty layer to the initial set of prefixes
	Network empty{};
	FactoredOutputSet emptyOutputs{ empty, n };
	prefixes.emplace_back(empty, ComputeSignature(emptyOutputs));

	for (uint8_t k = 0; k < d; k++)
	{
		Generate(k == 0);
		std::println("Pruning {}-layer prefixes...", k + 1);
		Prune(100);
		Prune();
	}

	// Strip the metadata from prefixes
	std::vector<Network> noMeta;
	for (const auto& networkMeta : prefixes)
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
			prefixes.emplace_back(extended, ComputeSignature(outputs));
		}
	}
}

void PrefixGenerator::Prune(size_t maxSearches)
{
	std::vector<NetworkMeta> pruned;
	for (size_t prefixIdx = 0; prefixIdx < prefixes.size(); prefixIdx++)
	{
		if (prefixIdx % 75 == 0)
			std::print("Progress: {:.3f}%\r", (double)prefixIdx / prefixes.size() * 100.0);
		const NetworkMeta& prefix = prefixes[prefixIdx];

		// Skip if 'prefix' is subsumed by another prefix already in pruned
		bool subsumed = false;
		for (const NetworkMeta& other : pruned)
			if (Subsumes(other, prefix, maxSearches)) { subsumed = true; break; }
		if (subsumed) continue;

		// Remove prefixes in pruned subsumed by 'prefix'
		std::erase_if(pruned, [this, &prefix, maxSearches](const NetworkMeta& other)
			{
				return Subsumes(prefix, other, maxSearches);
			});

		// Add to pruned
		pruned.push_back(prefix);

		// Prefixes in the pruned set have their outputs cached
		pruned.back().CacheOutputs(n);
	}

	std::swap(pruned, prefixes);
}

SubsumptionResult PrefixGenerator::SignaturePrecheck(const NetworkSignature& aSig, const NetworkSignature& bSig)
{
	if (aSig.numOutputs > bSig.numOutputs) return DoesntSubsume;

	return Unknown;
}

PrefixGenerator::NetworkSignature PrefixGenerator::ComputeSignature(const FactoredOutputSet& outputs)
{
	size_t numOutputs = outputs.Size();
	return NetworkSignature{ numOutputs };
}

bool PrefixGenerator::Subsumes(const NetworkMeta& a, const NetworkMeta& b, size_t maxSearches)
{
	// Run the signature-based prechecks
	SubsumptionResult precheck = SignaturePrecheck(a.signature, b.signature);
	if (precheck != Unknown) return (precheck == DoesSubsume);

	// Compute the outputs if not cached
	std::vector<uint64_t> aOutputs, bOutputs;
	if (!a.outputs) aOutputs = FactoredOutputSet{ a.prefix, n }.ToVector();
	if (!b.outputs) bOutputs = FactoredOutputSet{ b.prefix, n }.ToVector();

	// Run a full backtracking subsumption test
	SubsumptionResult result = solver.Solve(a.outputs.value_or(aOutputs), b.outputs.value_or(bOutputs), maxSearches);
	return result == DoesSubsume;
}