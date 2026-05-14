#include <print>
#include <optional>
#include <set>

#include <kissatlib.h>
#include <minisat.h>

#include "prefixes.h"
#include "WindowMinimizer.h"
#include "FormulaGenerator.h"
#include "Timer.h"

double outputFraction;
double runtime;

std::optional<Network> ExtendPrefixKissat(uint8_t n, uint8_t d, const Network& prefix, uint8_t prefixDepth, bool symmetric)
{
	// Optimize prefix
	WindowMinimizer minimizer{ n, symmetric, 1234 };
	Network prefixOpt = minimizer.Optimize(prefix, 128, 256);
	auto prefixOutputs = PrefixOutputs(n, prefixOpt, true, symmetric);

	size_t numOutputs = prefixOutputs.size();
	prefixOutputs.resize(numOutputs * outputFraction);

	// Build CNF formula
	FormulaGenerator generator{ n, (uint8_t)(d - prefixDepth), symmetric };
	Expression expr = generator.Generate(prefixOutputs);
	expr.SaveToFile("issame.cnf");

	// Solve formula
	KissatSolver solver;
	solver.LoadExpression(expr.NumVars(), expr.GetClauses());

	Timer t;
	KissatSolver::Result res = solver.Solve();
	t.Stop();
	runtime = t.GetSeconds();

	if (res != KissatSolver::Satisfiable) return std::nullopt;

	// Convert witness to postfix
	std::vector<int> witness = solver.GetWitness();
	std::vector<bool> assignment(expr.NumVars() + 1);
	for (int literal : witness)
		assignment[std::abs(literal)] = literal > 0;
	Network postfix = generator.ParseAssignment(assignment);

	// Append postfix to prefix and untangle
	Network network = Append(prefixOpt, postfix);
	Untangle(n, network);
	return network;
}

void LoadExpressionMinisat(Minisat::Solver& solver, const Expression& expr)
{
	// Initialize variables
	for (int64_t i = 0; i < expr.NumVars(); i++)
		solver.newVar();

	const auto& clauses = expr.GetClauses();
	for (const Clause& clause : clauses)
	{
		// Convert clause to minisat clause
		Minisat::vec<Minisat::Lit> mClause;
		for (Literal l : clause)
		{
			int var = std::abs(l) - 1;
			mClause.push((l > 0) ? Minisat::mkLit(var) : ~Minisat::mkLit(var));
		}

		// Add clause to solver
		solver.addClause(mClause);
	}
}

std::optional<Network> ExtendPrefixMinisat(uint8_t n, uint8_t d, const Network& prefix, uint8_t prefixDepth, bool symmetric)
{
	// Optimize prefix
	WindowMinimizer minimizer{ n, symmetric, 1234 };
	Network prefixOpt = minimizer.Optimize(prefix, 128, 256);
	PrintNetwork(prefixOpt);
	auto prefixOutputs = PrefixOutputs(n, prefixOpt, true, symmetric);

	size_t numOutputs = prefixOutputs.size();
	prefixOutputs.resize(numOutputs * outputFraction);

	// Build CNF formula
	FormulaGenerator generator{ n, (uint8_t)(d - prefixDepth), symmetric };
	Expression expr = generator.Generate(prefixOutputs);
	//expr.SanityCheck();
	expr.SaveToFile("net18.cnf");

	// Load expression into solver
	Minisat::Solver solver;
	solver.verbosity = 1;
	LoadExpressionMinisat(solver, expr);

	// Simplify
	Timer t;
	if (!solver.simplify())
		return std::nullopt;

	// Failed literal check
#if 1
	solver.failedLiteralCheck();
#endif

	// Solve
	Minisat::vec<Minisat::Lit> dummy;
	Minisat::lbool ret = solver.solveLimited(dummy);
	t.Stop();
	runtime = t.GetSeconds();
	solver.printStats();
	if (ret != Minisat::l_True) return std::nullopt;

	// Convert witness to postfix
	std::vector<bool> assignment(expr.NumVars() + 1);
	for (int i = 0; i < solver.nVars(); i++)
		assignment[i + 1] = (solver.model[i] == Minisat::l_True);
	Network postfix = generator.ParseAssignment(assignment);

	// Append postfix to prefix and untangle
	Network network = Append(prefixOpt, postfix);
	Untangle(n, network);
	return network;
}

int main()
{
#if 0
	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;
	Network prefix = {{
			{0,6},{1,10},{2,15},{3,5},{4,9},{7,16},{8,13},{11,17},{12,14},
			{0,12},{1,4},{3,11},{5,17},{6,14},{7,8},{9,10},{13,16}
		}};
	uint8_t prefixDepth = 2;
	// ==================

	std::vector<double> times;
	for (size_t f = 2; f <= 82; f += 2)
	{
		outputFraction = f / 100.0;
		std::println("Starting {}", outputFraction);
		auto networkOpt = ExtendPrefixMinisat(n, d, prefix, prefixDepth, symmetric);
		times.push_back(runtime);
	}

	std::println("{}", times);
#endif
#if 0
	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;
	Network prefix = { {
			{0,6},{1,10},{2,15},{3,5},{4,9},{7,16},{8,13},{11,17},{12,14},
			{0,12},{1,4},{3,11},{5,17},{6,14},{7,8},{9,10},{13,16}
		} };
	uint8_t prefixDepth = 2;
	// ==================

	// Optimize prefix
	WindowMinimizer minimizer{ n, symmetric, 1234 };
	Network prefixOpt = minimizer.Optimize(prefix, 128, 256);
	auto prefixOutputs = PrefixOutputs(n, prefixOpt, true, symmetric);
	uint64_t totalWindowWidth = WindowWidth(n, prefixOutputs, symmetric);

	size_t savings = 0;
	size_t s2 = 0;
	for (uint64_t input : prefixOutputs)
	{
		uint8_t leadingZeros = std::min<uint64_t>(n, std::countr_zero(input));
		uint8_t tailingOnes = std::countl_one(input << (64 - n));

		if (n - leadingZeros - tailingOnes == 2) s2++;

		size_t numZeros = 0, numOnes = 0;
		for (uint8_t i = leadingZeros; i < (n - tailingOnes); i++)
			if (input & (1ULL << i))
				numOnes++;
			else
				numZeros++;

		if (numZeros == 1 || numOnes == 1) savings++;
	}

	std::println("Num outputs: {}", prefixOutputs.size());
	std::println("Savings: {}", savings);
	std::println("S2: {}", s2);
#endif

#if 1
	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;
	Network prefix = { {
			{0,6},{1,10},{2,15},{3,5},{4,9},{7,16},{8,13},{11,17},{12,14},
			{0,12},{1,4},{3,11},{5,17},{6,14},{7,8},{9,10},{13,16}
		} };
	uint8_t prefixDepth = 2;
	// ==================

	outputFraction = 1.0;
	auto networkOpt = ExtendPrefixMinisat(n, d, prefix, prefixDepth, symmetric);
	if (!networkOpt.has_value()) std::println("Unextendable!");
	else PrintNetwork(networkOpt.value());
#endif

#if 0
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

	outputFraction = 1.0;
	auto networkOpt = ExtendPrefixMinisat(n, d, prefix, prefixDepth, symmetric);
	if (!networkOpt.has_value()) std::println("Unextendable!");
	else PrintNetwork(networkOpt.value());
#endif
}