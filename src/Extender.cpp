#include "Extender.h"

#include "Prefixes/WindowMinimizer.h"

Extender::Extender(uint8_t n_, bool symmetric_, const Network& prefix)
	: n(n_), symmetric(symmetric_)
{
	WindowMinimizer minimizer{ n, symmetric, 1234 };
	optimizedPrefix = minimizer.Optimize(prefix, 256, 256);
	OutputSet outputs = GetOutputs(optimizedPrefix, n, true, symmetric);
	prefixOutputs.assign(outputs.begin(), outputs.end());
}