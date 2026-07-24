#pragma once
#include <atomic>
#include <optional>

#include "NetworkSignature.h"

class PrefixDescriptor
{
protected:

public:
	size_t prevIdx, layerIdx;
	NetworkSignature signature;
	std::atomic<bool> isEmpty;
	bool isSubsumed;

	PrefixDescriptor(size_t prevIdx_, size_t layerIdx_, uint8_t n);
	PrefixDescriptor(PrefixDescriptor&& other) noexcept;
	PrefixDescriptor& operator=(PrefixDescriptor&& other) noexcept;

	void ComputeSignature(const std::vector<uint64_t>& outputs);
	void WaitForSignature();
	void MarkSubsumed();
	void FreeSignature();
	void ResetUnatomic();

	bool IsFreed() const;
};