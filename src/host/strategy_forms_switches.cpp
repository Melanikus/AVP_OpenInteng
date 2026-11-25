// The switches' forms (strategy_forms.h): 61 Linked Switch, 66 Multiple Target Switch, 68 Area Switch. Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The rows hold what the switch's page and its P form ("Switch Properties", strategy_screen.cpp SwitchPropertiesWindow) edit today, with the same fields and the same rules:
//   Rest State: bit 0 of the flags (52 / 61). Mode: Timer 0, Wait 1, Toggle 2, Time Delay (Autoexec) 5; a Linked Switch has no Time Delay 4 (inteng's form makes it 0 at Ok).
//   Seconds: the Time, inteng's 16.16 (seconds x 65536), 0 to 30000 seconds, for the modes with a time (the page shows it for 0, 4 and 5). Security: 0 or more.
//   Switch off message: bits 1 and 2 of the flags, for the modes 0 to 2 (the page shows it only for them): Reversed (default) 0, Same as switch on 2, None 4.
//   Target: the object the switch sends to; a new target makes the request 0 (FUN_0052db10, the page's T); Extra data... is the page's E.
//   Links: the switches that target this one, read only (the page's "Links:").

#include "strategy_forms.h"

namespace host
{
	namespace
	{
		int32_t ModeOf(int32_t kind, std::vector<uint8_t> const &payload)
		{
			int32_t mode = -1;

			return StrategyView_FieldNumber(kind, payload, "Mode", &mode) ? mode : -1;
		}

		void LinkedSwitchRows(std::vector<StrategyFormRow> *rows)
		{
			StrategyFormRow rest;

			rest.kind = StrategyFormRowKind::Bits;
			rest.label = "Rest State";
			rest.field = "flags";
			rest.mask = 1;
			rest.choices = { { "Off", 0 }, { "On", 1 } };
			rows->push_back(rest);

			StrategyFormRow mode;

			mode.kind = StrategyFormRowKind::Choice;
			mode.label = "Mode";
			mode.field = "Mode";
			mode.choices = { { "Timer", 0 }, { "Wait", 1 }, { "Toggle", 2 }, { "Time Delay (Autoexec)", 5 } };
			rows->push_back(mode);

			StrategyFormRow seconds;

			seconds.kind = StrategyFormRowKind::Number;
			seconds.label = "Seconds";
			seconds.field = "Time";
			seconds.factor = 65536.0;
			seconds.isFloat = true;
			seconds.lowest = 0.0;
			seconds.highest = 30000.0;
			seconds.shown = [](int32_t kind, std::vector<uint8_t> const &payload)
			{
				int32_t const now = ModeOf(kind, payload);

				return now == 0 || now == 4 || now == 5;
			};
			rows->push_back(seconds);

			StrategyFormRow security;

			security.kind = StrategyFormRowKind::Number;
			security.label = "Security";
			security.field = "Security";
			security.lowest = 0.0;
			security.highest = 2147483647.0;
			rows->push_back(security);

			StrategyFormRow off;

			off.kind = StrategyFormRowKind::Bits;
			off.label = "Switch off message";
			off.field = "flags";
			off.mask = 6;
			off.choices = { { "Reversed (default)", 0 }, { "Same as switch on", 2 }, { "None", 4 } };
			off.shown = [](int32_t kind, std::vector<uint8_t> const &payload)
			{
				int32_t const now = ModeOf(kind, payload);

				return now == 0 || now == 1 || now == 2;
			};
			rows->push_back(off);

			StrategyFormRow target;

			target.kind = StrategyFormRowKind::Target;
			target.label = "Target";
			target.field = "Target";
			target.pickRow = kPickSwitchTarget;
			target.request = "request";
			target.pickWords = "Select target for switch";   // 0x5eeec0, the page's T
			target.extra = true;
			rows->push_back(target);

			StrategyFormRow links;

			links.kind = StrategyFormRowKind::Links;
			links.label = "Links";
			rows->push_back(links);
		}
	}

	bool StrategyFormRows_Switches(int32_t kind, std::vector<StrategyFormRow> *rows)
	{
		if (kind == 61)
		{
			LinkedSwitchRows(rows);
			return true;
		}

		return false;   // 66 and 68 keep inteng's page until their target lists are done
	}
}
