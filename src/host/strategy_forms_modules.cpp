// The modules' forms (strategy_forms.h): 51 Lift, 50 Door, 57 Airlock, 59 Switch Door, 54 Platform Lift, 75 Fan, 64 Lighting Effects. Written 2026-11-24 by Melanikus
// (Vasquez, the empty start; Drake writes the rows) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "strategy_forms.h"

namespace host
{
	bool StrategyFormRows_Modules(int32_t kind, std::vector<StrategyFormRow> *rows)
	{
		(void)kind;
		(void)rows;
		return false;   // no kind of this group has its form yet: each keeps inteng's page
	}
}
