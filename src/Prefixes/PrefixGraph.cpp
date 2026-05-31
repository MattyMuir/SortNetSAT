#include "PrefixGraph.h"

#include <algorithm>
#include <fstream>

#include "../Timer.h"
#include "NetworkGraph.h"
#include "KosarajuSolver.h"

PrefixGraph::PrefixGraph(uint8_t n_, bool symmetric_)
	: n(n_), symmetric(symmetric_) {}

PrefixGraph::~PrefixGraph()
{
	for (Vertex* vertex : idxToVertex)
		delete vertex;
}

void PrefixGraph::AddPrefix(const Prefix& prefix)
{
	// Create new vertex and increment nextVertexIdx
	size_t idx = nextVertexIdx++;
	Vertex* vertex = new Vertex{ idx, { n } };

	// Sort network to ensure correct map lookup
	Prefix sortedPrefix{ prefix };
	for (Network& layer : sortedPrefix)
		std::sort(layer.begin(), layer.end());

	// Add entires in the maps
	prefixToIdx.emplace(sortedPrefix, idx);
	idxToVertex.push_back(vertex);
	idxToPrefix.push_back(sortedPrefix);
}

void PrefixGraph::ComputeOutputs()
{
	TIMER(computeOutputs);
	for (Vertex* vertex : idxToVertex)
		ComputeOutputs(vertex);
	STOP_LOG(computeOutputs);
}

void PrefixGraph::AddEquivalenceEdges()
{
	TIMER(equivalence);
	// Determine equivalence classes
	std::map<NetworkGraph, std::vector<size_t>> equivalenceClasses;
	for (size_t idx = 0; idx < nextVertexIdx; idx++)
	{
		const Prefix& prefix = idxToPrefix[idx];
		auto [it, inserted] = equivalenceClasses.try_emplace(NetworkGraph{ prefix, n }, std::vector<size_t>{ idx });
		if (!inserted) it->second.push_back(idx);
	}

	// Add bi-directional edges between all vertices in the same class
	for (const auto& [_, equivalenceClass] : equivalenceClasses)
		for (size_t idx1 : equivalenceClass)
			for (size_t idx2 : equivalenceClass)
				if (idx1 != idx2)
					AddEdge(idxToVertex[idx1], idxToVertex[idx2]);
	STOP_LOG(equivalence);
}

void PrefixGraph::AddOutputEdges()
{
	TIMER(addOutputEdges);
	for (Vertex* vertex : idxToVertex)
		AddOutputEdges(vertex);
	STOP_LOG(addOutputEdges);
}

std::vector<Network> PrefixGraph::GetRepresentatives() const
{
	TIMER(getRepresentatives);
	// Extract strongly-connected components from the graph
	KosarajuSolver kosaraju{ *this };
	auto sccs = kosaraju.ExtractComponents();

	// Create inverse mapping (vertex index) -> (SCC index)
	std::vector<size_t> idxToComponent(nextVertexIdx);
	for (size_t sccIdx = 0; sccIdx < sccs.size(); sccIdx++)
		for (size_t idx : sccs[sccIdx])
			idxToComponent[idx] = sccIdx;

	// Determine which SCCs have incoming edges
	std::vector<bool> hasIncoming(sccs.size(), false);
	for (Vertex* vertex : idxToVertex)
		for (Vertex* outChild : vertex->outgoing)
			if (idxToComponent[vertex->idx] != idxToComponent[outChild->idx])
				hasIncoming[idxToComponent[outChild->idx]] = true;

	// Take one representative from each SCC with no incoming edges
	std::vector<Network> representatives;
	for (size_t sccIdx = 0; sccIdx < sccs.size(); sccIdx++)
	{
		if (hasIncoming[sccIdx]) continue;
		size_t representativeIdx = *sccs[sccIdx].begin();
		const Prefix& prefix = idxToPrefix[representativeIdx];
		representatives.push_back(Concatenate(prefix));
	}

	STOP_LOG(getRepresentatives);
	return representatives;
}

void PrefixGraph::SaveGraphviz(const std::string& filepath) const
{
	// Write header
	std::ofstream file{ filepath };
	file << "digraph G {\n";

	// Save edges
	for (Vertex* v0 : idxToVertex)
	{
		std::string v0Str = std::format("\"{}\"", idxToPrefix[v0->idx].back());
		for (Vertex* v1 : v0->outgoing)
		{
			std::string v1Str = std::format("\"{}\"", idxToPrefix[v1->idx].back());
			file << std::format("{} -> {}\n", v0Str, v1Str);
		}
	}

	file << "}";
}

void PrefixGraph::ComputeOutputs(Vertex* vertex)
{
	const Prefix& prefix = idxToPrefix[vertex->idx];
	vertex->outputs = GetOutputs(Concatenate(prefix), n);
}

void PrefixGraph::AddEdge(Vertex* a, Vertex* b)
{
	a->outgoing.push_back(b);
	b->incoming.push_back(a);
}

OutputSet PrefixGraph::SwapChannels(const OutputSet& outputs, uint8_t i, uint8_t j)
{
	uint64_t leftMask = 1ULL << i;
	uint64_t rightMask = 1ULL << j;
	if (symmetric)
	{
		leftMask |= 1ULL << (n - 1 - j);
		rightMask |= 1ULL << (n - 1 - i);
	}
	uint64_t stationaryMask = ~(leftMask | rightMask);
	uint64_t shift = j - i;

	OutputSet flippedOutputs{ n, outputs.Size() };
	for (uint64_t output : outputs)
	{
		output = (output & stationaryMask)
			| (output & leftMask) << shift
			| (output & rightMask) >> shift;
		flippedOutputs.Insert(output);
	}

	return flippedOutputs;
}

static inline bool IsSubset(const OutputSet& a, const OutputSet& b)
{
	if (a.Size() >= b.Size()) return false;
	for (uint64_t x : a)
		if (!b.Contains(x))
			return false;
	return true;
}

void PrefixGraph::AddOutputEdges(Vertex* vertex)
{
	const Prefix& prefix = idxToPrefix[vertex->idx];
	uint64_t usedChannels = 0;
	for (auto [lo, hi] : prefix.back())
		usedChannels |= (1ULL << lo) | (1ULL << hi);

	for (uint8_t i = 0; i + 1 < n; i++)
	{
		for (uint8_t j = i + 1; j < n; j++)
		{
			// Check if this CE fits in the last layer of the prefix
			uint64_t ceMask = (1ULL << i) | (1ULL << j);
			if (symmetric) ceMask |= (1ULL << (n - 1 - j)) | (1ULL << (n - 1 - i));
			if (usedChannels & ((1ULL << i) | (1ULL << j))) continue;

			// Create the extended prefix by adding this CE
			Prefix extPrefix{ prefix };
			extPrefix.back().push_back({ i, j });
			if (symmetric && i != n - 1 - j)
				extPrefix.back().push_back({ (uint8_t)(n - 1 - j), (uint8_t)(n - 1 - i) });

			// Lookup the vertex for the extended prefix
			std::sort(extPrefix.back().begin(), extPrefix.back().end());
			size_t extIdx = prefixToIdx.at(extPrefix);
			Vertex* extVertex = idxToVertex[extIdx];

			// Check for identical outputs
			if (vertex->outputs == extVertex->outputs)
			{
				AddEdge(vertex, extVertex);
				AddEdge(extVertex, vertex);
				continue;
			}

			// Check for output subset
			if (IsSubset(extVertex->outputs, vertex->outputs))
			{
				AddEdge(extVertex, vertex);
				continue;
			}
			
			OutputSet flippedOutputs = SwapChannels(extVertex->outputs, i, j);
			
			// Check for identical outputs
			if (vertex->outputs == flippedOutputs)
			{
				AddEdge(vertex, extVertex);
				AddEdge(extVertex, vertex);
				continue;
			}

			// Check for output subset
			if (IsSubset(flippedOutputs, vertex->outputs))
			{
				AddEdge(extVertex, vertex);
				continue;
			}
		}
	}
}