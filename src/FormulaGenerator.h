#pragma once
#include <cstdint>
#include <unordered_map>

#include <sortnetutils.h>

#include "Expression.h"
#include "VariableFamily.h"

struct CEd
{
	uint8_t l, lo, hi;
};

class FormulaGenerator
{
public:
	FormulaGenerator(uint8_t n_, uint8_t d_, bool symmetric_);

	void Generate(const std::vector<uint64_t>& inputs_ = {});
	void AddInput(uint64_t input);
	void AddInputs(const std::vector<uint64_t>& inputs);
	void BanAssignment(const std::vector<bool>& assignment);
	
	// Getters
	const Expression& GetExpression() const;
	std::pair<Literal, Literal> CompVarRange() const; // Returns the range [gMin, gMax] of comparator variables

	// Expects the comparator variables in the range [gMin, gMax]
	Network ParseAssignment(const std::vector<bool>& assignment) const;
	void LogVariableInfo(Literal var);

protected:
	uint8_t n, d;
	bool symmetric;
	std::vector<uint64_t> inputs;

	Expression expr;
	Literal trueVar, falseVar;
	Literal compMin, compMax;
	VariableFamily comps, used, oneDown, oneUp;
	std::unordered_map<Clause, Literal, ClauseHasher> clauseVars;

	void CreateTrueFalse();
	void InitializeVariables();
	void InitializeVVariables(VariableFamily& v, uint64_t input);
	bool ShareChannel(uint8_t i0, uint8_t j0, uint8_t i1, uint8_t j1) const;
	void AddValid();
	void AddUsedDefinitions();
	void AddUpDownDefinitions();
	void AddOneDownDefinition(uint8_t k, uint8_t i, uint8_t j);
	void AddOneUpDefinition(uint8_t k, uint8_t i, uint8_t j);

	void AddPhi1();
	void AddPhi2();
	void AddPhi3();
	void AddPhi4();
	void AddPsi1();
	void AddPsi2a();
	void AddPsi2b();
	void AddPsi2c();
	void AddPsi3a();
	void AddPsi3b();
	void GreedyCompPlacement();
	void NoRedundantComps();

	Literal GetClauseVar(const Clause& clause);
	uint64_t LeadingZeros(uint64_t input) const;
	uint64_t TailingOnes(uint64_t input) const;
};