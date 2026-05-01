#pragma once
#include <cstdint>
#include <vector>

using Var = int64_t;
using Literal = int64_t;
using Clause = std::vector<Literal>;

struct ClauseHasher
{
    size_t operator()(const Clause& clause) const;
};

struct ClauseEq
{
    bool operator()(const Clause& a, const Clause& b) const;
};