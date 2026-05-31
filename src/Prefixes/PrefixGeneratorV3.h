#pragma once
#include <sortnetutils.h>

#include "PrefixGraph.h"

class PrefixGeneratorV3
{
public:
	PrefixGeneratorV3(uint8_t n_, uint8_t d_, bool symmetric_);

	std::vector<Network> GeneratePrefixes();

protected:
	uint8_t n, d;
	bool symmetric;

	std::vector<CE> alphabet;
	std::vector<Network> allLayers;
	PrefixGraph graph;

	bool CanAddCE(const Network& layer, CE ce) const;
	Network AddCE(const Network& layer, CE ce) const;
};