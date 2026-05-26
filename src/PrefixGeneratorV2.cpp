#include "PrefixGeneratorV2.h"

#include <print>
#include <set>
#include <iostream>

#include "NetworkGraph.h"
#include "prefixes.h"

PrefixGeneratorV2::PrefixGeneratorV2(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_), layerDAG(n, symmetric) {}

std::vector<Network> PrefixGeneratorV2::GeneratePrefixes()
{
	Prefix initialPrefix;
	initialPrefix.push_back(PrefixPar(n));
	R.push_back(initialPrefix);
	for (uint8_t k = 1; k < d; k++)
	{
		Generate();
		Prune();
	}

	std::vector<Network> allPrefixes;
	for (const Prefix& prefix : R)
	{
		Network network;
		for (const Network& layer : prefix)
			Append(network, layer);
		allPrefixes.push_back(network);
	}
	return allPrefixes;
}

void PrefixGeneratorV2::Generate()
{
	N.clear();
	for (const Prefix& partialPrefix : R)
	{
		// Get prefix outputs
		Network prefixConcat;
		for (const Network& layer : partialPrefix)
			Append(prefixConcat, layer);
		auto outputsVec = GetOutputs(prefixConcat, n);
		std::unordered_set<uint64_t> outputSet;
		outputSet.insert(outputsVec.begin(), outputsVec.end());

		// Get saturated layers
		layerDAG.PropagateOutputs(outputSet);
		layerDAG.FindRedundant();
		layerDAG.FindChildSubsets();
		auto saturatedLayers = layerDAG.GetSaturatedLayers();

		for (const Network& layer : saturatedLayers)
		{
			Prefix newPrefix{ partialPrefix };
			newPrefix.push_back(layer);
			N.emplace_back(newPrefix);
		}
	}
}

void PrefixGeneratorV2::Prune()
{
	R.clear();

	std::set<NetworkGraph> graphs;
	for (const Prefix& prefix : N)
	{
		auto [it, isNew] = graphs.emplace(prefix, n);
		if (isNew) R.push_back(prefix);
	}
}