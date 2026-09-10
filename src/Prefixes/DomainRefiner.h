#pragma once
#include <vector>

class DomainRefiner
{
protected:
	static constexpr uint8_t None = 255;

public:
	DomainRefiner(uint8_t n_);

	// domains[src] stores a mask of destination bits that 'src' can be mapped to under permutation
	// returns 'true' if the domains permit a perfect matching
	// Requires n <= 32
	bool Refine(std::vector<uint64_t>& domains);

protected:
	// Parameters
	uint8_t n;

	// Global state
	uint64_t visited;
	std::vector<uint8_t> matchedSrc;
	std::vector<uint64_t> outAdj, inAdj;
	std::vector<uint8_t> L, sccIdxs;

	// Matching
	bool TryAugment(uint8_t src, const std::vector<uint64_t>& domains);
	bool FindMatching(const std::vector<uint64_t>& domains);

	// Graph Construction
	void BuildGraph(const std::vector<uint64_t>& domains);

	// Kosaraju's Algorithm
	void Visit(uint8_t v);
	void Assign(uint8_t u, uint8_t idx);
	void ExtractSCCs();

	void DoRefine(std::vector<uint64_t>& domains);

	void ResetState();
};