#pragma once
#include <span>
#include <memory>

#include <sortnetutils.h>

enum SignatureType : size_t
{
	NumOutputs,
	PopcountSum,
	T2,
	T3z,
	T3o,
	T5,
	T6,
	T5c,
	_SignatureTypeEnd
};

class NetworkSignature
{
public:
	NetworkSignature(uint8_t n);

	void Construct(const std::vector<uint64_t>& outputs);
	void Free();
	bool IsFreed() const;
	size_t GetNumOutputs() const;

	bool operator>(const NetworkSignature& other) const;

protected:
	uint8_t n;
	std::unique_ptr<size_t[]> data;

	std::pair<size_t, size_t> GetDim(SignatureType sig) const;
	size_t GetSize(SignatureType sig) const;
	size_t GetOffset(SignatureType sig) const;
	std::span<uint64_t>			Get(SignatureType sig);
	std::span<const uint64_t>	Get(SignatureType sig) const;
	std::span<uint64_t>			Get(SignatureType sig, size_t row);
	std::span<const uint64_t>	Get(SignatureType sig, size_t row) const;

	std::vector<std::vector<uint64_t>> SplitIntoClusters(const std::vector<uint64_t>& outputs) const;
	void T5Signature(std::span<uint64_t> dst, const std::vector<uint64_t>& outputs) const;
	void T6Signature(std::span<uint64_t> dst, const std::vector<uint64_t>& outputs) const;

	bool TGreater(std::span<const uint64_t> at, std::span<const uint64_t> bt, size_t aSize, size_t bSize) const;
};