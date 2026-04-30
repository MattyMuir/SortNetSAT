#include "prefixes.h"

#include <print>
#include <random>
#include <numeric>
#include <bit>
#include <map>
#include <set>

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

static inline bool IsSorted(uint8_t n, uint64_t output)
{
	uint64_t numZeros = n - std::popcount(output);
	uint64_t sorted = ((1ULL << n) - 1) ^ ((1ULL << numZeros) - 1);
	return output == sorted;
}

static inline uint64_t Transpose8x8(uint64_t m)
{
	uint64_t t;
	t = (m ^ (m >> 7)) & 0x00AA00AA00AA00AAULL; m ^= t ^ (t << 7);
	t = (m ^ (m >> 14)) & 0x0000CCCC0000CCCCULL; m ^= t ^ (t << 14);
	t = (m ^ (m >> 28)) & 0x00000000F0F0F0F0ULL; m ^= t ^ (t << 28);
	return m;
}

static inline void Transpose(uint64_t* transposed, uint8_t n, const std::vector<uint64_t>& outputs)
{
	memset(transposed, 0, 64 * sizeof(uint64_t));

	for (uint8_t groupBase = 0; groupBase < n; groupBase += 8)
	{
		const uint8_t groupEnd = std::min<uint8_t>(groupBase + 8, n);
		const uint8_t groupIdx = groupBase >> 3; // which byte lane within each transposed uint64_t

		for (uint8_t byteIdx = 0; byteIdx < 8; byteIdx++)
		{
			// Gather byte `byteIdx` from each channel in this group into one uint64_t
			// (one channel per byte of `packed`)
			uint64_t packed = 0;
			for (uint8_t g = groupBase; g < groupEnd; g++)
				packed |= (uint64_t)((outputs[g] >> (byteIdx * 8)) & 0xFF) << ((g - groupBase) * 8);

			// Transpose the 8×8 bit matrix: rows were channels, columns were bit positions
			// After transpose: row p = all 8 channels' contribution to output column byteIdx*8+p
			uint64_t tp = Transpose8x8(packed);

			// Scatter: write byte p of tp into byte `groupIdx` of transposed[byteIdx*8+p].
			// All 8 writes land in the same cache line.
			for (uint8_t p = 0; p < 8; p++)
				reinterpret_cast<uint8_t*>(&transposed[byteIdx * 8 + p])[groupIdx] = (uint8_t)(tp >> (p * 8));
		}
	}
}

static inline uint64_t ReverseBits(uint64_t x)
{
	x = __builtin_bswap64(x);
	x = ((x >> 1) & 0x5555555555555555ULL) | ((x & 0x5555555555555555ULL) << 1);
	x = ((x >> 2) & 0x3333333333333333ULL) | ((x & 0x3333333333333333ULL) << 2);
	x = ((x >> 4) & 0x0F0F0F0F0F0F0F0FULL) | ((x & 0x0F0F0F0F0F0F0F0FULL) << 4);
	return x;
}

static inline bool HasSmallerMirror(uint8_t n, uint64_t input)
{
	uint64_t flipped = ~input;
	uint64_t reversed = ReverseBits(flipped) >> (64 - n);
	return input > reversed;
}

std::vector<uint64_t> UnsortedPrefixOutputs(uint8_t n, const Network& prefix, bool symmetric)
{
	std::vector<bool> isOutput((1ULL << n), false);
	std::vector<uint64_t> allOutputs;

	std::vector<uint64_t> inputs{
		0b1010101010101010101010101010101010101010101010101010101010101010,
		0b1100110011001100110011001100110011001100110011001100110011001100,
		0b1111000011110000111100001111000011110000111100001111000011110000,
		0b1111111100000000111111110000000011111111000000001111111100000000,
		0b1111111111111111000000000000000011111111111111110000000000000000,
		0b1111111111111111111111111111111100000000000000000000000000000000
	};
	inputs.resize(n);
	std::vector<uint64_t> outputs;

	uint64_t numPacks = ((1ULL << n) + 63) / 64;
	for (uint64_t packIdx = 0; packIdx < numPacks; packIdx++)
	{
		// Update inputs based on the pack index
		for (uint8_t i = 0; (int)i < n - 6; i++)
			inputs[i + 6] = (packIdx & (1ULL << i)) ? (uint64_t)-1 : 0;

		// Run the prefix
		outputs = inputs;
		for (auto [lo, hi] : prefix)
		{
			uint64_t newLo = outputs[lo] & outputs[hi];
			outputs[hi] = outputs[lo] | outputs[hi];
			outputs[lo] = newLo;
		}

		// Transpose the outputs
		uint64_t transposed[64];
		Transpose(transposed, n, outputs);

		// Add the distinct and unsorted outputs
		for (uint64_t output : transposed)
		{
			if (IsSorted(n, output)) continue;
			if (symmetric && HasSmallerMirror(n, output)) continue;
			if (!isOutput[output]) allOutputs.push_back(output);
			isOutput[output] = true;
		}
	}

	return allOutputs;
}

uint64_t WindowWidth(uint8_t n, const std::vector<uint64_t>& prefixOutputs)
{
	uint64_t windowWidth = 0;
	for (uint64_t output : prefixOutputs)
	{
		uint64_t leadingZeros = std::min<uint64_t>(n, std::countr_zero(output));
		uint64_t tailingOnes = std::countl_one(output << (64 - n));
		windowWidth += (n - leadingZeros - tailingOnes);
	}
	return windowWidth;
}

// Distinct random pair in [0, max]
static inline std::pair<uint8_t, uint8_t> RandomPair2(uint8_t max)
{
    static std::mt19937_64 gen{ std::random_device{}() };
    std::uniform_int_distribution<uint32_t> aDist{ 0, max };
    std::uniform_int_distribution<uint32_t> bDist{ 0, max - 1U };

    uint8_t a = (uint8_t)aDist(gen);
    uint8_t b = (uint8_t)bDist(gen);

    return { a, (b == a) ? max : b };
}

void Permute(uint8_t n, Network& network, bool symmetric)
{
    auto [a, b] = RandomPair2(n - 1);

    std::vector<uint8_t> mapsTo(n);
    std::iota(mapsTo.begin(), mapsTo.end(), 0);
    std::swap(mapsTo[a], mapsTo[b]);

	if (symmetric)
	{
		uint8_t aSym = n - 1 - b;
		uint8_t bSym = n - 1 - a;
		if (aSym != a) std::swap(mapsTo[aSym], mapsTo[bSym]);
	}
    
    for (CE& ce : network)
    {
        int from = mapsTo[ce.lo];
        int to = mapsTo[ce.hi];
        if (from > to)
        {
            // All further occurcences of "from" will be replaced by "to", and vice versa
            mapsTo[ce.lo] = to;
            mapsTo[ce.hi] = from;
            // Turn direction of comparator
            ce.lo = to;
            ce.hi = from;
        }
        else
        {
            // Correct direction, nothing to do
            ce.lo = from;
            ce.hi = to;
        }
    }
}

Network OptimizePrefix(uint8_t n, const Network& input, size_t runs, bool symmetric)
{
	// === Parameters ===
	static constexpr size_t PopulationSize = 32;
	// ==================

	// Add the initial prefix to the population
	std::multimap<uint64_t, Network> pop;
	uint64_t initial = WindowWidth(n, UnsortedPrefixOutputs(n, input, symmetric));
	pop.insert({ initial, input });

	// Get some more population members... Just do 20 random swaps
	for (int i = 0; i < 10; i++)
	{
		Network permuted{ input };

		for (size_t i = 0; i < 20; i++)
			Permute(n, permuted, symmetric);

		uint64_t width = WindowWidth(n, UnsortedPrefixOutputs(n, permuted, symmetric));
		pop.insert({ width, permuted });
	}

	for (size_t runIdx = 0; runIdx < runs; runIdx++)
	{
		std::println("Run: {:<6} Best: {:<10}", runIdx, pop.begin()->first);
		std::multimap<uint64_t, Network> nextGen{ pop };

		for (const auto& [windowWidth, prefix] : pop)
		{
			Network permuted{ prefix };
			Permute(n, permuted, symmetric);
			uint64_t width = WindowWidth(n, UnsortedPrefixOutputs(n, permuted, symmetric));
			nextGen.insert({ width, permuted });
		}

		pop.clear();
		std::set<Network> checker;
		for (const auto& [windowWidth, prefix] : nextGen)
		{
			if (pop.size() >= PopulationSize) break;
			if (checker.contains(prefix)) continue;

			pop.insert({ windowWidth, prefix });
			checker.insert(prefix);
		}
	}

	return pop.begin()->second;
}