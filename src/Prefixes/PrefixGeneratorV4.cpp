#include "PrefixGeneratorV4.h"

#include <print>

PrefixGeneratorV4::SignedDescriptor::SignedDescriptor(size_t prevIdx_, size_t layerIdx_, const std::vector<uint64_t>& outputs, uint8_t n)
	: prevIdx(prevIdx_), layerIdx(layerIdx_), signature(outputs, n) {}

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
	prevOutputs.emplace_back(FactoredOutputSet{ Network{}, 18 });

	while (prevD < d)
	{
		// Two-phase pruning
		auto newPrefixes = GenerateAndPrune(prevD == 0, 100);
		Prune(newPrefixes);

		// Update prevPrefixes
		prevPrefixes = ToNetworks(newPrefixes);
		prevD++;

		// Logging
		std::println("Number of {}-layer prefixes: {}", prevD, prevPrefixes.size());
	}

	return prevPrefixes;
}

FactoredOutputSet PrefixGeneratorV4::GetOutputs(size_t prevIdx, size_t layerIdx)
{
	FactoredOutputSet outputs{ prevOutputs[prevIdx] };
	outputs.ApplyCEs(allLayers[layerIdx]);
	return outputs;
}

void PrefixGeneratorV4::Insert(std::vector<SignedDescriptor>& reps, size_t prevIdx, size_t layerIdx, SubsumptionSolver& solver)
{
	// Get outputs
	FactoredOutputSet outputs = GetOutputs(prevIdx, layerIdx);

	// Skip redundant networks
	if (outputs.IsRedundant()) return;

	// Construct signed descriptor
	std::vector<uint64_t> outputsVec = std::move(outputs).ToVector();
	SignedDescriptor descriptor{ prevIdx, layerIdx, outputsVec, n };

	// Check if this prefix is already represented
	for (const SignedDescriptor& rep : reps)
	{
		// Signature precheck
		if (rep.signature > descriptor.signature)
			continue;

		// Run a full backtracking subsumption test
		std::vector<uint64_t> repOutputs = GetOutputs(rep.prevIdx, rep.layerIdx).ToVector();
		if (solver.Solve(repOutputs, outputsVec) == DoesSubsume)
			return;
	}

	// Erase existing representatives that this new one subsumes
	std::erase_if(reps, [&, this](const SignedDescriptor& rep) {
		if (descriptor.signature > rep.signature)
			return false;

		// Run a full backtracking subsumption test
		std::vector<uint64_t> repOutputs = GetOutputs(rep.prevIdx, rep.layerIdx).ToVector();
		return (solver.Solve(outputsVec, repOutputs) == DoesSubsume);
		});

	// Add this new prefix to the list of representatives
	reps.emplace_back(std::move(descriptor));
}

std::vector<PrefixGeneratorV4::SignedDescriptor> PrefixGeneratorV4::GenerateAndPrune(bool isFirst, size_t maxSearches)
{
	// Compute outputs of previous prefixes
	prevOutputs.clear();
	for (const Network& prevPrefix : prevPrefixes)
		prevOutputs.push_back(FactoredOutputSet{ prevPrefix, n });

	// Iterate over all previous prefixes and layers and insert into representatives
	std::vector<SignedDescriptor> reps;
	SubsumptionSolver solver{ n, symmetric, maxSearches };
	for (size_t prevIdx = 0; prevIdx < prevPrefixes.size(); prevIdx++)
		for (size_t layerIdx = 0; layerIdx < allLayers.size(); layerIdx++)
			if (!isFirst || allLayers[layerIdx].size() == n / 2)
				Insert(reps, prevIdx, layerIdx, solver);

	return reps;
}

void PrefixGeneratorV4::Prune(std::vector<SignedDescriptor>& reps, size_t maxSearches)
{
	// Iterate over all previous prefixes and layers and insert into representatives
	std::vector<SignedDescriptor> newReps;
	SubsumptionSolver solver{ n, symmetric, maxSearches };
	for (const auto& descriptor : reps)
		Insert(newReps, descriptor.prevIdx, descriptor.layerIdx, solver);
	std::swap(reps, newReps);
}

Network PrefixGeneratorV4::ToNetwork(const SignedDescriptor& descriptor)
{
	return prevPrefixes[descriptor.prevIdx] + allLayers[descriptor.layerIdx];
}

std::vector<Network> PrefixGeneratorV4::ToNetworks(const std::vector<SignedDescriptor>& reps)
{
	std::vector<Network> networks;
	networks.reserve(reps.size());
	for (const auto& descriptor : reps)
		networks.push_back(ToNetwork(descriptor));
	return networks;
}