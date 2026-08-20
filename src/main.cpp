#include <iostream>
#include <print>
#include <ranges>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "Timer.h"
#include "Prefixes/prefixes.h"
#include "Prefixes/PrefixGenerator.h"
#include "IncrementalExtender.h"
#include "BulkChecker.h"
#include "Prefixes/WindowMinimizer.h"

int main()
{
	BulkChecker checker{ 18, 10, true, "C:\\Users\\matty\\source\\repos\\SortNetSAT\\prefixes\\16_3_sym.txt" };
	checker.CheckAll();
}