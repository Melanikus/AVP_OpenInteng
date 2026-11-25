// The modules' forms (strategy_forms.h): 51 Lift, 50 Door, 57 Airlock, 59 Switch Door, 54 Platform Lift, 75 Fan, 64 Lighting Effects. Written 2026-11-24 by Melanikus
// (Vasquez, the empty start; Drake writes the rows) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Each kind's rows say what its page said (strategy_views.cpp), in inteng's own words, with the same fields, limits and writers: a kind answers true here only once its form is
// complete and tested (its acceptance), and any other kind keeps inteng's page. Switched on so far: 75 Fan, 54 Platform Lift, 50 Door (Drake 2026-11-24).

#include "strategy_forms.h"

#include "strategy_views.h"   // StrategyView_FieldNumber (a door's kind, for its rows' shown rules) and kPickSwitchDoor

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

	// 50 DOOR (strategy_views.cpp DoorLines / DoorClick, FUN_0052cbb0): its kind first, bits 2 and 8 of DoorFlags (a proximity door has 2, a switch door 8: DoorIsSwitch); then what
	// that kind shows: a proximity door's Locked (bit 1) and its way (bit 0x10, vertical or in the floor / ceiling), a switch door's Open / Closed (bit 4); then the two times, typed in
	// seconds and kept in tenths (as DoorTime: x 10 + 0.01, made whole; 0.1 to 25.5 seconds, 1 to 255 tenths)
	bool DoorIsSwitchIn(int32_t kind, std::vector<uint8_t> const &payload)
	{
		int32_t flags = 0;

		return host::StrategyView_FieldNumber(kind, payload, "DoorFlags", &flags) && ((uint32_t)flags & 2u) == 0 && ((uint32_t)flags & 8u) != 0;
	}

	host::StrategyFormRow DoorTimeRow(char const *label, char const *field)
	{
		host::StrategyFormRow row = NumberRow(label, field, 0.1, 25.5);

		row.factor = 10.0;
		row.bias = 0.01;   // 0x5ac438, as DoorTime: added before the whole number is taken
		row.isFloat = true;
		return row;
	}

	void DoorRows(std::vector<host::StrategyFormRow> *rows)
	{
		// DEBT: a door holding both bits 2 and 8 (drawn by the page as a proximity door) shows the first row whose bits are set, as the builder picks one
		rows->push_back(BitsRow("Door", "DoorFlags", 0xA, { { "Proximity door", 0x2 }, { "Switch activated door ('lift door')", 0x8 } }));

		host::StrategyFormRow locked = BitsRow("Lock", "DoorFlags", 0x1, { { "Door is not locked", 0 }, { "Door is locked", 0x1 } });
		host::StrategyFormRow way = BitsRow("Way", "DoorFlags", 0x10, { { "Door is vertical (standard)", 0 }, { "Door is horizontal (ie. in floor/ceiling)", 0x10 } });
		host::StrategyFormRow open = BitsRow("Starts", "DoorFlags", 0x4, { { "Closed", 0 }, { "Open", 0x4 } });

		locked.shown = [](int32_t kind, std::vector<uint8_t> const &payload) { return !DoorIsSwitchIn(kind, payload); };
		way.shown = locked.shown;
		open.shown = [](int32_t kind, std::vector<uint8_t> const &payload) { return DoorIsSwitchIn(kind, payload); };
		rows->push_back(locked);
		rows->push_back(way);
		rows->push_back(open);
		// labels that fit the form's label column (as the Fan's: Dietrich's MUST on 679b0594); the range words beside each box give the limits in seconds
		rows->push_back(DoorTimeRow("Time take to open", "TimeToOpen"));
		rows->push_back(DoorTimeRow("Time take to close", "TimeToClose"));
	}

	// 59 SWITCH DOOR (strategy_views.cpp SwitchDoorLinesAvp / SwitchDoorClickAvp, FUN_0052d490): its associated door, picked as T picks it (kPickSwitchDoor: the door named, and a
	// door with no strategy given a Switch Door that names this one). The page shows nothing else (inteng's times in spare1 the game ignores). NOT SWITCHED ON YET: it goes on with its own
	// HAND, once the page-mechanics scenarios that use a Switch Door as their example (view-box, shift-e, e-pages, pick-red, pick-modal) are moved (Drake 2026-11-24)
	void SwitchDoorRows(std::vector<host::StrategyFormRow> *rows)
	{
		host::StrategyFormRow door;

		door.kind = host::StrategyFormRowKind::Target;
		door.label = "Associated door";
		door.field = "AssocDoor";
		door.pickRow = host::kPickSwitchDoor;
		door.pickWords = "Select associated door";   // the page's pick words (0x5eec90)
		rows->push_back(door);
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
			case 50: DoorRows(rows); return true;
			default: return false;   // every other kind of this group keeps inteng's page for now
		}
	}
}
