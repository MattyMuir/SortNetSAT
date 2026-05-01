#include "Network.h"

#include <print>

void PrintNetwork(const Network& network)
{
	for (size_t i = 0; i < network.size(); i++)
	{
		if (i) std::print(",");
		std::print("({},{})", network[i].lo, network[i].hi);
	}
	std::println();
}

Network Append(const Network& a, const Network& b)
{
	Network ret{ a };
	ret.insert(ret.end(), b.begin(), b.end());
	return ret;
}