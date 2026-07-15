#pragma once
#include <thread>
#include <atomic>

#include <sortnetutils.h>

#include "NetworkSignature.h"
#include "SubsumptionSolver.h"

class PrefixGeneratorV4
{
protected:
	struct Descriptor
	{
		size_t prevIdx, layerIdx, numOutputs;
	};

	struct SignedDescriptor
	{
		SignedDescriptor() = default;
		SignedDescriptor(size_t prevIdx_, size_t layerIdx_, const std::vector<uint64_t>& outputs, uint8_t n);

		void MarkSubsumed();

		size_t prevIdx, layerIdx;
		NetworkSignature signature;
		bool subsumed = false;
	};

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
	std::vector<Descriptor> globalReps;
	std::vector<SignedDescriptor> globalNewReps;
	std::atomic<size_t> globalRepIdx, globalWriteIdx;
	std::vector<std::atomic<bool>> slotFilled;

	void CachePreviousOutputs();
	FactoredOutputSet GetOutputs(size_t prevIdx, size_t layerIdx);
	std::vector<Descriptor> Generate(bool isFirst);
	void ForwardPrune(std::vector<Descriptor>& reps, size_t maxSearches);

	// Multi-threaded prune
	void PruneWorker(size_t maxSearches);
	void ForwardPruneMulti(std::vector<Descriptor>& reps, size_t maxSearches);

	std::vector<Network> ToNetworks(const std::vector<Descriptor>& reps);
};