#include "BulkChecker.h"

#include <print>
#include <iostream>
#include <thread>
#include <algorithm>

#include "Prefixes/prefixes.h"
#include "IncrementalExtender.h"
#include "SimpleExtender.h"

BulkChecker::BulkChecker(uint8_t n_, uint8_t d_, bool symmetric_, const std::string& filepath)
	: n(n_), d(d_), symmetric(symmetric_), globalPrefixes(ParsePrefixFile(filepath))
{
	std::println("Loaded {} prefixes", globalPrefixes.size());
	std::cout << std::flush;
}

void BulkChecker::CheckAll()
{
	// Reset global state
	globalPrefixIdx = 0;
	startTime = Clock::now();
	numComplete = 0;
	totalTime = Duration{ 0 };

	// Launch worker threads
	size_t numThreads = std::thread::hardware_concurrency() - 1;
	//size_t numThreads = 2;
	std::vector<std::thread> threads;
	for (size_t i = 0; i < numThreads; i++)
		threads.emplace_back([this]() { CheckWorker(); });

	for (auto& thread : threads)
		thread.join();
}

void BulkChecker::CheckWorker()
{
	for (;;)
	{
		// Get the next prefix from globalPrefixes
		size_t prefixIdx = globalPrefixIdx.fetch_add(1, std::memory_order_relaxed);
		if (prefixIdx >= globalPrefixes.size()) break;
		const Network& prefix = globalPrefixes[prefixIdx];

		// Check if this prefix is extendable
		IncrementalExtender extender{ n, d, symmetric, prefix };
		auto start = Clock::now();
		bool extendable = extender.Extend();
		auto end = Clock::now();

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

	numComplete++;
	totalTime += duration;

	std::println("Completed {} [{:.5f}%] (Per-thread {:.3f}s) (Overall {:.3f}s)  |   Prefix N.{} is {}",
		numComplete,
		(double)numComplete / globalPrefixes.size() * 100.0,
		ToSeconds(totalTime) / numComplete,
		ToSeconds(Clock::now() - startTime) / numComplete,
		prefixIdx,
		extendable ? "===== EXTENDABLE =====" : "Unextendable");
	std::cout << std::flush;
}