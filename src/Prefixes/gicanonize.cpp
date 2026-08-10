#include "gicanonize.h"

#include <numeric>

#include <graph.hh>

static inline Permutation ExtractPermUnsym(const uint32_t* bitsPerm, uint8_t n)
{
	// Convert gather to scatter
	Permutation perm(n);
	for (size_t src = 0; src < n; src++)
		perm[bitsPerm[src]] = src;
	return perm;
}

static inline Permutation ExtractPermSym(const uint32_t* bitsPerm, uint8_t n)
{
	// Initialize the permutation
	Permutation perm(n);
	std::iota(perm.begin(), perm.end(), 0);

	// For each mirror pair, keep only the one with a smaller destination
	std::erase_if(perm, [&](uint8_t i) {
		return bitsPerm[i] > bitsPerm[n - 1 - i];
		});

	// Sort by destination
	std::ranges::sort(perm, {}, [&](uint8_t i) { return bitsPerm[i]; });

	// Expand centro-symmetrically
	perm.resize(n);
	for (uint8_t i = 0; i < n / 2; i++)
		perm[n - 1 - i] = n - 1 - perm[i];

	return perm;
}

Permutation GICanonize(const std::vector<uint64_t>& outputs, uint8_t n, bool symmetric)
{
	enum VertexType { VertexBit, VertexOutput };

	// Create bit vertices
	bliss::Graph g{ (uint32_t)(outputs.size() + n) };
	for (uint8_t bi = 0; bi < n; bi++)
		g.change_color(bi, VertexBit);
	if (symmetric)
		for (uint8_t bi = 0; bi < n / 2; bi++)
			g.add_edge(bi, n - 1 - bi);

	// Create output vertices and add edges
	uint32_t outputVertex = n;
	for (uint64_t output : outputs)
	{
		g.change_color(outputVertex, VertexOutput);
		for (uint8_t bi = 0; bi < n; bi++)
			if (output & (1ULL << bi))
				g.add_edge(outputVertex, bi);
		outputVertex++;
	}

	// Compute the canonical perm
	bliss::Stats stats;
	const uint32_t* perm = g.canonical_form(stats);

	// Extract permutation from canonical graph
	return symmetric ? ExtractPermSym(perm, n) : ExtractPermUnsym(perm, n);
}