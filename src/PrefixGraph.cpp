#include "PrefixGraph.h"

#include <algorithm>
#include <fstream>

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
	size_t idx = nextVertexIdx++;
	Vertex* vertex = new Vertex{ idx, {} };

	Prefix sortedPrefix{ prefix };
	for (Network& layer : sortedPrefix)
		std::sort(layer.begin(), layer.end());

	prefixToIdx.emplace(sortedPrefix, idx);
	idxToVertex.push_back(vertex);
	idxToPrefix.push_back(sortedPrefix);
}

void PrefixGraph::ComputeOutputs()
{
	for (Vertex* vertex : idxToVertex)
		ComputeOutputs(vertex);
}

void PrefixGraph::AddEquivalenceEdges()
{
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
	{
		for (size_t idx1 : equivalenceClass)
		{
			for (size_t idx2 : equivalenceClass)
			{
				if (idx1 == idx2) continue;

				Vertex* v1 = idxToVertex[idx1];
				Vertex* v2 = idxToVertex[idx2];
				AddEdge(v1, v2);
			}
		}
	}
}

void PrefixGraph::AddOutputEdges()
{
	for (Vertex* vertex : idxToVertex)
		AddOutputEdges(vertex);
}

std::vector<Network> PrefixGraph::GetRepresentatives() const
{
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

	std::vector<Network> representatives;
	for (size_t sccIdx = 0; sccIdx < sccs.size(); sccIdx++)
	{
		if (hasIncoming[sccIdx]) continue;
		size_t representativeIdx = *sccs[sccIdx].begin();
		const Prefix& prefix = idxToPrefix[representativeIdx];
		Network network;
		for (const Network& layer : prefix)
			Append(network, layer);
		representatives.push_back(network);
	}

	return representatives;
}

void PrefixGraph::SaveGraphviz(const std::string& filepath) const
{
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
	if (!vertex->outputs.empty()) return;

	const Prefix& prefix = idxToPrefix[vertex->idx];
	Network prefixNetwork;
	for (const Network& layer : prefix)
		Append(prefixNetwork, layer);
	auto outputsVec = GetOutputs(prefixNetwork, n);
	for (uint64_t output : outputsVec)
		vertex->outputs.insert(output);
}

void PrefixGraph::AddEdge(Vertex* a, Vertex* b)
{
	a->outgoing.insert(b);
	b->incoming.insert(a);
}

static inline bool IsSubset(const std::unordered_set<uint64_t>& a, const std::unordered_set<uint64_t>& b)
{
	if (a.size() >= b.size()) return false;
	for (uint64_t x : a)
		if (!b.contains(x))
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
			if (usedChannels & ((1ULL << i) | (1ULL << j))) continue;

			Prefix extPrefix{ prefix };
			extPrefix.back().push_back({ i, j });
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
			
			uint64_t leftMask = 1ULL << i;
			uint64_t rightMask = 1ULL << j;
			uint64_t stationaryMask = ~(leftMask | rightMask);
			uint64_t shift = j - i;

			std::unordered_set<uint64_t> flippedOutputs;
			for (uint64_t output : extVertex->outputs)
			{
				output = (output & stationaryMask)
					| (output & leftMask) << shift
					| (output & rightMask) >> shift;
				flippedOutputs.insert(output);
			}
			
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