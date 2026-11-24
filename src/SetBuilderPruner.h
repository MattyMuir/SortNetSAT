#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>

#include <sortnetutils.h>

#include "PrefixSet.h"

class SetBuilderPruner
{
public:
	SetBuilderPruner(PrefixSet& prefixes, uint8_t n_, uint8_t d_, bool symmetric_, size_t desiredSize_);

	void Prune();

protected:
	// Parameters
	PrefixSet& globalPrefixes;
	uint8_t n, d;
	bool symmetric;
	size_t desiredSize;

	std::vector<const Network*> GetRandomSubset(size_t maxSize) const;
	void CheckWorker();
	void Logger(std::stop_token st);
};