// The paths' forms (strategy_forms.h): 72 / 80 Track Object, 76 Placed Hierarchy, 69 Enemy, 79 Generator, and the E extra-data pages. Written 2026-11-24 by Melanikus
// (Vasquez, the empty start; Frost writes the rows) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "strategy_forms.h"

namespace host
{
	bool StrategyFormRows_Paths(int32_t kind, std::vector<StrategyFormRow> *rows)
	{
		(void)kind;
		(void)rows;
		return false;   // no kind of this group has its form yet: each keeps inteng's page
	}
}
