#pragma once
#include <unordered_set>

#include <sortnetutils.h>

class PrefixGeneratorV4;

class IsomorphicOutputSetV2
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
		PrefixGeneratorV4* generator;

		bool operator()(const OutputsKey& aKey, const OutputsKey& bKey) const;
	};

public:
	IsomorphicOutputSetV2(PrefixGeneratorV4* generator_);

	bool TryInsert(size_t prevIdx, size_t layerIdx);

protected:
	PrefixGeneratorV4* generator;
	std::unordered_set<OutputsKey, OutputsKeyHasher, OutputsKeyEq> set;
};