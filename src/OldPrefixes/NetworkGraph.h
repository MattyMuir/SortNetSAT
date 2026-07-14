#pragma once
#include <sortnetutils.h>
#include <digraph.hh>

class NetworkGraph
{
public:
	NetworkGraph(const LayeredNetwork& network, uint8_t n, bool symmetric = false);

	bool operator<(const NetworkGraph& other) const;
	bool operator==(const NetworkGraph& other) const;

	uint32_t GetHash() const;

protected:
	std::unique_ptr<bliss::Digraph> graph;
};