#include "SetBuilderChecker.h"

#include <iostream>
#include <print>
#include <random>
#include <numeric>
#include <thread>
#include <algorithm>

#include "Prefixes/prefixes.h"
#include "UnextendableSetBuilder.h"

SetBuilderChecker::SetBuilderChecker(uint8_t n_, uint8_t d_, bool symmetric_, const std::string& filepath)
	: n(n_), d(d_), symmetric(symmetric_), globalPrefixes(ParsePrefixFile(filepath)),
	numRemaining(globalPrefixes.size()), unextendable(globalPrefixes.size())
{
	std::println("Loaded {} prefixes", globalPrefixes.size());
	std::cout.flush();
}

void SetBuilderChecker::CheckAll()
{
	size_t numThreads = std::thread::hardware_concurrency() - 1;
	std::vector<std::thread> workers;
	for (size_t threadIdx = 0; threadIdx < numThreads; threadIdx++)
		workers.emplace_back([this]() { CheckWorker(); });

	std::thread logger{ [this]() { Logger(); } };
	logger.join();

	for (std::thread& worker : workers)
		worker.join();
}

void SetBuilderChecker::MarkUnextendable(size_t prefixIdx)
{
	bool isUnextendable = false;
	if (unextendable[prefixIdx].compare_exchange_strong(isUnextendable, true))
		numRemaining--;
}

std::vector<Network> SetBuilderChecker::GetRandomSubset(size_t maxSize) const
{
	// Produce a list of all prefix indices in a random order
	thread_local std::mt19937_64 gen{ std::random_device{}() };
	std::vector<size_t> idxs(globalPrefixes.size());
	std::iota(idxs.begin(), idxs.end(), 0);
	std::shuffle(idxs.begin(), idxs.end(), gen);

	// Select the first 'maxSize' prefixes which are not already unextendable
	std::vector<Network> prefixes;
	for (size_t prefixIdx : idxs)
	{
		if (unextendable[prefixIdx].load(std::memory_order_relaxed)) continue;
		prefixes.push_back(globalPrefixes[prefixIdx]);
		if (prefixes.size() == maxSize) break;
	}

	return prefixes;
}

void SetBuilderChecker::CheckWorker()
{
	for (;;)
	{
		std::vector<Network> prefixes = GetRandomSubset(5000);
		UnextendableSetBuilder builder{ n, d, symmetric, prefixes };
		auto unextendable = builder.Build();
		double satTime = builder.GetTotalSATTime();
		double scoreTime = builder.GetTotalScoreTime();
		std::println("\nFound unextendable: |X| = {}   SAT/Score: {:.2f}/{:.2f}", unextendable.size(), satTime, scoreTime);

		SubsumptionSolver solver{ n, symmetric };
		size_t numSubsumed = 0;
		for (size_t prefixIdx = 0; prefixIdx < globalPrefixes.size(); prefixIdx++)
		{
			const Network& otherPrefix = globalPrefixes[prefixIdx];
			std::vector<uint64_t> otherOutputs = FactoredOutputSet{ otherPrefix, n }.ToVector();

			solver.ForceUntangledPermutation(otherPrefix);
			auto result = solver.Solve(unextendable, otherOutputs);
			if (result == DoesSubsume)
				MarkUnextendable(prefixIdx);
		}
	}
}

void SetBuilderChecker::Logger()
{
	for (;;)
	{
		std::print("Remaining: {}     \r", numRemaining.load(std::memory_order_relaxed));
		std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });
	}
}