#pragma once
#include <unordered_map>

#include <sortnetutils.h>

#include "OutputGraph.h"

class IsomorphicOutputSet
{
protected:
	struct OutputsKey
	{
		uint8_t n;
		Network prefix;
		uint32_t hash;
		mutable std::optional<OutputGraph> graph;

		void ComputeGraph() const;
		void ForgetGraph() const;
	};

	struct OutputsKeyHasher
	{
		uint32_t operator()(const OutputsKey& key) const;
	};

	struct OutputsKeyEq
	{
		bool operator()(const OutputsKey& aKey, const OutputsKey& bKey) const;
	};

public:
	IsomorphicOutputSet(uint8_t n_);
	IsomorphicOutputSet(const IsomorphicOutputSet& other) = delete;
	IsomorphicOutputSet(IsomorphicOutputSet&& other) = default;

	void Insert(const Network& prefix, size_t idx);
	void Merge(IsomorphicOutputSet&& other);
	std::vector<std::vector<size_t>> GetEquivalenceClasses() const;

protected:
	uint8_t n;
	std::unordered_map<OutputsKey, std::vector<size_t>, OutputsKeyHasher, OutputsKeyEq> map;
	size_t numInserted = 0;

	void ForgetGraphs() const;
};