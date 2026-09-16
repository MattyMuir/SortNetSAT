#include <optional>

#include <sortnetutils.h>

#include "minisatutil.h"
#include "FormulaGenerator.h"
#include "Prefixes/SubsumptionSolver.h"

class UnextendableSetBuilder
{
public:
	UnextendableSetBuilder(uint8_t n_, uint8_t d_, bool symmetric_, const std::vector<Network>& prefixes_);

	std::vector<uint64_t> Build();
	double GetTotalSATTime() const;
	double GetTotalScoreTime() const;

protected:
	// Parameters
	uint8_t n, d;
	bool symmetric;
	std::vector<Network> prefixes;

	// Global state
	std::vector<std::vector<uint64_t>> prefixOutputs;
	std::vector<bool> subsumed;
	std::vector<Permutation> witnessPerms;
	FormulaGenerator generator;
	double lastSatTime, totalSatTime = 0.0, totalScoreTime = 0.0;
	Minisat::Solver satSolver;
	SubsumptionSolver subSolver;
	std::vector<uint64_t> X;

	bool IsSAT();
	bool SubsumedTrivially(size_t prefixIdx, uint64_t lastAdded);
	size_t ScoreElements(std::vector<size_t>& scores, std::optional<uint64_t> lastAdded);
	Network ReconstructPostfix() const;
	uint64_t ChooseNewInput(const std::vector<size_t>& scores) const;
	void AddNewInput(uint64_t x);
};