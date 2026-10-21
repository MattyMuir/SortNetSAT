#include <iostream>
#include <print>
#include <ranges>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <set>

#include "Timer.h"
#include "Prefixes/prefixes.h"
#include "Prefixes/PrefixGenerator.h"
#include "IncrementalExtender.h"
#include "BulkChecker.h"
#include "Prefixes/WindowMinimizer.h"
#include "Prefixes/IntersectionMaximizer.h"
#include "minisatutil.h"
#include "UnextendableSetBuilder.h"
#include "UnextendableSetBuilder2.h"
#include "SetBuilderPruner.h"

std::vector<uint64_t> GetUnextendableSubset()
{
	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;
	Network prefix = ParseNetwork("[(0,13),(4,17),(1,2),(15,16),(3,14),(5,6),(11,12),(7,10),(8,9),(0,11),(6,17),(1,16),(2,7),(10,15),(3,8),(9,14),(4,5),(12,13),(0,4),(13,17),(1,2),(15,16),(3,7),(10,14),(5,12),(6,11),(8,9)]");
	// ==================

	// Run the IncrementalExtender
	IncrementalExtender extender{ n, d, symmetric, prefix, true };
	extender.SetParameters(1);
	TIMER(t);
	bool extendable = extender.Extend();
	std::println();
	STOP_LOG(t);

	// Log result
	std::println("{}", extendable ? "SAT" : "UNSAT");
	if (extendable) return {};

	return extender.GetIncludedInputs();
}

std::vector<uint64_t> LoadOutputSet(const std::string& filepath)
{
	std::ifstream file{ filepath };

	std::string line;
	std::vector<uint64_t> outputs;
	while (std::getline(file, line))
		outputs.push_back(std::stoull(line));

	return outputs;
}

void SaveOutputSet(const std::string& filepath, const std::vector<uint64_t>& set)
{
	std::ofstream file{ filepath };
	for (uint64_t x : set)
		file << x << '\n';
}

void UnextendableSubsetPruning(const std::vector<uint64_t>& unextendableSet)
{
	// === Parameters ===
	uint8_t n = 18;
	bool symmetric = true;
	// ==================

	// Load all prefixes
	auto allPrefixes = ParsePrefixFile("C:\\Users\\matty\\source\\repos\\SortNetSAT\\prefixes\\18_3_sym.txt");

	TIMER(t);
	SubsumptionSolver solver{ n, symmetric };
	size_t numSubsumed = 0;
	for (size_t prefixIdx = 1; prefixIdx < allPrefixes.size(); prefixIdx++)
	{
		const Network& otherPrefix = allPrefixes[prefixIdx];
		std::vector<uint64_t> otherOutputs = FactoredOutputSet{ otherPrefix, n }.ToVector();

		auto result = solver.Solve(unextendableSet, otherOutputs, false, otherPrefix);
		if (result == DoesSubsume)
			numSubsumed++;

		// Log progress
		if (prefixIdx % 100 == 0)
			std::print("{}/{}   \r", numSubsumed, prefixIdx);
	}
	std::println();
	STOP_LOG(t);
}

int main()
{
#if 0
	// Load prefixes
	PrefixSet allPrefixes{ "C:\\Users\\matty\\source\\repos\\SortNetSAT\\prefixes\\16_3_sym.txt" };
	std::println("Loaded {} prefixes", allPrefixes.Size());
	std::cout.flush();

	// Launch saving thread
	std::thread saveThread{ [&]() {
		std::this_thread::sleep_for(std::chrono::seconds{ 60 });

		std::println("Saving...!");
		allPrefixes.SaveStatus("status.txt");
		std::println("Saved!");
		} };

	// Prune prefix set
	SetBuilderPruner pruner{ 18, 5, true, 0, allPrefixes };
	pruner.Prune();

	saveThread.join();
#endif

	Network prefix = ParseNetwork(R"(
		[(0,6),(1,10),(2,15),(3,5),(4,9),(7,16),(8,13),(11,17),(12,14)]
		[(0,12),(1,4),(3,11),(5,17),(6,14),(7,8),(9,10),(13,16)]
		[(1,13),(2,7),(4,16),(6,9),(8,11),(10,15)]
		[(0,1),(2,3),(4,12),(5,13),(7,9),(8,10),(14,15),(16,17)]
		)");

	IncrementalExtender extender{ 18, 11, true, prefix, true };
	bool extendable = extender.Extend();

	if (extendable)
	{
		std::println("Extendable!");
		std::println("{}", extender.GetNetwork());
	}
	else std::println("Unextendable");
}