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
		size_t popcountSum;
		std::vector<size_t> t2, t3z, t3o, t5, t6;
		std::vector<std::vector<size_t>> t5c;
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
	std::vector<double> GetTimings();

protected:
	uint8_t n, d;
	bool symmetric;

	std::vector<Network> allLayers;
	std::vector<NetworkMeta> prefixes;
	SubsumptionSolver solver;

	std::vector<double> timings;

	// Generating
	void Generate(bool isFirst);
	void SortByOutputs();

	// Pruning
	bool IsAlreadyRepresented(const NetworkMeta& prefix, const std::vector<size_t>& representatives, size_t maxSearches);
	void CacheOutputs(NetworkMeta& network);
	static void ClearCache(NetworkMeta& network);
	void Prune(size_t maxSearches = 0);

	// Signatures
	std::vector<std::vector<uint64_t>> SplitIntoClusters(const std::vector<uint64_t>& outputs) const;
	std::vector<size_t> T5Signature(const std::vector<uint64_t>& outputs) const;
	std::vector<size_t> T6Signature(const std::vector<uint64_t>& outputs) const;
	NetworkSignature ComputeSignature(const std::vector<uint64_t>& outputs) const;

	// Prechecking
	bool TPrecheck(const std::vector<size_t>& at, const std::vector<size_t>& bt, size_t aSize, size_t bSize) const;
	SubsumptionResult SignaturePrecheck(const NetworkSignature& aSig, const NetworkSignature& bSig) const;

	// Subsumption
	bool Subsumes(const NetworkMeta& a, const NetworkMeta& b, size_t maxSearches);
};