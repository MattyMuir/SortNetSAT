#pragma once
#include <sortnetutils.h>
#include <digraph.hh>

class OutputGraph
{
public:
	OutputGraph(const OutputSet& outputs, uint8_t n);
	OutputGraph(const OutputGraph& other) = delete;
	OutputGraph(OutputGraph&& other) noexcept;
	OutputGraph& operator=(const OutputGraph& other) = delete;
	OutputGraph& operator=(OutputGraph&& other) noexcept;
	~OutputGraph();

	bool operator<(const OutputGraph& other) const;
	bool operator==(const OutputGraph& other) const;

	uint32_t GetHash() const;

protected:
	bliss::Digraph* graph;
};