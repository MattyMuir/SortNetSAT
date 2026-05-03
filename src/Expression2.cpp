#include "Expression2.h"

#include <print>
#include <charconv>
#include <format>
#include <unordered_set>
#include <algorithm>

Var Expression2::NextVar()
{
	return nextVar++;
}

void Expression2::AddClause(const Clause& clause)
{
#if 0
	Clause sorted{ clause };
	std::sort(sorted.begin(), sorted.end());

	if (clauseSet.contains(sorted))
		bool jdfhj = true;
	else clauseSet.insert(sorted);

	std::erase_if(sorted, [](Literal l) { return std::abs(l) <= 2; });

	std::unordered_set<Var> usedVars;
	for (Literal l : sorted)
		usedVars.insert(std::abs(l));
	if (usedVars.size() != sorted.size())
		bool djhd = true;

	if (clause.size() == 1)
		bool dfjhj = true;
#endif

	clauses.push_back(clause);
}

void Expression2::AddEquals(Literal v, const Clause& clause)
{
	// Add v -> (clause[0] or clause[1] or ...)
	Clause ps{ clause };
	ps.push_back(-v);
	AddClause(ps);

	// Add (!v -> !clause[0]) and (!v -> !clause[1]) and ...
	for (Literal lit : clause)
		AddClause({ v, -lit });
}

void Expression2::SaveToFile(const std::string& filepath) const
{
	Serializer serializer{ filepath };
	serializer.Serialize(*this);
}

void Expression2::SanityCheck() const
{
	// Duplicate clauses
	size_t numDuplicate = 0;
	std::unordered_set<Clause, ClauseHasher> allClauses;
	for (const Clause& clause : clauses)
	{
		Clause sorted{ clause };
		std::sort(sorted.begin(), sorted.end());

		if (allClauses.contains(sorted)) numDuplicate++;
		else allClauses.insert(sorted);
	}

	// Unused variables
	std::vector<bool> isUsed(nextVar, false);
	for (const Clause& clause : clauses)
		for (Literal l : clause)
			isUsed[std::abs(l)] = true;
	size_t numUnused = 0;
	for (Var v = 1; v < nextVar; v++)
		if (!isUsed[v])
			numUnused++;

	// Empty clauses
	size_t numEmpty = 0;
	for (const Clause& clause : clauses)
		numEmpty += clause.empty();

	// Unit clauses
	size_t numUnit = 0;
	for (const Clause& clause : clauses)
		numUnit += (clause.size() == 1);

	// Clauses with duplicate literals
	size_t numDuplicateLiteral = 0;
	for (const Clause& clause : clauses)
	{
		std::unordered_set<Var> vars;
		for (Literal l : clause) vars.insert(std::abs(l));

		numDuplicateLiteral += (vars.size() != clause.size());
	}

	std::println("True clauses  : {}", allClauses.size());
	std::println("Num duplicate : {}", numDuplicate);
	std::println("Num unused    : {}", numUnused);
	std::println("Num empty     : {}", numEmpty);
	std::println("Num unit      : {}", numUnit);
	std::println("Num dup lit   : {}", numDuplicateLiteral);
}

Expression2::Serializer::Serializer(const std::string& filepath)
	: file(filepath) {}

void Expression2::Serializer::Serialize(const Expression2& expr)
{
	file << std::format("p cnf {} {}\n", expr.nextVar - 1, expr.clauses.size());

	for (const Clause& clause : expr.clauses)
		WriteClause(clause);

	if (writeIdx) FlushBuffer();
}

void Expression2::Serializer::WriteInt(int64_t x)
{
	if (writeIdx + 20 >= BufSize) FlushBuffer();
	auto [ptr, _] = std::to_chars(buf + writeIdx, buf + BufSize, x);
	writeIdx = ptr - buf;
}

void Expression2::Serializer::WriteChar(char c)
{
	if (writeIdx == BufSize) FlushBuffer();
	buf[writeIdx++] = c;
}

void Expression2::Serializer::WriteClause(const Clause& clause)
{
	for (size_t i = 0; i < clause.size(); i++)
	{
		WriteInt(clause[i]);
		WriteChar(' ');
	}
	WriteChar('0');
	WriteChar('\n');
}

void Expression2::Serializer::FlushBuffer()
{
	file.write(buf, writeIdx);
	writeIdx = 0;
}