#pragma once
#include <unordered_map>

#include <sortnetutils.h>

class IsomorphicOutputSetV2
{
protected:
	struct OutputsKey
	{
		Network prefix;
		std::vector<uint8_t> canonicalPerm;
		size_t hash;
	};

	struct OutputsKeyHasher
	{
		size_t operator()(const OutputsKey& key) const;
	};

	struct OutputsKeyEq
	{
		bool operator()(const OutputsKey& aKey, const OutputsKey& bKey) const;
	};

public:
	IsomorphicOutputSetV2(uint8_t n_);
	IsomorphicOutputSetV2(const IsomorphicOutputSetV2& other) = delete;

	void Insert(const Network& prefix, size_t idx);
	void Merge(const IsomorphicOutputSetV2& other);
	std::vector<std::vector<size_t>> GetEquivalenceClasses() const;

protected:
	uint8_t n;
	std::unordered_map<OutputsKey, std::vector<size_t>, OutputsKeyHasher, OutputsKeyEq> map;
};