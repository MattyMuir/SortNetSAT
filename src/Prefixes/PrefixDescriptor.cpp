#include "PrefixDescriptor.h"

#include <thread>

PrefixDescriptor::PrefixDescriptor(size_t prevIdx_, size_t layerIdx_, uint8_t n)
	: prevIdx(prevIdx_), layerIdx(layerIdx_), signature(n), isEmpty(true), isSubsumed(false)
{}

PrefixDescriptor::PrefixDescriptor(PrefixDescriptor&& other) noexcept
	: prevIdx(other.prevIdx), layerIdx(other.layerIdx), signature(std::move(other.signature)),
	isEmpty(other.isEmpty.load()), isSubsumed(other.isSubsumed)
{}

PrefixDescriptor& PrefixDescriptor::operator=(PrefixDescriptor&& other) noexcept
{
	prevIdx = other.prevIdx;
	layerIdx = other.layerIdx;
	signature = std::move(other.signature);
	isEmpty = other.isEmpty.load(std::memory_order_relaxed);
	isSubsumed = other.isSubsumed;
	return *this;
}

void PrefixDescriptor::ComputeSignature(const std::vector<uint64_t>& outputs)
{
	signature.Construct(outputs);
	isEmpty.store(false, std::memory_order_release);
	isEmpty.notify_all();
}

void PrefixDescriptor::WaitForSignature()
{
	isEmpty.wait(true, std::memory_order_relaxed);
}

void PrefixDescriptor::MarkSubsumed()
{
	isSubsumed = true;
}

void PrefixDescriptor::FreeSignature()
{
	signature.Free();
}

void PrefixDescriptor::ResetUnatomic()
{
	signature.Free();
	isEmpty.store(true, std::memory_order_relaxed);
}

bool PrefixDescriptor::IsFreed() const
{
	return signature.IsFreed();
}