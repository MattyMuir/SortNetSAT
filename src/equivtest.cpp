#include <print>
#include <algorithm>
#include <numeric>
#include <random>

#include <sortnetutils.h>

#include "OldPrefixes/NetworkGraph.h"

Permutation ExpandCentrosymmetric(const Permutation& halfPerm)
{
	uint8_t n = (uint8_t)(halfPerm.size() * 2);
	Permutation full{ halfPerm };
	full.resize(n);
	for (uint8_t k = n / 2; k < n; k++)
		full[k] = n - 1 - halfPerm[n - 1 - k];
	return full;
}

template <typename Func>
bool ForEachPermSym(uint8_t n, Func&& callback)
{
	uint8_t m = n / 2;
	Permutation sigma(m);
	std::iota(sigma.begin(), sigma.end(), 0);
	do
	{
		for (uint32_t orientMask = 0; orientMask < (1u << m); orientMask++)
		{
			Permutation halfPerm(m);
			for (uint8_t k = 0; k < m; k++)
			{
				bool flip = (orientMask >> k) & 1;
				uint8_t lo = sigma[k];
				uint8_t hi = n - 1 - sigma[k];
				halfPerm[k] = flip ? hi : lo;
			}
			if (callback(ExpandCentrosymmetric(halfPerm)))
				return true;
		}
	} while (std::next_permutation(sigma.begin(), sigma.end()));
	return false;
}

template <typename Func>
bool ForEachPerm(uint8_t n, Func&& callback)
{
	Permutation perm(n);
	std::iota(perm.begin(), perm.end(), 0);
	do
	{
		if (callback(perm))
			return true;
	} while (std::next_permutation(perm.begin(), perm.end()));
	return false;
}

bool AreEquivalentNaive(const Network& a, const Network& b, uint8_t n, bool symmetric)
{
	if (a.size() != b.size()) return false;

	auto compFunc = [&a, &b](const Permutation& perm) {
		Network aPerm{ a };
		aPerm.Permute(perm);
		aPerm.Untangle();
		return Network::Identical(aPerm, b);
		};

	if (symmetric) return ForEachPermSym(n, compFunc);
	return ForEachPerm(n, compFunc);
}

bool AreEquivalentFast(const Network& a, const Network& b, uint8_t n, bool symmetric)
{
	NetworkGraph aGraph{ LayeredNetwork{ a }, n, symmetric };
	NetworkGraph bGraph{ LayeredNetwork{ b }, n, symmetric };
	return aGraph == bGraph;
}

void EquivTest()
{
	uint8_t n = 8;
	uint8_t depth = 2;
	bool symmetric = true;

	for (;;)
	{
		Network a = RandomNetworkLayered(n, depth, symmetric);
		Network b = RandomNetworkLayered(n, depth, symmetric);

		bool areEquiv1 = AreEquivalentNaive(a, b, n, symmetric);
		bool areEquiv2 = AreEquivalentFast(a, b, n, symmetric);
		std::println("{} {}", areEquiv1, areEquiv2);

		if (areEquiv1 != areEquiv2)
		{
			std::println("{}\n{}", a, b);
			std::println("{:t}\n\n\n{:t}", a, b);
			return;
		}
	}
}

bool OutputsEquivalentNaive(const OutputSet& a, const OutputSet& b, uint8_t n, bool symmetric)
{
	auto compFunc = [&a, &b](const Permutation& perm) {
		OutputSet aPerm = OutputSet::Permute(a, perm);
		return aPerm == b;
		};

	return symmetric ? ForEachPermSym(n, compFunc) : ForEachPerm(n, compFunc);
}

bliss::Digraph* GetOutputGraph(const OutputSet& outputs, uint8_t n, bool symmetric)
{
	enum VertexType
	{
		VertexBit,
		VertexOutput
	};

	bliss::Digraph g;

	// Create bit vertices
	std::vector<uint32_t> bitVertices;
	for (uint8_t bi = 0; bi < n; bi++)
		bitVertices.push_back(g.add_vertex(VertexBit));

	if (symmetric)
		for (uint8_t bi = 0; bi < n; bi++)
			g.add_edge(bitVertices[bi], bitVertices[n - 1 - bi]);

	// Create output vertices and add edges
	for (uint64_t output : outputs)
	{
		uint32_t v = g.add_vertex(VertexOutput);
		for (uint8_t bi = 0; bi < n; bi++)
			if (output & (1ULL << bi))
				g.add_edge(v, bitVertices[bi]);
	}

	// Compute the canonical perm
	bliss::Stats stats;
	const uint32_t* u32perm = g.canonical_form(stats);

	return g.permute(u32perm);
}

bool OutputsEquivalentFast(const OutputSet& a, const OutputSet& b, uint8_t n, bool symmetric)
{
	bliss::Digraph* aGraph = GetOutputGraph(a, n, symmetric);
	bliss::Digraph* bGraph = GetOutputGraph(b, n, symmetric);
	bool areEquiv = (aGraph->cmp(*bGraph) == 0);
	delete aGraph;
	delete bGraph;
	return areEquiv;
}

void RandomSet(OutputSet& set, uint8_t n, size_t size)
{
	static std::mt19937_64 gen{ std::random_device{}() };
	std::uniform_int_distribution<uint64_t> dist{ 0, (1ULL << n) - 1 };

	for (size_t i = 0; i < size; i++)
	{
		uint64_t x;
		do
		{
			x = dist(gen);
		} while (set.Contains(x));
		set.Insert(x);
	}
}

void RandomEquivSets(OutputSet& a, OutputSet& b, uint8_t n, size_t size)
{
	RandomSet(a, n, size);

	Permutation perm(n);
	std::iota(perm.begin(), perm.end(), 0);

	static std::mt19937_64 gen{ std::random_device{}() };
	std::shuffle(perm.begin(), perm.end(), gen);

	b = OutputSet::Permute(a, perm);
}

void OutputEquivTest()
{
	static std::mt19937_64 gen{ std::random_device{}() };
	std::uniform_real_distribution<float> fDist{ 0.0f, 1.0f };

	uint8_t n = 4;
	bool symmetric = true;
	size_t size = 2;

	for (;;)
	{
		OutputSet a{ n }, b{ n };

		if (fDist(gen) < 0.5f)
		{
			RandomSet(a, n, size);
			RandomSet(b, n, size);
		}
		else RandomEquivSets(a, b, n, size);

		bool equivNaive = OutputsEquivalentNaive(a, b, n, symmetric);
		bool equivFast = OutputsEquivalentFast(a, b, n, symmetric);

		std::println("{} {}", equivNaive, equivFast);
		if (equivNaive != equivFast)
		{
			std::println("{::0>{}b}", a.ToVector(), n);
			std::println("{::0>{}b}", b.ToVector(), n);
			return;
		}
	}
}