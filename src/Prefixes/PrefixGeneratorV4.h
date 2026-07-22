#pragma once
#include <thread>
#include <atomic>

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

	// Pruning state
	std::vector<PrefixDescriptor> globalPrefixes;
	std::atomic<size_t> globalPrefixIdx;

	void CachePreviousOutputs();
	FactoredOutputSet GetOutputs(size_t prevIdx, size_t layerIdx);
	void Generate(bool isFirst);

	// Multi-threaded prune
	void SanitizeGlobalPrefixes();
	bool Subsumes(const PrefixDescriptor::Guard& guard, size_t otherPrefixIdx);
	void PruneWorker(size_t maxSearches);
	void PruneMulti(size_t maxSearches = 0);

	std::vector<Network> GetAllPrefixes();
};