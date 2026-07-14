#include <print>
#include <algorithm>
#include <numeric>

#include <sortnetutils.h>

#include "OldPrefixes/NetworkGraph.h"

Permutation ExpandCentrosymmetric(const Permutation& halfPerm)
{
	uint8_t n = (uint8_t)(halfPerm.size() * 2);
	Permutation full{ halfPerm };
	full.resize(n);
	for (uint8_t k = n / 2; k < n; k++)
		full[k] = n - 1 - halfPerm[n - 1 - k];
	return full;
}

template <typename Func>
bool ForEachPermSym(uint8_t n, Func&& callback)
{
	uint8_t m = n / 2;
	Permutation sigma(m);
	std::iota(sigma.begin(), sigma.end(), 0);
	do
	{
		for (uint32_t orientMask = 0; orientMask < (1u << m); orientMask++)
		{
			Permutation halfPerm(m);
			for (uint8_t k = 0; k < m; k++)
			{
				bool flip = (orientMask >> k) & 1;
				uint8_t lo = sigma[k];
				uint8_t hi = n - 1 - sigma[k];
				halfPerm[k] = flip ? hi : lo;
			}
			if (callback(ExpandCentrosymmetric(halfPerm)))
				return true;
		}
	} while (std::next_permutation(sigma.begin(), sigma.end()));
	return false;
}

template <typename Func>
bool ForEachPerm(uint8_t n, Func&& callback)
{
	Permutation perm(n);
	std::iota(perm.begin(), perm.end(), 0);
	do
	{
		if (callback(perm))
			return true;
	} while (std::next_permutation(perm.begin(), perm.end()));
	return false;
}

bool AreEquivalentNaive(const Network& a, const Network& b, uint8_t n, bool symmetric)
{
	if (a.size() != b.size()) return false;

	auto compFunc = [&a, &b](const Permutation& perm) {
		Network aPerm{ a };
		aPerm.Permute(perm);
		aPerm.Untangle();
		return Network::Identical(aPerm, b);
		};

	if (symmetric) return ForEachPermSym(n, compFunc);
	return ForEachPerm(n, compFunc);
}

bool AreEquivalentFast(const Network& a, const Network& b, uint8_t n, bool symmetric)
{
	NetworkGraph aGraph{ LayeredNetwork{ a }, n, symmetric };
	NetworkGraph bGraph{ LayeredNetwork{ b }, n, symmetric };
	return aGraph == bGraph;
}

void EquivTest()
{
	uint8_t n = 8;
	uint8_t depth = 2;
	bool symmetric = true;

	for (;;)
	{
		Network a = RandomNetworkLayered(n, depth, symmetric);
		Network b = RandomNetworkLayered(n, depth, symmetric);

		bool areEquiv1 = AreEquivalentNaive(a, b, n, symmetric);
		bool areEquiv2 = AreEquivalentFast(a, b, n, symmetric);
		std::println("{} {}", areEquiv1, areEquiv2);

		if (areEquiv1 != areEquiv2)
		{
			std::println("{}\n{}", a, b);
			std::println("{:t}\n\n\n{:t}", a, b);
			return;
		}
	}
}