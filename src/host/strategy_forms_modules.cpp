// The modules' forms (strategy_forms.h): 51 Lift, 50 Door, 57 Airlock, 59 Switch Door, 54 Platform Lift, 75 Fan, 64 Lighting Effects. Written 2026-11-24 by Melanikus
// (Vasquez, the empty start; Drake writes the rows) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Each kind's rows say what its page said (strategy_views.cpp), in inteng's own words, with the same fields, limits and writers: a kind answers true here only once its form is
// complete and tested (its acceptance), and any other kind keeps inteng's page. Switched on so far: 75 Fan, 54 Platform Lift (Drake 2026-11-24).

#include "strategy_forms.h"

namespace
{
	host::StrategyFormRow NumberRow(char const *label, char const *field, double lowest, double highest)
	{
		host::StrategyFormRow row;

		row.kind = host::StrategyFormRowKind::Number;
		row.label = label;
		row.field = field;
		row.lowest = lowest;
		row.highest = highest;
		return row;
	}

	host::StrategyFormRow BitsRow(char const *label, char const *field, uint32_t mask, std::vector<host::StrategyFormChoice> choices)
	{
		host::StrategyFormRow row;

		row.kind = host::StrategyFormRowKind::Bits;
		row.label = label;
		row.field = field;
		row.mask = mask;
		row.choices = std::move(choices);
		return row;
	}

	// 75 FAN (FUN_005356b0's page: strategy_views.cpp FanLines / FanClick): three typed numbers, as the page's NumberOf takes them: the two times in whole seconds, 1 to 999
	// (the model keeps them x 65536), the wind -1000 to 1000 percent
	void FanRows(std::vector<host::StrategyFormRow> *rows)
	{
		// the labels short enough for the form's label column (about 19 letters, Dietrich's MUST on 679b0594): the range words beside each box give the limits
		rows->push_back(NumberRow("Speed up time", "speed_up_time", 1, 999));
		rows->push_back(NumberRow("Slow down time", "slow_down_time", 1, 999));
		rows->push_back(NumberRow("Wind strength", "fan_wind_strength", -1000, 1000));
	}

	// 54 PLATFORM LIFT (strategy_views.cpp PlatformLines / PlatformClick): the two bits of its flags, each a drop-down of the page's two words for it (0x5ef020 / 0x5ef030,
	// 0x5eefcc / 0x5ef000)
	void PlatformRows(std::vector<host::StrategyFormRow> *rows)
	{
		rows->push_back(BitsRow("Start", "flags", 0x1, { { "Starts enabled", 0 }, { "Starts disabled", 0x1 } }));
		rows->push_back(BitsRow("Use", "flags", 0x2, { { "Once enabled , continues working until switched off", 0 }, { "Becomes disabled after one use", 0x2 } }));
	}
}

namespace host
{
	bool StrategyFormRows_Modules(int32_t kind, std::vector<StrategyFormRow> *rows)
	{
		if (!rows)
		{
			return false;
		}

		rows->clear();

		switch (kind)
		{
			case 75: FanRows(rows); return true;
			case 54: PlatformRows(rows); return true;
			default: return false;   // every other kind of this group keeps inteng's page for now
		}
	}
}
