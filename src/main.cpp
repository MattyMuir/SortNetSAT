#include <print>
#include <fstream>
#include <algorithm>
#include <set>

#include "Timer.h"

#include "prefixes.h"
#include "SimpleExtender.h"
#include "IncrementalExtender.h"
#include "PrefixGenerator.h"
#include "PrefixGeneratorV2.h"
#include "PrefixGeneratorV3.h"
#include "LayerDAG.h"
#include "Pattern.h"

void FractionBenchmark()
{
	// === Parameters ===
	uint8_t n = 16;
	uint8_t d = 9;
	bool symmetric = true;
	Network prefix = { {
			{0,5},{1,4},{2,12},{3,13},{6,7},{8,9},{10,15},{11,14},
			{0,2},{1,10},{3,6},{4,7},{5,14},{8,11},{9,12},{13,15}
		} };
	// ==================

	static constexpr size_t Repeats = 15;

	std::vector<double> times;
	for (size_t f = 1; f <= 20; f += 1)
	{
		double inputFraction = f / 100.0;
		std::println("Starting {}", inputFraction);

		double timeSum = 0.0;
		for (size_t i = 0; i < Repeats; i++)
		{
			SimpleExtender extender{ n, d, symmetric, prefix };
			Timer t;
			extender.Extend();
			t.Stop();
			timeSum += log(t.GetSeconds());
		}
		times.push_back(timeSum / Repeats);
	}

	for (double t : times)
		std::println("{}", t);
}

void Wang()
{
	// === Parameters ===
	uint8_t n = 28;
	uint8_t d = 13;
	bool symmetric = true;
	Network prefix = { {
			{0,27},{1,26},{2,25},{3,24},{4,23},{5,22},{6,21},{7,20},{8,9},{10,11},{12,15},{13,14},{16,17},{18,19},
			{0,1},{2,3},{4,5},{6,7},{8,10},{9,11},{12,14},{13,15},{16,18},{17,19},{20,21},{22,23},{24,25},{26,27},
			{0,2},{1,3},{4,6},{5,7},{8,19},{9,12},{10,14},{11,16},{13,17},{15,18},{20,22},{21,23},{24,26},{25,27},
			{0,4},{1,5},{2,20},{3,21},{6,24},{7,25},{8,13},{9,11},{10,17},{12,15},{14,19},{16,18},{22,26},{23,27},
			{1,2},{3,24},{4,6},{5,22},{7,20},{8,9},{10,12},{11,13},{14,16},{15,17},{18,19},{21,23},{25,26},
			{0,8},{1,4},{2,6},{3,9},{5,7},{10,11},{12,13},{14,15},{16,17},{18,24},{19,27},{20,22},{21,25},{23,26}
		} };
	// ==================

	SimpleExtender extender{ n, d, symmetric, prefix };
	bool extendable = extender.Extend();
	if (!extendable) std::println("Unextendable!");
	else std::println("{:t}", extender.GetNetwork());
}

void Net18Full()
{
	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;
	Network prefix = { {
			{0,6},{1,10},{2,15},{3,5},{4,9},{7,16},{8,13},{11,17},{12,14},
			{0,12},{1,4},{3,11},{5,17},{6,14},{7,8},{9,10},{13,16}
		} };
	// ==================

	SimpleExtender extender{ n, d, symmetric, prefix };
	bool extendable = extender.Extend();
	if (!extendable) std::println("Unextendable!");
	else std::println("{:l}", extender.GetNetwork());
}

void GenerateCactusPlot()
{
	// === Parameters ===
	uint8_t n = 17;
	uint8_t d = 8;
	bool symmetric = false;
	// ==================

	std::ifstream file{ "C:/sdks/JCSS/prefixes/opt/pref_opt17.txt" };

	std::vector<double> times;
	for (size_t i = 0; i < 50; i++)
	{
		std::string prefixStr;
		std::getline(file, prefixStr);
		Network prefix = ParseNetwork(prefixStr);

		IncrementalExtender extender{ n, d, symmetric, prefix };
		extender.SetParameters(6);
		Timer t;
		bool extendable = extender.Extend();
		t.Stop();
		times.push_back(t.GetSeconds());
	}

	std::sort(times.begin(), times.end());
	std::println();
	for (double t : times) std::println("{:.3f}", t);
}

bool IsSaturated(const std::vector<Network>& layers, uint8_t n)
{
	// Check for redundancy
	for (auto [lo, hi] : layers[1])
		if (lo % 2 == 0 && hi == lo + 1)
			return false;

	// If second layer is maximal
	if (layers[1].size() == n / 2)
		return true;

	// Look for unused i (min channel) and j (max channel) with i < j
	std::vector<bool> usedChannels(n, false);
	for (auto [lo, hi] : layers[1]) { usedChannels[lo] = true, usedChannels[hi] = true; }
	std::vector<uint8_t> unusedMin, unusedMax;
	for (uint8_t i = 0; i < n; i += 2) if (!usedChannels[i]) unusedMin.push_back(i);
	for (uint8_t i = 1; i < n; i += 2) if (!usedChannels[i]) unusedMax.push_back(i);
	if (!unusedMin.empty() && !unusedMax.empty())
		if (unusedMin[0] < unusedMax.back())
			return false;

	// Any unused channel in the first layer must be used in the second
	//if (n % 2 == 1 && !usedChannels[n - 1])
		//return false;

	Pattern p3a{
		.m = 4,
		.ces = { std::vector<CE>{ {0,1}, {2,3} }, std::vector<CE>{ {0,2} } },
		.singletons = { std::vector<uint8_t>{}, std::vector<uint8_t>{} }
	};

	Pattern p3b{
		.m = 4,
		.ces = { std::vector<CE>{ {0,1}, {2,3} }, std::vector<CE>{ {1,3} } },
		.singletons = { std::vector<uint8_t>{}, std::vector<uint8_t>{} }
	};

	std::vector<Pattern> ps{ p3a, p3b };
	for (const Pattern& p : ps)
		if (ContainsPattern(layers, n, p))
			return false;

	return true;
}

int main()
{
#if 0
	// Get prefix outputs
	auto outputsVec = GetOutputs(PrefixPar(n), n);
	std::unordered_set<uint64_t> outputs;
	outputs.insert(outputsVec.begin(), outputsVec.end());

	// Construct layer DAG
	LayerDAG layerDag{ n, symmetric };
	std::println("Num layers: {}", layerDag.Size());

	// Propagate outputs
	std::println("Propagating outputs...");
	layerDag.PropagateOutputs(outputs);

	// Assign vertex properties
	std::println("Assigning vertex properties...");
	layerDag.FindRedundant();
	layerDag.FindChildSubsets();

	layerDag.SaveGraphviz("layerdag.gv");

	std::println("Num saturated: {}", layerDag.GetSaturatedLayers().size());
#endif

#if 0
	for (uint8_t n = 3; n <= 11; n++)
	{
		LayerDAG layerDag{ n, false };
		Network firstLayer = PrefixPar(n);
		auto allLayers = layerDag.GetLayers();

		size_t numSaturated = 0;
		for (const Network& secondLayer : allLayers)
		{
			bool isSaturated = IsSaturated({ firstLayer, secondLayer }, n);
			if (isSaturated)
				numSaturated++;
		}

		std::print("{} ", numSaturated);
	}
	std::println();

	for (uint8_t n = 3; n <= 11; n++)
	{
		auto outputsVec = GetOutputs(PrefixPar(n), n);
		std::unordered_set<uint64_t> outputs;
		outputs.insert(outputsVec.begin(), outputsVec.end());

		// Construct layer DAG
		LayerDAG layerDag{ n, false };

		// Propagate outputs
		layerDag.PropagateOutputs(outputs);

		// Assign vertex properties
		layerDag.FindRedundant();
		layerDag.FindChildSubsets();

		std::print("{} ", layerDag.GetSaturatedLayers().size());
	}
#endif

#if 0
	uint8_t n = 7;

	// Generate prefixes using 'IsSaturated'
	std::set<Network> satPrefixes1;
	{
		LayerDAG layerDag{ n, false };
		Network firstLayer = PrefixPar(n);
		auto allLayers = layerDag.GetLayers();

		for (const Network& secondLayer : allLayers)
			if (IsSaturated({ firstLayer, secondLayer }, n))
				satPrefixes1.insert(secondLayer);
	}

	// Generate prefixes using LayerDAG
	std::set<Network> satPrefixes2;
	{
		auto outputsVec = GetOutputs(PrefixPar(n), n);
		std::unordered_set<uint64_t> outputs;
		outputs.insert(outputsVec.begin(), outputsVec.end());

		// Construct layer DAG
		LayerDAG layerDag{ n, false };

		// Propagate outputs
		layerDag.PropagateOutputs(outputs);

		// Assign vertex properties
		layerDag.FindRedundant();
		layerDag.FindChildSubsets();

		for (const Network& secondLayer : layerDag.GetSaturatedLayers())
			satPrefixes2.insert(secondLayer);
	}

	std::println("Num saturated 1: {}", satPrefixes1.size());
	std::println("Num saturated 2: {}", satPrefixes2.size());

	for (const Network& layer : satPrefixes2)
		if (!satPrefixes1.contains(layer))
			std::println("{}", layer);
#endif

	PrefixGeneratorV3 generator{ 7, false };
	auto allPrefixes = generator.Generate();
	std::print("{} ", allPrefixes.size());
}