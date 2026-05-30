#include "prefixes.h"

#include <random>
#include <bit>
#include <ranges>
#include <fstream>
#include <algorithm>

Network PrefixPar(uint8_t n)
{
	Network prefix;
	for (uint8_t i = 0; i < n - 1; i += 2)
		prefix.push_back(CE{ i, (uint8_t)(i + 1) });
	return prefix;
}

Network PrefixBZ(uint8_t n)
{
	Network prefix;
	for (uint8_t i = 0; i < n / 2; i++)
		prefix.push_back(CE{ i, (uint8_t)(n - 1 - i) });
	return prefix;
}

uint64_t WindowWidth(uint8_t n, uint64_t input)
{
	uint64_t leadingZeros = std::min<uint64_t>(n, std::countr_zero(input));
	uint64_t tailingOnes = std::countl_one(input << (64 - n));
	return n - leadingZeros - tailingOnes;
}

uint64_t WindowWidth(uint8_t n, const std::vector<uint64_t>& prefixOutputs, bool symmetric)
{
	uint64_t windowWidth = 0;
	for (uint64_t output : prefixOutputs)
		if (!symmetric || !HasSmallerMirror(n, output))
			windowWidth += WindowWidth(n, output);

	return windowWidth;
}

void SortByWindowWidth(uint8_t n, std::vector<uint64_t>& outputs)
{
	std::sort(outputs.begin(), outputs.end(), [n](uint64_t a, uint64_t b) {
		return WindowWidth(n, a) < WindowWidth(n, b);
		});
}

static inline bool ShareChannel(uint8_t i0, uint8_t j0, uint8_t i1, uint8_t j1, uint8_t n, bool symmetric)
{
	bool shares = i0 == i1
		|| i0 == j1
		|| j0 == i1
		|| j0 == j1;

	if (!symmetric) return shares;

	uint8_t i0Sym = n - 1 - j0;
	uint8_t j0Sym = n - 1 - i0;
	bool sharesSym = i0Sym == i1
		|| i0Sym == j1
		|| j0Sym == i1
		|| j0Sym == j1;

	return shares || sharesSym;
}

Network GreedyPrefix(uint8_t n, uint8_t d, bool symmetric)
{
	static std::mt19937_64 gen{ std::random_device{}() };
	Network prefix = PrefixBZ(n);

	for (uint8_t k = 0; k < d - 1; k++)
	{
		// Prepare comparator alphabet
		std::vector<CE> alphabet;
		for (uint8_t i = 0; i < n - 1; i++)
			for (uint8_t j = i + 1; j < n; j++)
				if (symmetric && i <= n - 1 - j) alphabet.push_back({ i, j });

		while (!alphabet.empty())
		{
			// Find the greedy best CEs
			std::vector<CE> bestCEs;
			uint64_t fewestRemaining = UINT64_MAX;
			for (CE ce : alphabet)
			{
				prefix.push_back(ce);
				uint64_t numOutputs = GetOutputs(prefix, n, true, symmetric).size();
				prefix.pop_back();

				if (numOutputs > fewestRemaining) continue;
				if (numOutputs < fewestRemaining) bestCEs.clear();
				bestCEs.push_back(ce);
				fewestRemaining = numOutputs;
			}

			// Randomly choose a tied best CE
			std::uniform_int_distribution<size_t> dist{ 0, bestCEs.size() - 1 };
			CE newCE = bestCEs[dist(gen)];
			prefix.push_back(newCE);
			if (newCE.lo != n - 1 - newCE.hi)
				prefix.push_back({ (uint8_t)(n - 1 - newCE.hi), (uint8_t)(n - 1 - newCE.lo) });

			// Remove conflicting comparators from the alphabet
			std::erase_if(alphabet, [&](CE alphCE) {
				return ShareChannel(newCE.lo, newCE.hi, alphCE.lo, alphCE.hi, n, symmetric);
				});
		}
	}

	return prefix;
}

static inline std::vector<std::string> Split(std::string_view str, std::string_view delim)
{
	std::vector<std::string> parts;
	for (const auto& part : std::views::split(str, delim))
		parts.emplace_back(part.begin(), part.end());
	return parts;
}

std::vector<Network> ParsePrefixFile(const std::string& filepath)
{
	std::vector<Network> prefixes;

	std::ifstream file{ filepath };
	std::string line;
	while (std::getline(file, line))
	{
		Network prefix;

		auto parts = Split(line, ";");
		for (size_t ceIdx = 0; ceIdx < parts.size() / 3; ceIdx++)
		{
			uint8_t lo = std::stoul(parts[ceIdx * 3 + 1]);
			uint8_t hi = std::stoul(parts[ceIdx * 3 + 2]);
			prefix.push_back({ lo, hi });
		}

		prefixes.push_back(prefix);
	}

	return prefixes;
}