#include "IsomorphicOutputSet.h"

void IsomorphicOutputSet::OutputsKey::ComputeGraph() const
{
	OutputSet outputSet{ FactoredOutputSet{ prefix, n } };
	graph = OutputGraph{ outputSet, n };
}

void IsomorphicOutputSet::OutputsKey::ForgetGraph() const
{
	graph.reset();
}

uint32_t IsomorphicOutputSet::OutputsKeyHasher::operator()(const OutputsKey& key) const
{
	return key.hash;
}

bool IsomorphicOutputSet::OutputsKeyEq::operator()(const OutputsKey& aKey, const OutputsKey& bKey) const
{
	if (aKey.hash != bKey.hash) return false;
	if (!aKey.graph) aKey.ComputeGraph();
	if (!bKey.graph) bKey.ComputeGraph();
	return aKey.graph == bKey.graph;
}

IsomorphicOutputSet::IsomorphicOutputSet(uint8_t n_)
	: n(n_) {}

void IsomorphicOutputSet::Insert(const Network& prefix, size_t idx)
{
	// Initialize the key object
	OutputsKey key{ n, prefix };
	key.ComputeGraph();
	key.hash = key.graph->GetHash();

	// Insert into map
	auto [it, inserted] = map.try_emplace(std::move(key), std::vector<size_t>{ idx });
	if (!inserted) it->second.push_back(idx);
	numInserted++;

	// Periodically forget graphs to save memory
	if (numInserted % 10 == 0)
		ForgetGraphs();
}

void IsomorphicOutputSet::Merge(IsomorphicOutputSet&& other)
{
	size_t numMerged = 0;
	for (auto& [key, vec] : other.map)
	{
		auto [it, inserted] = map.try_emplace(std::move(const_cast<OutputsKey&>(key)), std::move(vec));
		if (!inserted) it->second.insert(it->second.end(),
			std::make_move_iterator(vec.begin()),
			std::make_move_iterator(vec.end()));

		//ForgetGraphs();
	}
}

std::vector<std::vector<size_t>> IsomorphicOutputSet::GetEquivalenceClasses() const
{
	std::vector<std::vector<size_t>> equivalenceClasses;
	for (const auto& [_, eqClass] : map)
		equivalenceClasses.push_back(eqClass);
	return equivalenceClasses;
}

void IsomorphicOutputSet::ForgetGraphs() const
{
	for (const auto& [key, _] : map)
		key.ForgetGraph();
}