#include "KosarajuSolver.h"

#include <ranges>

#include "PrefixGraph.h"

KosarajuSolver::KosarajuSolver(const PrefixGraph& graph_)
	: graph(graph_), visited(graph.nextVertexIdx, false), assigned(graph.nextVertexIdx, false)
{
	L.reserve(graph.nextVertexIdx);
}

std::vector<std::unordered_set<size_t>> KosarajuSolver::ExtractComponents()
{
	for (size_t idx = 0; idx < graph.nextVertexIdx; idx++)
		Visit(idx);

	std::vector<std::unordered_set<size_t>> components;
	for (size_t idx : L | std::views::reverse)
	{
		if (assigned[idx]) continue;

		std::unordered_set<size_t> component;
		Assign(idx, component);
		components.push_back(component);
	}

	return components;
}

void KosarajuSolver::Visit(size_t idx)
{
	if (visited[idx]) return;

	// Mark nodes as visited
	visited[idx] = true;

	// Visit all out-children of this node
	for (PrefixGraph::Vertex* outChild : graph.idxToVertex[idx]->outgoing)
		Visit(outChild->idx);

	L.push_back(idx);
}

void KosarajuSolver::Assign(size_t idx, std::unordered_set<size_t>& component)
{
	if (assigned[idx]) return;

	// Assign node to component
	assigned[idx] = true;
	component.insert(idx);

	// Assign all in-children of this node
	PrefixGraph::Vertex* vertex = graph.idxToVertex[idx];
	for (PrefixGraph::Vertex* inChild : graph.idxToVertex[idx]->incoming)
		Assign(inChild->idx, component);
}