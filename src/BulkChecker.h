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
	BulkChecker(uint8_t n_, uint8_t d_, bool symmetric_, const std::string& filepath);

	void CheckAll();

protected:
	uint8_t n, d;
	bool symmetric;
	std::vector<Network> globalPrefixes;
	
	std::atomic<size_t> globalPrefixIdx;
	TimePoint startTime;
	std::mutex loggingMutex;
	size_t numComplete;
	Duration totalTime;

	void CheckWorker();
	static double ToSeconds(Duration duration);
	void LogProgress(size_t prefixIdx, bool extendable, Duration duration);
};