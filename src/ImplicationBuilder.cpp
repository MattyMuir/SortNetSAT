#include "ImplicationBuilder.h"

#include <random>

#include "FormulaGenerator2.h"
#include "Prefixes/prefixes.h"

ImplicationBuilder::ImplicationBuilder(uint8_t n_, uint8_t d_, bool symmetric_)
    : n(n_), d(d_), symmetric(symmetric_) {}

std::vector<uint64_t> ImplicationBuilder::Build(size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        auto satNetworks = GetSATNetworks(50);

        // Score elements by how often they are unsorted
        std::vector<size_t> elemCounts(1ULL << n);
        for (uint64_t elem = 0; elem < (1ULL << n); elem++)
            for (const Network& network : satNetworks)
                if (!IsSorted(n, network(elem)))
                    elemCounts[elem]++;

        L.push_back(ChooseNewElement(elemCounts));
    }

    return L;
}

std::vector<bool> ImplicationBuilder::GetAssignment(const Minisat::Solver& solver, size_t compMin, size_t compMax) const
{
    std::vector<bool> assignment(compMax - compMin + 1);
    for (Literal v = compMin; v <= compMax; v++)
        assignment[v - compMin] = (solver.model[v - 1] == Minisat::l_True);
    return assignment;
}

std::vector<Network> ImplicationBuilder::GetSATNetworks(size_t maxNum)
{
    // Construct formula
    FormulaGenerator2 generator{ n, d, symmetric };
    generator.Initialize();
    for (uint64_t l : L) generator.AddInput(l);

    // Random number source
    std::mt19937_64 gen{ std::random_device{}() };
    std::uniform_int_distribution<uint32_t> seedDist{ 1, 2147483646 };

    std::vector<Network> networks;
    for (size_t i = 0; i < maxNum; i++)
    {
        // Load into SAT solver
        Minisat::Solver solver;
        solver.random_seed = seedDist(gen);
        solver.random_var_freq = 0.01;
        solver.rnd_init_act = true;
        LoadExpressionMinisat(solver, generator.GetExpression());

        // Generate a network that sorts everything in L
        Minisat::vec<Minisat::Lit> dummy;
        Minisat::lbool ret = solver.solveLimited(dummy);
        if (ret != Minisat::l_True) break;

        // Ban assignment from future solves
        auto [compMin, compMax] = generator.CompVarRange();
        auto assignment = GetAssignment(solver, compMin, compMax);
        generator.BanAssignment(assignment);

        // Add network to vector
        networks.emplace_back(generator.ParseAssignment(assignment));
    }

    return networks;
}

uint64_t ImplicationBuilder::ChooseNewElement(const std::vector<size_t>& elemCounts) const
{
    size_t maxCount = std::ranges::max(elemCounts);

    uint64_t bestWidth = UINT64_MAX;
    uint64_t bestX;
    for (uint64_t x = 0; x < (1ULL << n); x++)
    {
        if (elemCounts[x] < maxCount) continue;
        uint64_t windowWidth = WindowWidth(n, x);
        if (windowWidth < bestWidth)
        {
            bestWidth = windowWidth;
            bestX = x;
        }
    }

    return bestX;
}
