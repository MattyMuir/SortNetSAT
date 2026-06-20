#include "IsomorphicOutputSet.h"

#include <digraph.hh>

size_t IsomorphicOutputSet::OutputsKeyHasher::operator()(const OutputsKey& key) const
{
	return key.hash;
}

bool IsomorphicOutputSet::OutputsKeyEq::operator()(const OutputsKey& aKey, const OutputsKey& bKey) const
{
	if (aKey.hash != bKey.hash) return false;

	OutputSet aOutputs = GetOutputs(aKey.prefix, aKey.canonicalPerm.size());
	OutputSet bOutputs = GetOutputs(bKey.prefix, bKey.canonicalPerm.size());

	OutputSet aOutputsCanonical = Permute(aOutputs, aKey.canonicalPerm);
	OutputSet bOutputsCanonical = Permute(bOutputs, bKey.canonicalPerm);

	return aOutputsCanonical == bOutputsCanonical;
}

IsomorphicOutputSet::IsomorphicOutputSet(uint8_t n_)
	: n(n_) {}

void IsomorphicOutputSet::Insert(const Network& prefix, size_t idx)
{
	// Compute the canonical permutation of these outputs
	OutputSet outputs = GetOutputs(prefix, n);
	std::vector<uint8_t> canonicalPerm = GetCanonicalPermutation(outputs);
	OutputSet outputsCanonical = Permute(outputs, canonicalPerm);
	size_t hash = OutputSetHasher{}(outputsCanonical);

	// Insert into map
	auto [it, inserted] = map.try_emplace(OutputsKey{ prefix, canonicalPerm, hash }, std::vector<size_t>{ idx });
	if (!inserted) it->second.push_back(idx);
}

void IsomorphicOutputSet::Merge(const IsomorphicOutputSet& other)
{
	for (const auto& [key, eqClass] : other.map)
	{
		auto [it, inserted] = map.try_emplace(key, eqClass);
		if (!inserted)
			it->second.insert(it->second.end(), eqClass.begin(), eqClass.end());
	}
}

std::vector<std::vector<size_t>> IsomorphicOutputSet::GetEquivalenceClasses() const
{
	std::vector<std::vector<size_t>> equivalenceClasses;
	equivalenceClasses.reserve(map.size());
	for (const auto& [_, eqClass] : map)
		equivalenceClasses.push_back(eqClass);
	return equivalenceClasses;
}

std::vector<uint8_t> IsomorphicOutputSet::GetCanonicalPermutation(const OutputSet& outputs) const
{
	enum VertexType
	{
		VertexBit,
		VertexOutput
	};

	bliss::Digraph g;

	// Create bit vertices
	std::vector<uint32_t> bitVertices;
	bitVertices.reserve(n);
	for (uint8_t bi = 0; bi < n; bi++)
		bitVertices.push_back(g.add_vertex(VertexBit));

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

	// Convert the permutation from uint32's to uint8's
	std::vector<uint8_t> u8perm(n);
	for (size_t i = 0; i < n; i++)
		u8perm[i] = u32perm[i];

	// bliss uses the 'scatter' permutation convention so we have to invert it
	return InvertPerm(u8perm);
}