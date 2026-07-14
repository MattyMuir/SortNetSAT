#include <print>
#include <fstream>
#include <algorithm>
#include <set>
#include <numeric>

#include "Timer.h"

#include "Prefixes/prefixes.h"
#include "SimpleExtender.h"
#include "IncrementalExtender.h"
#include "Prefixes/PrefixGenerator.h"
#include "OldPrefixes/PrefixGeneratorV2.h"
#include "OldPrefixes/PrefixGeneratorV3.h"
#include "OldPrefixes/LayerDAG.h"
#include "Prefixes/WindowMinimizer.h"
#include "OldPrefixes/NetworkGraph.h"
#include "Prefixes/SubsumptionSolver.h"
#include "Prefixes/PrefixGeneratorV4.h"

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
		double inputFraction = f / 100.0;
		std::println("Starting {}", inputFraction);

		double timeSum = 0.0;
		for (size_t i = 0; i < Repeats; i++)
		{
			SimpleExtender extender{ n, d, symmetric, prefix };
			Timer t;
			extender.Extend();
			t.Stop();
			timeSum += log(t.GetSeconds());
		}
		times.push_back(timeSum / Repeats);
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
			//{1,2},{3,24},{4,6},{5,22},{7,20},{8,9},{10,12},{11,13},{14,16},{15,17},{18,19},{21,23},{25,26},
			//{0,8},{1,4},{2,6},{3,9},{5,7},{10,11},{12,13},{14,15},{16,17},{18,24},{19,27},{20,22},{21,25},{23,26}
		} };
	// ==================

	IncrementalExtender extender{ n, d, symmetric, prefix };
	bool extendable = extender.Extend();
	if (!extendable) std::println("Unextendable!");
	else std::println("{:t}", extender.GetNetwork());
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

	SimpleExtender extender{ n, d, symmetric, prefix };
	bool extendable = extender.Extend();
	if (!extendable) std::println("Unextendable!");
	else std::println("{:l}", extender.GetNetwork());
}

void GenerateCactusPlot()
{
	// === Parameters ===
	uint8_t n = 17;
	uint8_t d = 8;
	bool symmetric = false;
	// ==================

	std::ifstream file{ "C:/sdks/JCSS/prefixes/opt/pref_opt17.txt" };

	std::vector<double> times;
	for (size_t i = 0; i < 50; i++)
	{
		std::string prefixStr;
		std::getline(file, prefixStr);
		Network prefix = ParseNetwork(prefixStr);

		IncrementalExtender extender{ n, d, symmetric, prefix };
		extender.SetParameters(6);
		Timer t;
		extender.Extend();
		t.Stop();
		times.push_back(t.GetSeconds());
	}

	std::sort(times.begin(), times.end());
	std::println();
	for (double t : times) std::println("{:.3f}", t);
}

void EquivTest();

int main()
{
	EquivTest();
}