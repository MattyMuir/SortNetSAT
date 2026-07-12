#include "PrefixGenerator.h"

#include <print>
#include <numeric>
#include <algorithm>
#include <ranges>
#include <random>

#include "prefixes.h"
#include "SubsumptionSolver.h"
#include "../Timer.h"

PrefixGenerator::PrefixGenerator(uint8_t n_, uint8_t d_, bool symmetric_)
	: n(n_), d(d_), symmetric(symmetric_), pool(std::thread::hardware_concurrency() - 1),
	allLayers(GetAllLayers(n, symmetric))
{}

size_t solvedByPrecheck = 0;
size_t solvedByBacktracking = 0;
std::vector<Network> PrefixGenerator::GeneratePrefixes()
{
	// Add the empty layer to the initial set of prefixes
	allPrefixes.emplace_back(Network{}, 1ULL << n);

	for (uint8_t k = 0; k < d; k++)
	{
		Generate(k == 0);

		std::vector<size_t> idxs(allPrefixes.size());
		std::iota(idxs.begin(), idxs.end(), 0);
		
		Timer t;
		ParallelPrune(idxs, 100);
		ParallelPrune(idxs);
		t.Stop();
		timings.push_back(t.GetSeconds());

		std::vector<NetworkMeta> newAllPrefixes;
		newAllPrefixes.reserve(idxs.size());
		for (size_t idx : idxs)
			newAllPrefixes.emplace_back(std::move(allPrefixes[idx]));
		std::swap(allPrefixes, newAllPrefixes);

		std::println("Number of {}-layer prefixes: {}", k + 1, idxs.size());
	}

	std::println("Precheck:     {}", solvedByPrecheck);
	std::println("Backtracking: {}", solvedByBacktracking);

	// Strip the metadata from prefixes
	std::vector<Network> noMeta;
	for (auto& networkMeta : allPrefixes)
		noMeta.emplace_back(std::move(networkMeta.prefix));
	return noMeta;
}

std::vector<double> PrefixGenerator::GetTimings()
{
	return timings;
}

void PrefixGenerator::Generate(bool isFirst)
{
	std::vector<std::vector<NetworkMeta>> allExtended(allPrefixes.size());
	pool.detach_sequence(0, allPrefixes.size(), [this, isFirst, &allExtended](size_t partialIdx) {
		const NetworkMeta& partial = allPrefixes[partialIdx];

		for (const Network& layer : allLayers)
		{
			// For the first layer, skip unfull layers
			if (isFirst && layer.size() != n / 2)
				continue;

			// Compute the new extended prefix
			Network extended = Concatenate(partial.prefix, layer);

			// Skip redundant networks
			FactoredOutputSet outputs{ extended, n };
			if (outputs.IsRedundant()) continue;

			// Compute the signature and add this prefix
			allExtended[partialIdx].emplace_back(extended, outputs.Size());
		}
		});
	pool.wait();

	allPrefixes.clear();
	for (auto& extended : allExtended)
		allPrefixes.insert(allPrefixes.end(), std::make_move_iterator(extended.begin()), std::make_move_iterator(extended.end()));
}

void PrefixGenerator::SortByOutputs(std::vector<size_t>& idxs)
{
	std::sort(idxs.begin(), idxs.end(), [this](size_t idx1, size_t idx2) {
		return allPrefixes[idx1].numOutputs > allPrefixes[idx2].numOutputs;
		});
}

void PrefixGenerator::CacheOutputs(NetworkMeta& network)
{
	network.outputs = FactoredOutputSet{ network.prefix, n }.ToVector();
	network.signature = ComputeSignature(*network.outputs);
}

void PrefixGenerator::ClearCache(NetworkMeta& network)
{
	network.outputs.reset();
	network.signature.reset();
}

void PrefixGenerator::Prune(std::vector<size_t>& idxs, size_t maxSearches)
{
	SortByOutputs(idxs);

	SubsumptionSolver solver{ n, symmetric, maxSearches };
	std::vector<size_t> representatives;

	for (size_t progress = 0; progress < idxs.size(); progress++)
	{
		size_t prefixIdx = idxs[progress];

		// Update progress
		if (prefixIdx % 75 == 0)
			status.at(std::this_thread::get_id()).progress = (double)progress / idxs.size() * 100.0;

		// Get the current prefix and cache its outputs
		NetworkMeta& prefix = allPrefixes[prefixIdx];
		CacheOutputs(prefix);
		
		// Skip this prefix if its already represented
		bool isRepresented = false;
		for (size_t repIdx : representatives)
			if (Subsumes(allPrefixes[repIdx], prefix, solver))
				{ isRepresented = true; break; }
		if (isRepresented)
		{
			ClearCache(prefix);
			continue;
		}

		// Remove representatives that are subsumed by the new prefix (and clear their caches)
		std::erase_if(representatives, [&, this](size_t repIdx)
			{
				bool subsumes = Subsumes(prefix, allPrefixes[repIdx], solver);
				if (subsumes) ClearCache(allPrefixes[repIdx]);
				return subsumes;
			});

		// Add the new prefix as a representative
		representatives.push_back(prefixIdx);
	}

	// Clear all caches
	for (size_t idx : idxs)
		ClearCache(allPrefixes[idx]);

	// Update idxs variable
	std::swap(representatives, idxs);

	status.at(std::this_thread::get_id()).progress = 100.0;
}

void PrefixGenerator::Remove(std::vector<size_t>& si, const std::vector<size_t>& sj, size_t maxSearches)
{
	SubsumptionSolver solver{ n, symmetric, maxSearches };

	std::vector<size_t> newSi;
	for (size_t prefixIdx : si)
	{
		// Get the current prefix and cache its outputs
		NetworkMeta& prefix = allPrefixes[prefixIdx];
		CacheOutputs(prefix);

		// Skip this prefix if its already represented
		bool isRepresented = false;
		for (size_t repIdx : sj)
			if (Subsumes(allPrefixes[repIdx], prefix, solver))
				{ isRepresented = true; break; }

		if (!isRepresented)
			newSi.push_back(prefixIdx);

		ClearCache(prefix);
	}

	std::swap(si, newSi);
}

void PrefixGenerator::ParallelPrune(std::vector<size_t>& idxs, size_t maxSearches)
{
	auto threadIDs = pool.get_thread_ids();
	for (auto id : threadIDs)
		status[id] = ThreadStatus{ 0.0 };

	// Initial prune step
	// Leave prefixes in the order they were generated... sharing initial layers makes them more likely to subsume?
	// Or dont

	auto repsFut = pool.submit_blocks(0, idxs.size(), [this, &idxs, maxSearches](size_t blockStart, size_t blockEnd) {
		std::vector<size_t> idxBlock{ idxs.begin() + blockStart, idxs.begin() + blockEnd };
		Prune(idxBlock, maxSearches);
		return idxBlock;
		}, 19 * 2);

	for (;;)
	{
		std::print("Rem {:^5} |", pool.get_tasks_queued());
		for (const auto& [id, s] : status)
			std::print("{:^5.1f}|", s.progress);
		std::print("\r");
		bool isDone = pool.wait_for(std::chrono::milliseconds{ 100 });
		if (isDone) break;
	}
	std::println();
	repsFut.wait();
	std::vector<std::vector<size_t>> reps = repsFut.get();

	std::sort(reps.begin(), reps.end(), [](const auto& a, const auto& b) {
		return a.size() > b.size();
		});

	// Removal step
	for (size_t j = 0; j < reps.size(); j++)
	{
		std::print("Removal step {}\r", j);
		// Cache outputs for sj
		for (size_t prefixIdx : reps[j])
			CacheOutputs(allPrefixes[prefixIdx]);

		// Parallel removal step
		pool.detach_sequence(0, reps.size(), [this, &reps, j, maxSearches](size_t i) {
			if (i == j) return;
			Remove(reps[i], reps[j], maxSearches);
			});
		pool.wait();
	}

	// Concatenate representative sets from each thread
	idxs.clear();
	for (const auto& threadReps : reps)
		idxs.insert(idxs.end(), threadReps.begin(), threadReps.end());
}

std::vector<std::vector<uint64_t>> PrefixGenerator::SplitIntoClusters(const std::vector<uint64_t>& outputs) const
{
	std::vector<std::vector<uint64_t>> clusters(n + 1);
	for (uint64_t output : outputs)
		clusters[std::popcount(output)].push_back(output);
	return clusters;
}

std::vector<size_t> PrefixGenerator::T5Signature(const std::vector<uint64_t>& outputs) const
{
	std::vector<size_t> t5(n);
	for (uint64_t output : outputs)
		for (uint8_t bi = 0; bi < n; bi++)
			t5[bi] += (output >> bi) & 1ULL;
	std::sort(t5.begin(), t5.end());
	return t5;
}

std::vector<size_t> PrefixGenerator::T6Signature(const std::vector<uint64_t>& outputs) const
{
	std::vector<size_t> t6(n);
	for (uint64_t output : outputs)
	{
		uint64_t popcount = std::popcount(output);
		for (uint8_t bi = 0; bi < n; bi++)
			if ((output >> bi) & 1)
				t6[bi] += popcount;
	}
	std::sort(t6.begin(), t6.end());
	return t6;
}

PrefixGenerator::NetworkSignature PrefixGenerator::ComputeSignature(const std::vector<uint64_t>& outputs) const
{
	// === T1 Signature ===
	size_t numOutputs = outputs.size();
	size_t popcountSum = 0;
	for (uint64_t output : outputs)
		popcountSum += std::popcount(output);

	// === T2 Signature ===
	auto clusters = SplitIntoClusters(outputs);
	std::vector<size_t> t2(n + 1);
	for (uint8_t popcount = 0; popcount <= n; popcount++)
		t2[popcount] = clusters[popcount].size();

	// === T3 Signature ===
	std::vector<uint64_t> zeroPos(n + 1), onePos(n + 1);
	for (uint64_t output : outputs)
	{
		uint64_t popcount = std::popcount(output);
		zeroPos[popcount] |= (~output) & ((1ULL << n) - 1);
		onePos[popcount] |= output;
	}
	std::vector<size_t> t3z(n + 1), t3o(n + 1);
	for (uint8_t popcount = 0; popcount <= n; popcount++)
	{
		t3z[popcount] = std::popcount(zeroPos[popcount]);
		t3o[popcount] = std::popcount(onePos[popcount]);
	}

	// === T5 Signature ===
	std::vector<size_t> t5 = T5Signature(outputs);
	std::vector<std::vector<size_t>> t5c;
	for (const auto& cluster : clusters)
		t5c.push_back(T5Signature(cluster));

	// === T6 Signature ===
	std::vector<size_t> t6 = T6Signature(outputs);

	return NetworkSignature{ popcountSum, t2, t3z, t3o, t5, t6, t5c };
}

bool PrefixGenerator::TPrecheck(const std::vector<size_t>& at, const std::vector<size_t>& bt, size_t aSize, size_t bSize) const
{
	for (uint8_t i = 0; i < n; i++)
		if (at[i] > bt[i] || bt[i] - at[i] > bSize - aSize)
			return true;
	return false;
}

SubsumptionResult PrefixGenerator::SignaturePrecheck(const NetworkMeta& a, const NetworkMeta& b) const
{
	const NetworkSignature& aSig = *a.signature;
	const NetworkSignature& bSig = *b.signature;

	// === T1 Signature ===
	if (a.numOutputs > b.numOutputs)
		return DoesntSubsume;

	// === T3 Signature ===
	for (uint8_t popcount = 0; popcount <= n; popcount++)
		if (aSig.t3z[popcount] > bSig.t3z[popcount] || aSig.t3o[popcount] > bSig.t3o[popcount])
			return DoesntSubsume;

	// === T5 Signature ===
	if (TPrecheck(aSig.t5, bSig.t5, a.numOutputs, b.numOutputs))
		return DoesntSubsume;
	for (uint8_t popcount = 0; popcount <= n; popcount++)
		if (TPrecheck(aSig.t5c[popcount], bSig.t5c[popcount], aSig.t2[popcount], bSig.t2[popcount]))
			return DoesntSubsume;

	// === T6 Signature ===
	if (TPrecheck(aSig.t6, bSig.t6, aSig.popcountSum, bSig.popcountSum))
		return DoesntSubsume;

	return Unknown;
}

bool PrefixGenerator::Subsumes(const NetworkMeta& a, const NetworkMeta& b, SubsumptionSolver& solver)
{
	// Run the signature-based prechecks
	SubsumptionResult precheck = SignaturePrecheck(a, b);
	if (precheck != Unknown)
	{
		solvedByPrecheck++;
		return (precheck == DoesSubsume);
	}

	// Run a full backtracking subsumption test
	solvedByBacktracking++;
	SubsumptionResult result = solver.Solve(*a.outputs, *b.outputs);
	return result == DoesSubsume;
}