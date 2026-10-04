#include "BulkChecker.h"

#include <print>
#include <iostream>
#include <fstream>
#include <thread>
#include <algorithm>
#include <random>

#include "IncrementalExtender.h"

BulkChecker::BulkChecker(uint8_t n_, uint8_t d_, bool symmetric_, PrefixSet& prefixes)
	: n(n_), d(d_), symmetric(symmetric_), globalPrefixes(prefixes) {}

void BulkChecker::CheckRange(size_t start, size_t end)
{
	// Reset global state
	globalPrefixIdx = start;
	globalEndIdx = end;
	startTime = Clock::now();
	numComplete = 0;
	totalTime = Duration{ 0 };

	// Launch worker threads
	size_t numThreads = std::thread::hardware_concurrency() - 1;
	std::vector<std::thread> threads;
	for (size_t i = 0; i < numThreads; i++)
		threads.emplace_back([this]() { CheckWorker(); });

	for (auto& thread : threads)
		thread.join();
}

void BulkChecker::CheckAll()
{
	CheckRange(0, globalPrefixes.Size());
}

std::vector<double> BulkChecker::GetSATTimings() const
{
	return satTimings;
}

void BulkChecker::CheckWorker()
{
	for (;;)
	{
		// Get the next prefix from globalPrefixes
		size_t prefixIdx = globalPrefixIdx.fetch_add(1, std::memory_order_relaxed);
		if (prefixIdx >= globalEndIdx) break;
		const Network& prefix = globalPrefixes[prefixIdx];

		// Check if this prefix is extendable
		IncrementalExtender extender{ n, d, symmetric, prefix };
		auto start = Clock::now();
		bool extendable = extender.Extend();
		auto end = Clock::now();

		globalPrefixes.SetStatus(prefixIdx, extendable);
		LogProgress(prefixIdx, extendable, end - start);
	}
}

double BulkChecker::ToSeconds(Duration duration)
{
	using namespace std::chrono_literals;
	static constexpr size_t CountPerSec = std::chrono::duration_cast<Duration>(1s).count();
	return (double)duration.count() / CountPerSec;
}

void BulkChecker::LogProgress(size_t prefixIdx, bool extendable, Duration duration)
{
	std::lock_guard lock{ loggingMutex };

	// Update statistics
	numComplete++;
	totalTime += duration;
	satTimings.push_back(ToSeconds(duration));

	std::println("Completed {} [{:.5f}%] (Per-thread {:.3f}s) (Overall {:.3f}s)  |   Prefix N.{} is {}",
		numComplete,
		(double)numComplete / globalPrefixes.Size() * 100.0,
		ToSeconds(totalTime) / numComplete,
		ToSeconds(Clock::now() - startTime) / numComplete,
		prefixIdx,
		extendable ? "===== EXTENDABLE =====" : "Unextendable");
	std::cout.flush();
}