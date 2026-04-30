#pragma once
#include "Network.h"

Network PrefixPar(uint8_t n);
Network PrefixBZ(uint8_t n);
std::vector<uint64_t> PrefixOutputs(uint8_t n, const Network& prefix, bool onlyUnsorted, bool symmetric);
uint64_t WindowWidth(uint8_t n, const std::vector<uint64_t>& prefixOutputs, bool symmetric);
void PermuteAndUntangle(Network& network, const std::vector<uint8_t>& perm);
void Permute(Network& network, const std::vector<uint8_t>& perm);