#include "PrefixGeneratorV4.h"

#include <print>
#include <random>
#include <numeric>

PrefixGeneratorV4::PrefixGeneratorV4(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_), allLayers(GetAllLayers(n, symmetric)) {}

void PrefixGeneratorV4::LoadPrevious(uint8_t prevD_, const std::vector<Network>& prevPrefixes_)
{
	prevD = prevD_;
	prevPrefixes = prevPrefixes_;
}

std::vector<Network> PrefixGeneratorV4::GeneratePrefixes()
{
	if (!prevD)
		prevPrefixes = { Network{} };

	while (prevD < d)
	{
		// Generation
		CachePreviousOutputs();
		GenerateMulti(prevD == 0);

		// Pruning
		PruneMulti(100);
		PruneMulti();

		// Update prevPrefixes
		prevPrefixes = GetAllPrefixes();
		prevD++;

		// Logging
		std::println("Number of {}-layer prefixes: {}", prevD, prevPrefixes.size());
	}

	return prevPrefixes;
}

void PrefixGeneratorV4::CachePreviousOutputs()
{
	prevOutputs.clear();
	for (const Network& prevPrefix : prevPrefixes)
		prevOutputs.push_back(FactoredOutputSet{ prevPrefix, n });
}

FactoredOutputSet PrefixGeneratorV4::GetOutputs(size_t prevIdx, size_t layerIdx) const
{
	FactoredOutputSet outputs{ prevOutputs[prevIdx] };
	outputs.ApplyCEs(allLayers[layerIdx]);
	return outputs;
}

void PrefixGeneratorV4::GenerateWorker(bool isFirst)
{
	std::vector<PrefixDescriptor> prefixes;
	std::vector<size_t> numOutputs;
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
			prefixes.emplace_back(prevIdx, layerIdx, n);
			numOutputs.push_back(outputs.Size());
		}
	}

	// Add results to global variables (serializing threads here is a minor overhead)
	std::lock_guard lock{ appendMutex };
	globalPrefixes.insert(globalPrefixes.end(), std::make_move_iterator(prefixes.begin()), std::make_move_iterator(prefixes.end()));
	globalNumOutputs.insert(globalNumOutputs.end(), numOutputs.begin(), numOutputs.end());
}

template <typename Ty, typename Proj>
static inline void SortProjected(std::vector<Ty>& arr, const std::vector<Proj>& proj, bool reverse = false)
{
	// Initialize index array
	size_t n = arr.size();
	std::vector<std::size_t> idxs(n);
	std::iota(idxs.begin(), idxs.end(), std::size_t{ 0 });

	// Sort index array
	std::sort(idxs.begin(), idxs.end(), [&proj, reverse](size_t idx0, size_t idx1) {
		return reverse ? (proj[idx0] > proj[idx1]) : (proj[idx0] < proj[idx1]);
		});

	// Apply the permutation to arr in-place using cycle following
	for (size_t i = 0; i < n; ++i)
	{
		// Check if element already correctly positioned
		if (idxs[i] == i) continue;

		Ty temp = std::move(arr[i]);
		std::size_t j = i;

		// Follow the cycle
		while (idxs[j] != i)
		{
			size_t next = idxs[j];
			arr[j] = std::move(arr[next]);
			idxs[j] = j;
			j = next;
		}

		// One final swap to close the cycle
		arr[j] = std::move(temp);
		idxs[j] = j;
	}
}

void PrefixGeneratorV4::GenerateMulti(bool isFirst)
{
	// Reset global state
	globalPrefixes.clear();
	globalNumOutputs.clear();
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
	SortProjected(globalPrefixes, globalNumOutputs, true);
	globalNumOutputs.clear();
}

void PrefixGeneratorV4::SanitizeGlobalPrefixes()
{
	// Erase subsumed prefixes from globalPrefixes
	std::erase_if(globalPrefixes, [](const PrefixDescriptor& descriptor) {
		return descriptor.isSubsumed;
		});

	// Reset all descriptors
	for (PrefixDescriptor& descriptor : globalPrefixes)
		descriptor.ResetUnatomic();
}

void PrefixGeneratorV4::PruneWorker(size_t workerIdx, size_t maxSearches)
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

		// Check if subsumed in the range [0, prefixIdx)
		bool subsumed = false;
		for (size_t otherPrefixIdx = 0; otherPrefixIdx < prefixIdx; otherPrefixIdx++)
		{
			// Check if either descriptor is subsumed
			PrefixDescriptor& otherDescriptor = globalPrefixes[prefixIdx - 1 - otherPrefixIdx];
			if (descriptor.isSubsumed) break;
			if (otherDescriptor.isSubsumed) continue;

			// Compare signatures
			otherDescriptor.WaitForSignature();
			if (descriptor.signature.GetNumOutputs() > otherDescriptor.signature.GetNumOutputs()) break;
			if (otherDescriptor.signature > descriptor.signature) continue;

			// Increment thread counter
			threadCounters[workerIdx].value.store(++threadCounter, std::memory_order_release);

			// Run a full backtracking subsumption test
			std::vector<uint64_t> otherOutputs = GetOutputs(otherDescriptor.prevIdx, otherDescriptor.layerIdx).ToVector();
			if (symmetric) std::erase_if(otherOutputs, [this](uint64_t x) { return HasSmallerMirror(n, x); });
			if (solver.Solve(otherOutputs, outputs) == DoesSubsume)
			{
				subsumed = true;
				break;
			}
		}

		if (subsumed)
		{
			descriptor.MarkSubsumed();
			continue;
		}

		// Now that outputs is on the LHS of the subsumption tests, strip mirrors
		if (symmetric)
			std::erase_if(outputs, [this](uint64_t x) { return HasSmallerMirror(n, x); });

		// Check for subsumption in the range [0, prefixIdx)
		for (size_t otherPrefixIdx = 0; otherPrefixIdx < prefixIdx; otherPrefixIdx++)
		{
			// Check if either descriptor is subsumed
			PrefixDescriptor& otherDescriptor = globalPrefixes[otherPrefixIdx];
			if (descriptor.isSubsumed) break;
			if (otherDescriptor.isSubsumed) continue;

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

void PrefixGeneratorV4::CleanupWorker()
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

void PrefixGeneratorV4::PruneMulti(size_t maxSearches)
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
	std::println("Pruning 100.000%");

	SanitizeGlobalPrefixes();
}

std::vector<Network> PrefixGeneratorV4::GetAllPrefixes()
{
	std::vector<Network> networks;
	networks.reserve(globalPrefixes.size());
	for (const auto& descriptor : globalPrefixes)
		networks.push_back(prevPrefixes[descriptor.prevIdx] + allLayers[descriptor.layerIdx]);
	return networks;
}