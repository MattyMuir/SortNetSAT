#include "IsomorphicOutputSet.h"

#include <algorithm>

#include "PrefixGenerator.h"
#include "fastcanonize.h"
#include "gicanonize.h"

uint64_t IsomorphicOutputSet::OutputsKeyHasher::operator()(const OutputsKey& key) const
{
	return key.hash;
}

static inline bool AreEqual(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b, uint8_t n)
{
	if (a.size() != b.size()) return false;

	// Initialize LUT
	size_t outputSpaceSize = 1ULL << n;
	thread_local BitVec hasOutput(outputSpaceSize);
	if (hasOutput.Size() < outputSpaceSize)
		hasOutput.Resize(outputSpaceSize);

	// Fill LUT with elements in a
	for (uint64_t ax : a)
		hasOutput.SetBit(ax);

	// Check if all elements in b are in a
	bool areEqual = std::ranges::all_of(b, [](uint64_t bx) { return hasOutput[bx]; });

	// Reset LUT
	for (uint64_t ax : a)
		hasOutput.ClearBitLazy(ax);

	return areEqual;
}

bool IsomorphicOutputSet::OutputsKeyEq::operator()(const OutputsKey& aKey, const OutputsKey& bKey) const
{
	if (aKey.hash != bKey.hash) return false;

	std::vector<uint64_t> aOutputs = generator->GetOutputs(aKey.prevIdx, aKey.layerIdx).ToVector();
	std::vector<uint64_t> bOutputs = generator->GetOutputs(bKey.prevIdx, bKey.layerIdx).ToVector();

	for (uint64_t& x : aOutputs) x = aKey.canonicalPerm(x);
	for (uint64_t& x : bOutputs) x = bKey.canonicalPerm(x);

	return AreEqual(aOutputs, bOutputs, aKey.canonicalPerm.size());
}

IsomorphicOutputSet::IsomorphicOutputSet(PrefixGenerator* generator_)
	: generator(generator_), set(0, OutputsKeyHasher{}, OutputsKeyEq{ generator }) {}

static inline Permutation CanonicalPermutation(const std::vector<uint64_t>& outputs, uint8_t n, bool symmetric)
{
	auto fastPerm = FastCanonize(outputs, n, symmetric);
	if (fastPerm) return *fastPerm;

	return GICanonize(outputs, n, symmetric);
}

bool IsomorphicOutputSet::TryInsert(size_t prevIdx, size_t layerIdx)
{
	// Compute the canonical permutation of these outputs
	std::vector<uint64_t> outputs = generator->GetOutputs(prevIdx, layerIdx).ToVector();
	Permutation canonicalPerm = CanonicalPermutation(outputs, generator->n, generator->symmetric);
	for (uint64_t& x : outputs) x = canonicalPerm(x);
	uint64_t hash = HashOutputs(outputs);

	// Insert into set
	auto [it, inserted] = set.emplace(OutputsKey{ prevIdx, layerIdx, canonicalPerm, hash });
	return inserted;
}