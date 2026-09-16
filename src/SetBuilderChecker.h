#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>

#include <sortnetutils.h>

class SetBuilderChecker
{
public:
	SetBuilderChecker(uint8_t n_, uint8_t d_, bool symmetric_, const std::string& filepath);

	void CheckAll();

protected:
	// Parameters
	uint8_t n, d;
	bool symmetric;
	std::vector<Network> globalPrefixes;

	// Global state
	std::atomic<size_t> numRemaining;
	std::vector<std::atomic<bool>> unextendable;

	void MarkUnextendable(size_t prefixIdx);
	std::vector<Network> GetRandomSubset(size_t maxSize) const;
	void CheckWorker();
	void Logger();
};