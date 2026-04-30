#pragma once
#include <cstdint>
#include <vector>

struct CE
{
	uint8_t lo, hi;
	auto operator<=>(const CE& other) const = default;
};
using Network = std::vector<CE>;

void PrintNetwork(const Network& network);