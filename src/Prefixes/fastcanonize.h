#pragma once
#include <optional>

#include <sortnetutils.h>

std::optional<Permutation> FastCanonize(const std::vector<uint64_t>& outputs, uint8_t n, bool symmetric);