#pragma once
#include <sortnetutils.h>

#include "PrefixGraph.h"

class PrefixGeneratorV3
{
public:
	PrefixGeneratorV3(uint8_t n_, bool symmetric_);

	std::vector<Network> Generate();

protected:
	uint8_t n;
	bool symmetric;

	std::vector<CE> alphabet;
	std::vector<Network> allLayers;
	PrefixGraph graph;

	bool CanAddCE(const Network& layer, CE ce) const;
	Network AddCE(const Network& layer, CE ce) const;
};