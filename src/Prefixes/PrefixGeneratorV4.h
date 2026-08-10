#pragma once
#include <thread>
#include <atomic>
#include <mutex>

#include <sortnetutils.h>

#include "SubsumptionSolver.h"
#include "PrefixDescriptor.h"

class PrefixGeneratorV4
{
public:
	PrefixGeneratorV4(uint8_t n_, uint8_t d_, bool symmetric_);

	void LoadPrevious(uint8_t prevD_, const std::vector<Network>& prevPrefixes_);
	std::vector<Network> GeneratePrefixes();

protected:
	// Constant state
	uint8_t n, d;
	bool symmetric;
	std::vector<Network> allLayers;

	// Previous prefixes state
	uint8_t prevD = 0;
	std::vector<Network> prevPrefixes;
	std::vector<FactoredOutputSet> prevOutputs;

	// Generating state
	std::vector<PrefixDescriptor> globalPrefixes;
	std::mutex appendMutex;
	std::atomic<size_t> globalLayerIdx;

	// Pruning state
	std::atomic<size_t> globalOutputClass, globalPrefixIdx;
	struct alignas(64) ThreadCounter { std::atomic<uint64_t> value{ 0 }; };
	std::vector<ThreadCounter> threadCounters;

	// Generating
	void CachePreviousOutputs();
	void GenerateWorker(bool isFirst);
	void GenerateMulti(bool isFirst);

	// Pruning
	void OutputPruneWorker();
	void OutputPruneMulti();
	void OutputEquivPruneWorker(const std::vector<std::pair<size_t, size_t>>& outputClasses);
	void OutputEquivPruneMulti();
	void PruneWorker(size_t workerIdx, size_t maxSearches);
	void CleanupWorker();
	void PruneMulti(size_t maxSearches = 0);

	// Helpers
	FactoredOutputSet GetOutputs(size_t prevIdx, size_t layerIdx) const;
	void SanitizeGlobalPrefixes();
	std::vector<Network> GetAllPrefixes();

	friend class IsomorphicOutputSetV2;
};