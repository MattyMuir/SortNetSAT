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

void UnextendableSubsetPruning()
{
	/*
	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;
	Network prefix = ParseNetwork(R"(
		[(0,13),(4,17),(1,2),(15,16),(3,14),(5,6),(11,12),(7,10),(8,9)]
		[(0,11),(6,17),(1,16),(2,9),(8,15),(3,10),(7,14),(4,5),(12,13)]
		[(0,4),(13,17),(1,8),(9,16),(2,15),(3,7),(10,14),(5,12),(6,11)]
		)");
	// ==================

#if 0
	// Find an unextendable subset of 'prefix'
	IncrementalExtender extender{ n, d, symmetric, prefix, true };
	extender.SetParameters(1);
	TIMER(t);
	bool extendable = extender.Extend();
	std::println();
	STOP_LOG(t)
		std::println("{}", extendable ? "SAT" : "UNSAT");
	if (extendable) return 0;
	auto unextendableSet = extender.GetIncludedInputs();

	{
		std::ofstream file{ "unextendable4.txt" };
		for (uint64_t x : unextendableSet)
			file << x << '\n';
	}
#else
	std::ifstream file{ "unextendable.txt" };
	std::vector<uint64_t> unextendableSet;
	std::string line;
	while (std::getline(file, line))
		unextendableSet.push_back(std::stoull(line));
#endif

	NetworkSignature unextendableSig{ n };
	unextendableSig.Construct(unextendableSet);


	// Load all prefixes
	auto allPrefixes = ParsePrefixFile("C:\\Users\\matty\\source\\repos\\SortNetSAT\\prefixes\\18_3_sym.txt");

	SubsumptionSolver solver{ n, symmetric };
	size_t numSubsumed = 0, numPrechecked = 0, totalSearches = 0, maxSearches = 0;
	for (size_t prefixIdx = 1; prefixIdx < allPrefixes.size(); prefixIdx++)
	{
		const Network& otherPrefix = allPrefixes[prefixIdx];
		std::vector<uint64_t> otherOutputs = FactoredOutputSet{ otherPrefix, n }.ToVector();
		NetworkSignature otherSig{ n };
		otherSig.Construct(otherOutputs);

		if (unextendableSig > otherSig)
			numPrechecked++;

		solver.ForceUntangledPermutation(otherPrefix);
		auto result = solver.Solve(unextendableSet, otherOutputs);
		if (result == DoesSubsume)
			numSubsumed++;

		size_t numSearches = solver.GetNumSearches();
		totalSearches += numSearches;
		maxSearches = std::max(maxSearches, numSearches);

		if (prefixIdx % 100 == 0)
			std::print("{}/{}  PR: {:.3f}%   Searches:{:.3f} / {}  \r",
				numSubsumed,
				prefixIdx, (double)numPrechecked / prefixIdx * 100.0,
				(double)totalSearches / prefixIdx,
				maxSearches);
	}
	*/
}

int main()
{
	PrefixGenerator generator{ 14, 3, true };
	generator.LoadPrevious(2, ParsePrefixFile("C:\\Users\\matty\\source\\repos\\SortNetSAT\\prefixes\\14_2_sym.txt"));

	TIMER(t);
	auto allPrefixes = generator.GeneratePrefixes();
	STOP_LOG(t);
}