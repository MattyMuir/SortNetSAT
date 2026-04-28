#include "Assignment.h"

#include <fstream>
#include <ranges>

std::vector<std::string> Split(std::string_view str, std::string_view delim)
{
	std::vector<std::string> parts;
	for (const auto& part : std::views::split(str, delim))
		parts.emplace_back(part.begin(), part.end());
	return parts;
}

Assignment::Assignment(const std::string& kissatOutput)
{
	std::ifstream file{ kissatOutput };

	std::string line;
	while (std::getline(file, line))
	{
		if (!line.starts_with('v')) continue;
		line = line.substr(2);

		auto parts = Split(line, " ");
		for (const std::string& varStr : parts)
			RegisterVar(varStr);
	}
}

bool Assignment::GetValue(Var var) const
{
	return vars[var];
}

void Assignment::RegisterVar(const std::string& varStr)
{
	int64_t lit = std::stoll(varStr);
	int64_t var = std::abs(lit);
	bool val = lit > 0;
	if (std::ssize(vars) <= var) vars.resize(var + 1);
	vars[var] = val;
}