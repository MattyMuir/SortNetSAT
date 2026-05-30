#include "PrefixGeneratorV2.h"

#include <print>
#include <set>
#include <iostream>
#include <map>

#include "NetworkGraph.h"
#include "prefixes.h"

PrefixGeneratorV2::PrefixGeneratorV2(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_), layerDAG(n, symmetric) {}

std::vector<Network> PrefixGeneratorV2::GeneratePrefixes()
{
	Prefix initialPrefix;
	initialPrefix.push_back(PrefixPar(n));
	allPrefixes.push_back(initialPrefix);
	for (uint8_t k = 1; k < d; k++)
		Generate();

	std::vector<Network> ret;
	for (const Prefix& prefix : allPrefixes)
	{
		Network network;
		for (const Network& layer : prefix)
			Append(network, layer);
		ret.push_back(network);
	}
	return ret;
}

void PrefixGeneratorV2::Generate()
{
	std::map<NetworkGraph, Prefix> nextAllPrefixes;

	for (const Prefix& partialPrefix : allPrefixes)
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
		auto unsaturatedLayers = layerDAG.GetUnsaturatedLayers();

		for (const Network& layer : saturatedLayers)
		{
			Prefix newPrefix{ partialPrefix };
			newPrefix.push_back(layer);
			nextAllPrefixes.emplace(NetworkGraph{ newPrefix, n }, newPrefix);
		}

		/*
		for (const Network& layer : unsaturatedLayers)
		{
			Prefix newPrefix{ partialPrefix };
			newPrefix.push_back(layer);
			nextAllPrefixes.erase(NetworkGraph{ newPrefix, n });
		}
		*/
	}

	allPrefixes.clear();

	for (const auto& [graph, prefix] : nextAllPrefixes)
		allPrefixes.push_back(prefix);
}