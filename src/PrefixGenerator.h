#pragma once
#include <unordered_set>

#include <sortnetutils.h>

class PrefixGenerator
{
protected:
	struct NetworkOutputs
	{
		Network network;
		std::unordered_set<uint64_t> outputs;

		NetworkOutputs() = default;
		NetworkOutputs(const Network& network_, uint8_t n);
	};

public:
	PrefixGenerator(uint8_t n_, uint8_t d_, bool symmetric_);

	std::vector<Network> GeneratePrefixes();

protected:
	uint8_t n, d;
	bool symmetric;

	std::vector<CE> alphabet;
	std::vector<Network> allLayers;
	std::vector<NetworkOutputs> R, N;

	void Generate();
	void Prune();
	bool CanAddCE(const Network& layer, CE ce) const;
	void AddCEInplace(Network& layer, CE ce) const;
	Network AddCE(const Network& layer, CE ce) const;
	void ReduceOutputs(std::unordered_set<uint64_t>& outputs, CE ce) const;
	NetworkOutputs AddLayer(const NetworkOutputs& network, const Network& layer) const;

	// Sumsumption testing
	bool CannotSubsume1(const NetworkOutputs& a, const NetworkOutputs& b) const;
	bool IsPermutedSubset(const std::unordered_set<uint64_t>& a, const std::unordered_set<uint64_t>& b) const;
	bool Subsumes(const NetworkOutputs& a, const NetworkOutputs& b) const;
};