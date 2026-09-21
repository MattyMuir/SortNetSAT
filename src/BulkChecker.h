#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>

#include <sortnetutils.h>

class BulkChecker
{
protected:
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;
	using Duration = Clock::duration;

public:
	BulkChecker(uint8_t n_, uint8_t d_, bool symmetric_, const std::vector<Network>& prefixes);

	void ShufflePrefixes();
	void CheckRange(size_t start, size_t end);
	void CheckAll();
	std::vector<double> GetSATTimings() const;

protected:
	// Parameters
	uint8_t n, d;
	bool symmetric;
	std::vector<Network> globalPrefixes;
	
	// Global state
	std::atomic<size_t> globalPrefixIdx;
	size_t globalEndIdx;
	std::mutex saveMutex, loggingMutex;

	// Statistics
	TimePoint startTime;
	size_t numComplete;
	Duration totalTime;
	std::vector<double> satTimings;

	void CheckWorker();
	static double ToSeconds(Duration duration);
	void SaveNetwork(const Network& network);
	void LogProgress(size_t prefixIdx, bool extendable, Duration duration);
};