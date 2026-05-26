#include "LayerDAG.h"

#include <cassert>
#include <print>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>

LayerDAG::LayerDAG(uint8_t n_, bool symmetric_)
	: n(n_), symmetric(symmetric_), root(new Vertex({}, 0, {}))
{
	// Initialize alphabet
	std::vector<CE> alphabet;
	for (uint8_t i = 0; i + 1 < n; i++)
		for (uint8_t j = i + 1; j < n; j++)
			if (!symmetric || i <= n - 1 - j)
				alphabet.push_back({ i, j });

	// Generate all vertices
	std::map<Network, Vertex*> vertices;
	vertices.emplace(Network{}, root);
	for (CE ce : alphabet)
	{
		// Generate all new vertices by adding this comparator if it fits
		std::map<Network, Vertex*> newVertices;
		for (const auto& [_, vertex] : vertices)
		{
			if (!CanAddCE(vertex->usedChannels, ce)) continue;

			// Create the new vertex
			Vertex* newVertex = AddCE(vertex, ce);

			// Sort layer before inserting into map
			std::sort(newVertex->layer.begin(), newVertex->layer.end());
			newVertices.emplace(newVertex->layer, newVertex);
		}

		// Add all new vertices to main vertex map
		vertices.insert(newVertices.begin(), newVertices.end());
	}

	// Add all edges
	for (const auto& [_, parent] : vertices)
	{
		for (CE ce : alphabet)
		{
			// Cannot produce a child layer if this CE doesn't fit
			if (!CanAddCE(parent->usedChannels, ce)) continue;

			// Construct the child layer
			Network childLayer = AddCE(parent->layer, ce);

			// Sort layer before querying map
			std::sort(childLayer.begin(), childLayer.end());
			Vertex* child = vertices.at(childLayer);

			// Add edge
			parent->children.push_back(child);
		}
	}
}

LayerDAG::~LayerDAG()
{
	std::unordered_set<Vertex*> vertices;
	CollectVertices(root, vertices);

	for (Vertex* vertex : vertices)
		delete vertex;
}

size_t LayerDAG::Size() const
{
	std::unordered_set<Vertex*> vertices;
	CollectVertices(root, vertices);
	return vertices.size();
}

std::vector<Network> LayerDAG::GetRedundantLayers() const
{
	std::unordered_set<Vertex*> vertices;
	CollectVertices(root, vertices);

	std::vector<Network> redundant;
	for (Vertex* vertex : vertices)
		if (vertex->redundant)
			redundant.push_back(vertex->layer);

	return redundant;
}

std::vector<Network> LayerDAG::GetSaturatedLayers() const
{
	std::unordered_set<Vertex*> vertices;
	CollectVertices(root, vertices);

	std::vector<Network> saturated;
	for (Vertex* vertex : vertices)
		if (!vertex->redundant && !vertex->childSubset)
			saturated.push_back(vertex->layer);

	return saturated;
}

void LayerDAG::PropagateOutputs(const std::unordered_set<uint64_t>& outputs)
{
	ClearOutputs(root);
	root->outputs = outputs;
	for (Vertex* child : root->children)
		PropagateOutputs(child, root);
}

void LayerDAG::FindRedundant()
{
	FindRedundant(root, nullptr);
}

void LayerDAG::FindChildSubsets()
{
	FindChildSubsets(root);
}

void LayerDAG::SaveGraphviz(const std::string& filepath) const
{
	std::ofstream file{ filepath };
	file << "digraph G {\n";

	// Add vertex colors
	std::unordered_set<Vertex*> vertices;
	CollectVertices(root, vertices);
	for (Vertex* vertex : vertices)
	{
		const char* color;
		switch (vertex->redundant | (vertex->childSubset << 1))
		{
		break; case 0b00: color = "white";
		break; case 0b01: color = "red";
		break; case 0b10: color = "blue";
		break; case 0b11: color = "purple";
		}
		file << std::format("\"{}\" [style=filled, fillcolor={}]\n", vertex->layer, color);
	}

	// Add edges
	for (Vertex* parent : vertices)
	{
		std::string parentStr = std::format("\"{}\"", parent->layer);
		for (Vertex* child : parent->children)
		{
			std::string childStr = std::format("\"{}\"", child->layer);
			file << std::format("{} -> {}\n", parentStr, childStr);
		}
	}

	file << "}";
}

void LayerDAG::CollectVertices(Vertex* vertex, std::unordered_set<Vertex*>& vertices)
{
	vertices.insert(vertex);
	for (Vertex* child : vertex->children)
		CollectVertices(child, vertices);
}

bool LayerDAG::CanAddCE(uint64_t usedChannels, CE ce) const
{
	uint64_t ceMask = (1ULL << ce.lo) | (1ULL << ce.hi);
	if (symmetric) ceMask |= (1ULL << (n - 1 - ce.hi)) | (1ULL << (n - 1 - ce.lo));
	return !(ceMask & usedChannels);
}

Network LayerDAG::AddCE(const Network& layer, CE ce) const
{
	Network newLayer{ layer };
	newLayer.push_back(ce);
	if (symmetric && ce.lo != n - 1 - ce.hi)
		newLayer.push_back({ (uint8_t)(n - 1 - ce.hi), (uint8_t)(n - 1 - ce.lo) });
	return newLayer;
}

LayerDAG::Vertex* LayerDAG::AddCE(Vertex* parent, CE ce) const
{
	// Construct child vertex with its new layer and no children
	Vertex* child = new Vertex(AddCE(parent->layer, ce), 0, {});

	// Set usedChannels
	uint64_t ceMask = (1ULL << ce.lo) | (1ULL << ce.hi);
	if (symmetric) ceMask |= (1ULL << (n - 1 - ce.hi)) | (1ULL << (n - 1 - ce.lo));
	child->usedChannels = parent->usedChannels | ceMask;

	return child;
}

void LayerDAG::ClearOutputs(Vertex* vertex)
{
	vertex->outputs.clear();
	for (Vertex* child : vertex->children)
		ClearOutputs(child);
}

static inline std::set<CE> NetworkDifference(const Network& a, const Network& b)
{
	std::set<CE> aCEs;
	aCEs.insert(a.begin(), a.end());
	for (CE ce : b) aCEs.erase(ce);
	return aCEs;
}

void LayerDAG::PropagateOutputs(Vertex* vertex, Vertex* parent)
{
	// Skip vertices whose outputs have already been calculated
	if (!vertex->outputs.empty()) return;

	// Find all CEs that the child has but the parent does not
	std::set<CE> newCEs = NetworkDifference(vertex->layer, parent->layer);

	// Reduce the output set by these CEs
	std::unordered_set<uint64_t> oldOutputs = parent->outputs;
	std::unordered_set<uint64_t> newOutputs;
	for (CE ce : newCEs)
	{
		uint64_t loMask = (1ULL << ce.lo);
		uint64_t hiMask = (1ULL << ce.hi);
		uint64_t ceMask = loMask | hiMask;
		for (uint64_t output : oldOutputs)
		{
			if ((output & loMask) && (~output & hiMask))
				output ^= ceMask;
			newOutputs.insert(output);
		}

		std::swap(newOutputs, oldOutputs);
		newOutputs.clear();
	}
	vertex->outputs = oldOutputs;

	// Recursively propagate outputs to all children
	for (Vertex* child : vertex->children)
		PropagateOutputs(child, vertex);
}

void LayerDAG::MarkTreeRedundant(Vertex* vertex)
{
	vertex->redundant = true;
	for (Vertex* child : vertex->children)
		MarkTreeRedundant(child);
}

void LayerDAG::FindRedundant(Vertex* vertex, Vertex* parent)
{
	if (parent && parent->outputs == vertex->outputs)
	{
		MarkTreeRedundant(vertex);
		return;
	}

	vertex->redundant = false;
	for (Vertex* child : vertex->children)
		FindRedundant(child, vertex);
}

static inline bool SubsetOrEq(const std::unordered_set<uint64_t>& a, const std::unordered_set<uint64_t>& b)
{
	if (a.size() > b.size()) return false;
	for (uint64_t x : a)
		if (!b.contains(x))
			return false;

	return true;
}

void LayerDAG::FindChildSubsets(Vertex* vertex)
{
	bool hasSubsetChild = false;
	for (Vertex* child : vertex->children)
	{
		if (SubsetOrEq(child->outputs, vertex->outputs))
		{
			hasSubsetChild = true;
			break;
		}
	}

	vertex->childSubset = hasSubsetChild;
	for (Vertex* child : vertex->children)
		FindChildSubsets(child);
}