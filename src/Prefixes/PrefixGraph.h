#pragma once
#include <map>
#include <unordered_set>

#include <sortnetutils.h>

#include "IsomorphicOutputSet.h"

class PrefixGraph
{
protected:
	using Prefix = std::vector<Network>;

	struct Vertex
	{
		size_t idx;
		bool doneOutputEdges = false;
		std::vector<Vertex*> incoming, outgoing;
	};

	enum EdgeType
	{
		IsoOutputsEdge,
		SubsetEdge,
		OutputEdge
	};

public:
	PrefixGraph(uint8_t n_, bool symmetric_);
	PrefixGraph(const PrefixGraph& other) = delete;
	PrefixGraph(PrefixGraph&& other) = delete;
	~PrefixGraph();

	void AddPrefix(const Prefix& prefix);
	void AddIsomorphicOutputsEdgesV1();
	void AddIsomorphicOutputsEdgesV2();
	void AddIsomorphicOutputsEdgesV3();
	void AddSubsetEdges();
	void AddOutputEdges();

	std::vector<Prefix> GetRepresentatives() const;

	void SaveGraphviz(const std::string& filepath) const;

protected:
	uint8_t n;
	bool symmetric;

	size_t nextVertexIdx = 0;
	std::map<Prefix, size_t> prefixToIdx;
	std::vector<Vertex*> idxToVertex;
	std::vector<Prefix> idxToPrefix;
	std::map<std::pair<size_t, size_t>, EdgeType> edgeTypes;
	size_t numVerticesProcessed = 0;

	void AddEdge(Vertex* a, Vertex* b, EdgeType type);
	void IsomorphicOutputsStrided(IsomorphicOutputSet& isoOutputsSet, double& progress, size_t threadIdx, size_t numThreads);
	std::vector<std::pair<Vertex*, CE>> GetExtensions(Vertex* vertex) const;
	void ApplyCE(FactoredOutputSet& outputs, CE ce) const;
	void SwapBits(FactoredOutputSet& outputs, CE ce) const;
	void AddOutputEdges(Vertex* vertex, const FactoredOutputSet& outputs);

	friend class KosarajuSolver;
	friend class GraphvizSerializer;
};