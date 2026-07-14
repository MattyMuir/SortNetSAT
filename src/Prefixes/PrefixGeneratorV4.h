#pragma once
#include <sortnetutils.h>

#include "NetworkSignature.h"
#include "SubsumptionSolver.h"

class PrefixGeneratorV4
{
protected:
	struct SignedDescriptor
	{
		SignedDescriptor(size_t prevIdx_, size_t layerIdx_, const std::vector<uint64_t>& outputs, uint8_t n);

		size_t prevIdx, layerIdx;
		NetworkSignature signature;
	};

public:
	PrefixGeneratorV4(uint8_t n_, uint8_t d_, bool symmetric_);

	void LoadPrevious(uint8_t prevD_, const std::vector<Network>& prevPrefixes_);
	std::vector<Network> GeneratePrefixes();

protected:
	uint8_t n, d;
	bool symmetric;
	std::vector<Network> allLayers;

	uint8_t prevD = 0;
	std::vector<Network> prevPrefixes;
	std::vector<FactoredOutputSet> prevOutputs;

	FactoredOutputSet GetOutputs(size_t prevIdx, size_t layerIdx);
	void Insert(std::vector<SignedDescriptor>& reps, size_t prevIdx, size_t layerIdx, SubsumptionSolver& solver);
	std::vector<SignedDescriptor> GenerateAndPrune(bool isFirst, size_t maxSearches);
	void Prune(std::vector<SignedDescriptor>& reps, size_t maxSearches = 0);
	Network ToNetwork(const SignedDescriptor& descriptor);
	std::vector<Network> ToNetworks(const std::vector<SignedDescriptor>& reps);
};