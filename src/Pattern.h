#pragma once
#include <sortnetutils.h>

struct Pattern
{
	uint8_t m;
	std::vector<Network> ces;
	std::vector<std::vector<uint8_t>> singletons;
};

bool ContainsPattern(const std::vector<Network>& layers, uint8_t n, const Pattern& pattern);