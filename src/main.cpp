#include <print>
#include <optional>
#include <set>
#include <array>
#include <algorithm>
#include <ranges>
#include <random>
#include <fstream>

#include <sortnetutils.h>
#include <kissatlib.h>
#include <minisat.h>

#include "prefixes.h"
#include "WindowMinimizer.h"
#include "FormulaGenerator.h"
#include "Timer.h"

double outputFraction;
double runtime;

std::optional<Network> ExtendPrefixKissat(uint8_t n, uint8_t d, const Network& prefix, bool symmetric)
{
	// Optimize prefix
	WindowMinimizer minimizer{ n, symmetric, 1234 };
	Network prefixOpt = minimizer.Optimize(prefix, 128, 256);
	auto prefixOutputs = GetOutputs(prefixOpt, n, true, symmetric);

	size_t numOutputs = prefixOutputs.size();
	prefixOutputs.resize(numOutputs * outputFraction);

	// Build CNF formula
	FormulaGenerator generator{ n, (uint8_t)(d - ComputeDepth(prefix)), symmetric };
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
	Network network = Concatenate(prefixOpt, postfix);
	Untangle(network, n);
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

std::optional<Network> DoExtendPrefixMinisat(uint8_t n, uint8_t d, const std::vector<uint64_t>& prefixOutputs, bool symmetric)
{
	// Build CNF formula
	FormulaGenerator generator{ n, d, symmetric };
	Expression expr = generator.Generate(prefixOutputs);
	expr.SanityCheck();

	// Load expression into solver
	Minisat::Solver solver;
	solver.verbosity = 1;
	LoadExpressionMinisat(solver, expr);

	// Simplify
	Timer t;
	if (!solver.simplify()) return std::nullopt;
	solver.failedLiteralCheck();

	// Solve
	Minisat::vec<Minisat::Lit> dummy;
	Minisat::lbool ret = solver.solveLimited(dummy);
	t.Stop();
	runtime = t.GetSeconds();
	if (ret != Minisat::l_True) return std::nullopt;

	// Convert witness to postfix
	std::vector<bool> assignment(expr.NumVars() + 1);
	for (int i = 0; i < solver.nVars(); i++)
		assignment[i + 1] = (solver.model[i] == Minisat::l_True);
	Network postfix = generator.ParseAssignment(assignment);
	return postfix;
}

std::optional<Network> ExtendPrefixMinisat(uint8_t n, uint8_t d, const Network& prefix, bool symmetric)
{
	// Optimize prefix
	WindowMinimizer minimizer{ n, symmetric, 1234 };
	Network prefixOpt = minimizer.Optimize(prefix, 128, 256);
	auto prefixOutputs = GetOutputs(prefixOpt, n, true, symmetric);

	// Take a random fraction of the outputs
	static std::mt19937_64 gen{ std::random_device{}() };
	std::shuffle(prefixOutputs.begin(), prefixOutputs.end(), gen);
	size_t numOutputs = prefixOutputs.size();
	prefixOutputs.resize(numOutputs * outputFraction);

	auto postfixOpt = DoExtendPrefixMinisat(n, d - ComputeDepth(prefix), prefixOutputs, symmetric);
	if (!postfixOpt.has_value()) return std::nullopt;

	// Append postfix to prefix and untangle
	Network network = Concatenate(prefixOpt, postfixOpt.value());
	Untangle(network, n);
	return network;
}

void FractionBenchmark()
{
	// === Parameters ===
	uint8_t n = 16;
	uint8_t d = 9;
	bool symmetric = true;
	Network prefix = { {
			{0,5},{1,4},{2,12},{3,13},{6,7},{8,9},{10,15},{11,14},
			{0,2},{1,10},{3,6},{4,7},{5,14},{8,11},{9,12},{13,15}
		} };
	// ==================

	static constexpr size_t Repeats = 15;

	std::vector<double> times;
	for (size_t f = 1; f <= 20; f += 1)
	{
		outputFraction = f / 100.0;
		std::println("Starting {}", outputFraction);

		double time = 0.0;
		for (size_t i = 0; i < Repeats; i++)
		{
			auto networkOpt = ExtendPrefixMinisat(n, d, prefix, symmetric);
			time += log(runtime);
		}
		times.push_back(time / Repeats);
	}

	for (double t : times)
		std::println("{}", t);
}

void Wang()
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
	// ==================

	outputFraction = 1.0;
	auto networkOpt = ExtendPrefixMinisat(n, d, prefix, symmetric);
	if (!networkOpt.has_value()) std::println("Unextendable!");
	else std::println("{}", networkOpt.value());
}

void Net18Full()
{
	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;
	Network prefix = { {
			{0,6},{1,10},{2,15},{3,5},{4,9},{7,16},{8,13},{11,17},{12,14},
			{0,12},{1,4},{3,11},{5,17},{6,14},{7,8},{9,10},{13,16}
		} };
	// ==================

	outputFraction = 1.0;
	auto networkOpt = ExtendPrefixMinisat(n, d, prefix, symmetric);
	if (!networkOpt.has_value()) std::println("Unextendable!");
	else std::println("{}", networkOpt.value());
}

void Test()
{
	// === Parameters ===
	uint8_t n = 16;
	uint8_t d = 9;
	bool symmetric = true;
	Network prefix = { {
			{0,5},{1,4},{2,12},{3,13},{6,7},{8,9},{10,15},{11,14},
			{0,2},{1,10},{3,6},{4,7},{5,14},{8,11},{9,12},{13,15}
		} };
	// ==================

	outputFraction = 1.0;
	auto networkOpt = ExtendPrefixMinisat(n, d, prefix, symmetric);
	if (!networkOpt.has_value()) std::println("Unextendable!");
	else std::println("{}", networkOpt.value());
}

void IncrementalSolve()
{
	std::ofstream log{ "log.log" };

	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;
	Network prefix = { {
			{0,6},{1,10},{2,15},{3,5},{4,9},{7,16},{8,13},{11,17},{12,14},
			{0,12},{1,4},{3,11},{5,17},{6,14},{7,8},{9,10},{13,16}
		} };

	size_t numInitial = 4;
	size_t maxAddNum = 4;
	// ==================

	// Optimize prefix
	WindowMinimizer minimizer{ n, symmetric, 1234 };
	Network prefixOpt = minimizer.Optimize(prefix, 128, 256);
	auto prefixOutputs = GetOutputs(prefixOpt, n, true, symmetric);
	size_t numOutputs = prefixOutputs.size();
	std::println("Total Outputs: {}", numOutputs);

	// Sort outputs by window width
	SortByWindowWidth(n, prefixOutputs);

	// Initialize test inputs
	std::vector<uint64_t> testInputs{ prefixOutputs.begin(), prefixOutputs.begin() + numInitial};
	prefixOutputs.erase(prefixOutputs.begin(), prefixOutputs.begin() + numInitial);

	for (;;)
	{
		auto postfixOpt = DoExtendPrefixMinisat(n, d - ComputeDepth(prefix), testInputs, symmetric);
		if (!postfixOpt.has_value())
		{
			std::println("\nUNSAT");
			log << "UNSAT\n";
			break;
		}

		// Collect all failing inputs
		std::vector<size_t> failing;
		for (size_t i = 0; i < prefixOutputs.size(); i++)
		{
			uint64_t input = prefixOutputs[i];
			uint64_t output = RunNetwork(postfixOpt.value(), input);
			if (!IsSorted(n, output))
				failing.push_back(i);
		}

		double passing = 1.0 - (double)failing.size() / numOutputs;
		std::print("{} inputs  {:.3f}% passing\r", testInputs.size(), passing * 100.0);
		log << std::format("{} inputs  {:.3f}% passing\n", testInputs.size(), passing * 100.0);
		log << std::flush;

		if (failing.empty())
		{
			std::println("\nAll inputs pass!");
			log << std::format("All inputs pass!\n");

			// Reconstruct network
			Network network = Concatenate(prefixOpt, postfixOpt.value());
			Untangle(network, n);
			std::println("{:l}", network);
			log << std::format("{:l}", network);
			break;
		}

		// Add a subset of failing inputs
		size_t numToAdd = std::min<size_t>(failing.size(), maxAddNum);
		failing.resize(numToAdd);

		for (size_t i : failing)
			testInputs.push_back(prefixOutputs[i]);

		for (size_t i : failing | std::views::reverse)
			prefixOutputs.erase(prefixOutputs.begin() + i);
	}
	std::println();
}

void CompConstraintsTest()
{
	// === Parameters ===
	uint8_t n = 18;
	uint8_t d = 10;
	bool symmetric = true;
	Network prefix = { {
			{0,6},{1,10},{2,15},{3,5},{4,9},{7,16},{8,13},{11,17},{12,14},
			{0,12},{1,4},{3,11},{5,17},{6,14},{7,8},{9,10},{13,16}
		} };

	size_t numOutputs = 1200;
	// ==================

	d -= ComputeDepth(prefix);

	// Optimize prefix
	WindowMinimizer minimizer{ n, symmetric, 1234 };
	Network prefixOpt = minimizer.Optimize(prefix, 128, 256);

	// Get prefix outputs
	auto prefixOutputs = GetOutputs(prefixOpt, n, true, symmetric);
	std::println("Total Outputs: {}", prefixOutputs.size());
	if (numOutputs < prefixOutputs.size())
		prefixOutputs.resize(numOutputs);

	for (uint8_t k = 0; k < d; k++)
	{
		for (uint8_t i = 0; i + 1 < n; i++)
		{
			for (uint8_t j = i + 1; j < n; j++)
			{
				if (symmetric && i > n - 1 - j) continue;

				// Build CNF formula
				FormulaGenerator generator{ n, d, symmetric};
				Expression expr = generator.Generate(prefixOutputs, { { { k, i, j } } });

				// Load expression into solver
				Minisat::Solver solver;
				solver.verbosity = 0;
				LoadExpressionMinisat(solver, expr);

				bool isSat = solver.simplify();
				if (isSat)
				{
					solver.failedLiteralCheck();

					// Solve
					Minisat::vec<Minisat::Lit> dummy;
					Minisat::lbool ret = solver.solveLimited(dummy);
					isSat = (ret == Minisat::l_True);
				}

				std::println("({:<2},{:<2},{:<2}): {}", k, i, j, isSat ? "SAT" : "UNSAT");
			}
		}
	}
}

int main()
{
	Net18Full();
}