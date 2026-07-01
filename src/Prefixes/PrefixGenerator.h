#pragma once
#include <optional>

#include <sortnetutils.h>

#include "SubsumptionSolver.h"

class PrefixGenerator
{
protected:
	struct NetworkSignature
	{
		size_t numOutputs;
		std::vector<size_t> t2;
		std::vector<size_t> t3z, t3o;
	};

	struct NetworkMeta
	{
		Network prefix;
		std::optional<std::vector<uint64_t>> outputs = std::nullopt;
		std::optional<NetworkSignature> signature = std::nullopt;
	};

public:
	PrefixGenerator(uint8_t n_, uint8_t d_, bool symmetric_);

	std::vector<Network> GeneratePrefixes();

protected:
	uint8_t n, d;
	bool symmetric;

	std::vector<Network> allLayers;
	std::vector<NetworkMeta> prefixes;
	SubsumptionSolver solver;

	void Generate(bool isFirst);
	bool IsAlreadyRepresented(const NetworkMeta& prefix, const std::vector<size_t> representatives, size_t maxSearches);
	void Prune(size_t maxSearches = 0);

	// Sumsumption testing
	NetworkSignature ComputeSignature(const std::vector<uint64_t>& outputs) const;
	void CacheOutputs(NetworkMeta& network);
	static void ClearCache(NetworkMeta& network);
	SubsumptionResult SignaturePrecheck(const NetworkSignature& aSig, const NetworkSignature& bSig) const;
	bool Subsumes(const NetworkMeta& a, const NetworkMeta& b, size_t maxSearches);
};