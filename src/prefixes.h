#pragma once
#include "Network.h"

Network PrefixPar(uint8_t n);
Network PrefixBZ(uint8_t n);
std::vector<uint64_t> UnsortedPrefixOutputs(uint8_t n, const Network& prefix, bool symmetric);
uint64_t WindowWidth(uint8_t n, const std::vector<uint64_t>& prefixOutputs);
void Permute(uint8_t n, Network& network, bool symmetric);
Network OptimizePrefix(uint8_t n, const Network& input, size_t runs, bool symmetric);