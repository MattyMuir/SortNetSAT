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
		OutputSet outputs;
		std::vector<Vertex*> incoming, outgoing;
	};

public:
	PrefixGraph(uint8_t n_, bool symmetric_);
	PrefixGraph(const PrefixGraph& other) = delete;
	PrefixGraph(PrefixGraph&& other) = delete;
	~PrefixGraph();

	void AddPrefix(const Prefix& prefix);
	void ComputeOutputs();
	void AddEquivalenceEdges();
	void AddOutputEdges();

	std::vector<Network> GetRepresentatives() const;

	void SaveGraphviz(const std::string& filepath) const;

protected:
	uint8_t n;
	bool symmetric;

	size_t nextVertexIdx = 0;
	std::map<Prefix, size_t> prefixToIdx;
	std::vector<Vertex*> idxToVertex;
	std::vector<Prefix> idxToPrefix;

	void ComputeOutputs(Vertex* vertex);
	void AddEdge(Vertex* a, Vertex* b);
	OutputSet SwapChannels(const OutputSet& outputs, uint8_t i, uint8_t j);
	void AddOutputEdges(Vertex* vertex);

	friend class KosarajuSolver;
};