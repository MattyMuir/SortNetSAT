#pragma once
#include <string>
#include <vector>

#include "Expression.h"

class Assignment
{
public:
	Assignment(const std::string& kissatOutput);

	bool GetValue(Var var) const;

protected:
	std::vector<uint8_t> vars;

	void RegisterVar(const std::string& varStr);
};