#include <iostream>
#include <print>
#include <ranges>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "Timer.h"
#include "Prefixes/prefixes.h"
#include "Prefixes/PrefixGenerator.h"
#include "IncrementalExtender.h"
#include "BulkChecker.h"
#include "Prefixes/WindowMinimizer.h"

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

void CheckAllPrefixes()
{
	std::vector<Network> allPrefixes = ParsePrefixFile("C:\\Users\\matty\\source\\repos\\SortNetSAT\\prefixes\\18_3_sym.txt");
	std::ranges::reverse(allPrefixes);

	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;
	// ==================

	auto startTime = std::chrono::system_clock::now();
	for (size_t prefixIdx = 0; prefixIdx < allPrefixes.size(); prefixIdx++)
	{
		const Network& prefix = allPrefixes[prefixIdx];

		IncrementalExtender extender{ n, d, symmetric, prefix };
		bool extendable = extender.Extend();
		auto elapsed = std::chrono::system_clock::now() - startTime;
		std::println("{:<5}   {:>7.3f}%   {:%T}   {:>7.3f}s | {}",
			prefixIdx,
			(double)prefixIdx / allPrefixes.size() * 100.0,
			elapsed,
			std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / (prefixIdx + 1) * 1e-3,
			extendable ? "=== EXTENDABLE ===" : "Unextendable");
	}
}

int main()
{
	/*
	std::println("Starting in {}", std::filesystem::current_path().string());
	std::cout << std::flush;
	BulkChecker checker{ 18, 10, true, "./prefixes/18_3_sym.txt" };
	checker.CheckAll();
	*/

	PrefixGenerator generator{ 16, 3, true };
	generator.LoadPrevious(2, ParsePrefixFile("C:\\Users\\matty\\source\\repos\\SortNetSAT\\prefixes\\16_2_sym.txt"));
	TIMER(t);
	auto allPrefix = generator.GeneratePrefixes();
	STOP_LOG(t);
}