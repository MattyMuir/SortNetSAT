#pragma once
#include <unordered_set>

#include <sortnetutils.h>

#include "LayerDAG.h"

class PrefixGeneratorV2
{
public:
	PrefixGeneratorV2(uint8_t n_, uint8_t d_, bool symmetric_);

	std::vector<Network> GeneratePrefixes();

protected:
	uint8_t n, d;
	bool symmetric;

	LayerDAG layerDAG;
	std::vector<LayeredNetwork> allPrefixes;

	void Generate();
};