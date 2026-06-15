#include "OutputGraph.h"

enum VertexType
{
	VertexBit,
	VertexOutput
};

OutputGraph::OutputGraph(const OutputSet& outputs, uint8_t n)
{
	bliss::Digraph g;

	// Create bit vertices
	std::vector<uint32_t> bitVertices;
	for (uint8_t bi = 0; bi < n; bi++)
		bitVertices.push_back(g.add_vertex(VertexBit));

	// Create output vertices and add edges
	for (uint64_t output : outputs)
	{
		uint32_t v = g.add_vertex(VertexOutput);
		for (uint8_t bi = 0; bi < n; bi++)
			if (output & (1ULL << bi))
				g.add_edge(v, bitVertices[bi]);
	}

	// Rewrite g in canonical form
	bliss::Stats stats;
	const uint32_t* perm = g.canonical_form(stats);
	graph = g.permute(perm);
}

OutputGraph::OutputGraph(OutputGraph&& other) noexcept
	: graph(other.graph)
{
	other.graph = nullptr;
}

OutputGraph& OutputGraph::operator=(OutputGraph&& other) noexcept
{
	delete graph;
	graph = other.graph;
	other.graph = nullptr;
}

OutputGraph::~OutputGraph()
{
	delete graph;
}

bool OutputGraph::operator<(const OutputGraph & other) const
{
	return graph->cmp(*other.graph) < 0;
}

bool OutputGraph::operator==(const OutputGraph& other) const
{
	return graph->cmp(*other.graph) == 0;
}

uint32_t OutputGraph::GetHash() const
{
	return graph->get_hash();
}