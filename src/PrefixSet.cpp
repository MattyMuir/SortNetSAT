#include "PrefixSet.h"

#include <fstream>

PrefixSet::PrefixSet(const std::string& filepath)
{
	// Load prefixes from file
	std::ifstream file{ filepath };
	std::string line;
	while (std::getline(file, line))
		prefixes.emplace_back(ParseNetwork(line));

	// Initialze prefixes status and numRemaining
	status = std::vector<std::atomic<Status>>(prefixes.size());
	numRemaining.store(prefixes.size(), std::memory_order_relaxed);
}

void PrefixSet::SetStatus(size_t prefixIdx, bool extendable)
{
	Status newStatus = extendable ? Extendable : Unextendable;
	Status oldStatus = status[prefixIdx].exchange(newStatus, std::memory_order_relaxed);
	if (oldStatus == Unknown)
		numRemaining.fetch_sub(1, std::memory_order_relaxed);
}

size_t PrefixSet::Size() const
{
	return prefixes.size();
}

PrefixSet::Status PrefixSet::State(size_t idx) const
{
	return status[idx].load(std::memory_order_relaxed);
}

size_t PrefixSet::NumRemaining() const
{
	return numRemaining.load(std::memory_order_relaxed);
}

const Network& PrefixSet::operator[](size_t idx) const
{
	return prefixes[idx];
}

void PrefixSet::SaveStatus(const std::string& filepath) const
{
	std::ofstream file{ filepath };
	for (size_t prefixIdx = 0; prefixIdx < prefixes.size(); prefixIdx++)
		file << status[prefixIdx].load(std::memory_order_relaxed) << '\n';
}