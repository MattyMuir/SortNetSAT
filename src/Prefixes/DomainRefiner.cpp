#include "DomainRefiner.h"

#include <algorithm>
#include <bit>
#include <ranges>

#define FOREACH_SET_BIT(x, var)\
auto bits = x;\
auto numSet = std::popcount(bits);\
for (uint8_t _ = 0; _ < numSet; _++)\
{\
	uint8_t var = std::countr_zero(bits);\
	bits &= bits - 1;

#define FOREACH_END }

DomainRefiner::DomainRefiner(uint8_t n_)
	: n(n_), visited(0), matchedSrc(n), outAdj(n * 2), inAdj(n * 2), L(n * 2), sccIdxs(n * 2) {}

bool DomainRefiner::Refine(std::vector<uint64_t>& domains)
{
	ResetState();
	if (!FindMatching(domains)) return false;
	BuildGraph(domains);
	ExtractSCCs();
	DoRefine(domains);

	return true;
}

bool DomainRefiner::TryAugment(uint8_t src, const std::vector<uint64_t>& domains)
{
	uint64_t candidates = domains[src] & ~visited;
	FOREACH_SET_BIT(candidates, dst)
	{
		visited |= (1ULL << dst);

		uint8_t existingSrc = matchedSrc[dst];
		if (existingSrc == None || TryAugment(existingSrc, domains))
		{
			matchedSrc[dst] = src;
			return true;
		}
	}
	FOREACH_END

	return false;
}

bool DomainRefiner::FindMatching(const std::vector<uint64_t>& domains)
{
	// Kuhn's algorithm
	for (uint8_t src = 0; src < n; src++)
	{
		visited = 0;
		if (!TryAugment(src, domains)) return false;
	}
	return true;
}

void DomainRefiner::BuildGraph(const std::vector<uint64_t>& domains)
{
	// Build a bipartite graph so that matched egdes point dst -> src
	for (uint8_t src = 0; src < n; src++)
	{
		FOREACH_SET_BIT(domains[src], dst)
		{
			auto* adj1 = &outAdj;
			auto* adj2 = &inAdj;
			if (matchedSrc[dst] != src) std::swap(adj1, adj2);

			(*adj1)[dst + n] |= 1ULL << src;
			(*adj2)[src] |= 1ULL << (dst + n);
		}
		FOREACH_END
	}
}

void DomainRefiner::Visit(uint8_t v)
{
	if ((visited >> v) & 1ULL) return;
	visited |= 1ULL << v;

	FOREACH_SET_BIT(outAdj[v], out)
		Visit(out);
	FOREACH_END

	L.push_back(v);
}

void DomainRefiner::Assign(uint8_t u, uint8_t idx)
{
	if (sccIdxs[u] != None) return;

	sccIdxs[u] = idx;
	FOREACH_SET_BIT(inAdj[u], in)
		Assign(in, idx);
	FOREACH_END
}

void DomainRefiner::ExtractSCCs()
{
	// Visit traversal
	visited = 0;
	for (uint8_t u = 0; u < n * 2; u++)
		Visit(u);

	// Assign traversal
	uint8_t idx = 0;
	for (uint8_t u : L | std::views::reverse)
	{
		if (sccIdxs[u] != None) continue;
		Assign(u, idx);
		idx++;
	}
}

void DomainRefiner::DoRefine(std::vector<uint64_t>&domains)
{
	for (uint8_t src = 0; src < n; src++)
	{
		FOREACH_SET_BIT(domains[src], dst)
		{
			// If the edge is already matched or has endpoints in the same SCC, keep it
			if (matchedSrc[dst] == src) continue;
			if (sccIdxs[src] == sccIdxs[dst + n]) continue;

			// Remove edge
			domains[src] &= ~(1ULL << dst);
		}
		FOREACH_END
	}
}

void DomainRefiner::ResetState()
{
	std::ranges::fill(matchedSrc, None);
	std::ranges::fill(outAdj, 0);
	std::ranges::fill(inAdj, 0);
	L.clear();
	std::ranges::fill(sccIdxs, None);
}