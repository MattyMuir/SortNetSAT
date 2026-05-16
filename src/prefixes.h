#pragma once
#include <string>

#include <sortnetutils.h>

Network PrefixPar(uint8_t n);
Network PrefixBZ(uint8_t n);
uint64_t WindowWidth(uint8_t n, uint64_t input);
uint64_t WindowWidth(uint8_t n, const std::vector<uint64_t>& prefixOutputs, bool symmetric);
Network GreedyPrefix(uint8_t n, uint8_t d, bool symmetric);
std::vector<Network> ParsePrefixFile(const std::string& filepath);