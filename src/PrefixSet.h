#pragma once
#include <string>
#include <atomic>

#include <sortnetutils.h>

class PrefixSet
{
public:
	enum Status
	{
		Unknown = 0,
		Unextendable = 1,
		Extendable = 2
	};

public:
	PrefixSet(const std::string& filepath);

	void SetStatus(size_t prefixIdx, bool extendable);
	size_t Size() const;
	Status State(size_t idx) const;
	size_t NumRemaining() const;

	const Network& operator[](size_t idx) const;

	void SaveStatus(const std::string& filepath) const;

protected:
	std::vector<Network> prefixes;
	std::vector<std::atomic<Status>> status;
	std::atomic<size_t> numRemaining;
};