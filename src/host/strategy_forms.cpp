// See strategy_forms.h. Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "strategy_forms.h"

namespace host
{
	namespace
	{
		// Asks the four groups in turn; the first that knows the kind gives its rows. A kind belongs to one group only (strategy_forms.h), so the order doesn't matter.
		bool AskTheGroups(int32_t kind, std::vector<StrategyFormRow> *rows)
		{
			return StrategyFormRows_Switches(kind, rows) || StrategyFormRows_Modules(kind, rows) || StrategyFormRows_Objects(kind, rows) ||
			       StrategyFormRows_Paths(kind, rows);
		}
	}

	bool StrategyForm_Has(int32_t kind)
	{
		std::vector<StrategyFormRow> rows;
		return AskTheGroups(kind, &rows) && !rows.empty();
	}

	// The pages' own words (strategy_views.cpp, the (200, 0) line of each page); a page that picks its words from the record (51: Lift or Airlock, 62: Light Object, 72) gives the usual one here
	std::string StrategyForm_KindWords(int32_t kind)
	{
		switch (kind)
		{
			case 50: return " : Door";
			case 51: return " : Lift";
			case 52: return " : Binary Switch";
			case 54: return " : Platform Lift";
			case 57: return " : Airlock";
			case 59: return " : Switch door";
			case 61: return " : Linked Switch";
			case 62: return " : Placed Object";
			case 64: return " : Lighting Effects";
			case 66: return " : Multiple Target Switch";
			case 68: return " : Area Switch";
			case 69: return " : Enemy";
			case 70: return " : Mission Objective";
			case 72: return " : Track";
			case 73: return " : Message";
			case 75: return " : Fan";
			case 76: return " : Hierarchy";
			case 77: return " : Volume of Death";
			case 78: return " : Self Destruct";
			case 79: return " : Generator";
			case 80: return " : Track (Destructable)";
			default: return "";
		}
	}

	std::vector<StrategyFormRow> StrategyForm_Rows(int32_t kind)
	{
		std::vector<StrategyFormRow> rows;
		if (!AskTheGroups(kind, &rows))
			rows.clear();
		return rows;
	}
}
