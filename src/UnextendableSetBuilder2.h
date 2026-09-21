#include <optional>

#include <random>

#include <sortnetutils.h>

#include "minisatutil.h"
#include "FormulaGenerator.h"
#include "Prefixes/SubsumptionSolver.h"

class UnextendableSetBuilder2
{
public:
	UnextendableSetBuilder2(uint8_t n_, uint8_t d_, size_t maxWitnesses_, bool symmetric_, const std::vector<Network>& prefixes_);

	std::vector<uint64_t> Build();

protected:
	// Parameters
	uint8_t n, d;
	size_t maxWitnesses;
	bool symmetric;
	std::vector<Network> prefixes;

	// Global state
	std::vector<std::vector<uint64_t>> prefixOutputs;

	// SAT State
	FormulaGenerator generator;
	Minisat::Solver satSolver;
	double lastSatTime, totalSatTime = 0.0, totalScoreTime = 0.0;

	// Build state
	std::vector<uint64_t> X;
	SubsumptionSolver subSolver;
	std::vector<std::vector<Permutation>> witnessPerms;
	std::vector<bool> isComplete;

	Permutation RandomPerm(std::mt19937_64& gen) const;
	void InitializeWitnesses();
	bool IsSAT();
	void RebuildWitnesses(bool forceUntangled);
	void FilterWitnesses(size_t prefixIdx, uint64_t lastAdded);
	std::vector<size_t> ScoreElements();
	Network ReconstructPostfix() const;
	std::optional<uint64_t> ChooseNewInput(const std::vector<size_t>& scores) const;
	void AddNewInput(uint64_t x);
};