#pragma once
#include "minisatutil.h"
#include "Extender.h"
#include "FormulaGenerator.h"

class SimpleExtender : public Extender
{
public:
	enum InputOrder
	{
		OrderDefault,
		OrderWindowWidth,
		OrderRandom
	};

public:
	SimpleExtender(uint8_t n_, uint8_t d_, bool symmetric_, const Network& prefix);

	void SetParameters(InputOrder inputOrder_, double inputFraction_);
	bool Extend() override;
	Network GetNetwork() const override;

protected:
	InputOrder inputOrder = OrderDefault;
	double inputFraction = 1.0;

	FormulaGenerator generator;
	Minisat::Solver solver;

	void OrderInputs();
};