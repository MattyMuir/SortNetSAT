#pragma once
#include <sortnetutils.h>
#include <digraph.hh>

class NetworkGraph
{
public:
	NetworkGraph(const std::vector<Network>& layers, uint8_t n);
	NetworkGraph(const NetworkGraph& other) = delete;
	NetworkGraph(NetworkGraph&& other) noexcept;
	~NetworkGraph();

	bool operator<(const NetworkGraph& other) const;
	bool operator==(const NetworkGraph& other) const;

protected:
	bliss::Digraph* graph;
};