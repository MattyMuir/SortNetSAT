#include <format>
#include <bit>
#include <print>
#include <algorithm>

#include "Expression.h"
#include "Assignment.h"
#include "Network.h"
#include "prefixes.h"
#include "WindowMinimizer.h"
#include "Timer.h"
#include "Expression2.h"
#include "FormulaGenerator.h"

#define COMP_VAR(k, i, j)		expr.GetVar(std::format("g^{}_{},{}", k, i, j))
#define USED_VAR(k, i)			expr.GetVar(std::format("used^{}_{}", k, i))
#define V_VAR(input, k, i)		expr.GetVar(std::format("v{}^{}_{}", input, k, i))
#define ONEDOWN_VAR(k, i, j)	expr.GetVar(std::format("oneDown^{}_{},{}", k, i, j))
#define ONEUP_VAR(k, i, j)		expr.GetVar(std::format("oneUp^{}_{},{}", k, i, j))

std::vector<Clause> Once(uint8_t n, const Expression& expr, uint8_t k, uint8_t i)
{
	std::vector<Clause> clauses;

	for (uint8_t j = 1; j <= n; j++)
	{
		if (j == i) continue;
		for (uint8_t l = 1; l <= n; l++)
		{
			if (l == i) continue;
			if (j == l) continue;

			Var first	= COMP_VAR(k, std::min(i, j), std::max(i, j));
			Var second	= COMP_VAR(k, std::min(i, l), std::max(i, l));
			clauses.push_back({ -first, -second });
		}
	}

	return clauses;
}

Clause Used(uint8_t n, const Expression& expr, uint8_t k, uint8_t i)
{
	Clause ret;
	for (uint8_t j = 1; j < i; j++)			ret.push_back(COMP_VAR(k, j, i));
	for (uint8_t j = i + 1; j <= n; j++)	ret.push_back(COMP_VAR(k, i, j));
	return ret;
}

void AddUpdate(uint8_t n, Expression& expr, uint8_t k, uint8_t i, const std::vector<Var>& v, Var w)
{
	expr.AddAImpliesBEqualC(-USED_VAR(k, i), w, v[i - 1]);

	for (uint8_t j = 1; j < i; j++)
		expr.AddAImpliesBEqualCOrD(COMP_VAR(k, j, i), w, v[j - 1], v[i - 1]);

	for (uint8_t j = i + 1; j <= n; j++)
		expr.AddAImpliesBEqualCAndD(COMP_VAR(k, i, j), w, v[j - 1], v[i - 1]);
}

void AddInput(uint8_t n, uint8_t d, Expression& expr, uint64_t input)
{
	// Create 'v' variables
	for (uint8_t k = 0; k <= d; k++)
		for (uint8_t i = 1; i <= n; i++)
			expr.CreateVar(std::format("v{}^{}_{}", input, k, i));

	// Hard-wire leading zeros and trailing ones into internal layer outputs
	uint64_t leadingZeros = std::min<uint64_t>(n, std::countr_zero(input));
	uint64_t tailingOnes = std::countl_one(input << (64 - n));
	for (uint8_t k = 1; k < d; k++)
	{
		for (uint8_t i = 1; i <= leadingZeros; i++)
			expr.AddClause({ -V_VAR(input, k, i) });
		for (uint8_t i = n - tailingOnes + 1; i <= n; i++)
			expr.AddClause({ V_VAR(input, k, i) });
	}

	// Ensure that the zero-th layer values equal the inputs
	for (uint8_t i = 1; i <= n; i++)
	{
		Var v0i = V_VAR(input, 0, i);
		Literal lit = (input & (1ULL << (i - 1))) ? v0i : -v0i;
		expr.AddClause({ lit });
	}

	// Ensure that intermediate layer results are updated according to the comparators
	for (uint8_t k = 1; k <= d; k++)
	{
		// Get the variables from the previous layer
		std::vector<Var> prevV;
		for (uint8_t i = 1; i <= n; i++) prevV.push_back(V_VAR(input, k - 1, i));

		for (uint8_t i = 1; i <= n; i++)
			AddUpdate(n, expr, k, i, prevV, V_VAR(input, k, i));
	}

	// Ensure that the output layer v^d is sorted
	uint64_t numZeros = n - std::popcount(input);
	for (uint8_t i = 1; i <= n; i++)
	{
		Var vdi = V_VAR(input, d, i);
		Literal lit = (i > numZeros) ? vdi : -vdi;
		expr.AddClause({ lit });
	}
}

Clause OneDown(const Expression& expr, uint8_t k, uint8_t i, uint8_t j)
{
	Clause oneDown;
	for (uint8_t l = i + 1; l <= j; l++) oneDown.push_back(COMP_VAR(k, i, l));
	return oneDown;
}

Clause OneUp(const Expression& expr, uint8_t k, uint8_t i, uint8_t j)
{
	Clause oneUp;
	for (uint8_t l = i; l < j; l++) oneUp.push_back(COMP_VAR(k, l, j));
	return oneUp;
}

void CreateOneUpDown(uint8_t n, uint8_t d, Expression& expr)
{
	for (uint8_t k = 1; k <= d; k++)
	{
		for (uint8_t i = 1; i <= n; i++)
		{
			for (uint8_t j = i; j <= n; j++)
			{
				// Note: when i == j, oneDown and oneUp should always be false.
				// In this case, the functions 'OneDown' and 'OneUp' return the empty clause
				// 'AddEquals' simply adds the clauses (-oneDown) and (-oneUp) as intended

				Var oneDown		= expr.CreateVar(std::format("oneDown^{}_{},{}", k, i, j));
				Var oneUp		= expr.CreateVar(std::format("oneUp^{}_{},{}", k, i, j));

				expr.AddEquals(oneDown, OneDown(expr, k, i, j));
				expr.AddEquals(oneUp, OneUp(expr, k, i, j));
			}
		}
	}
}

void AddInputImproved(uint8_t n, uint8_t d, Expression& expr, uint64_t input)
{
	// Create 'v' variables
	for (uint8_t k = 0; k <= d; k++)
		for (uint8_t i = 1; i <= n; i++)
			expr.CreateVar(std::format("v{}^{}_{}", input, k, i));

	// Hard-wire leading zeros and trailing ones into internal layer outputs
	uint64_t leadingZeros = std::min<uint64_t>(n, std::countr_zero(input));
	uint64_t tailingOnes = std::countl_one(input << (64 - n));
	for (uint8_t k = 1; k < d; k++)
	{
		for (uint8_t i = 1; i <= leadingZeros; i++)
			expr.AddClause({ -V_VAR(input, k, i) });
		for (uint8_t i = n - tailingOnes + 1; i <= n; i++)
			expr.AddClause({ V_VAR(input, k, i) });
	}

	// Ensure that the zero-th layer values equal the inputs
	for (uint8_t i = 1; i <= n; i++)
	{
		Var v0i = V_VAR(input, 0, i);
		Literal lit = (input & (1ULL << (i - 1))) ? v0i : -v0i;
		expr.AddClause({ lit });
	}

	// Ensure that intermediate layer results are updated according to the comparators
	for (uint8_t k = 1; k <= d; k++)
	{
		for (uint8_t i = leadingZeros + 1; i <= (n - tailingOnes); i++)
		{
			// Let us assume that the value on this channel is "1". 
			// Thus, this will stay unless there is a comparator down inside the window,
			// and the other input is "0". 
			expr.AddClause({ ONEDOWN_VAR(k, i, n - tailingOnes), -V_VAR(input, k - 1, i), V_VAR(input, k, i) });

			// Not let us assume there is a comparator to a higher index. 
			// Then, this "1" will disappear if the other input is a "0"
			for (uint8_t j = i + 1; j <= n - tailingOnes; j++)
			{
				// If the other input is "0"...
				expr.AddClause({ -COMP_VAR(k, i, j), V_VAR(input, k - 1, j), -V_VAR(input, k, i) });

				// If both inputs equal "1"
				expr.AddClause({ -COMP_VAR(k, i, j), -V_VAR(input, k - 1, j), -V_VAR(input, k - 1, i), V_VAR(input, k, i) });
			}
			// What if the input is "0"? 
			// No comparator to lower indices -> output will be zero
			expr.AddClause({ ONEUP_VAR(k, leadingZeros + 1, i), V_VAR(input, k - 1, i), -V_VAR(input, k, i) });

			for (uint8_t j = leadingZeros + 1; j < i; j++)
			{
				// Comparator chosen, and other value="1" -> swap
				expr.AddClause({ -COMP_VAR(k, j, i), -V_VAR(input, k - 1, j), V_VAR(input, k, i) });

				// Comparator chosen, and other value="0" -> still "0"
				expr.AddClause({ -COMP_VAR(k, j, i), V_VAR(input, k - 1, j), V_VAR(input, k - 1, i), -V_VAR(input, k, i) });
			}
		}
	}

	// Ensure that the output layer v^d is sorted
	uint64_t numZeros = n - std::popcount(input);
	for (uint8_t i = 1; i <= n; i++)
	{
		Var vdi = V_VAR(input, d, i);
		Literal lit = (i > numZeros) ? vdi : -vdi;
		expr.AddClause({ lit });
	}
}

void Phi1(uint8_t n, uint8_t d, Expression& expr, uint8_t l)
{
	for (uint8_t i = 1; i <= n - 2; i++)
	{
		for (uint8_t j = i + 2; j <= n; j++)
		{
			Clause clause{ -COMP_VAR(l, i, j) };
			for (uint8_t k = l + 1; k <= d; k++)
			{
				clause.push_back(USED_VAR(k, i));
				clause.push_back(USED_VAR(k, j));
			}
			expr.AddClause(clause);
		}
	}
}

void Phi2(uint8_t n, uint8_t d, Expression& expr)
{
	for (uint8_t i = 1; i <= n - 4; i++)
		for (uint8_t j = i + 4; j <= n; j++)
			expr.AddClause({ -COMP_VAR(d - 1, i, j) });
}

void Phi3(uint8_t n, uint8_t d, Expression& expr)
{
	for (uint8_t i = 1; i <= n - 3; i++)
	{
		expr.AddClause({
			-COMP_VAR(d - 1, i, i + 3),
			COMP_VAR(d, i, i + 1)
			});

		expr.AddClause({
			-COMP_VAR(d - 1, i, i + 3),
			COMP_VAR(d, i + 2, i + 3)
			});
	}
}

void Phi4(uint8_t n, uint8_t d, Expression& expr)
{
	for (uint8_t i = 1; i <= n - 2; i++)
	{
		expr.AddClause({
			-COMP_VAR(d - 1, i, i + 2),
			COMP_VAR(d, i, i + 1),
			COMP_VAR(d, i + 1, i + 2),
			});
	}
}

void Psi1(uint8_t n, uint8_t d, Expression& expr)
{
	for (uint8_t i = 1; i <= n - 1; i++)
	{
		expr.AddClause({
			USED_VAR(d, i),
			USED_VAR(d, i + 1),
			});
	}
}

void Psi2a(uint8_t n, uint8_t d, Expression& expr)
{
	for (uint8_t i = 1; i <= n - 3; i++)
	{
		expr.AddClause({
			-COMP_VAR(d, i, i + 1),
			-COMP_VAR(d, i + 2, i + 3),
			USED_VAR(d - 1, i),
			USED_VAR(d - 1, i + 2)
			});

		expr.AddClause({
			-COMP_VAR(d, i, i + 1),
			-COMP_VAR(d, i + 2, i + 3),
			USED_VAR(d - 1, i),
			USED_VAR(d - 1, i + 3)
			});

		expr.AddClause({
			-COMP_VAR(d, i, i + 1),
			-COMP_VAR(d, i + 2, i + 3),
			USED_VAR(d - 1, i + 1),
			USED_VAR(d - 1, i + 2)
			});

		expr.AddClause({
			-COMP_VAR(d, i, i + 1),
			-COMP_VAR(d, i + 2, i + 3),
			USED_VAR(d - 1, i + 1),
			USED_VAR(d - 1, i + 3)
			});
	}
}

void Psi2b(uint8_t n, uint8_t d, Expression& expr)
{
	for (uint8_t i = 1; i <= n - 2; i++)
	{
		expr.AddClause({
			-COMP_VAR(d, i, i + 1),
			USED_VAR(d, i + 2),
			USED_VAR(d - 1, i),
			USED_VAR(d - 1, i + 2)
			});

		expr.AddClause({
			-COMP_VAR(d, i, i + 1),
			USED_VAR(d, i + 2),
			USED_VAR(d - 1, i + 1),
			USED_VAR(d - 1, i + 2)
			});
	}
}

void Psi2c(uint8_t n, uint8_t d, Expression& expr)
{
	for (uint8_t i = 1; i <= n - 2; i++)
	{
		expr.AddClause({
			-COMP_VAR(d, i + 1, i + 2),
			USED_VAR(d, i),
			USED_VAR(d - 1, i),
			USED_VAR(d - 1, i + 1)
			});

		expr.AddClause({
			-COMP_VAR(d, i + 1, i + 2),
			USED_VAR(d, i),
			USED_VAR(d - 1, i),
			USED_VAR(d - 1, i + 2)
			});
	}
}

void Psi3a(uint8_t n, uint8_t d, Expression& expr)
{
	for (uint8_t i = 1; i <= n - 2; i++)
	{
		expr.AddClause({
			-COMP_VAR(d, i, i + 1),
			USED_VAR(d, i + 2),
			USED_VAR(d - 1, i),
			USED_VAR(d - 1, i + 1),
			});
	}
}

void Psi3b(uint8_t n, uint8_t d, Expression& expr)
{
	for (uint8_t i = 2; i <= n - 1; i++)
	{
		expr.AddClause({
			-COMP_VAR(d, i, i + 1),
			USED_VAR(d, i - 1),
			USED_VAR(d - 1, i),
			USED_VAR(d - 1, i + 1)
			});
	}
}

void ForceSymmetry(uint8_t n, uint8_t d, Expression& expr)
{
	for (uint8_t k = 1; k <= d; k++)
	{
		for (uint8_t i = 1; i <= n - 1; i++)
		{
			for (uint8_t j = i + 1; j <= n; j++)
			{
				uint8_t iSym = n + 1 - j;
				uint8_t jSym = n + 1 - i;

				if (iSym < i || (iSym == i && jSym < j))
					expr.AddEquals(COMP_VAR(k, i, j), COMP_VAR(k, iSym, jSym));
			}
		}
	}
}

Expression BuildNetworkExpr(uint8_t n, uint8_t d, const std::vector<uint64_t> inputs, bool improved, bool symmetric)
{
	Expression expr;

	// === Comparator variables ===
	for (uint8_t k = 1; k <= d; k++)
		for (uint8_t i = 1; i <= n - 1; i++)
			for (uint8_t j = i + 1; j <= n; j++)
				expr.CreateVar(std::format("g^{}_{},{}", k, i, j));

	// === 'valid' constraint ===
	for (uint8_t k = 1; k <= d; k++)
		for (uint8_t i = 1; i <= n; i++)
			expr.AddClauses(Once(n, expr, k, i));

	if (improved)
	{
		// === 'oneDown' and 'oneUp' variables ===
		CreateOneUpDown(n, d, expr);
	}

	// === 'used' variables ===
	for (uint8_t k = 1; k <= d; k++)
	{
		for (uint8_t i = 1; i <= n; i++)
		{
			Var used = expr.CreateVar(std::format("used^{}_{}", k, i));
			expr.AddEquals(used, Used(n, expr, k, i));
		}
	}
	
	// === 'sorts' constraints ===
	for (uint64_t input : inputs)
		if (improved)
			AddInputImproved(n, d, expr, input);
		else
			AddInput(n, d, expr, input);

	if (symmetric) ForceSymmetry(n, d, expr);

	Phi1(n, d, expr, d);
	Phi2(n, d, expr);
	Phi3(n, d, expr);
	Phi4(n, d, expr);
	Psi1(n, d, expr);
	Psi2a(n, d, expr);
	Psi2b(n, d, expr);
	Psi2c(n, d, expr);
	Psi3a(n, d, expr);
	Psi3b(n, d, expr);

	return expr;
}

void ParseAssignment(const std::string& filepath, uint8_t n, uint8_t d, Expression& expr)
{
	Assignment assignment{ filepath };

	for (uint8_t k = 1; k <= d; k++)
	{
		for (uint8_t i = 1; i <= n - 1; i++)
			for (uint8_t j = i + 1; j <= n; j++)
				if (assignment.GetValue(COMP_VAR(k, i, j)))
					std::print("({},{}),", i - 1, j - 1);

		std::println();
	}
}

void GenerateFormula()
{
	// === Parameters ===
	uint8_t n = 28;
	uint8_t d = 13;
	bool symmetric = true;

	Network prefix = { {
			{0,27},{1,26},{2,25},{3,24},{4,23},{5,22},{6,21},{7,20},{8,9},{10,11},{12,15},{13,14},{16,17},{18,19},
			{0,1},{2,3},{4,5},{6,7},{8,10},{9,11},{12,14},{13,15},{16,18},{17,19},{20,21},{22,23},{24,25},{26,27},
			{0,2},{1,3},{4,6},{5,7},{8,19},{9,12},{10,14},{11,16},{13,17},{15,18},{20,22},{21,23},{24,26},{25,27},
			{0,4},{1,5},{2,20},{3,21},{6,24},{7,25},{8,13},{9,11},{10,17},{12,15},{14,19},{16,18},{22,26},{23,27},
			{1,2},{3,24},{4,6},{5,22},{7,20},{8,9},{10,12},{11,13},{14,16},{15,17},{18,19},{21,23},{25,26},
			{0,8},{1,4},{2,6},{3,9},{5,7},{10,11},{12,13},{14,15},{16,17},{18,24},{19,27},{20,22},{21,25},{23,26}
		} };
	uint8_t prefixDepth = 6;
	// ==================

	// Optimize prefix
	WindowMinimizer minimizer{ n, symmetric };
	prefix = minimizer.Optimize(prefix, 300, 300);

	auto prefixOutputs = PrefixOutputs(n, prefix, true, symmetric);
	std::println("Window width: {}", WindowWidth(n, prefixOutputs, symmetric));
	PrintNetwork(prefix);

	// Build CNF formula
	Expression expr = BuildNetworkExpr(n, d - prefixDepth, prefixOutputs, true, symmetric);
	expr.SanityCheck();
	expr.SaveToFile(std::format("wang.cnf", n, d));

	ParseAssignment("assignment.txt", n, d - prefixDepth, expr);
}

int main()
{
	// === Parameters ===
	uint8_t n = 16;
	uint8_t d = 9;
	bool symmetric = true;

	Network prefix = { {
			{0,5},{1,4},{2,12},{3,13},{6,7},{8,9},{10,15},{11,14},
			{0,2},{1,10},{3,6},{4,7},{5,14},{8,11},{9,12},{13,15},
			{0,8},{1,3},{2,11},{4,13},{5,9},{6,10},{7,15},{12,14},
			{0,1},{2,4},{3,8},{5,6},{7,12},{9,10},{11,13},{14,15}
		} };
	uint8_t prefixDepth = 4;
	// ==================

	// Optimize prefix
	//WindowMinimizer minimizer{ n, symmetric };
	//prefix = minimizer.Optimize(prefix, 300, 300);

	auto prefixOutputs = PrefixOutputs(n, prefix, true, symmetric);
	std::println("Window width: {}", WindowWidth(n, prefixOutputs, symmetric));
	PrintNetwork(prefix);

	// Build CNF formula
	FormulaGenerator generator{ n, (uint8_t)(d - prefixDepth), symmetric };
	Expression2 expr = generator.Generate(prefixOutputs);
	expr.SanityCheck();
	expr.SaveToFile("wang.cnf");
}