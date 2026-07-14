#include "NetworkGraph.h"

enum VertexType
{
	VertexComparator,
	VertexMin,
	VertexMax,
	VertexPair,
};

static inline void AddEdge(bliss::Digraph& g, uint32_t source, uint32_t target, bool isMax)
{
	uint32_t intermediate = g.add_vertex(isMax ? VertexMax : VertexMin);

	g.add_edge(source, intermediate);
	g.add_edge(intermediate, target);
}

NetworkGraph::NetworkGraph(const LayeredNetwork& network, uint8_t n, bool symmetric)
{
	bliss::Digraph g;

	// Create all comparator vertices
	for (const Network& layer : network)
		for (const auto& _ : layer)
			g.add_vertex(VertexComparator);

	if (symmetric)
	{
		uint32_t ceIdxBase2 = 0;
		for (const Network& layer : network)
		{
			for (size_t ceIdx = 0; ceIdx < layer.size(); ceIdx++)
			{
				auto [i, j] = layer[ceIdx];
				if (i >= n - 1 - j) continue;

				uint32_t pairVertex = g.add_vertex(VertexPair);
				auto it = std::find(layer.begin(), layer.end(), CE{ (uint8_t)(n - 1 - j), (uint8_t)(n - 1 - i) });
				uint32_t otherCEIdx = ceIdxBase2 + std::distance(layer.begin(), it);

				g.add_edge(pairVertex, ceIdxBase2 + ceIdx);
				g.add_edge(pairVertex, otherCEIdx);
			}
			ceIdxBase2 += layer.size();
		}
	}

	// 'channelSources' stores which comparator the value in each channel came from
	constexpr uint32_t IsInput = 1ULL << 31;
	constexpr uint32_t IsMax = 1ULL << 30;
	constexpr uint32_t CompMask = ~(IsInput | IsMax);
	std::vector<uint32_t> channelSources(n, IsInput);

	uint32_t ceIdxBase = 0;
	for (const Network& layer : network)
	{
		// Add edges from the previous layer to this one
		for (uint32_t ceIdx = 0; ceIdx < layer.size(); ceIdx++)
		{
			auto [lo, hi] = layer[ceIdx];
			uint32_t loSource = channelSources[lo];
			uint32_t hiSource = channelSources[hi];

			if (loSource != IsInput) AddEdge(g, loSource & CompMask, ceIdxBase + ceIdx, loSource & IsMax);
			if (hiSource != IsInput) AddEdge(g, hiSource & CompMask, ceIdxBase + ceIdx, hiSource & IsMax);
		}

		// Update channelSources
		for (uint32_t ceIdx = 0; ceIdx < layer.size(); ceIdx++)
		{
			auto [lo, hi] = layer[ceIdx];
			channelSources[lo] = (ceIdxBase + ceIdx);
			channelSources[hi] = (ceIdxBase + ceIdx) | IsMax;
		}

		ceIdxBase += layer.size();
	}

	// Rewrite g in canonical form
	bliss::Stats stats;
	const uint32_t* perm = g.canonical_form(stats);
	graph = std::unique_ptr<bliss::Digraph>{ g.permute(perm) };
}

bool NetworkGraph::operator<(const NetworkGraph& other) const
{
	return graph->cmp(*other.graph) < 0;
}

bool NetworkGraph::operator==(const NetworkGraph& other) const
{
	return graph->cmp(*other.graph) == 0;
}

uint32_t NetworkGraph::GetHash() const
{
	return graph->get_hash();
}