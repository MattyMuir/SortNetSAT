#pragma once
#include "minisatutil.h"
#include "Extender.h"
#include "FormulaGenerator.h"

class IncrementalExtender : public Extender
{
protected:
	struct FailingInput
	{
		size_t idx;
		uint64_t output;
	};

public:
	IncrementalExtender(uint8_t n_, uint8_t d_, bool symmetric_, const Network& prefix);

	void SetParameters(size_t maxAddNum_);
	bool Extend() override;
	Network GetNetwork() const override;

protected:
	size_t maxAddNum = 4;

	FormulaGenerator generator;
	Minisat::Solver solver;

	std::vector<uint64_t> excludedInputs, includedInputs;

	Network ReconstructPostfix() const;
	std::vector<FailingInput> GetFailingInputs() const;
	void LogProgress(size_t numFailing) const;
	size_t ComputeSimilarity(uint64_t input) const;
	uint64_t CountInversions(uint64_t output) const;
	std::vector<size_t> ChooseNewTestors(const std::vector<FailingInput>& failing) const;
	void IncludeNewInputs(const std::vector<size_t>& newTestors);
};