#include "NetworkSignature.h"

#include <algorithm>
#include <utility>
#include <cassert>

#define GET_REFS(prefix, obj)\
decltype(auto) prefix##numOutputs = obj.Get(NumOutputs)[0];\
decltype(auto) prefix##popcountSum = obj.Get(PopcountSum)[0];\
auto prefix##t2 = obj.Get(T2);\
[[maybe_unused]] auto prefix##t3z = obj.Get(T3z);\
[[maybe_unused]] auto prefix##t3o = obj.Get(T3o);\
auto prefix##t5 = obj.Get(T5);\
auto prefix##t6 = obj.Get(T6);\
auto prefix##t5c = obj.Get(T5c);

NetworkSignature::NetworkSignature(uint8_t n_)
	: n(n_) {}

void NetworkSignature::Construct(const std::vector<uint64_t>& outputs, bool sorted_)
{
	// Allocate and zero-initialize memory
	sorted = sorted_;
	data = std::make_unique<size_t[]>(GetOffset(_SignatureTypeEnd));
	Initialize();

	// Get references to each signature type for readability
	GET_REFS(, (*this));

	// Compute all signatures
	numOutputs = outputs.size();
	for (uint64_t x : outputs)
	{
		uint64_t cluster = std::popcount(x);
		popcountSum += cluster;
		t2[cluster]++;
		t3z[cluster] |= ~x;
		t3o[cluster] |= x;

		// Loop over every set bit
		for (uint8_t i = 0; i < cluster; i++)
		{
			uint64_t bi = std::countr_zero(x);
			x &= x - 1;

			t5[bi]++;
			t6[bi] += cluster;
			t5c[cluster * n + bi]++;
		}
	}
	for (uint8_t cluster = 0; cluster <= n; cluster++)
	{
		t3o[cluster] = std::popcount(t3o[cluster]);
		t3z[cluster] &= ((1ULL << n) - 1);
		t3z[cluster] = std::popcount(t3z[cluster]);
	}
		

	// Sort T5, T6, and T5c if requested
	if (sorted)
	{
		std::ranges::sort(t5);
		std::ranges::sort(t6);
		for (uint8_t cluster = 0; cluster <= n; cluster++)
			std::sort(t5c.data() + cluster * n, t5c.data() + (cluster + 1) * n);
	}
}

void NetworkSignature::Free()
{
	data.reset();
}

bool NetworkSignature::IsFreed() const
{
	return !data;
}

bool NetworkSignature::operator>(const NetworkSignature& other) const
{
	assert(sorted);

	// Get references to each signature type for readability
	GET_REFS(a, (*this));
	GET_REFS(b, other);

	// Initial comparisons
	if (anumOutputs > bnumOutputs) return true;
	for (uint8_t cluster = 0; cluster <= n; cluster++)
	{
		if (at2[cluster] > bt2[cluster]) return true;
		if (at3z[cluster] > bt3z[cluster]) return true;
		if (at3o[cluster] > bt3o[cluster]) return true;
	}

	// T5
	for (uint8_t bi = 0; bi < n; bi++)
	{
		if (at5[bi] > bt5[bi]) return true;
		if (anumOutputs - at5[bi] > bnumOutputs - bt5[bi]) return true;
	}

	// T6
	for (uint8_t bi = 0; bi < n; bi++)
	{
		if (at6[bi] > bt6[bi]) return true;
		if (apopcountSum - at6[bi] > bpopcountSum - bt6[bi]) return true;
	}

	// T5c
	for (uint8_t cluster = 0; cluster <= n; cluster++)
	{
		for (uint8_t bi = 0; bi < n; bi++)
		{
			if (at5c[cluster * n + bi] > bt5c[cluster * n + bi]) return true;
			if (at2[cluster] - at5c[cluster * n + bi] > bt2[cluster] - bt5c[cluster * n + bi]) return true;
		}
	}

	return false;
}

std::vector<uint64_t> NetworkSignature::GetDomains(const NetworkSignature& src, const NetworkSignature& dst)
{
	assert(!src.sorted && !dst.sorted);

	// Get references to each signature type for readability
	uint8_t n = src.n;
	GET_REFS(src, src);
	GET_REFS(dst, dst);

	std::vector<uint64_t> domains(n, (1ULL << n) - 1);
	for (uint8_t src = 0; src < n; src++)
	{
		for (uint8_t dst = 0; dst < n; dst++)
		{
			uint64_t clearDst = ~(1ULL << dst);

			// T5
			if (srct5[src] > dstt5[dst])									domains[src] &= clearDst;
			if (srcnumOutputs - srct5[src] > dstnumOutputs - dstt5[dst])	domains[src] &= clearDst;

			// T6
			if (srct6[src] > dstt6[dst])									domains[src] &= clearDst;
			if (srcpopcountSum - srct6[src] > dstpopcountSum - dstt6[dst])	domains[src] &= clearDst;

			// T5c
			for (uint8_t cluster = 0; cluster <= n; cluster++)
			{
				if (srct5c[cluster * n + src] > dstt5c[cluster * n + dst])										domains[src] &= clearDst;
				if (srct2[cluster] - srct5c[cluster * n + src] > dstt2[cluster] - dstt5c[cluster * n + dst])	domains[src] &= clearDst;
			}
		}
	}

	return domains;
}

size_t NetworkSignature::GetSize(SignatureType sig) const
{
	switch (sig)
	{
	case NumOutputs:	return 1;
	case PopcountSum:	return 1;
	case T2:			return n + 1;
	case T3z:			return n + 1;
	case T3o:			return n + 1;
	case T5:			return n;
	case T6:			return n;
	case T5c:			return (n + 1) * n;
	default: std::unreachable();
	}
}

size_t NetworkSignature::GetOffset(SignatureType sig) const
{
	size_t offset = 0;
	for (size_t sigType = 0; sigType < sig; sigType++)
		offset += GetSize((SignatureType)sigType);
	return offset;
}

std::span<uint64_t> NetworkSignature::Get(SignatureType sig)
{
	return { data.get() + GetOffset(sig), GetSize(sig) };
}

std::span<const uint64_t> NetworkSignature::Get(SignatureType sig) const
{
	return { data.get() + GetOffset(sig), GetSize(sig) };
}

void NetworkSignature::Initialize()
{
	for (size_t sigType = 0; sigType < _SignatureTypeEnd; sigType++)
	{
		std::span<uint64_t> sig = Get((SignatureType)sigType);
		std::ranges::fill(sig, 0);
	}
}