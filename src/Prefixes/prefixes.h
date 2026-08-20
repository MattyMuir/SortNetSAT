#pragma once
#include <string>
#include <span>

#include <sortnetutils.h>

Network PrefixPar(uint8_t n);
Network PrefixBZ(uint8_t n);
uint64_t WindowWidth(uint8_t n, uint64_t input);
uint64_t WindowWidth(uint8_t n, std::span<const uint64_t> prefixOutputs, bool symmetric);
void SortByWindowWidth(uint8_t n, std::vector<uint64_t>& outputs);
Network GreedyPrefix(uint8_t n, uint8_t d, bool symmetric);
std::vector<Network> ParsePrefixFile(const std::string& filepath);
void SavePrefixFile(const std::string& filepath, const std::vector<Network>& prefixes);
void SortPrefixFile(const std::string& filepath);