#include "fastcanonize.h"

#include <numeric>
#include <algorithm>
#include <ranges>
#include <set>

std::vector<uint8_t> InitialColouring(const std::vector<uint64_t>& outputs, uint8_t n)
{
	// Compute sums for each bit-position
	std::vector<uint64_t> bitSums(n);
	for (uint64_t x : outputs)
		for (uint8_t i = 0; i < n; i++, x >>= 1)
			if (x & 1ULL)
				bitSums[i]++;

	// Sort bit positions by sum
	std::vector<uint8_t> sortedPositions(n);
	std::iota(sortedPositions.begin(), sortedPositions.end(), 0);
	std::ranges::sort(sortedPositions, {}, [&](uint8_t i) { return bitSums[i]; });

	// Assign colours by increasing sums, giving the same colour to bits with the same sum
	std::vector<uint8_t> colours(n);
	uint8_t nextColour = 0;
	for (size_t i = 1; i < sortedPositions.size(); i++)
	{
		uint8_t prevPos = sortedPositions[i - 1];
		uint8_t pos = sortedPositions[i];
		if (bitSums[pos] != bitSums[prevPos]) nextColour++;
		colours[pos] = nextColour;
	}

	return colours;
}

static inline uint64_t SplitMix(uint64_t x)
{
	x ^= x >> 30;
	x *= 0xbf58476d1ce4e5b9ULL;
	x ^= x >> 27;
	x *= 0x94d049bb133111ebULL;
	x ^= x >> 31;
	return x;
}

void RefineColouring(std::vector<uint8_t>& colours, const std::vector<uint64_t>& outputs)
{
	uint8_t n = (uint8_t)colours.size();

	for (;;)
	{
		// Compute output hashes
		std::vector<uint64_t> outputHashes(outputs.size());
		for (size_t outputIdx = 0; outputIdx < outputs.size(); outputIdx++)
		{
			uint64_t x = outputs[outputIdx];
			uint64_t hash = 0;
			for (uint8_t bi = 0; bi < n; bi++)
				if (x & (1ULL << bi))
					hash += SplitMix(colours[bi]);
			outputHashes[outputIdx] = hash;
		}

		// Compute coordinate hashes
		std::vector<uint64_t> coordHashes(n, 0);
		for (size_t outputIdx = 0; outputIdx < outputs.size(); outputIdx++)
		{
			uint64_t x = outputs[outputIdx];
			uint64_t xHash = outputHashes[outputIdx];
			for (uint8_t bi = 0; bi < n; bi++)
				if (x & (1ULL << bi))
					coordHashes[bi] += xHash;
		}

		// Recolour positions of coordinate hashes differ
		uint8_t numColours = std::ranges::max(colours) + 1;
		std::vector<uint8_t> newColours(n);
		uint8_t nextColour = 0;
		for (uint8_t colour = 0; colour < numColours; colour++)
		{
			std::vector<uint8_t> positions;
			for (uint8_t bi = 0; bi < n; bi++)
				if (colours[bi] == colour)
					positions.push_back(bi);

			std::ranges::sort(positions, {}, [&](uint8_t i) { return coordHashes[i]; });

			newColours[positions[0]] = nextColour;
			for (size_t i = 1; i < positions.size(); i++)
			{
				uint8_t prevPos = positions[i - 1];
				uint8_t pos = positions[i];
				if (coordHashes[pos] != coordHashes[prevPos]) nextColour++;
				newColours[pos] = nextColour;
			}
			nextColour++;
		}

		if (colours == newColours) break;
		colours = newColours;
	}
}

Permutation ColoursToPermUnsym(const std::vector<uint8_t>& colours)
{
	Permutation perm(colours.begin(), colours.end());
	perm.Invert();
	return perm;
}

Permutation ColoursToPermSym(const std::vector<uint8_t>& colours)
{
	uint8_t n = (uint8_t)colours.size();

	// Get colour-pairs
	std::vector<std::pair<uint8_t, uint8_t>> pairs;
	for (uint8_t i = 0; i < n / 2; i++)
	{
		uint8_t c1 = colours[i];
		uint8_t c2 = colours[n - 1 - i];
		pairs.emplace_back(std::min(c1, c2), std::max(c1, c2));
	}

	// Sort pairs by pair colourings
	Permutation perm(n / 2);
	std::iota(perm.begin(), perm.end(), 0);
	std::ranges::sort(perm, {}, [&](uint8_t i) { return pairs[i]; });

	// Flip pairs by colours
	for (uint8_t& i : perm)
		if (colours[n - 1 - i] < colours[i])
			i = n - 1 - i;

	// Expand centro-symmetrically
	perm.resize(n);
	for (uint8_t i = 0; i < n / 2; i++)
		perm[n - 1 - i] = n - 1 - perm[i];

	return perm;
}

bool IsResolvable(const std::vector<uint8_t>& colours, bool symmetric)
{
	uint8_t n = (uint8_t)colours.size();
	if (!symmetric) return std::ranges::max(colours) == n - 1;

	// Check if colours are distinct within each pair
	for (uint8_t i = 0; i < n / 2; i++)
		if (colours[i] == colours[n - 1 - i])
			return false;

	// Check if colour-pairs are distinct
	std::set<std::pair<uint8_t, uint8_t>> pairs;
	for (uint8_t i = 0; i < n / 2; i++)
	{
		uint8_t c1 = colours[i];
		uint8_t c2 = colours[n - 1 - i];
		pairs.emplace(std::min(c1, c2), std::max(c1, c2));
	}
	return pairs.size() == n / 2;
}

std::optional<Permutation> FastCanonize(const std::vector<uint64_t>& outputs, uint8_t n, bool symmetric)
{
	std::vector<uint8_t> colours = InitialColouring(outputs, n);

	// If unresolvable, try refining
	if (!IsResolvable(colours, symmetric)) RefineColouring(colours, outputs);

	// If still unresolvable, failed
	if (!IsResolvable(colours, symmetric)) return std::nullopt;

	if (std::ranges::max(colours) != n - 1)
		bool sdjkhdsjk = true;

	return symmetric ? ColoursToPermSym(colours) : ColoursToPermUnsym(colours);
}