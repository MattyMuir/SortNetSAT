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

	void Construct(const std::vector<uint64_t>& outputs, bool sorted_);
	void Free();
	bool IsFreed() const;

	bool operator>(const NetworkSignature& other) const;
	static std::vector<uint64_t> GetDomains(const NetworkSignature& src, const NetworkSignature& dst);

protected:
	uint8_t n;
	bool sorted;
	std::unique_ptr<uint64_t[]> data;

	size_t GetSize(SignatureType sig) const;
	size_t GetOffset(SignatureType sig) const;
	std::span<uint64_t>			Get(SignatureType sig);
	std::span<const uint64_t>	Get(SignatureType sig) const;

	void Initialize();
};