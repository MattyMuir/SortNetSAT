#pragma once
#include <unordered_set>

class PrefixGraph;

class KosarajuSolver
{
public:
	KosarajuSolver(const PrefixGraph& graph_);

	std::vector<std::unordered_set<size_t>> ExtractComponents();

protected:
	const PrefixGraph& graph;

	std::vector<bool> visited, assigned;
	std::vector<size_t> L;

	void Visit(size_t idx);
	void Assign(size_t idx, std::unordered_set<size_t>& component);
};