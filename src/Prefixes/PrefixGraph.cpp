#include "PrefixGraph.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <ranges>
#include <thread>

#include "../Timer.h"
#include "KosarajuSolver.h"
#include "GraphvizSerializer.h"

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
	Vertex* vertex = new Vertex{ idx, false, {}, {} };

	// Sort network to ensure correct map lookup
	Prefix sortedPrefix{ prefix };
	for (Network& layer : sortedPrefix)
		std::sort(layer.begin(), layer.end());

	// Add entires in the maps
	prefixToIdx.emplace(sortedPrefix, idx);
	idxToVertex.push_back(vertex);
	idxToPrefix.push_back(sortedPrefix);
}

void PrefixGraph::AddIsomorphicOutputsEdges()
{
	TIMER(addIsomorphicOutputsEdges);

	// Reserve storage for each thread's isomorphic output set
	size_t numThreads = std::thread::hardware_concurrency() - 1;
	std::vector<IsomorphicOutputSet> outputSets;
	outputSets.reserve(numThreads);
	for (size_t i = 0; i < numThreads; i++)
		outputSets.emplace_back(n);

	// Initialize the threads to process strided vertices
	std::vector<double> progress(numThreads);
	std::vector<std::thread> threads;
	threads.reserve(numThreads);
	for (size_t threadIdx = 0; threadIdx < numThreads; threadIdx++)
	{
		threads.emplace_back([this, threadIdx, numThreads, &outputSets, &progress]()
			{
				IsomorphicOutputsStrided(outputSets[threadIdx], progress[threadIdx], threadIdx, numThreads);
			});
	}

	// Log the progress of thread 0
	for (;;)
	{
		std::print("isomorphicOutputs {:.3f}%\r", progress[0] * 100.0);
		if (progress[0] == 1.0) break;
		std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
	}

	for (auto& thread : threads) thread.join();

	// Merge output sets
	std::print("Merging isomorphic output sets\r");
	for (size_t i = 1; i < numThreads; i++)
		outputSets[0].Merge(outputSets[i]);

	// Get equivalence classes from the set
	auto equivalenceClasses = outputSets[0].GetEquivalenceClasses();

	// Add a cycle through all vertices of the class to make them an SCC
	for (const std::vector<size_t>& equivalenceClass : equivalenceClasses)
	{
		if (equivalenceClass.size() == 1) continue;
		for (size_t i = 0; i < equivalenceClass.size(); i++)
		{
			size_t idx1 = equivalenceClass[i];
			size_t idx2 = equivalenceClass[(i + 1) % equivalenceClass.size()];
			AddEdge(idxToVertex[idx1], idxToVertex[idx2], IsoOutputsEdge);
		}
	}
	STOP_LOG(addIsomorphicOutputsEdges);
}

static inline bool StrictSubset(const OutputSet& a, const OutputSet& b)
{
	if (a.Size() >= b.Size()) return false;

	return std::ranges::all_of(a, [&b](uint64_t x) { return b.Contains(x); });
}

void PrefixGraph::AddSubsetEdges()
{
	TIMER(addSubsetEdges);
	size_t numPairs = nextVertexIdx * (nextVertexIdx - 1);

	size_t progress = 0;
	for (size_t idx1 = 0; idx1 < nextVertexIdx; idx1++)
	{
		for (size_t idx2 = 0; idx2 < nextVertexIdx; idx2++)
		{
			if (idx1 == idx2) continue;

			OutputSet outputs1 = GetOutputs(Concatenate(idxToPrefix[idx1]), n);
			OutputSet outputs2 = GetOutputs(Concatenate(idxToPrefix[idx2]), n);

			if (outputs1 == outputs2 || StrictSubset(outputs1, outputs2))
				AddEdge(idxToVertex[idx1], idxToVertex[idx2], SubsetEdge);

			progress++;
			if (progress % 1000 == 0)
				std::print("subsetEdges: {:.3f}%\r", (double)progress / numPairs * 100.0);
		}
	}
	STOP_LOG(addSubsetEdges);
}

void PrefixGraph::AddOutputEdges()
{
	TIMER(addOutputEdges);
	for (Vertex* vertex : idxToVertex)
	{
		// Check if this node is a 'root' (has an empty final layer)
		const Prefix& prefix = idxToPrefix[vertex->idx];
		if (!prefix.back().empty()) continue;

		// Compute outputs from scratch
		FactoredOutputSet outputs{ Concatenate(prefix), n };

		// DFS into all children
		AddOutputEdges(vertex, outputs);
	}
	STOP_LOG(addOutputEdges);
}

std::vector<PrefixGraph::Prefix> PrefixGraph::GetRepresentatives() const
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
	std::vector<Prefix> representatives;
	for (size_t sccIdx = 0; sccIdx < sccs.size(); sccIdx++)
	{
		if (hasIncoming[sccIdx]) continue;
		size_t representativeIdx = *sccs[sccIdx].begin();
		const Prefix& prefix = idxToPrefix[representativeIdx];
		representatives.push_back(prefix);
	}

	STOP_LOG(getRepresentatives);
	return representatives;
}

void PrefixGraph::SaveGraphviz(const std::string& filepath) const
{
	GraphvizSerializer serializer{ *this, filepath };
	serializer.Serialize(true, true);
}

void PrefixGraph::AddEdge(Vertex* a, Vertex* b, EdgeType type)
{
	if (edgeTypes.contains({ a->idx, b->idx })) return;

	edgeTypes.emplace(std::pair<size_t, size_t>{ a->idx, b->idx }, type);
	a->outgoing.push_back(b);
	b->incoming.push_back(a);
}

void PrefixGraph::IsomorphicOutputsStrided(IsomorphicOutputSet& isoOutputsSet, double& progress, size_t threadIdx, size_t numThreads)
{
	for (size_t idx = threadIdx; idx < nextVertexIdx; idx += numThreads)
	{
		isoOutputsSet.Insert(&idxToPrefix[idx], idx);
		progress = (double)idx / nextVertexIdx;
	}
	progress = 1.0;
}

std::vector<std::pair<PrefixGraph::Vertex*, CE>> PrefixGraph::GetExtensions(Vertex* vertex) const
{
	// Determine which channels are used in this prefix
	const Prefix& prefix = idxToPrefix[vertex->idx];
	uint64_t usedChannels = 0;
	for (auto [lo, hi] : prefix.back())
		usedChannels |= (1ULL << lo) | (1ULL << hi);

	std::vector<std::pair<Vertex*, CE>> extendedVertices;
	for (uint8_t i = 0; i + 1 < n; i++)
	{
		for (uint8_t j = i + 1; j < n; j++)
		{
			if (symmetric && i > n - 1 - j) continue;

			// Check if this CE fits in the last layer of the prefix
			uint64_t ceMask = (1ULL << i) | (1ULL << j);
			if (symmetric) ceMask |= (1ULL << (n - 1 - j)) | (1ULL << (n - 1 - i));
			if (usedChannels & ceMask) continue;

			// Create the extended prefix by adding this CE
			Prefix extPrefix{ prefix };
			extPrefix.back().push_back({ i, j });
			if (symmetric && i != n - 1 - j)
				extPrefix.back().push_back({ (uint8_t)(n - 1 - j), (uint8_t)(n - 1 - i) });

			// Lookup the vertex for the extended prefix
			std::sort(extPrefix.back().begin(), extPrefix.back().end());
			size_t extIdx = prefixToIdx.at(extPrefix);
			Vertex* extVertex = idxToVertex[extIdx];

			extendedVertices.emplace_back(extVertex, CE{ i, j });
		}
	}

	return extendedVertices;
}

void PrefixGraph::ApplyCE(FactoredOutputSet& outputs, CE ce) const
{
	outputs.ApplyCE(ce.lo, ce.hi);
	if (symmetric && ce.lo != (n - 1 - ce.hi))
		outputs.ApplyCE(n - 1 - ce.hi, n - 1 - ce.lo);
}

void PrefixGraph::SwapBits(FactoredOutputSet& outputs, CE ce) const
{
	outputs.SwapBits(ce.lo, ce.hi);
	if (symmetric && ce.lo != (n - 1 - ce.hi))
		outputs.SwapBits(n - 1 - ce.hi, n - 1 - ce.lo);
}

void PrefixGraph::AddOutputEdges(Vertex* vertex, const FactoredOutputSet& outputs)
{
	if (vertex->doneOutputEdges) return;

	auto extendedVertices = GetExtensions(vertex);
	OutputSet outputSet{ outputs };

	for (auto [extVertex, addedCE] : extendedVertices)
	{
		FactoredOutputSet extOutputs{ outputs };
		ApplyCE(extOutputs, addedCE);
		OutputSet extOutputSet{ extOutputs };

		if (StrictSubset(extOutputSet, outputSet)) // Check for output subset
		{
			AddEdge(extVertex, vertex, OutputEdge);
		}
		else // Try adding the comparator(s) upside-down
		{
			FactoredOutputSet flippedOutputs{ extOutputs };
			SwapBits(flippedOutputs, addedCE);
			OutputSet flippedOutputSet{ flippedOutputs };

			if (StrictSubset(flippedOutputSet, outputSet)) // Check for output subset
				AddEdge(extVertex, vertex, OutputEdge);
		}

		AddOutputEdges(extVertex, extOutputs);
	}

	vertex->doneOutputEdges = true;
	if (++numVerticesProcessed % 100 == 0)
		std::print("outputs: {:.3f}%\r", (double)numVerticesProcessed / idxToVertex.size() * 100.0);
}