#include "PrefixDescriptor.h"

#include <thread>

PrefixDescriptor::Guard::Guard(PrefixDescriptor* descriptor_)
	: descriptor(descriptor_) {}

PrefixDescriptor::Guard::Guard(Guard&& other) noexcept
	: descriptor(std::exchange(other.descriptor, nullptr)) {}

PrefixDescriptor::Guard::~Guard()
{
	if (descriptor)
		descriptor->state.fetch_sub(CountUnit, std::memory_order_release);
}

const NetworkSignature& PrefixDescriptor::Guard::Signature() const
{
	return descriptor->signature;
}

PrefixDescriptor::PrefixDescriptor(size_t prevIdx_, size_t layerIdx_, uint8_t n)
	: prevIdx(prevIdx_), layerIdx(layerIdx_), signature(n), state(Empty) {}

PrefixDescriptor::PrefixDescriptor(PrefixDescriptor&& other) noexcept
	: prevIdx(other.prevIdx), layerIdx(other.layerIdx), signature(std::move(other.signature)), state(other.state.load()) {}

PrefixDescriptor& PrefixDescriptor::operator=(PrefixDescriptor&& other) noexcept
{
	prevIdx = other.prevIdx;
	layerIdx = other.layerIdx;
	signature = std::move(other.signature);
	state = other.state.load();
	return *this;
}

void PrefixDescriptor::ComputeSignature(const std::vector<uint64_t>& outputs)
{
	signature.Construct(outputs);
	state.store(Ready, std::memory_order_release);
	state.notify_all();
}

void PrefixDescriptor::MarkSubsumedAndFree()
{
	uint32_t word = state.load(std::memory_order_relaxed);
	for (;;)
	{
		if ((word & StateMask) != Ready)
			return; // someone else already subsumed it - not our job

		uint32_t desired = (word & ~StateMask) | Subsumed;
		if (state.compare_exchange_weak(word, desired,
			std::memory_order_acq_rel, std::memory_order_relaxed))
			break; // we won - we own the drain + free
	}

	// Drain any readers that acquired before our flip. New readers arriving
	// after this point see Subsumed and back out without touching data.
	while ((state.load(std::memory_order_acquire) >> 2) != 0)
		std::this_thread::yield();

	signature.Free();
}

bool PrefixDescriptor::IsSubsumed() const
{
	return (state & StateMask) == Subsumed;
}

std::optional<PrefixDescriptor::Guard> PrefixDescriptor::AcquireGuard()
{
	uint32_t word = state.load(std::memory_order_acquire);
	for (;;)
	{
		while ((word & StateMask) == Empty)
		{
			state.wait(word, std::memory_order_relaxed); // real blocking wait, not a spin
			word = state.load(std::memory_order_acquire);
		}
		if ((word & StateMask) == Subsumed)
			return std::nullopt; // same meaning as the old `if (subsumed) continue;`

		uint32_t prev = state.fetch_add(CountUnit, std::memory_order_acquire);
		if ((prev & StateMask) == Ready)
			return Guard{ this }; // we're now registered as an active reader

		// Raced with a subsumption between our load and our fetch_add - back out and reloop
		state.fetch_sub(CountUnit, std::memory_order_relaxed);
		word = prev;
	}
}