#pragma once
#include <map>
#include <unordered_set>

#include <sortnetutils.h>

#include "OutputSet.h"

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

public:
	PrefixGraph(uint8_t n_, bool symmetric_);
	PrefixGraph(const PrefixGraph& other) = delete;
	PrefixGraph(PrefixGraph&& other) = delete;
	~PrefixGraph();

	void AddPrefix(const Prefix& prefix);
	void AddEquivalenceEdges();
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
	size_t numVerticesProcessed = 0;

	void AddEdge(Vertex* a, Vertex* b);
	OutputSet SwapChannels(const OutputSet& outputs, uint8_t i, uint8_t j);
	std::vector<std::pair<Vertex*, CE>> GetExtensions(Vertex* vertex) const;
	OutputSet ReduceSet(const OutputSet& outputs, CE ce) const;
	OutputSet ReduceSetUnsym(const OutputSet& outputs, CE ce) const;
	OutputSet ReduceSetSym(const OutputSet& outputs, CE ce) const;
	void AddOutputEdges(Vertex* vertex, const OutputSet& outputs);

	friend class KosarajuSolver;
	friend class GraphvizSerializer;
};