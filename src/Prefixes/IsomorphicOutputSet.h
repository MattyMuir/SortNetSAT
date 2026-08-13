#pragma once
#include <unordered_set>

#include <sortnetutils.h>

class PrefixGenerator;

class IsomorphicOutputSet
{
protected:
	struct OutputsKey
	{
		size_t prevIdx, layerIdx;
		Permutation canonicalPerm;
		uint64_t hash;
	};

	struct OutputsKeyHasher
	{
		uint64_t operator()(const OutputsKey& key) const;
	};

	struct OutputsKeyEq
	{
		PrefixGenerator* generator;

		bool operator()(const OutputsKey& aKey, const OutputsKey& bKey) const;
	};

public:
	IsomorphicOutputSet(PrefixGenerator* generator_);

	bool TryInsert(size_t prevIdx, size_t layerIdx);

protected:
	PrefixGenerator* generator;
	std::unordered_set<OutputsKey, OutputsKeyHasher, OutputsKeyEq> set;
};