#pragma once
#include <string>
#include <unordered_set>

#include <sortnetutils.h>

class LayerDAG
{
protected:
	struct Vertex
	{
		Network layer;
		uint64_t usedChannels;
		std::vector<Vertex*> children;
		std::unordered_set<uint64_t> outputs;

		bool redundant, childSubset;
	};

public:
	LayerDAG(uint8_t n_, bool symmetric_);
	LayerDAG(uint8_t n_, const std::vector<Network>& allLayers);
	LayerDAG(const LayerDAG& other) = delete;
	LayerDAG(LayerDAG&& other) = delete;
	~LayerDAG();

	size_t Size() const;
	std::vector<Network> GetLayers() const;
	std::vector<Network> GetRedundantLayers() const;
	std::vector<Network> GetSaturatedLayers() const;
	std::vector<Network> GetUnsaturatedLayers() const;

	void PropagateOutputs(const std::unordered_set<uint64_t>& outputs);
	void FindRedundant();
	void FindChildSubsets();

	void SaveGraphviz(const std::string& filepath) const;

protected:
	uint8_t n;
	bool symmetric;
	Vertex* root;

	// General utilities
	static void CollectVertices(Vertex* vertex, std::unordered_set<Vertex*>& vertices);

	// Construction
	bool CanAddCE(uint64_t usedChannels, CE ce) const;
	Network AddCE(const Network& layer, CE ce) const;
	Vertex* AddCE(Vertex* parent, CE ce) const;

	// Output propagation
	static void ClearOutputs(Vertex* vertex);
	static void PropagateOutputs(Vertex* vertex, Vertex* parent);

	// Vertex properties
	static void MarkTreeRedundant(Vertex* vertex);
	static void FindRedundant(Vertex* vertex, Vertex* parent);
	static void FindChildSubsets(Vertex* vertex);
};