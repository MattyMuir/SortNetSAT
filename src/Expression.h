#pragma once
#include <string>
#include <array>
#include <unordered_map>
#include <unordered_set>

#include "clause.h"

class Expression
{
public:
    void SaveToFile(const std::string& filename);
    Var CreateVar(const std::string& name);

    // Clause creation
    void AddClause(const Clause& clause);
    void AddClauses(const std::vector<Clause>& newClauses);
    void AddEquals(Literal a, Literal b);
    void AddEquals(Literal v, const Clause& clause); // v = (clause)
    void AddAImpliesBEqualCOrD(Literal a, Literal b, Literal c, Literal d); // a -> (b = (c or d))
    void AddAImpliesBEqualCAndD(Literal a, Literal b, Literal c, Literal d); // a -> (b = (c and d))
    void AddAImpliesBEqualC(Literal a, Literal b, Literal c); // a -> (b = c)

    Var GetVar(const std::string& name) const;
    std::string GetName(Var var) const;

    void PrettyPrint() const;
    void SanityCheck() const;

protected:
    Var nextVar = 1;
    std::unordered_map<std::string, Var> nameToVar;
    std::vector<std::string> varToName = { "INVALID" };

    std::vector<Clause> clauses;

    std::string LiteralToStr(Literal l) const;
    void PrettyPrint(const Clause& clause) const;
};