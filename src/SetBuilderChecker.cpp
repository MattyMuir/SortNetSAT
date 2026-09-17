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
	bool oldValue = unextendable[prefixIdx].exchange(true);
	if (!oldValue) numRemaining--;
}

std::vector<Network> SetBuilderChecker::GetRandomSubset(size_t maxSize) const
{
	// Produce a list of all prefix indices in a random order
	thread_local std::mt19937_64 gen{ std::random_device{}() };
	std::vector<size_t> idxs(globalPrefixes.size());
	std::iota(idxs.begin(), idxs.end(), 0);
	std::shuffle(idxs.begin(), idxs.end(), gen);

	size_t otherSize = (numRemaining + 1) / 2;
	maxSize = std::min(maxSize, otherSize);

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
		// Get a random subset and build an unextendable set
		std::vector<Network> prefixes = GetRandomSubset(5000);
		UnextendableSetBuilder builder{ n, d, symmetric, prefixes };
		auto unextendableSet = builder.Build();

		// Log result
		double satTime = builder.GetTotalSATTime();
		double scoreTime = builder.GetTotalScoreTime();
		if (unextendableSet.empty())
			std::println("\nBuild failed   SAT/Score: {:.2f}/{:.2f}", satTime, scoreTime);
		else
			std::println("\nFound unextendable: |X| = {}   SAT/Score: {:.2f}/{:.2f}", unextendableSet.size(), satTime, scoreTime);
		std::cout.flush();
		if (unextendableSet.empty()) continue;

		SubsumptionSolver solver{ n, symmetric };
		size_t numSubsumed = 0;
		for (size_t prefixIdx = 0; prefixIdx < globalPrefixes.size(); prefixIdx++)
		{
			// Skip already-unextendable prefixes
			if (unextendable[prefixIdx].load(std::memory_order_relaxed)) continue;

			// Get outputs of other prefix
			const Network& otherPrefix = globalPrefixes[prefixIdx];
			std::vector<uint64_t> otherOutputs = FactoredOutputSet{ otherPrefix, n }.ToVector();

			// Check for subsumption
			solver.ForceUntangledPermutation(otherPrefix);
			auto result = solver.Solve(unextendableSet, otherOutputs);
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
		std::cout.flush();
		std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });
	}
}