#pragma once
#include <string>
#include <fstream>

#include "clause.h"

class Expression2
{
protected:
    class Serializer
    {
    public:
        Serializer(const std::string& filepath);

        void Serialize(const Expression2& expr);

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
    Var NextVar();
    void AddClause(const Clause& clause);
    void AddEquals(Literal v, const Clause& clause);

    void SaveToFile(const std::string& filepath);
    void SanityCheck() const;

protected:
    Var nextVar = 1;
    std::vector<Clause> clauses;
};