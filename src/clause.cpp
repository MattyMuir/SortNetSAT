#include "clause.h"

static inline size_t HashCombine(size_t a, size_t b)
{
	return a ^= b + 0x9e3779b97f4a7c15ULL + (a << 12) + (a >> 4);
}

size_t ClauseHasher::operator()(const Clause& clause) const
{
	size_t hash = clause.size();
	for (Literal l : clause)
		hash = HashCombine(hash, l);
	return hash;
}