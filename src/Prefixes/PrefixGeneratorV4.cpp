#include "PrefixGeneratorV4.h"

#include <print>
#include <random>

PrefixGeneratorV4::SignedDescriptor::SignedDescriptor(size_t prevIdx_, size_t layerIdx_, const std::vector<uint64_t>& outputs, uint8_t n)
	: prevIdx(prevIdx_), layerIdx(layerIdx_), signature(outputs, n) {}

void PrefixGeneratorV4::SignedDescriptor::MarkSubsumed()
{
	subsumed = true;
	//signature.Free(); // At the moment, this is not safe... more thought required
}

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
		auto newPrefixes = Generate(prevD == 0);

		// Pruning phase 1
		ForwardPruneMulti(newPrefixes, 100);
		std::reverse(newPrefixes.begin(), newPrefixes.end());
		ForwardPruneMulti(newPrefixes, 100);

		// Pruning phase 2
		ForwardPruneMulti(newPrefixes, 0);
		std::reverse(newPrefixes.begin(), newPrefixes.end());
		ForwardPruneMulti(newPrefixes, 0);

		// Update prevPrefixes
		prevPrefixes = ToNetworks(newPrefixes);
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

std::vector<PrefixGeneratorV4::Descriptor> PrefixGeneratorV4::Generate(bool isFirst)
{
	std::vector<Descriptor> reps;
	for (size_t layerIdx = 0; layerIdx < allLayers.size(); layerIdx++)
	{
		// Skip un-full layers for depth-1 prefixes
		if (isFirst && allLayers[layerIdx].size() != n / 2) continue;

		for (size_t prevIdx = 0; prevIdx < prevPrefixes.size(); prevIdx++)
		{
			FactoredOutputSet outputs = GetOutputs(prevIdx, layerIdx);
			if (outputs.IsRedundant()) continue;
			reps.emplace_back(prevIdx, layerIdx, outputs.Size());
		}
	}

	std::sort(reps.begin(), reps.end(), [](Descriptor a, Descriptor b) {
		return a.numOutputs < b.numOutputs;
		});
	return reps;
}

void PrefixGeneratorV4::ForwardPrune(std::vector<Descriptor>& reps, size_t maxSearches)
{
	// Construct subsumption solver
	SubsumptionSolver solver{ n, symmetric, maxSearches };

	std::vector<SignedDescriptor> newReps;
	for (auto [prevIdx, layerIdx, _] : reps)
	{
		// Construct signed descriptor
		std::vector<uint64_t> outputsVec = GetOutputs(prevIdx, layerIdx).ToVector();
		SignedDescriptor descriptor{ prevIdx, layerIdx, outputsVec, n };

		// Erase existing representatives that this new one subsumes
		std::erase_if(newReps, [&, this](const SignedDescriptor& rep) {
			if (descriptor.signature > rep.signature)
				return false;

			// Run a full backtracking subsumption test
			std::vector<uint64_t> repOutputs = GetOutputs(rep.prevIdx, rep.layerIdx).ToVector();
			return (solver.Solve(outputsVec, repOutputs) == DoesSubsume);
			});

		// Add this prefix to newReps
		newReps.emplace_back(std::move(descriptor));
	}

	// Update reps
	reps.clear();
	for (const SignedDescriptor& rep : newReps)
		reps.emplace_back(rep.prevIdx, rep.layerIdx);
}

void PrefixGeneratorV4::PruneWorker(size_t maxSearches)
{
	SubsumptionSolver solver{ n, symmetric, maxSearches };
	for (;;)
	{
		// Get the next prefix from globalReps
		size_t repIdx = globalRepIdx++;
		if (repIdx >= globalReps.size()) return;
		auto [prevIdx, layerIdx, _] = globalReps[repIdx];

		// Construct signed descriptor
		std::vector<uint64_t> outputsVec = GetOutputs(prevIdx, layerIdx).ToVector();
		SignedDescriptor descriptor{ prevIdx, layerIdx, outputsVec, n };

		// Mark existing representatives as subsumed
		for (size_t newRepIdx = 0; newRepIdx < globalWriteIdx; newRepIdx++)
		{
			if (!slotFilled[newRepIdx]) continue;

			SignedDescriptor& newRep = globalNewReps[newRepIdx];
			if (newRep.subsumed) continue;
			if (descriptor.signature > newRep.signature) continue;

			// Run a full backtracking subsumption test
			std::vector<uint64_t> repOutputs = GetOutputs(newRep.prevIdx, newRep.layerIdx).ToVector();
			if (solver.Solve(outputsVec, repOutputs) == DoesSubsume)
				newRep.MarkSubsumed();
		}

		// Add this prefix to globalNewReps
		size_t writeIdx = globalWriteIdx++;
		globalNewReps[writeIdx] = std::move(descriptor);
		slotFilled[writeIdx] = true;
	}
}

void PrefixGeneratorV4::ForwardPruneMulti(std::vector<Descriptor>& reps, size_t maxSearches)
{
	// Initialize global state
	size_t numReps = reps.size();
	globalReps = std::move(reps);
	globalNewReps.resize(numReps);
	globalRepIdx = 0;
	globalWriteIdx = 0;
	slotFilled = std::vector<std::atomic<bool>>(numReps);

	// Launch worker threads
	size_t numThreads = std::thread::hardware_concurrency() - 1;
	std::vector<std::thread> threads;
	for (size_t i = 0; i < numThreads; i++)
		threads.emplace_back([this, maxSearches]() { PruneWorker(maxSearches); });

	for (;;)
	{
		size_t repIdx = globalRepIdx;
		if (repIdx >= globalReps.size()) break;
		std::print("{:>7.3f}%\r", (double)repIdx / globalReps.size() * 100.0);
		std::this_thread::sleep_for(std::chrono::milliseconds{ 20 });
	}

	for (auto& thread : threads) thread.join();

	// Update reps
	reps.clear();
	for (size_t newRepIdx = 0; newRepIdx < globalWriteIdx; newRepIdx++)
	{
		const SignedDescriptor& newRep = globalNewReps[newRepIdx];
		if (newRep.subsumed) continue;
		reps.emplace_back(newRep.prevIdx, newRep.layerIdx);
	}
}

std::vector<Network> PrefixGeneratorV4::ToNetworks(const std::vector<Descriptor>& reps)
{
	std::vector<Network> networks;
	networks.reserve(reps.size());
	for (const auto& descriptor : reps)
		networks.push_back(prevPrefixes[descriptor.prevIdx] + allLayers[descriptor.layerIdx]);
	return networks;
}