#include "KosarajuSolver.h"

#include <ranges>
#include <stack>

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

void KosarajuSolver::Visit(size_t startIdx)
{
	if (visited[startIdx]) return;

	std::stack<std::pair<size_t, bool>> stack;
	stack.emplace(startIdx, false);

	while (!stack.empty())
	{
		auto [idx, ready] = stack.top();
		stack.pop();

		if (ready)
		{
			L.push_back(idx);
			continue;
		}

		if (visited[idx])
			continue;

		visited[idx] = true;

		stack.emplace(idx, true);

		for (auto* child : graph.idxToVertex[idx]->outgoing | std::views::reverse)
			stack.emplace(child->idx, false);
	}
}

void KosarajuSolver::Assign(size_t startIdx, std::unordered_set<size_t>& component)
{
	if (assigned[startIdx]) return;

	std::stack<size_t> stack;
	stack.push(startIdx);

	while (!stack.empty())
	{
		size_t idx = stack.top();
		stack.pop();

		if (assigned[idx])
			continue;

		assigned[idx] = true;
		component.insert(idx);

		for (auto* child : graph.idxToVertex[idx]->incoming)
			stack.push(child->idx);
	}
}