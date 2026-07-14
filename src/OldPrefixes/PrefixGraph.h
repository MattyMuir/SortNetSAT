#pragma once
#include <map>
#include <unordered_set>

#include <sortnetutils.h>

#include "IsomorphicOutputSet.h"

class PrefixGraph
{
protected:
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

	void AddPrefix(const LayeredNetwork& prefix);
	void AddIsomorphicOutputsEdges();
	void AddSubsetEdges();
	void AddOutputEdges();

	std::vector<LayeredNetwork> GetRepresentatives() const;

	void SaveGraphviz(const std::string& filepath) const;

protected:
	uint8_t n;
	bool symmetric;

	size_t nextVertexIdx = 0;
	std::map<LayeredNetwork, size_t> prefixToIdx;
	std::vector<Vertex*> idxToVertex;
	std::vector<LayeredNetwork> idxToPrefix;
	std::map<std::pair<size_t, size_t>, EdgeType> edgeTypes;
	size_t numVerticesProcessed = 0;

	void AddEdge(Vertex* a, Vertex* b, EdgeType type);
	std::vector<std::pair<Vertex*, CE>> GetExtensions(Vertex* vertex) const;
	void ApplyCE(FactoredOutputSet& outputs, CE ce) const;
	void SwapBits(FactoredOutputSet& outputs, CE ce) const;
	void AddOutputEdges(Vertex* vertex, const FactoredOutputSet& outputs);

	friend class KosarajuSolver;
	friend class GraphvizSerializer;
};