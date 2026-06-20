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
	OutputsKey key{ n, prefix, 0, std::nullopt };
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
	while (!other.map.empty())
	{
		// Extract the node from the other set
		auto node = other.map.extract(other.map.begin());
		std::vector<size_t> eqClass{ node.mapped() };

		// Insert the node into this map
		// If the key already exists, concatenate the equivalence classes
		auto insertResult = map.insert(std::move(node));
		if (!insertResult.inserted)
		{
			std::vector<size_t>& srcClass = insertResult.position->second;
			srcClass.insert(srcClass.end(), eqClass.begin(), eqClass.end());
		}

		// Forget the graphs in this map
		if (++numMerged % 10 == 0)
			ForgetGraphs();
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

void IsomorphicOutputSet::ForgetGraphs() const
{
	for (const auto& [key, _] : map)
		key.ForgetGraph();
}