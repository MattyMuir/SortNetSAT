#include "Expression.h"

#include <fstream>
#include <format>
#include <print>

void Expression::SaveToFile(const std::string& filename)
{
	std::ofstream file{ filename };
	file << std::format("p cnf {} {}\n", nextVar - 1, clauses.size());

	for (const auto& clause : clauses)
	{
		for (Literal l : clause) file << l << ' ';
		file << "0\n";
	}
}

Var Expression::CreateVar(const std::string & name)
{
	nameToVar[name] = nextVar;
	varToName.emplace_back(name);
	return nextVar++;
}

void Expression::AddClause(const std::vector<Literal>& clause)
{
	clauses.emplace_back(clause);
}

void Expression::AddClauses(const std::vector<Clause>& newClauses)
{
	clauses.insert(clauses.end(), newClauses.begin(), newClauses.end());
}

void Expression::AddEquals(Literal a, Literal b)
{
	AddClause({ -a, b });
	AddClause({ a, -b });
}

void Expression::AddEquals(Literal v, const Clause& clause)
{
	// Add v -> (clause[0] or clause[1] or ...)
	Clause ps{ clause };
	ps.push_back(-v);
	AddClause(ps);

	// Add (!v -> !clause[0]) and (!v -> !clause[1]) and ...
	for (Literal lit : clause)
		AddClause({ v, -lit });
}

void Expression::AddAImpliesBEqualCOrD(Literal a, Literal b, Literal c, Literal d)
{
	AddClause({ -a, b, -c });
	AddClause({ -a, b, -d });
	AddClause({ -a, -b, c, d });
}

void Expression::AddAImpliesBEqualCAndD(Literal a, Literal b, Literal c, Literal d)
{
	AddClause({ -a, -b, c });
	AddClause({ -a, -b, d });
	AddClause({ -a, b, -c, -d });
}

void Expression::AddAImpliesBEqualC(Literal a, Literal b, Literal c)
{
	AddClause({ -a, -b, c });
	AddClause({ -a, b, -c });
}

Var Expression::GetVar(const std::string & name) const
{
	return nameToVar.at(name);
}

std::string Expression::GetName(Var var) const
{
	return varToName[var];
}

void Expression::PrettyPrint() const
{
	for (Var v = 1; v < nextVar; v++)
		std::println("{}", varToName[v]);
	std::println("=================");

	for (const Clause& clause : clauses)
		PrettyPrint(clause);
}

void Expression::SanityCheck() const
{
	std::println("=== Duplicate Clauses ===");
	std::unordered_set<Clause, ClauseHasher> allClauses;
	for (const Clause& clause : clauses)
	{
		if (allClauses.contains(clause)) PrettyPrint(clause);
		else allClauses.insert(clause);
	}

	std::println("=== Unused Variables  ===");
	std::vector<bool> isUsed(nextVar, false);
	for (const Clause& clause : clauses)
		for (Literal l : clause)
			isUsed[std::abs(l)] = true;
	for (Var v = 1; v < nextVar; v++)
		if (!isUsed[v])
			std::println("{}", LiteralToStr(v));

	std::println("=========================");
	size_t numEmpty = 0;
	for (const Clause& clause : clauses)
		numEmpty += clause.empty();
	std::println("Num empty: {}", numEmpty);

	size_t numUnit = 0;
	for (const Clause& clause : clauses)
		numUnit += (clause.size() == 1);
	std::println("Num unit: {}", numUnit);
}

std::string Expression::LiteralToStr(Literal l) const
{
	std::string name = varToName[std::abs(l)];
	if (l < 0) name = "!" + name;
	return name;
}

void Expression::PrettyPrint(const Clause& clause) const
{
	for (size_t li = 0; li < clause.size(); li++)
	{
		if (li) std::print(" or ");
		std::print("{}", LiteralToStr(clause[li]));
	}
	std::println();
}