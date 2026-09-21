#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>

#include <sortnetutils.h>

class SetBuilderPruner
{
public:
	SetBuilderPruner(uint8_t n_, uint8_t d_, bool symmetric_, size_t desiredSize_, const std::vector<Network>& prefixes);

	void Prune();
	std::vector<Network> GetRemaining() const;

protected:
	// Parameters
	uint8_t n, d;
	bool symmetric;
	size_t desiredSize;
	std::vector<Network> globalPrefixes;

	// Global state
	std::atomic<size_t> numRemaining;
	std::vector<std::atomic<bool>> unextendable;

	void MarkUnextendable(size_t prefixIdx);
	std::vector<Network> GetRandomSubset(size_t maxSize) const;
	void CheckWorker();
	void Logger(std::stop_token st);
};