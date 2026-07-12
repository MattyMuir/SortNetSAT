#include "PrefixGeneratorV4.h"

#include <print>

PrefixGeneratorV4::SignedDescriptor::SignedDescriptor(size_t prevIdx_, size_t layerIdx_, const std::vector<uint64_t>& outputs, uint8_t n)
	: prevIdx(prevIdx_), layerIdx(layerIdx_), signature(outputs, n) {}

PrefixGeneratorV4::PrefixGeneratorV4(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_), allLayers(GetAllLayers(n, symmetric)) {}

std::vector<Network> PrefixGeneratorV4::GeneratePrefixes()
{
	prevPrefixes.emplace_back(Network{});
	prevOutputs.emplace_back(FactoredOutputSet{ Network{}, 18 });

	for (uint8_t k = 0; k < d; k++)
	{
		prevPrefixes = GenerateAndPrune(100);
		std::println("Number of {}-layer prefixes: {}", k + 1, prevPrefixes.size());
	}

	return prevPrefixes;
}

FactoredOutputSet PrefixGeneratorV4::GetOutputs(size_t prevIdx, size_t layerIdx)
{
	FactoredOutputSet outputs{ prevOutputs[prevIdx] };
	outputs.ApplyCEs(allLayers[layerIdx]);
	return outputs;
}

std::vector<Network> PrefixGeneratorV4::GenerateAndPrune(size_t maxSearches)
{
	// Compute outputs of previous prefixes
	prevOutputs.clear();
	for (const Network& prevPrefix : prevPrefixes)
		prevOutputs.push_back(FactoredOutputSet{ prevPrefix, n });

	std::vector<SignedDescriptor> representatives;
	SubsumptionSolver solver{ n, symmetric, maxSearches };
	for (size_t prevIdx = 0; prevIdx < prevPrefixes.size(); prevIdx++)
	{
		for (size_t layerIdx = 0; layerIdx < allLayers.size(); layerIdx++)
		{
			// Construct prefix and get outputs
			const Network& lastLayer = allLayers[layerIdx];
			Network prefix = Concatenate(prevPrefixes[prevIdx], lastLayer);
			FactoredOutputSet outputs{ prevOutputs[prevIdx] };
			outputs.ApplyCEs(lastLayer);

			// Skip redundant networks
			if (outputs.IsRedundant()) continue;

			// Construct signed descriptor
			std::vector<uint64_t> outputsVec = std::move(outputs).ToVector();
			SignedDescriptor descriptor{ prevIdx, layerIdx, outputsVec, n };

			// Check if this prefix is already represented
			bool isRepresented = false;
			for (const SignedDescriptor& rep : representatives)
			{
				// Signature precheck
				if (rep.signature > descriptor.signature)
					continue;
				
				// Compute representative outputs
				std::vector<uint64_t> repOutputs = GetOutputs(rep.prevIdx, rep.layerIdx).ToVector();
				if (solver.Solve(repOutputs, outputsVec) == DoesSubsume)
				{
					isRepresented = true;
					break;
				}
			}
			if (isRepresented) continue;

			// Erase existing representatives that this new one subsumes
			std::erase_if(representatives, [&, this](const SignedDescriptor& rep) {
				if (descriptor.signature > rep.signature)
					return false;

				// Compute representative outputs
				std::vector<uint64_t> repOutputs = GetOutputs(rep.prevIdx, rep.layerIdx).ToVector();
				return (solver.Solve(outputsVec, repOutputs) == DoesSubsume);
				});

			// Add this new prefix to the list of representatives
			representatives.emplace_back(std::move(descriptor));
		}
	}

	// Convert representatives to networks
	std::vector<Network> pruned;
	for (const auto& rep : representatives)
		pruned.emplace_back(Concatenate(prevPrefixes[rep.prevIdx], allLayers[rep.layerIdx]));
	return pruned;
}