#include "NetworkSignature.h"

#include <algorithm>
#include <utility>

NetworkSignature::NetworkSignature(const std::vector<uint64_t>& outputs, uint8_t n_)
	: n(n_), data(std::make_unique<size_t[]>(GetOffset(_SignatureTypeEnd)))
{
	// === T1 Signature ===
	Get(NumOutputs)[0] = outputs.size();

	// === T2 Signature ===
	auto clusters = SplitIntoClusters(outputs);
	for (uint8_t popcount = 0; popcount <= n; popcount++)
		Get(T2)[popcount] = clusters[popcount].size();

	// === T3 Signature ===
	std::vector<uint64_t> zeroPos(n + 1), onePos(n + 1);
	for (uint64_t output : outputs)
	{
		uint64_t popcount = std::popcount(output);
		zeroPos[popcount] |= (~output) & ((1ULL << n) - 1);
		onePos[popcount] |= output;
	}
	for (uint8_t popcount = 0; popcount <= n; popcount++)
	{
		Get(T3z)[popcount] = std::popcount(zeroPos[popcount]);
		Get(T3o)[popcount] = std::popcount(onePos[popcount]);
	}

	// === T5 Signature ===
	T5Signature(Get(T5), outputs);
	std::vector<std::vector<size_t>> t5c;
	for (uint8_t popcount = 0; popcount <= n; popcount++)
		T5Signature(Get(T5c, popcount), clusters[popcount]);

	// === T6 Signature ===
	size_t popcountSum = 0;
	for (uint64_t output : outputs)
		popcountSum += std::popcount(output);
	Get(PopcountSum)[0] = popcountSum;
	T6Signature(Get(T6), outputs);
}

bool NetworkSignature::operator>(const NetworkSignature& other) const
{
	// === T1 Signature ===
	if (Get(NumOutputs)[0] > other.Get(NumOutputs)[0])
		return true;

	// === T3 Signature ===
	for (uint8_t popcount = 0; popcount <= n; popcount++)
	{
		if (Get(T3z)[popcount] > other.Get(T3z)[popcount]
			|| Get(T3o)[popcount] > other.Get(T3o)[popcount])
			return true;
	}

	// === T5 Signature ===
	if (TGreater(Get(T5), other.Get(T5), Get(NumOutputs)[0], other.Get(NumOutputs)[0]))
		return true;
	for (uint8_t popcount = 0; popcount <= n; popcount++)
		if (TGreater(Get(T5c, popcount), other.Get(T5c, popcount), Get(T2)[popcount], other.Get(T2)[popcount]))
			return true;

	// === T6 Signature ===
	return TGreater(Get(T6), other.Get(T6), Get(PopcountSum)[0], other.Get(PopcountSum)[0]);
}

void NetworkSignature::Free()
{
	data.reset();
}

std::pair<size_t, size_t> NetworkSignature::GetDim(SignatureType sig) const
{
	switch (sig)
	{
	case NumOutputs:	return { 1, 1 };
	case PopcountSum:	return { 1, 1 };
	case T2:			return { n + 1, 1 };
	case T3z:			return { n + 1, 1 };
	case T3o:			return { n + 1, 1 };
	case T5:			return { n, 1 };
	case T6:			return { n, 1 };
	case T5c:			return { n + 1, n };
	default: std::unreachable();
	}
}

size_t NetworkSignature::GetSize(SignatureType sig) const
{
	auto [sx, sy] = GetDim(sig);
	return sx * sy;
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

std::span<uint64_t> NetworkSignature::Get(SignatureType sig, size_t row)
{
	auto [sx, sy] = GetDim(sig);
	size_t offset = GetOffset(sig) + sy * row;
	return { data.get() + offset, sy };
}

std::span<const uint64_t> NetworkSignature::Get(SignatureType sig, size_t row) const
{
	auto [sx, sy] = GetDim(sig);
	size_t offset = GetOffset(sig) + sy * row;
	return { data.get() + offset, sy };
}

std::vector<std::vector<uint64_t>> NetworkSignature::SplitIntoClusters(const std::vector<uint64_t>& outputs) const
{
	std::vector<std::vector<uint64_t>> clusters(n + 1);
	for (uint64_t output : outputs)
		clusters[std::popcount(output)].push_back(output);
	return clusters;
}

void NetworkSignature::T5Signature(std::span<uint64_t> dst, const std::vector<uint64_t>& outputs) const
{
	std::fill(dst.begin(), dst.end(), 0);
	for (uint64_t output : outputs)
		for (uint8_t bi = 0; bi < n; bi++)
			dst[bi] += (output >> bi) & 1ULL;
	std::sort(dst.begin(), dst.end());
}

void NetworkSignature::T6Signature(std::span<uint64_t> dst, const std::vector<uint64_t>& outputs) const
{
	std::fill(dst.begin(), dst.end(), 0);
	for (uint64_t output : outputs)
	{
		uint64_t popcount = std::popcount(output);
		for (uint8_t bi = 0; bi < n; bi++)
			if ((output >> bi) & 1)
				dst[bi] += popcount;
	}
	std::sort(dst.begin(), dst.end());
}

bool NetworkSignature::TGreater(std::span<const uint64_t> at, std::span<const uint64_t> bt, size_t aSize, size_t bSize) const
{
	for (uint8_t i = 0; i < n; i++)
		if (at[i] > bt[i] || bt[i] - at[i] > bSize - aSize)
			return true;
	return false;
}