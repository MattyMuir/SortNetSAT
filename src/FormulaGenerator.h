#pragma once
#include <cstdint>
#include <unordered_map>

#include <sortnetutils.h>

#include "Expression.h"

class VariableFamily
{
public:
	VariableFamily() = default;
	VariableFamily(size_t xWidth, size_t yWidth = 1, size_t zWidth = 1);

	Var& operator()(size_t x, size_t y = 0, size_t z = 0);
	const Var& operator()(size_t x, size_t y = 0, size_t z = 0) const;

protected:
	size_t yStride, zStride;
	std::vector<Var> vars;
};

class FormulaGenerator
{
public:
	FormulaGenerator(uint8_t n_, uint8_t d_, bool symmetric_);

	Expression Generate(const std::vector<uint64_t>& inputs_);
	Network ParseAssignment(const std::vector<bool>& assignment);
	std::vector<Var> GetSamplingVariables() const;

	void LogVariableInfo(Var var);

protected:
	uint8_t n, d;
	bool symmetric;
	std::vector<uint64_t> inputs;

	Expression expr;
	Var trueVar, falseVar;
	VariableFamily comps, used, oneDown, oneUp, v;
	std::unordered_map<Clause, Var, ClauseHasher> clauseVars;

	void CreateTrueFalse();
	void InitializeVariables();
	void InitializeVVariables(size_t inputIdx);
	bool ShareChannel(uint8_t i0, uint8_t j0, uint8_t i1, uint8_t j1) const;
	void AddValid();
	void AddUsedDefinitions();
	void AddUpDownDefinitions();
	void AddOneDownDefinition(uint8_t k, uint8_t i, uint8_t j);
	void AddOneUpDefinition(uint8_t k, uint8_t i, uint8_t j);
	void AddInput(size_t inputIdx);

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
	void EveryAdjacentComparator();

	void AddSamplingComment();

	Var GetClauseVar(const Clause& clause);
	uint64_t LeadingZeros(uint64_t input) const;
	uint64_t TailingOnes(uint64_t input) const;
};