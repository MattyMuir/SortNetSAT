#pragma once
#include <string>
#include <fstream>

#include "clause.h"

class Expression
{
protected:
    class Serializer
    {
    public:
        Serializer(const std::string& filepath);

        void Serialize(const Expression& expr);

    protected:
        std::ofstream file;

        static constexpr size_t BufSize = 512;
        char buf[BufSize];
        size_t writeIdx = 0;

        void WriteInt(int64_t x);
        void WriteChar(char c);
        void WriteClause(const Clause& clause);
        void FlushBuffer();
    };

public:
    Literal NextVar();
    void AddClause(const Clause& clause);
    void AddEquals(Literal v, const Clause& clause);
    void AddComment(const std::string& comment);

    size_t NumVars() const;
    size_t NumClauses() const;
    const std::vector<Clause>& GetClauses() const;

    void SaveToFile(const std::string& filepath) const;
    void SanityCheck() const;

protected:
    Literal nextVar = 1;
    std::vector<Clause> clauses;
    std::vector<std::string> comments;
};