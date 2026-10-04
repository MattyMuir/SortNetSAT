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
	SetBuilderPruner(uint8_t n_, uint8_t d_, bool symmetric_, size_t desiredSize_, PrefixSet& prefixes);

	void Prune();

protected:
	// Parameters
	uint8_t n, d;
	bool symmetric;
	size_t desiredSize;
	PrefixSet& globalPrefixes;

	std::vector<const Network*> GetRandomSubset(size_t maxSize) const;
	void CheckWorker();
	void Logger(std::stop_token st);
};