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
	prevPrefixes.emplace_back(Network{});
	while (prevD < d)
	{
		// Generation
		CachePreviousOutputs();
		Generate(prevD == 0);

		// Pruning phase 1
		ForwardPruneMulti(100);
		std::reverse(globalPrefixes.begin(), globalPrefixes.end());
		ForwardPruneMulti(100);

		// Pruning phase 2
		ForwardPruneMulti();
		std::reverse(globalPrefixes.begin(), globalPrefixes.end());
		ForwardPruneMulti();

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

FactoredOutputSet PrefixGeneratorV4::GetOutputs(size_t prevIdx, size_t layerIdx)
{
	FactoredOutputSet outputs{ prevOutputs[prevIdx] };
	outputs.ApplyCEs(allLayers[layerIdx]);
	return outputs;
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

void PrefixGeneratorV4::Generate(bool isFirst)
{
	globalPrefixes.clear();

	std::vector<size_t> numOutputs;
	for (size_t layerIdx = 0; layerIdx < allLayers.size(); layerIdx++)
	{
		// Skip un-full layers for depth-1 prefixes
		if (isFirst && allLayers[layerIdx].size() != n / 2) continue;

		for (size_t prevIdx = 0; prevIdx < prevPrefixes.size(); prevIdx++)
		{
			FactoredOutputSet outputs = GetOutputs(prevIdx, layerIdx);
			if (outputs.IsRedundant()) continue;
			globalPrefixes.emplace_back(prevIdx, layerIdx, n);
			numOutputs.push_back(outputs.Size());
		}
	}

	SortProjected(globalPrefixes, numOutputs, false);
}

void PrefixGeneratorV4::PruneWorker(size_t maxSearches)
{
	SubsumptionSolver solver{ n, symmetric, maxSearches };
	for (;;)
	{
		// Get the next prefix from globalPrefixes
		size_t prefixIdx = globalPrefixIdx++;
		if (prefixIdx >= globalPrefixes.size()) return;
		PrefixDescriptor& descriptor = globalPrefixes[prefixIdx];

		// Compute signature
		std::vector<uint64_t> outputs = GetOutputs(descriptor.prevIdx, descriptor.layerIdx).ToVector();
		descriptor.ComputeSignature(outputs);
		auto guard = descriptor.AcquireGuard();
		if (!guard) continue;

		// Check for subsumption in the range [0, prefixIdx)
		for (size_t otherPrefixIdx = 0; otherPrefixIdx < prefixIdx; otherPrefixIdx++)
		{
			PrefixDescriptor& otherDescriptor = globalPrefixes[otherPrefixIdx];

			// Acquire a guard to access the signature
			auto otherGuard = otherDescriptor.AcquireGuard();
			if (!otherGuard) continue;

			// Compare signatures and release guard
			bool signaturePrecheck = (guard->Signature() > otherGuard->Signature());
			otherGuard.reset();
			if (signaturePrecheck) continue;

			// Run a full backtracking subsumption test
			std::vector<uint64_t> otherOutputs = GetOutputs(otherDescriptor.prevIdx, otherDescriptor.layerIdx).ToVector();
			if (solver.Solve(outputs, otherOutputs) == DoesSubsume)
				otherDescriptor.MarkSubsumedAndFree();
		}
	}
}

void PrefixGeneratorV4::SanitizeGlobalPrefixes()
{
	// Erase subsumed prefixes from globalPrefixes
	std::erase_if(globalPrefixes, [](const PrefixDescriptor& descriptor) {
		return descriptor.IsSubsumed();
		});

	// Reset all descriptors
	for (PrefixDescriptor& descriptor : globalPrefixes)
		descriptor.ForceReset();
}

void PrefixGeneratorV4::ForwardPruneMulti(size_t maxSearches)
{
	// Initialize global state
	globalPrefixIdx = 0;

	// Launch worker threads
	size_t numThreads = std::thread::hardware_concurrency() - 1;
	std::vector<std::thread> threads;
	for (size_t i = 0; i < numThreads; i++)
		threads.emplace_back([this, maxSearches]() { PruneWorker(maxSearches); });

	// Logging
	for (;;)
	{
		size_t progress = globalPrefixIdx;
		if (progress >= globalPrefixes.size()) break;
		std::print("{:>7.3f}%\r", (double)progress / globalPrefixes.size() * 100.0);
		std::this_thread::sleep_for(std::chrono::milliseconds{ 20 });
	}

	// Join all worker threads
	for (auto& thread : threads) thread.join();

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