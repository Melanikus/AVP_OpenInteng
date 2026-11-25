// The objects' forms (strategy_forms.h): 62 Placed Object, 77 Volume of Death, 78 Self Destruct, 73 Message, 70 Mission Objective. Written 2026-11-24 by Melanikus
// (Vasquez, the empty start; Dietrich writes the rows) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "strategy_forms.h"

namespace host
{
	bool StrategyFormRows_Objects(int32_t kind, std::vector<StrategyFormRow> *rows)
	{
		(void)kind;
		(void)rows;
		return false;   // no kind of this group has its form yet: each keeps inteng's page
	}
}
