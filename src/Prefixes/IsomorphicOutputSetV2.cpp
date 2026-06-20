#include "IsomorphicOutputSetV2.h"

#include "OutputGraph.h"

size_t IsomorphicOutputSetV2::OutputsKeyHasher::operator()(const OutputsKey& key) const
{
	return key.hash;
}

bool IsomorphicOutputSetV2::OutputsKeyEq::operator()(const OutputsKey& aKey, const OutputsKey& bKey) const
{
	if (aKey.hash != bKey.hash) return false;

	OutputSet aOutputs = GetOutputs(aKey.prefix, aKey.canonicalPerm.size());
	OutputSet bOutputs = GetOutputs(bKey.prefix, bKey.canonicalPerm.size());

	OutputSet aOutputsCanonical = Permute(aOutputs, aKey.canonicalPerm);
	OutputSet bOutputsCanonical = Permute(bOutputs, bKey.canonicalPerm);

	return aOutputsCanonical == bOutputsCanonical;
}

IsomorphicOutputSetV2::IsomorphicOutputSetV2(uint8_t n_)
	: n(n_) {}

void IsomorphicOutputSetV2::Insert(const Network& prefix, size_t idx)
{
	// Compute the canonical permutation of these outputs
	OutputSet outputs = GetOutputs(prefix, n);
	OutputGraph graph{ outputs, n };
	std::vector<uint8_t> canonicalPerm = graph.GetPerm();
	OutputSet outputsCanonical = Permute(outputs, canonicalPerm);
	size_t hash = OutputSetHasher{}(outputsCanonical);

	// Insert into map
	auto [it, inserted] = map.try_emplace(OutputsKey{ prefix, canonicalPerm, hash }, std::vector<size_t>{ idx });
	if (!inserted) it->second.push_back(idx);
}

void IsomorphicOutputSetV2::Merge(const IsomorphicOutputSetV2& other)
{
	for (const auto& [key, eqClass] : other.map)
	{
		auto [it, inserted] = map.try_emplace(key, eqClass);
		if (!inserted)
			it->second.insert(it->second.end(), eqClass.begin(), eqClass.end());
	}
}

std::vector<std::vector<size_t>> IsomorphicOutputSetV2::GetEquivalenceClasses() const
{
	std::vector<std::vector<size_t>> equivalenceClasses;
	equivalenceClasses.reserve(map.size());
	for (const auto& [_, eqClass] : map)
		equivalenceClasses.push_back(eqClass);
	return equivalenceClasses;
}