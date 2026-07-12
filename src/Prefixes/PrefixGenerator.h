#pragma once
#include <optional>

#include <sortnetutils.h>
#include "BS/BS_thread_pool.hpp"

#include "SubsumptionSolver.h"

class PrefixGenerator
{
protected:
	struct ThreadStatus
	{
		double progress;
	};

	struct NetworkSignature
	{
		size_t popcountSum;
		std::vector<size_t> t2, t3z, t3o, t5, t6;
		std::vector<std::vector<size_t>> t5c;
	};

	struct NetworkMeta
	{
		Network prefix;
		size_t numOutputs;
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
	BS::thread_pool<BS::tp::none> pool;
	std::unordered_map<std::thread::id, ThreadStatus> status;

	std::vector<Network> allLayers;
	std::vector<NetworkMeta> allPrefixes;
	std::vector<double> timings;

	// Generating
	void Generate(bool isFirst);
	void SortByOutputs(std::vector<size_t>& idxs);

	// Pruning
	void CacheOutputs(NetworkMeta& network);
	static void ClearCache(NetworkMeta& network);
	void Prune(std::vector<size_t>& idxs, size_t maxSearches = 0);
	void Remove(std::vector<size_t>& si, const std::vector<size_t>& sj, size_t maxSearches = 0);
	void ParallelPrune(std::vector<size_t>& idxs, size_t maxSearches = 0);

	// Signatures
	std::vector<std::vector<uint64_t>> SplitIntoClusters(const std::vector<uint64_t>& outputs) const;
	std::vector<size_t> T5Signature(const std::vector<uint64_t>& outputs) const;
	std::vector<size_t> T6Signature(const std::vector<uint64_t>& outputs) const;
	NetworkSignature ComputeSignature(const std::vector<uint64_t>& outputs) const;

	// Prechecking
	bool TPrecheck(const std::vector<size_t>& at, const std::vector<size_t>& bt, size_t aSize, size_t bSize) const;
	SubsumptionResult SignaturePrecheck(const NetworkMeta& a, const NetworkMeta& b) const;

	// Subsumption
	bool Subsumes(const NetworkMeta& a, const NetworkMeta& b, SubsumptionSolver& solver);
};