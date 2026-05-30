#include "PrefixGeneratorV3.h"

#include "prefixes.h"

PrefixGeneratorV3::PrefixGeneratorV3(uint8_t n_, bool symmetric_)
	: n(n_), symmetric(symmetric_), graph(n, symmetric)
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

std::vector<Network> PrefixGeneratorV3::Generate()
{
	// Add all prefixes to graph
	Network firstLayer = PrefixPar(n);
	for (const Network& secondLayer : allLayers)
		graph.AddPrefix({ firstLayer, secondLayer });

	graph.ComputeOutputs();
	graph.AddEquivalenceEdges();
	graph.AddOutputEdges();
	graph.SaveGraphviz("graph.gv");

	return graph.GetRepresentatives();
}

bool PrefixGeneratorV3::CanAddCE(const Network& layer, CE ce) const
{
	uint64_t usedMask = 0;
	for (auto [i, j] : layer)
		usedMask |= (1ULL << i) | (1ULL << j);

	uint64_t ceMask = (1ULL << ce.lo) | (1ULL << ce.hi);
	if (symmetric) ceMask |= (1ULL << (n - 1 - ce.hi)) | (1ULL << (n - 1 - ce.lo));

	return (usedMask & ceMask) == 0;
}

Network PrefixGeneratorV3::AddCE(const Network& layer, CE ce) const
{
	Network ret{ layer };
	ret.push_back(ce);
	if (symmetric && ce.lo != n - 1 - ce.hi)
		ret.push_back({ (uint8_t)(n - 1 - ce.hi), (uint8_t)(n - 1 - ce.lo) });
	return ret;
}