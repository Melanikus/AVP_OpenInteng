// The switches' forms (strategy_forms.h): 61 Linked Switch, 66 Multiple Target Switch, 68 Area Switch. Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The rows hold what the switch's page and its P form ("Switch Properties", strategy_screen.cpp SwitchPropertiesWindow) edit today, with the same fields and the same rules:
//   Rest State: bit 0 of the flags (52 / 61). Mode: Timer 0, Wait 1, Toggle 2, Time Delay (Autoexec) 5; a Linked Switch has no Time Delay 4 (inteng's form makes it 0 at Ok).
//   Seconds: the Time, inteng's 16.16 (seconds x 65536), 0 to 30000 seconds, for the modes with a time (the page shows it for 0, 4 and 5). Security: 0 or more.
//   Switch off message: bits 1 and 2 of the flags, for the modes 0 to 2 (the page shows it only for them): Reversed (default) 0, Same as switch on 2, None 4.
//   Target: the object the switch sends to; a new target makes the request 0 (FUN_0052db10, the page's T); Extra data... is the page's E.
//   Links: the switches that target this one, read only (the page's "Links:").
// 66 / 68: see MultiSwitchRows. 51 Lift and 57 Airlock (a 51 with LiftFlags bit 2; taken from Drake's group 2026-11-25, Bishop's split): see LiftRows.

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

		// 66 Multiple Target / 68 Area Switch (FUN_0052f700, its P form FUN_0052ea70): Rest State is its own word (RestState), the modes are all five (Time Delay 4 too), the
		// switch-off message is bits 1 and 2 of the flags (+0x38); the targets are a list (StrategyView_MultiTargets: an ID and a request each, bit 0 on / off), whose lines do what
		// the page's keys do on a target row: S (On), E (Extra data...), D (Duplicate), Shift + D (Remove), T (Pick); a new target is on (request 1, MultiTargetsEnd 0x53035f).
		// The Area Switch adds "V : Change Trigger Volume" (the volume editor, as V on its page).
		void MultiSwitchRows(int32_t kind, std::vector<StrategyFormRow> *rows)
		{
			StrategyFormRow rest;

			rest.kind = StrategyFormRowKind::Choice;
			rest.label = "Rest State";
			rest.field = "RestState";
			rest.choices = { { "Off", 0 }, { "On", 1 } };
			rows->push_back(rest);

			StrategyFormRow mode;

			mode.kind = StrategyFormRowKind::Choice;
			mode.label = "Mode";
			mode.field = "Mode";
			mode.choices = { { "Timer", 0 }, { "Wait", 1 }, { "Toggle", 2 }, { "Time Delay", 4 }, { "Time Delay (Autoexec)", 5 } };
			rows->push_back(mode);

			StrategyFormRow seconds;

			seconds.kind = StrategyFormRowKind::Number;
			seconds.label = "Seconds";
			seconds.field = "Time";
			seconds.factor = 65536.0;
			seconds.isFloat = true;
			seconds.lowest = 0.0;
			seconds.highest = 30000.0;
			seconds.shown = [](int32_t kindNow, std::vector<uint8_t> const &payload)
			{
				int32_t const now = ModeOf(kindNow, payload);

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
			off.shown = [](int32_t kindNow, std::vector<uint8_t> const &payload)
			{
				int32_t const now = ModeOf(kindNow, payload);

				return now == 0 || now == 1 || now == 2;
			};
			rows->push_back(off);

			StrategyFormRow targets;

			targets.kind = StrategyFormRowKind::List;
			targets.label = "Targets";
			targets.pickWords = "Select targets for switch";   // 0x5ef178, the page's T
			targets.entryChecks = { { "On", 1u } };
			targets.newWord = 1u;
			targets.entryExtra = true;
			targets.entryDuplicate = true;
			targets.readList = [](std::vector<uint8_t> const &payload, std::vector<StrategyFormEntry> *entries)
			{
				std::vector<MultiTarget> now;

				if (!StrategyView_MultiTargets(payload, &now))
				{
					return false;
				}

				for (MultiTarget const &one : now)
				{
					StrategyFormEntry entry;

					entry.id = one.id;
					entry.word = one.request;
					entries->push_back(entry);
				}

				return true;
			};
			targets.writeList = [](std::vector<uint8_t> const &payload, std::vector<StrategyFormEntry> const &entries, std::vector<uint8_t> *bytes)
			{
				std::vector<MultiTarget> now;

				for (StrategyFormEntry const &entry : entries)
				{
					MultiTarget one;

					one.id = entry.id;
					one.request = entry.word;
					now.push_back(one);
				}

				return StrategyView_MultiWith(payload, now, bytes);   // the count and the list; the links, pad and flags after them kept
			};
			rows->push_back(targets);

			if (kind == 68)
			{
				StrategyFormRow volume;

				volume.kind = StrategyFormRowKind::Action;
				volume.label = "Trigger Volume";
				volume.words = "V : Change Trigger Volume";   // 0x5e4138, the page's line
				volume.button = "Edit volume...";
				volume.key = 'V';
				rows->push_back(volume);
			}

			StrategyFormRow links;

			links.kind = StrategyFormRowKind::Links;
			links.label = "Links";
			rows->push_back(links);
		}

		// 51 LIFT / 57 AIRLOCK (strategy_views.cpp LiftLines / LiftClick, FUN_0052c400): the lift's positions (the page's collector, FUN_0052b360: modules only, never itself, each once;
		// then "Would you like the other positions altered to agree with this one?"), its Door, Call Switch and Floor Switch (the last only without LiftFlags bit 4), each picked with the
		// page's own rule (kPickLiftDoor / Call / Floor: the door gets a Door strategy and the lift its Facing; the switches theirs); where it starts (the page's window "Select Lift Start
		// Position") and its exit side (flag 8, then the positions agree), each the page's own line pressed; and its Radius, Floor and Facing, read only as the page shows them.
		void LiftRows(std::vector<StrategyFormRow> *rows)
		{
			StrategyFormRow positions;

			positions.kind = StrategyFormRowKind::List;
			positions.label = "Lift positions";
			positions.pickWords = "Select other positions for this lift";   // the collector's words (0x5ee8d0)
			positions.canTakeKind = 51;
			positions.liftPositions = true;
			positions.addWords = "add a position";
			positions.readList = [](std::vector<uint8_t> const &payload, std::vector<StrategyFormEntry> *entries)
			{
				std::vector<std::array<uint8_t, 8>> ids;

				if (!StrategyView_LiftPositions(payload, &ids))
				{
					return false;
				}

				for (std::array<uint8_t, 8> const &id : ids)
				{
					StrategyFormEntry entry;

					entry.id = id;
					entries->push_back(entry);
				}

				return true;
			};
			rows->push_back(positions);   // (written by the builder as the collector writes them: liftPositions)

			auto const target = [rows](char const *label, char const *field, int32_t pickRow, char const *pickWords)
			{
				StrategyFormRow row;

				row.kind = StrategyFormRowKind::Target;
				row.label = label;
				row.field = field;
				row.pickRow = pickRow;
				row.pickWords = pickWords;
				rows->push_back(row);
			};

			target("Door", "AssocDoor", kPickLiftDoor, "Select door for this lift position");   // 0x5ee8b4
			target("Call Switch", "AssocCallSwitch", kPickLiftCall, "Select call switch");      // 0x5ee8ec
			target("Floor Switch", "AssocFloorSwitch", kPickLiftFloor, "Select floor switch");  // 0x5ee914
			rows->back().shown = [](int32_t kind, std::vector<uint8_t> const &payload)
			{
				int32_t flags = 0;

				return StrategyView_FieldNumber(kind, payload, "LiftFlags", &flags) && (flags & 4) == 0;   // the page draws no Floor Switch with bit 4 (LiftRowsOf)
			};

			StrategyFormRow start;

			start.kind = StrategyFormRowKind::Action;
			start.label = "Start";
			start.pageWordsStarts = { "Lift starts at", "Lift has no start position" };   // 0x5ee968, 0x5ee94c
			start.button = "Choose...";
			start.pagePickRow = kWindowLiftStart;
			rows->push_back(start);

			StrategyFormRow exit;

			exit.kind = StrategyFormRowKind::Action;
			exit.label = "Exit";
			exit.pageWordsStarts = { "Exit same side", "Exit opposite side" };   // 0x5ee928, 0x5ee938
			exit.button = "Change";
			exit.pagePickRow = kRunLiftExit;
			rows->push_back(exit);

			StrategyFormRow foot;

			foot.kind = StrategyFormRowKind::PageWords;
			foot.label = "Lift";
			foot.pageWordsStarts = { "Radius ", "Floor ", "Facing " };   // 0x5ee9e0, 0x5ee9d4, 0x5ee9c8
			foot.pageWordsX = 200;   // the page's foot column: not its "Floor Switch :" heading at 16
			rows->push_back(foot);
		}
	}

	bool StrategyFormRows_Switches(int32_t kind, std::vector<StrategyFormRow> *rows)
	{
		if (kind == 61)
		{
			LinkedSwitchRows(rows);
			return true;
		}

		if (kind == 66 || kind == 68)
		{
			MultiSwitchRows(kind, rows);
			return true;
		}

		if (kind == 51)
		{
			LiftRows(rows);   // and 57: an Airlock is a 51 (strategy_fields.cpp: made as this kind, with a flag set)
			return true;
		}

		return false;
	}
}
