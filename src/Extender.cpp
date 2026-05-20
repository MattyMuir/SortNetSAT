#include "Extender.h"

#include "WindowMinimizer.h"

Extender::Extender(uint8_t n_, uint8_t d_, bool symmetric_, const Network& prefix)
	: n(n_), d(d_), dPost(d - ComputeDepth(prefix)), symmetric(symmetric_)
{
	WindowMinimizer minimizer{ n, symmetric, 1234 };
	optimizedPrefix = minimizer.Optimize(prefix, 128, 256);
	prefixOutputs = GetOutputs(optimizedPrefix, n, true, symmetric);
}