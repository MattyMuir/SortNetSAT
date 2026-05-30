#include "Pattern.h"

#include <bit>

static inline bool MeetsPattern(const std::vector<Network>& prefix, const Pattern& pattern, uint8_t n, const std::vector<uint8_t>& c)
{
	uint8_t m = pattern.m;
	uint8_t d = pattern.ces.size();

	for (uint8_t k = 0; k < d; k++)
	{
		// Check that every full comparator in the pattern is contained in the prefix
		for (auto [loP, hiP] : pattern.ces[k])
		{
			uint8_t lo = c[loP];
			uint8_t hi = c[hiP];
			auto it = std::find(prefix[k].begin(), prefix[k].end(), CE{ lo, hi });
			if (it == prefix[k].end()) return false;
		}

		// Check that every external comparator in the pattern is contained in the prefix
		for (uint8_t singletonP : pattern.singletons[k])
		{
			uint8_t singleton = c[singletonP];

			bool hasExternal = false;
			for (auto [lo, hi] : prefix[k])
			{
				if (lo != singleton && hi != singleton) continue;
				uint8_t other = (lo == singleton) ? hi : lo;
				auto it = std::find(c.begin(), c.end(), other);
				if (it == c.end()) hasExternal = true;
			}

			if (!hasExternal) return false;
		}

		// Determine which channels are used in the pattern
		std::vector<bool> patternChannels(m, false);
		for (auto [loP, hiP] : pattern.ces[k])
		{
			patternChannels[loP] = true;
			patternChannels[hiP] = true;
		}
		for (uint8_t singleton : pattern.singletons[k])
			patternChannels[singleton] = true;

		// Loop over unused pattern channels and ensure nothing connects to them
		for (uint8_t iP = 0; iP < m; iP++)
		{
			if (patternChannels[iP]) continue;
			uint8_t i = c[iP];
			for (auto [lo, hi] : prefix[k])
				if (lo == i || hi == i)
					return false;
		}
	}

	return true;
}

bool ContainsPattern(const std::vector<Network>& layers, uint8_t n, const Pattern& pattern)
{
	if (layers.size() < pattern.ces.size()) return false;

	// Useful constants
	uint8_t m = pattern.m;
	uint8_t d = pattern.ces.size();

	// Extract d-layer prefix of 'layers'
	std::vector<Network> prefix{ layers.begin(), layers.begin() + d };

	// Enumerate every m-subset of the n channels
	for (uint64_t subset = 0; subset < (1ULL << n); subset++)
	{
		if (std::popcount(subset) != m) continue;

		std::vector<uint8_t> c;
		for (uint8_t i = 0; i < n; i++)
			if ((subset >> i) & 1)
				c.push_back(i);

		// Check if this subset meets the pattern
		if (MeetsPattern(prefix, pattern, n, c))
			return true;
	}

	return false;
}