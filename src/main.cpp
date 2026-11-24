#include "SimpleMoietyPruner.h"

int main()
{
	PrefixSet prefixes{ "C:\\Users\\matty\\source\\repos\\SortNetSAT\\prefixes\\18_3_sym.txt" };
	SimpleMoietyPruner pruner{ prefixes, 18, 7, true, 100 };
	pruner.Prune();
}