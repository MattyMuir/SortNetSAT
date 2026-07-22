#pragma once
#include <atomic>
#include <optional>

#include "NetworkSignature.h"

class PrefixDescriptor
{
protected:
	static constexpr uint32_t Empty			= 0b000;
	static constexpr uint32_t Ready			= 0b001;
	static constexpr uint32_t Subsumed		= 0b010;
	static constexpr uint32_t StateMask		= 0b011;
	static constexpr uint32_t CountUnit		= 0b100;

public:
	class Guard
	{
	public:
		explicit Guard(PrefixDescriptor* descriptor_);
		Guard(Guard&& other) noexcept;
		Guard(const Guard& other) = delete;
		~Guard();

		const NetworkSignature& Signature() const;

	protected:
		PrefixDescriptor* descriptor;
	};

public:
	PrefixDescriptor(size_t prevIdx_, size_t layerIdx_, uint8_t n);
	PrefixDescriptor(PrefixDescriptor&& other) noexcept;
	PrefixDescriptor& operator=(PrefixDescriptor&& other) noexcept;

	size_t prevIdx, layerIdx;

	void ComputeSignature(const std::vector<uint64_t>& outputs);
	void MarkSubsumed();
	void ResetUnatomic();

	bool IsSubsumed() const;
	std::optional<Guard> AcquireGuard();

protected:
	NetworkSignature signature;
	std::atomic<uint32_t> state;
};