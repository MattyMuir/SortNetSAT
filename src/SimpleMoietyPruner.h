#pragma once
#include <optional>
#include <mutex>

#include "PrefixSet.h"

class SimpleMoietyPruner
{
public:
	SimpleMoietyPruner(PrefixSet& prefixes, uint8_t n_, uint8_t d_, bool symmetric_, size_t totalMoieties_);

	void Prune();

protected:
	// Parameters
	PrefixSet& globalPrefixes;
	uint8_t n, d;
	bool symmetric;
	size_t totalMoieties;

	// State
	size_t numMoieties = 0;
	std::mutex nextIdxMutex;
	size_t nextPrefixIdx = 0;

	void PruneWorker();
	size_t PropagateMoiety(const std::vector<uint64_t>& moiety);
	std::optional<size_t> GetPrefixIdx();
};