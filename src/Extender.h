#pragma once
#include <sortnetutils.h>

class Extender
{
public:
	Extender(uint8_t n_, bool symmetric_, const Network& prefix);

	virtual bool Extend() = 0;
	virtual Network GetNetwork() const = 0;

protected:
	uint8_t n;
	bool symmetric;

	Network optimizedPrefix;
	std::vector<uint64_t> prefixOutputs;
};