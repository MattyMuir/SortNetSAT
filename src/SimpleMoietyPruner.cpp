#include "SimpleMoietyPruner.h"

#include <thread>

#include <subsumption.h>

#include "IncrementalExtender.h"
#include "Timer.h"

SimpleMoietyPruner::SimpleMoietyPruner(PrefixSet& prefixes, uint8_t n_, uint8_t d_, bool symmetric_, size_t totalMoieties_)
	: globalPrefixes(prefixes), n(n_), d(d_), symmetric(symmetric_), totalMoieties(totalMoieties_) {}

void SimpleMoietyPruner::Prune()
{
	size_t numThreads = std::thread::hardware_concurrency();
	std::vector<std::thread> workers;
	for (size_t threadIdx = 0; threadIdx < numThreads; threadIdx++)
		workers.emplace_back([this]() { PruneWorker(); });

	for (std::thread& worker : workers)
		worker.join();
}

void SimpleMoietyPruner::PruneWorker()
{
	for (;;)
	{
		// Get a prefix to work on
		auto prefixIdxOpt = GetPrefixIdx();
		if (!prefixIdxOpt) break;
		size_t prefixIdx = *prefixIdxOpt;

		// Check if this prefix is extendable
		IncrementalExtender extender{ n, d, symmetric, globalPrefixes[prefixIdx] };
		Timer extendTimer;
		bool extendable = extender.Extend();
		extendTimer.Stop();
		globalPrefixes.SetStatus(prefixIdx, extendable);

		// Check if this moiety subsumes other prefixes
		auto moiety = extender.GetIncludedInputs();
		Timer propagateTimer;
		size_t numSubsumed = PropagateMoiety(moiety);
		propagateTimer.Stop();

		// Log result
		std::println("Found Moiety: size={},subsumed={},extend_time={:.3f},propagate_time={:.3f},remaining={}",
			moiety.size(),
			numSubsumed,
			extendTimer.GetSeconds(),
			propagateTimer.GetSeconds(),
			globalPrefixes.NumRemaining());
	}
}

size_t SimpleMoietyPruner::PropagateMoiety(const std::vector<uint64_t>& moiety)
{
	SubsumptionSolver solver{ n, symmetric };

	size_t numSubsumed = 0;
	for (size_t prefixIdx = 0; prefixIdx < globalPrefixes.Size(); prefixIdx++)
	{
		// Skip already-unextendable prefixes
		if (globalPrefixes.State(prefixIdx) != PrefixSet::Unknown) continue;

		// Get outputs of other prefix
		const Network& otherPrefix = globalPrefixes[prefixIdx];
		std::vector<uint64_t> otherOutputs = FactoredOutputSet{ otherPrefix, n }.ToVector();

		// Check for subsumption
		auto result = solver.Solve(moiety, otherOutputs, 1, otherPrefix);
		if (result == DoesSubsume)
		{
			globalPrefixes.SetStatus(prefixIdx, false);
			numSubsumed++;
		}
	}

	return numSubsumed;
}

std::optional<size_t> SimpleMoietyPruner::GetPrefixIdx()
{
	std::lock_guard lock{ nextIdxMutex };

	// Check if the maximum number of moieties is reached
	if (numMoieties >= totalMoieties) return std::nullopt;

	// Scan for a prefix whose status is unknown
	for (;; nextPrefixIdx++)
	{
		if (nextPrefixIdx >= globalPrefixes.Size()) return std::nullopt;
		if (globalPrefixes.State(nextPrefixIdx) == PrefixSet::Unknown) break;
	}

	numMoieties++;
	return nextPrefixIdx++;
}