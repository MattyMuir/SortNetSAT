#include "clause.h"

#include <algorithm>

static inline size_t HashCombine(size_t a, size_t b)
{
	return a ^= b + 0x9e3779b97f4a7c15ULL + (a << 12) + (a >> 4);
}

size_t ClauseHasher::operator()(const Clause& clause) const
{
	Clause sorted{ clause };
	std::sort(sorted.begin(), sorted.end());

	size_t hash = sorted.size();
	for (Literal l : sorted)
		hash = HashCombine(hash, l);
	return hash;
}

bool ClauseEq::operator()(const Clause& a, const Clause& b) const
{
	if (a.size() != b.size()) return false;

	Clause aSorted{ a };
	Clause bSorted{ a };
	std::sort(aSorted.begin(), aSorted.end());
	std::sort(bSorted.begin(), bSorted.end());
	return aSorted == bSorted;
}