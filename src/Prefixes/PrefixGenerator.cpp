#include "PrefixGenerator.h"

#include <print>
#include <random>
#include <numeric>
#include <unordered_set>
#include <algorithm>
#include <ranges>

#include "IsomorphicOutputSet.h"

PrefixGenerator::PrefixGenerator(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_), allLayers(GetAllLayers(n, symmetric)) {}

void PrefixGenerator::LoadPrevious(uint8_t prevD_, const std::vector<Network>& prevPrefixes_)
{
	prevD = prevD_;
	prevPrefixes = prevPrefixes_;
}

std::vector<Network> PrefixGenerator::GeneratePrefixes()
{
	if (!prevD)
		prevPrefixes = { Network{} };

	while (prevD < d)
	{
		// Generation
		CachePreviousOutputs();
		GenerateMulti(prevD == 0);

		// Pruning
		OutputSubsetPruneMulti();
		std::println("Pruned to {}    ", globalPrefixes.size());
		OutputEquivPruneMulti();
		std::println("Pruned to {}    ", globalPrefixes.size());
		PruneMulti();
		/*
		std::println("Pruned to {}    ", globalPrefixes.size());
		PruneMulti();
		*/

		// Update prevPrefixes
		prevPrefixes = GetAllPrefixes();
		prevD++;

		// Logging
		std::println("Number of {}-layer prefixes: {}", prevD, prevPrefixes.size());
	}

	return prevPrefixes;
}

void PrefixGenerator::CachePreviousOutputs()
{
	prevOutputs.clear();
	for (const Network& prevPrefix : prevPrefixes)
		prevOutputs.push_back(FactoredOutputSet{ prevPrefix, n });
}

void PrefixGenerator::GenerateWorker(bool isFirst)
{
	std::vector<PrefixDescriptor> prefixes;
	for (;;)
	{
		// Get a last-layer to generate new prefixes for
		size_t layerIdx = globalLayerIdx.fetch_add(1, std::memory_order_relaxed);
		if (layerIdx >= allLayers.size()) break;

		// Skip un-full layers for depth-1 prefixes
		if (isFirst && allLayers[layerIdx].size() != n / 2) continue;

		// Generate all non-redundant prefixes ending with this last-layer
		for (size_t prevIdx = 0; prevIdx < prevPrefixes.size(); prevIdx++)
		{
			FactoredOutputSet outputs = GetOutputs(prevIdx, layerIdx);
			if (outputs.IsRedundant()) continue;
			prefixes.emplace_back(prevIdx, layerIdx, outputs.Size(), n);
		}
	}

	// Add results to global variables (serializing threads here is a minor overhead)
	std::lock_guard lock{ appendMutex };
	globalPrefixes.insert(globalPrefixes.end(), std::make_move_iterator(prefixes.begin()), std::make_move_iterator(prefixes.end()));
}

void PrefixGenerator::GenerateMulti(bool isFirst)
{
	// Reset global state
	globalPrefixes.clear();
	globalLayerIdx = 0;

	// Launch worker threads
	size_t numThreads = std::thread::hardware_concurrency() - 1;
	std::vector<std::thread> threads;
	for (size_t threadIdx = 0; threadIdx < numThreads; threadIdx++)
		threads.emplace_back([this, isFirst]() { GenerateWorker(isFirst); });

	// Logging
	for (;;)
	{
		size_t progress = globalLayerIdx;
		if (progress >= allLayers.size()) break;
		std::print("Generating {:>7.3f}%\r", (double)progress / allLayers.size() * 100.0);
		std::this_thread::sleep_for(std::chrono::milliseconds{ 20 });
	}

	// Join worker threads
	for (std::thread& thread : threads)
		thread.join();

	std::println("Generated {} prefixes", globalPrefixes.size());

	// Sort prefixes in descending order of num outputs
	std::sort(globalPrefixes.begin(), globalPrefixes.end(), [](const PrefixDescriptor& a, const PrefixDescriptor& b) {
		return a.numOutputs > b.numOutputs;
		});
}

void PrefixGenerator::OutputSubsetPruneWorker()
{
	for (;;)
	{
		// Get the next prefix from globalPrefixes
		size_t prefixIdx = globalPrefixIdx.fetch_add(1, std::memory_order_relaxed);
		if (prefixIdx >= globalPrefixes.size()) break;
		PrefixDescriptor& descriptor = globalPrefixes[prefixIdx];
		if (allLayers[descriptor.layerIdx].size() == n / 2) continue;

		// Compute outputs
		FactoredOutputSet factoredOutputs = GetOutputs(descriptor.prevIdx, descriptor.layerIdx);
		OutputSet outputs{ factoredOutputs };

		// Get layer mask
		Network layer = allLayers[descriptor.layerIdx];
		uint64_t layerMask = 0;
		for (auto [i, j] : layer)
			layerMask |= (1ULL << i) | (1ULL << j);

		// Iterate over all CEs to add
		for (uint8_t i = 0; i + 1 < n; i++)
		{
			for (uint8_t j = i + 1; j < n; j++)
			{
				if (symmetric && i > n - 1 - j) continue;
				if (layerMask & ((1ULL << i) | (1ULL << j))) continue;

				// Compute child outputs
				Network newCEs{ CE{ i, j } };
				if (symmetric) newCEs.emplace_back(n - 1 - j, n - 1 - i);
				FactoredOutputSet childOutputs{ factoredOutputs };
				childOutputs.ApplyCEs(newCEs);

				if (OutputSet::StrictSubset(OutputSet{ std::move(childOutputs) }, outputs))
				{
					descriptor.isSubsumed = true;
					goto next_prefix;
				}
			}
		}
next_prefix:
	}
}

void PrefixGenerator::OutputSubsetPruneMulti()
{
	// Initialize global state
	globalPrefixIdx = 0;

	// Launch worker threads
	size_t numThreads = std::thread::hardware_concurrency() - 1;
	std::vector<std::thread> threads;
	for (size_t i = 0; i < numThreads; i++)
		threads.emplace_back([this]() { OutputSubsetPruneWorker(); });

	for (;;)
	{
		size_t progress = globalPrefixIdx.load(std::memory_order_relaxed);
		if (progress >= globalPrefixes.size()) break;
		std::print("Pruning {:>7.3f}%\r", (double)progress / globalPrefixes.size() * 100.0);
		std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });
	}

	// Join all threads
	for (auto& thread : threads) thread.join();

	SanitizeGlobalPrefixes();
}

void PrefixGenerator::OutputEquivPruneWorker(const std::vector<std::pair<size_t, size_t>>& outputClasses)
{
	for (;;)
	{
		// Get an  output class to work on
		size_t outputClassIdx = globalOutputClass.fetch_add(1, std::memory_order_relaxed);
		if (outputClassIdx >= outputClasses.size()) return;
		auto [classStart, classEnd] = outputClasses[outputClassIdx];
		if (classEnd - classStart <= 1) continue;

		IsomorphicOutputSet graphs{ this };
		for (size_t prefixIdx = classStart; prefixIdx < classEnd; prefixIdx++)
		{
			PrefixDescriptor& descriptor = globalPrefixes[prefixIdx];

			bool inserted = graphs.TryInsert(descriptor.prevIdx, descriptor.layerIdx);
			if (!inserted) descriptor.isSubsumed = true;
		}
	}
}

void PrefixGenerator::OutputEquivPruneMulti()
{
	// Reset global state
	globalOutputClass = 0;

	// Determine output classes
	std::vector<std::pair<size_t, size_t>> outputClasses;
	auto sameNumOutputs = [](const PrefixDescriptor& d1, const PrefixDescriptor& d2) { return d1.numOutputs == d2.numOutputs; };
	for (auto outputClass : globalPrefixes | std::views::chunk_by(sameNumOutputs))
	{
		size_t startIdx = std::distance(globalPrefixes.begin(), outputClass.begin());
		outputClasses.emplace_back(startIdx, startIdx + outputClass.size());
	}

	// Launch worker threads
	size_t numThreads = std::thread::hardware_concurrency() - 1;
	std::vector<std::thread> threads;
	for (size_t i = 0; i < numThreads; i++)
		threads.emplace_back([this, &outputClasses]() { OutputEquivPruneWorker(outputClasses); });

	for (;;)
	{
		size_t classIdx = globalOutputClass.load(std::memory_order_relaxed);
		if (classIdx >= outputClasses.size()) break;
		std::print("Pruning {:>7.3f}%\r", (double)outputClasses[classIdx].first / globalPrefixes.size() * 100.0);
		std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });
	}

	// Join all threads
	for (auto& thread : threads) thread.join();

	SanitizeGlobalPrefixes();
}

void PrefixGenerator::PruneWorker(size_t workerIdx, size_t maxSearches)
{
	SubsumptionSolver solver{ n, symmetric, maxSearches };
	uint64_t threadCounter = 0;

	for (;;)
	{
		// Get the next prefix from globalPrefixes
		size_t prefixIdx = globalPrefixIdx.fetch_add(1, std::memory_order_relaxed);
		if (prefixIdx >= globalPrefixes.size()) break;
		PrefixDescriptor& descriptor = globalPrefixes[prefixIdx];

		// Compute signature
		std::vector<uint64_t> outputs = GetOutputs(descriptor.prevIdx, descriptor.layerIdx).ToVector();
		descriptor.ComputeSignature(outputs);

		// Check for subsumption in the range [0, prefixIdx)
		for (size_t otherPrefixIdx = 0; otherPrefixIdx < prefixIdx; otherPrefixIdx++)
		{
			// Check if either descriptor is subsumed
			PrefixDescriptor& otherDescriptor = globalPrefixes[otherPrefixIdx];
			if (descriptor.isSubsumed) break;
			if (otherDescriptor.isSubsumed) continue;

			// Thanks to equivalent-output pruning, skip descriptors with the same number of outputs
			if (otherDescriptor.numOutputs == descriptor.numOutputs) break;

			// Compare signatures
			otherDescriptor.WaitForSignature();
			if (descriptor.signature > otherDescriptor.signature) continue;

			// Increment thread counter
			threadCounters[workerIdx].value.store(++threadCounter, std::memory_order_release);

			// Run a full backtracking subsumption test
			std::vector<uint64_t> otherOutputs = GetOutputs(otherDescriptor.prevIdx, otherDescriptor.layerIdx).ToVector();
			if (solver.Solve(outputs, otherOutputs) == DoesSubsume)
				otherDescriptor.MarkSubsumed();
		}
	}

	threadCounters[workerIdx].value.store(UINT64_MAX, std::memory_order_release);
}

void PrefixGenerator::CleanupWorker()
{
	for (;;)
	{
		if (globalPrefixIdx.load(std::memory_order_relaxed) >= globalPrefixes.size())
			break;

		// Get a list of all prefixes that need freeing
		std::vector<size_t> freeList;
		for (size_t prefixIdx = 0; prefixIdx < globalPrefixes.size(); prefixIdx++)
			if (globalPrefixes[prefixIdx].isSubsumed && !globalPrefixes[prefixIdx].IsFreed())
				freeList.push_back(prefixIdx);

		// Take a snapshot of all thread counters
		std::vector<size_t> snapshots;
		for (size_t workerIdx = 0; workerIdx < threadCounters.size(); workerIdx++)
			snapshots.push_back(threadCounters[workerIdx].value.load(std::memory_order_acquire));

		// Wait until all snapshots have increased
		for (size_t workerIdx = 0; workerIdx < threadCounters.size(); workerIdx++)
			if (snapshots[workerIdx] != UINT64_MAX)
				while (threadCounters[workerIdx].value.load(std::memory_order_acquire) <= snapshots[workerIdx])
					std::this_thread::yield();

		// Free signatures
		for (size_t prefixIdx : freeList)
			globalPrefixes[prefixIdx].FreeSignature();

		// Log progress
		size_t progress = globalPrefixIdx.load(std::memory_order_relaxed);
		std::print("Pruning {:>7.3f}%\r", (double)progress / globalPrefixes.size() * 100.0);
	}
}

void PrefixGenerator::PruneMulti(size_t maxSearches)
{
	// Initialize global state
	globalPrefixIdx = 0;
	size_t numThreads = std::thread::hardware_concurrency() - 1;
	threadCounters = std::vector<ThreadCounter>(numThreads);

	// Launch worker threads
	std::vector<std::thread> threads;
	for (size_t i = 0; i < numThreads; i++)
		threads.emplace_back([this, i, maxSearches]() { PruneWorker(i, maxSearches); });

	// Launch cleanup thread
	std::thread cleanupThread{ [this]() { CleanupWorker(); } };

	// Join all threads
	for (auto& thread : threads) thread.join();
	cleanupThread.join();

	SanitizeGlobalPrefixes();
}

FactoredOutputSet PrefixGenerator::GetOutputs(size_t prevIdx, size_t layerIdx) const
{
	FactoredOutputSet outputs{ prevOutputs[prevIdx] };
	outputs.ApplyCEs(allLayers[layerIdx]);
	return outputs;
}

void PrefixGenerator::SanitizeGlobalPrefixes()
{
	// Erase subsumed prefixes from globalPrefixes
	std::erase_if(globalPrefixes, [](const PrefixDescriptor& descriptor) {
		return descriptor.isSubsumed;
		});

	// Reset all descriptors
	for (PrefixDescriptor& descriptor : globalPrefixes)
		descriptor.ResetUnatomic();
}

std::vector<Network> PrefixGenerator::GetAllPrefixes()
{
	std::vector<Network> networks;
	networks.reserve(globalPrefixes.size());
	for (const auto& descriptor : globalPrefixes)
		networks.push_back(prevPrefixes[descriptor.prevIdx] + allLayers[descriptor.layerIdx]);
	return networks;
}