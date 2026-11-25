// The objects' forms (strategy_forms.h): 62 Placed Object, 77 Volume of Death, 78 Self Destruct, 73 Message, 70 Mission Objective. Written 2026-11-24 by Melanikus
// (Vasquez, the empty start; Dietrich writes the rows) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The rows hold what each kind's page edits today (strategy_views.cpp, its Lines and Click functions), with the same fields and the same rules; the words are the page's own:
//   78 Self Destruct: the Time, in seconds, 0 to 32767 (the page's "Time : %d"; its box asked "Time until destruction (seconds)").
//   77 Volume of Death: bit 0 of the flags (Starts Inactive / Active), bit 1 (Only harmful on collision. / Collision not required.), the Damage per second (0 or more; 0 kills
//      at once, "Damage : Infinite (instant death)" on the page), and the page's V, Edit Volume.
//   62 Placed Object: bit 0 of the target request (Turn Target Off / On), the Event Target (the page's pick, with Extra data..., the page's E), and the two events that send
//      the request, bits 0 and 1 of the flags (Object destroyed, Object picked up).
//   73 Message: bit 0 of the flags (Message starts activated / deactivated), the String number (0 to 450, the loader's range), and the page's press on its String line, which
//      opens our Message window: its box lists the numbers the FMVs folder has a film for, and its Play plays one.
//   70 Mission Objective: the Description and Completed Message numbers (0 to 450), the Order Number (any whole number), Shown and Possible at start (bits 0 and 1 of the
//      flags), Special (Nothing / Complete level), and the alterations upon completion as a list (Vasquez's List row): each line an objective and its Visible / Possible bits.
// A kind is switched to its form (its case below gives rows and true) only once it is complete and tested.

#include "strategy_forms.h"

namespace host
{
	namespace
	{
		void SelfDestructRows(std::vector<StrategyFormRow> *rows)
		{
			StrategyFormRow time;

			// NAMED DIFFERENCE (the form's rule for every number): a negative time is refused with the range said; inteng's box kept it as 0 (strategy_views.cpp clampBelow)
			time.kind = StrategyFormRowKind::Number;
			time.label = "Time (seconds)";
			time.field = "timer";
			time.lowest = 0.0;
			time.highest = 32767.0;
			rows->push_back(time);
		}

		void DeathVolumeRows(std::vector<StrategyFormRow> *rows)
		{
			StrategyFormRow starts;

			starts.kind = StrategyFormRowKind::Bits;
			starts.label = "Starts";
			starts.field = "flags";
			starts.mask = 1;
			starts.choices = { { "Inactive", 0 }, { "Active", 1 } };   // the page's "Starts Inactive" / "Starts Active" (0x5e4878, 0x5e43f8)
			rows->push_back(starts);

			StrategyFormRow collision;

			collision.kind = StrategyFormRowKind::Bits;
			collision.label = "Collision";
			collision.field = "flags";
			collision.mask = 2;
			collision.choices = { { "Only harmful on collision.", 0 }, { "Collision not required.", 2 } };   // 0x5efeb4, 0x5efed0
			rows->push_back(collision);

			StrategyFormRow damage;

			// DEBT: the form doesn't say that 0 kills at once (the page said "Damage : Infinite (instant death)"); its range words are the builder's, "(0 or more)"
			damage.kind = StrategyFormRowKind::Number;
			damage.label = "Damage per second";
			damage.field = "damage";
			damage.lowest = 0.0;
			damage.highest = 2147483647.0;
			rows->push_back(damage);

			StrategyFormRow volume;

			volume.kind = StrategyFormRowKind::Action;
			volume.label = "Volume";
			volume.button = "Edit Volume";   // the page's "V : Edit Volume" (0x5efe6c)
			volume.key = 'V';
			rows->push_back(volume);
		}

		void PlacedObjectRows(std::vector<StrategyFormRow> *rows)
		{
			StrategyFormRow request;

			request.kind = StrategyFormRowKind::Bits;
			request.label = "Target request";
			request.field = "target_request";
			request.mask = 1;   // bit 0 only: the higher bits are the extra message data, kept (as the page's click, 0x5310dc)
			request.choices = { { "Turn Target Off", 0 }, { "Turn Target On", 1 } };   // 0x5ef31c, 0x5ef32c
			rows->push_back(request);

			StrategyFormRow target;

			target.kind = StrategyFormRowKind::Target;
			target.label = "Event Target";
			target.field = "targetID";
			target.pickRow = kPickTarget;
			target.request = "target_request";
			target.pickWords = "Select target";   // 0x5e4048, the page's pick (FUN_00531160)
			target.extra = true;                  // the page's "E : Edit extra message data for target"
			rows->push_back(target);

			// "Events that cause target request to be sent to target :" (0x5ef2a0), one box each, as the page's
			StrategyFormRow destroyed;

			destroyed.kind = StrategyFormRowKind::Check;
			destroyed.label = "Object destroyed";   // 0x5ef28c
			destroyed.field = "flags";
			destroyed.mask = 1;
			rows->push_back(destroyed);

			StrategyFormRow picked = destroyed;

			picked.label = "Object picked up";   // 0x5ef278
			picked.mask = 2;
			rows->push_back(picked);
		}

		void MessageRows(std::vector<StrategyFormRow> *rows)
		{
			StrategyFormRow starts;

			starts.kind = StrategyFormRowKind::Bits;
			starts.label = "Starts";
			starts.field = "flags";
			starts.mask = 1;
			starts.choices = { { "Activated", 0 }, { "Deactivated", 1 } };   // the page's "Message starts activated" / "... deactivated" (0x5ef960, 0x5ef97c)
			rows->push_back(starts);

			StrategyFormRow number;

			number.kind = StrategyFormRowKind::Number;
			number.label = "String";   // the page's "String :%d" (0x5ef954)
			number.field = "message_string";
			number.lowest = 0.0;
			number.highest = 450.0;   // the loader's range (strategy_fields.cpp, message_string); 0 is none
			rows->push_back(number);

			StrategyFormRow film;

			// DEBT: the list of the FMVs and Play stay in our Message window, which this button opens (the press on the page's String line): the form has no place to play a film
			film.kind = StrategyFormRowKind::Action;
			film.label = "Video";
			film.words = "Its FMV: the list, and Play";
			film.button = "Play...";
			film.pageX = 16;
			film.pageY = 32;
			rows->push_back(film);
		}

		// 70 MISSION OBJECTIVE (strategy_views.cpp MissionLines / MissionClick; S-K70-1..8): the two message numbers (the page opened a window with a number box for each), the order
		// number (any whole number: inteng made no check), bits 0 and 1 of the flags, Special, and the alterations upon completion: one line per objective it alters, a check box for
		// each of the two bits of its word (the page's V and P on a line), and the "+" line that appends one with both bits (as the page's collector adds a new one, word 3).
		void MissionObjectiveRows(std::vector<StrategyFormRow> *rows)
		{
			StrategyFormRow description;

			description.kind = StrategyFormRowKind::Number;
			description.label = "Description";   // the page's "Description :%d" (0x5ef7c4); its window "Mission Description" (0x5ef6a8)
			description.field = "mission_description_string";
			description.lowest = 0.0;
			description.highest = 450.0;   // the loader's range (strategy_fields.cpp); 0 is none
			rows->push_back(description);

			StrategyFormRow completed = description;

			completed.label = "Completed";   // the page's "Completed Message :%d" (0x5ef7ac), its window 0x5ef694; "Completed Message" ran into its box (picture, 11-25)
			completed.field = "mission_complete_string";
			rows->push_back(completed);

			StrategyFormRow order;

			order.kind = StrategyFormRowKind::Number;
			order.label = "Order Number";   // 0x5ef798; the page's box "Mission Order Number" took any whole number (0x5ef67c: no check)
			order.field = "mission_number";
			order.lowest = -2147483648.0;
			order.highest = 2147483647.0;
			rows->push_back(order);

			StrategyFormRow shown;

			shown.kind = StrategyFormRowKind::Bits;
			shown.label = "Shown";
			shown.field = "flags";
			shown.mask = 1;
			shown.choices = { { "Hidden at start", 0 }, { "Shown at start", 1 } };   // 0x5ef774 (its trailing space left off), 0x5ef788
			rows->push_back(shown);

			StrategyFormRow possible;

			possible.kind = StrategyFormRowKind::Bits;
			possible.label = "Possible";
			possible.field = "flags";
			possible.mask = 2;
			possible.choices = { { "Not possible at start", 0 }, { "Possible at start", 2 } };   // 0x5ef748, 0x5ef760
			rows->push_back(possible);

			StrategyFormRow special;

			special.kind = StrategyFormRowKind::Choice;
			special.label = "Special";
			special.field = "mission_completion_effect";
			special.choices = { { "Nothing", 0 }, { "Complete level", 1 } };   // 0x5ef708, 0x5ef71c; another value shows "Unknown" (the page's 0x5ef734) and writes nothing
			rows->push_back(special);

			StrategyFormRow alterations;

			alterations.kind = StrategyFormRowKind::List;
			alterations.label = "Alterations";   // "Mission alterations upon completion :" (0x5ef6e0)
			alterations.pickWords = "Select mission objectives";   // the page's collector (editor_panels.cpp, copyPurpose 2)
			alterations.entryChecks = { { "Visible", 1u }, { "Possible", 2u } };   // the page's "(make visible)" (0x5ef6cc) and "(make possible)" (0x5ef6bc)
			alterations.newWord = 3u;
			alterations.targetKinds = { 70, 71 };   // only objectives (MissionTargetsEnd: "Targets must have mission objective strategies", 0x5ef85c)
			alterations.readList = [](std::vector<uint8_t> const &payload, std::vector<StrategyFormEntry> *entries)
			{
				std::vector<MissionTarget> now;

				if (!StrategyView_MissionTargets(payload, &now))
				{
					return false;   // a count the bytes don't hold (a hostile file): no list
				}

				for (MissionTarget const &one : now)
				{
					StrategyFormEntry entry;

					entry.id = one.id;
					entry.word = (uint32_t)one.word;
					entries->push_back(entry);
				}

				return true;
			};
			alterations.writeList = [](std::vector<uint8_t> const &payload, std::vector<StrategyFormEntry> const &entries, std::vector<uint8_t> *bytes)
			{
				std::vector<MissionTarget> now;

				for (StrategyFormEntry const &entry : entries)
				{
					MissionTarget one;

					one.id = entry.id;
					one.word = (int32_t)entry.word;
					now.push_back(one);
				}

				return StrategyView_WithMissionTargets(payload, now, bytes);   // the count and the list; the request targets and the pads after them kept
			};
			rows->push_back(alterations);
		}
	}

	bool StrategyFormRows_Objects(int32_t kind, std::vector<StrategyFormRow> *rows)
	{
		switch (kind)
		{
			case 78:
				SelfDestructRows(rows);
				return true;

			case 77:
				DeathVolumeRows(rows);
				return true;

			case 62:
				PlacedObjectRows(rows);
				return true;

			case 73:
				MessageRows(rows);
				return true;

			case 70:
				MissionObjectiveRows(rows);
				return true;

			default:
				return false;
		}
	}
}
