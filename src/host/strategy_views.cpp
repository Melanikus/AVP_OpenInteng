// See strategy_views.h. Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "strategy_views.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "field_model.h"

namespace host
{
	namespace
	{
		// The numbers of a strategy's payload by the model's field names
		struct Reader
		{
			std::vector<uint8_t> const &payload;
			int32_t kind;

			model::Field const *Find(char const *name, model::StrategyKind const **owner) const
			{
				model::StrategyKind const *const known = model::FindStrategyKind(kind);

				for (size_t i = 0; known && i < known->fieldCount; i++)
				{
					if (strcmp(known->fields[i].name, name) == 0)
					{
						*owner = known;
						return &known->fields[i];
					}
				}

				return nullptr;
			}

			// the value of a field as the editor shows it: after the mask and brought down by the shift of a masked field
			bool Number(char const *name, int32_t *value) const
			{
				model::StrategyKind const *known = nullptr;
				model::Field const *const field = Find(name, &known);
				double number = 0.0;

				if (!field || !model::ReadField(*known, *field, payload.data(), payload.size(), &number, nullptr))
				{
					return false;
				}

				*value = (int32_t)(int64_t)number;   // ReadField gives a masked field's value brought down by its shift already
				return true;
			}

			int32_t N(char const *name) const
			{
				int32_t value = 0;

				return Number(name, &value) ? value : 0;
			}

			// a time of the Fan: the model gives whole seconds (it is kept as 16.16 in the file)
			int32_t Seconds(char const *name) const
			{
				return N(name);   // the model reads a time as whole seconds (as SetField takes it)
			}

			// an object field: the 8 bytes of the ID, and whether it names an object at all
			bool Object(char const *name, uint8_t id[8]) const
			{
				model::StrategyKind const *known = nullptr;
				model::Field const *const field = Find(name, &known);
				uint32_t first = 0, second = 0;

				if (!field || !model::ReadFieldObject(*known, *field, payload.data(), payload.size(), &first, &second))
				{
					return false;
				}

				memcpy(id, &first, 4);
				memcpy(id + 4, &second, 4);
				return first != 0 || second != 0;
			}
		};

		std::string Format(char const *format, ...)
		{
			char text[512];
			va_list args;

			va_start(args, format);
			vsnprintf(text, sizeof(text), format, args);
			va_end(args);
			text[sizeof(text) - 1] = '\0';
			return text;
		}

		void Put(std::vector<StrategyViewLine> *lines, int32_t x, int32_t y, std::string const &text)
		{
			StrategyViewLine line;

			line.x = x;
			line.y = y;
			line.text = text;
			lines->push_back(line);
		}

		StrategyViewClick ToggleOf(char const *field, uint32_t mask)
		{
			StrategyViewClick click;

			click.action = StrategyViewClick::Toggle;
			click.field = field;
			click.mask = mask;
			return click;
		}

		StrategyViewClick SetOf(char const *field, double value)
		{
			StrategyViewClick click;

			click.action = StrategyViewClick::Set;
			click.field = field;
			click.value = value;
			return click;
		}

		// a whole number typed in a text dialog (FUN_00474340): an answer from lowest to highest is kept as it is
		StrategyViewClick NumberOf(char const *field, char const *title, int64_t initial, int64_t lowest, int64_t highest)
		{
			StrategyViewClick click;

			click.action = StrategyViewClick::Number;
			click.field = field;
			click.title = title;
			click.initialText = std::to_string((long long)initial);
			click.lowest = (double)lowest;
			click.highest = (double)highest;
			return click;
		}

		bool Row(int32_t y, int32_t top)
		{
			return y >= top && y < top + 16;
		}

		struct Context
		{
			Reader const &r;
			std::string const &name;
			int32_t areaHeight;
			StrategyViewNames const &names;
			uint8_t const *ownId;
			StrategyViewWorld const *world;
		};

		// ---- 75 Fan: FUN_005356b0 (vtable 0x5af728 +0x14): the lines at y 0, 16, 32, 48; a click on 16, 32, 48 asks for the number (a time is taken above 0 and below 1000, the wind from -1000 to 1000)
		void FanLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			Put(lines, 200, 0, c.name + " : Fan");                                                          // 0x5efe0c   [S-K75-1]
			Put(lines, 16, 16, Format("speed up time : %d  seconds", c.r.Seconds("speed_up_time")));        // 0x5efdf0
			Put(lines, 16, 32, Format("slow down time :%d  seconds", c.r.Seconds("slow_down_time")));       // 0x5efdd4
			Put(lines, 16, 48, Format("Wind strength :%d  (default is 100)", c.r.N("fan_wind_strength")));  // 0x5efdb0
		}

		bool FanClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (Row(y, 16))
			{
				*out = NumberOf("speed_up_time", "Speed up time (seconds)", r.Seconds("speed_up_time"), 1, 999);   // 0x5efd98   [S-K75-2]
			}
			else if (Row(y, 32))
			{
				*out = NumberOf("slow_down_time", "Slow down time (seconds)", r.Seconds("slow_down_time"), 1, 999);   // 0x5efd7c   [S-K75-2]
			}
			else if (Row(y, 48))
			{
				*out = NumberOf("fan_wind_strength", "Wind strength (-1000 to 1000)", r.N("fan_wind_strength"), -1000, 1000);   // 0x5efd5c   [S-K75-3]
			}
			else
			{
				return false;
			}

			return true;
		}

		// ---- 77 Volume of Death: FUN_00535970 (vtable 0x5af75c +0x14): the lines at y 0, 16, 32, 48 and "V : Edit Volume" at (100, height - 20); a click on 16 and 32 flips a bit of the flags word (obj +0x2c), on 48 the
		// damage (obj +0x30) is typed (FUN_00474340 takes a number from 0 up)
		void DeathLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			uint32_t const flags = (uint32_t)c.r.N("flags");
			int32_t const damage = c.r.N("damage");

			Put(lines, 200, 0, c.name + " : Volume of Death");                                                                            // 0x5efee8   [S-K77-1]
			Put(lines, 16, 16, (flags & 1) == 0 ? "Starts Inactive" : "Starts Active");                                                  // 0x5e4878, 0x5e43f8
			Put(lines, 16, 32, (flags & 2) == 0 ? "Only harmful on collision." : "Collision not required.");                             // 0x5efeb4, 0x5efed0
			Put(lines, 16, 48, damage == 0 ? std::string("Damage : Infinite (instant death)") : Format("Damage : %d/second", damage));   // 0x5efe7c, 0x5efea0
			Put(lines, 100, c.areaHeight - 20, "V : Edit Volume");                                                                       // 0x5efe6c
		}

		bool DeathClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (Row(y, 16))
			{
				*out = ToggleOf("flags", 0x1);
			}
			else if (Row(y, 32))
			{
				*out = ToggleOf("flags", 0x2);
			}
			else if (Row(y, 48))
			{
				*out = NumberOf("damage", "Type in the damage per second.\nType 0 for infinite damage.", r.N("damage"), 0, 2147483647);   // 0x5efe30   [S-K77-3]
			}
			else
			{
				return false;
			}

			return true;
		}

		StrategyViewClick PickOf(char const *field, char const *words)
		{
			StrategyViewClick click;

			click.action = StrategyViewClick::Pick;
			click.field = field;
			click.pickWords = words;
			return click;
		}
		// ---- 51 Lift (and 57 Airlock = flag 2, 60 = flag 4): FUN_0052c370 (vtable 0x5af380 +0x14; reassess/strategy-data-kinds.md S-K51-1..12). The positions are the list after NumAssocLifts (8-byte IDs at payload 24 + 8 i).
		bool LiftPositionsOf(std::vector<uint8_t> const &payload, std::vector<std::array<uint8_t, 8>> *ids, int32_t *count)
		{
			ids->clear();
			*count = 0;

			if (payload.size() < 24)
			{
				return false;
			}

			int32_t n = 0;

			memcpy(&n, payload.data() + 20, 4);
			*count = n;

			for (int32_t i = 0; i < n; i++)
			{
				size_t const at = 24 + 8 * (size_t)i;

				if (at + 8 > payload.size())
				{
					return false;   // a count the bytes do not hold: the list stops there (a hostile file)
				}

				std::array<uint8_t, 8> one;

				memcpy(one.data(), payload.data() + at, 8);
				ids->push_back(one);
			}

			return true;
		}

		// the rows of the Lift's view: r = the row of "Door :", n = the row of "Lift starts at"
		struct LiftRows
		{
			int32_t r, n;
			bool floorLines;
		};

		LiftRows LiftRowsOf(Reader const &reader)
		{
			std::vector<std::array<uint8_t, 8>> ids;
			int32_t count = 0;

			LiftPositionsOf(reader.payload, &ids, &count);

			LiftRows rows;

			rows.r = count == 0 ? 3 : (count < 0 ? 2 : 2 + count);   // 0x52c488-0x52c51d: a negative count draws nothing and keeps row 2
			rows.floorLines = (reader.N("LiftFlags") & 4) == 0;
			rows.n = rows.r + (rows.floorLines ? 6 : 4);
			return rows;
		}

		std::string LiftObjectName(Context const &c, uint8_t const *id)
		{
			std::string name;

			return c.names && c.names(id, &name) ? name : std::string("(null)");   // FUN_005297f0: no object -> a null string, which printf prints "(null)"
		}

		void LiftLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			uint32_t const flags = (uint32_t)c.r.N("LiftFlags");
			std::vector<std::array<uint8_t, 8>> ids;
			int32_t count = 0;

			LiftPositionsOf(c.r.payload, &ids, &count);

			LiftRows const rows = LiftRowsOf(c.r);
			auto radiusOf = [&c](uint8_t const *id) -> double
			{
				double r = 1.0;

				return c.world && c.world->radius && c.world->radius(id, &r) ? r : 1.0;   // 1.0 (double 0x5ab2b0) when the object is not in the level
			};

			Put(lines, 200, 0, c.name + ((flags & 2) != 0 ? " : Airlock" : ((flags & 4) != 0 ? " : Lift (switches don't teleport)" : " : Lift")));   // 0x5eea1c, 0x5ee9f8, 0x5ee9ec
			Put(lines, 200, c.areaHeight - 60, Format("Radius %f", c.ownId ? radiusOf(c.ownId) : 1.0));   // 0x5ee9e0
			Put(lines, 200, c.areaHeight - 40, Format("Floor %d", c.r.N("Floor")));                       // 0x5ee9d4
			Put(lines, 200, c.areaHeight - 20, Format("Facing %d", c.r.N("Facing")));                     // 0x5ee9c8
			Put(lines, 16, 16, "Lift positions :");                                                         // 0x5ee9b4

			if (count == 0)
			{
				Put(lines, 80, 32, "None");   // 0x5d126c
			}

			for (size_t i = 0; i < ids.size(); i++)
			{
				Put(lines, 80, 32 + 16 * (int32_t)i, Format("%s        R:%f", LiftObjectName(c, ids[i].data()).c_str(), radiusOf(ids[i].data())));   // 0x5ee9a4
			}

			uint8_t id[8];
			int32_t const r = rows.r;

			Put(lines, 16, 16 * r, "Door :");   // 0x5ee99c
			Put(lines, 80, 16 * (r + 1), c.r.Object("AssocDoor", id) ? LiftObjectName(c, id) : std::string("None"));
			Put(lines, 16, 16 * (r + 2), "Call Switch :");   // 0x5ee98c
			Put(lines, 80, 16 * (r + 3), c.r.Object("AssocCallSwitch", id) ? LiftObjectName(c, id) : std::string("None"));

			if (rows.floorLines)
			{
				Put(lines, 16, 16 * (r + 4), "Floor Switch :");   // 0x5ee97c
				Put(lines, 80, 16 * (r + 5), c.r.Object("AssocFloorSwitch", id) ? LiftObjectName(c, id) : std::string("None"));
			}

			// FUN_0052cb10: this lift when its flag 1 is set, else the first position whose strategy is a Lift with flag 1
			std::string start;

			if ((flags & 1) != 0)
			{
				start = c.name;
			}
			else
			{
				for (size_t i = 0; i < ids.size() && start.empty(); i++)
				{
					if (c.world && c.world->liftStartsHere && c.world->liftStartsHere(ids[i].data()))
					{
						start = LiftObjectName(c, ids[i].data());
					}
				}
			}

			Put(lines, 16, 16 * rows.n, start.empty() ? std::string("Lift has no start position") : "Lift starts at " + start);   // 0x5ee94c, 0x5ee968
			Put(lines, 16, 16 * (rows.n + 1), (flags & 8) != 0 ? "Exit opposite side" : "Exit same side");                    // 0x5ee938, 0x5ee928
		}

		bool LiftClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			LiftRows const rows = LiftRowsOf(r);
			int32_t const doorY = 16 * rows.r, callY = 16 * (rows.r + 2), floorY = 16 * (rows.r + 4), startY = 16 * rows.n, exitY = 16 * (rows.n + 1);

			if (y >= 16 && y < doorY)
			{
				// the positions block (the label included): the camera first goes to the position clicked (y >= 32, a list not empty), then the positions are collected (FUN_0052b360)
				out->action = StrategyViewClick::Collect;
				out->pickRow = kCollectLiftPositions;
				out->index = (y >= 32 && r.N("NumAssocLifts") != 0) ? (y - 32) / 16 : -1;
				return true;
			}

			if (y >= doorY && y < callY)
			{
				*out = PickOf("AssocDoor", "Select door for this lift position");   // 0x5ee8b4, FUN_0052bd70
				out->pickRow = kPickLiftDoor;
				return true;
			}

			if (y >= callY && y < floorY)
			{
				*out = PickOf("AssocCallSwitch", "Select call switch");   // 0x5ee8ec, FUN_0052bf80
				out->pickRow = kPickLiftCall;
				return true;
			}

			if (rows.floorLines && y >= floorY && y < startY)
			{
				*out = PickOf("AssocFloorSwitch", "Select floor switch");   // 0x5ee914, FUN_0052c170
				out->pickRow = kPickLiftFloor;
				return true;
			}

			if (y >= startY && y < startY + 16)
			{
				out->action = StrategyViewClick::Window;
				out->pickRow = kWindowLiftStart;
				return true;
			}

			if (y >= exitY && y < exitY + 16)
			{
				out->action = StrategyViewClick::Run;
				out->pickRow = kRunLiftExit;
				return true;
			}

			return false;
		}

		// ---- 54 Platform Lift: FUN_0052eec0 (vtable 0x5af3b4 +0x14): two lines, each a bit of the flags word (obj +0x14) flipped by a click on it
		void PlatformLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			uint32_t const flags = (uint32_t)c.r.N("flags");

			Put(lines, 200, 0, c.name + " : Platform Lift");                                                                                     // 0x5ef040   [S-K54-1]
			Put(lines, 16, 16, (flags & 1) == 0 ? "Starts enabled" : "Starts disabled");                                                        // 0x5ef020, 0x5ef030
			Put(lines, 16, 32, (flags & 2) == 0 ? "Once enabled , continues working until switched off" : "Becomes disabled after one use");   // 0x5eefcc, 0x5ef000
		}

		bool PlatformClick(Reader const &, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (Row(y, 16))
			{
				*out = ToggleOf("flags", 0x1);
			}
			else if (Row(y, 32))
			{
				*out = ToggleOf("flags", 0x2);
			}
			else
			{
				return false;
			}

			return true;
		}

		// ---- 50 Door: FUN_0052cbb0 (vtable 0x5af3e8 +0x14, read from the listing). The flags word (obj +0x14): 1 locked, 2 proximity door, 4 starts open (switch door), 8 switch activated ("lift door"), 0x10 horizontal; the open
		// time (+0x18) and the close time (+0x19) are bytes in tenths of a second. On entry a time of 0 is made 10 (1.0 s) and 5 (0.5 s). A door with neither bit 2 nor bit 8 is made a proximity door (bit 2 set) at once.
		bool DoorIsSwitch(uint32_t flags)
		{
			return (flags & 2) == 0 && (flags & 8) != 0;
		}

		void DoorLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			uint32_t const flags = (uint32_t)c.r.N("DoorFlags");
			int32_t row = 0;

			Put(lines, 200, 0, c.name + " : Door");                                                               // 0x5eeb6c   [S-K50-1]

			if (DoorIsSwitch(flags))
			{
				Put(lines, 16, 16, "Switch activated door ('lift door')");                                        // 0x5eeacc   [S-K50-4]
				Put(lines, 16, 32, (flags & 4) == 0 ? "Closed" : "Open");                                         // 0x5eeabc, 0x5eeac4
				row = 3;
			}
			else
			{
				Put(lines, 16, 16, "Proximity door");                                                             // 0x5eeb5c   [S-K50-3]
				Put(lines, 16, 32, (flags & 1) == 0 ? "Door is not locked" : "Door is locked");                   // 0x5eeb38, 0x5eeb4c
				Put(lines, 16, 48, (flags & 0x10) == 0 ? "Door is vertical (standard)" : "Door is horizontal (ie. in floor/ceiling)");   // 0x5eeaf0, 0x5eeb0c
				row = 4;
			}

			Put(lines, 16, row * 16, Format("Time take to open : %.1f seconds", c.r.N("TimeToOpen") * 0.1));       // 0x5eea98   [S-K50-5]
			Put(lines, 16, (row + 1) * 16, Format("Time take to close : %.1f seconds", c.r.N("TimeToClose") * 0.1));   // 0x5eea74
		}

		StrategyViewClick DoorTime(char const *field, char const *title, int32_t tenths)
		{
			StrategyViewClick click;

			click.action = StrategyViewClick::Number;
			click.field = field;
			click.title = title;
			click.initialText = Format("%g", tenths * 0.1);
			click.isFloat = true;
			click.factor = 10.0;
			click.bias = 0.01;       // 0x5ac438: added before the whole number is taken
			click.lowest = 1.0;      // 0x5ab2b0
			click.highest = 255.0;   // 0x5af910, below it
			click.highestIncluded = false;
			return click;
		}

		bool DoorClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			uint32_t const flags = (uint32_t)r.N("DoorFlags");
			bool const sw = DoorIsSwitch(flags);
			int32_t const row = sw ? 3 : 4;

			if (!sw && Row(y, 32))
			{
				*out = ToggleOf("DoorFlags", 0x1);   // [S-K50-7]
			}
			else if (sw && Row(y, 32))
			{
				*out = ToggleOf("DoorFlags", 0x4);   // [S-K50-8]
			}
			else if (!sw && Row(y, 48))
			{
				*out = ToggleOf("DoorFlags", 0x10);   // [S-K50-9]
			}
			else if (Row(y, 16))
			{
				// the first line switches the kind of door (0x52cdff-0x52ce55): bit 8 is tested FIRST: set -> bit 8 out, bit 2 in (a proximity door); clear -> bit 2 out, bit 8 in. A door
				// holding both bits (drawn as a proximity door) so becomes a proximity door with bit 2 only
				*out = SetOf("DoorFlags", (double)((flags & 8u) != 0 ? ((flags & ~8u) | 2u) : ((flags & ~2u) | 8u)));   // [S-K50-6]
			}
			else if (Row(y, row * 16))
			{
				*out = DoorTime("TimeToOpen", "Time take to open (0.1 - 25 sec)", r.N("TimeToOpen"));    // 0x5eea50   [S-K50-10]
			}
			else if (Row(y, (row + 1) * 16))
			{
				*out = DoorTime("TimeToClose", "Time take to close (0.1 - 25 sec)", r.N("TimeToClose"));   // 0x5eea2c   [S-K50-11]
			}
			else
			{
				return false;
			}

			return true;
		}

		// the name an object field shows after its label: the object's name, or nothing (0x5d126c) when the field names no object the level has
		std::string ObjectName(Context const &c, char const *field)
		{
			uint8_t id[8];
			std::string name;

			return c.r.Object(field, id) && !(c.ownId && memcmp(id, c.ownId, 8) == 0) && c.names && c.names(id, &name) ? name : std::string();
		}

		// the glyphs the small font draws for the codes 1 and 3 (a ticked and an empty box): ours has none, so a box in letters (NAMED)
		char const *Box(bool ticked)
		{
			return ticked ? "[x]" : "[ ]";
		}





		// ---- 59 Switch Door, as AvP draws it (reassess/strategy-data-kinds.md S-K59-1..3). FUN_0052d060: "%s : Switch door" (200, 0), "Associated door :" (16, 16) and, at (80, 32), "None" (0x5d126c) for the null ID or
		// the door's name (an ID no object has prints "(null)"). The "Switch Operated" box and the two time lines are drawn only when FUN_0057aa00(1) (the game number is 1; in AvP it is 0): NOT AVP, so not drawn,
		// and their y ranges stay 10000 (no click). A click on y 16 to 47 is the door pick (FUN_0052d490, "Select associated door").
		void SwitchDoorLinesAvp(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			uint8_t door[8];
			std::string name;

			Put(lines, 200, 0, c.name + " : Switch door");   // 0x5eec6c   [S-K59-1]
			Put(lines, 16, 16, "Associated door :");          // 0x5eec58
			Put(lines, 80, 32, !c.r.Object("AssocDoor", door) ? std::string("None") : (c.names && c.names(door, &name) ? name : std::string("(null)")));   // 0x5d126c; FUN_005297f0
		}

		bool SwitchDoorClickAvp(Reader const &, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (y < 16 || y >= 48)
			{
				return false;
			}

			*out = PickOf("AssocDoor", "Select associated door");   // 0x5eec90   [S-K59-2]
			out->pickRow = kPickSwitchDoor;
			return true;
		}



		// ---- 78 Self Destruct: FUN_00535cc0 (vtable 0x5af790 +0x14, from the listing): "Time : %d" (obj +0x14, seconds); a click on it asks "Time until destruction (seconds)" and keeps the whole number, a negative one as 0
		void SelfDestructLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			Put(lines, 200, 0, c.name + " : Self Destruct");                      // 0x5eff30   [S-K78-1]
			Put(lines, 16, 16, Format("Time : %d", c.r.N("timer")));              // 0x5eff24
		}

		bool SelfDestructClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (!Row(y, 16))
			{
				return false;
			}

			*out = NumberOf("timer", "Time until destruction (seconds)", r.N("timer"), 0, 32767);   // 0x5eff00   [S-K78-2]
			out->clampBelow = true;
			return true;
		}

		// ---- 73 Message: FUN_005336c0 (vtable 0x5af658 +0x14, from the listing): "Message starts deactivated" / "Message starts activated" (bit 1 of obj +0x18, flipped by a click), "String :%d" (obj +0x14) and, for a number from 1 to 450,
		// the text of that string of English.txt at (50, 48) (NOT built: the tools' table of the game's strings is not read); a click on or below the string line asks for the number in a dialog titled "Message" (a 500 x 400 window with a list in inteng)
		void MessageLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			Put(lines, 200, 0, c.name + " : Message");                                                                                      // 0x5ef998
			Put(lines, 16, 16, (c.r.N("flags") & 1) != 0 ? "Message starts deactivated" : "Message starts activated");                      // 0x5ef97c, 0x5ef960
			Put(lines, 16, 32, Format("String :%d", c.r.N("message_string")));                                                                // 0x5ef954
			// for 0 < n <= 450 the message's text at (50, 48): empty (the table is filled only from Rebellion's network share, S-K70-1), so nothing is drawn and the String rows end at 48 / 16 + 1 = row 4
		}

		bool MessageClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (Row(y, 16))
			{
				*out = ToggleOf("flags", 0x1);
			}
			else if (y >= 32 && y < 16 * (r.N("message_string") > 0 && r.N("message_string") <= 450 ? 4 : 3))
			{
				out->action = StrategyViewClick::Window;   // 0x5337f2: the 500 x 400 window (FUN_00532550) with the words "Message" (0x5edb98)
				out->pickRow = kWindowMessageText;
			}
			else
			{
				return false;
			}

			return true;
		}

		// S-KC-12 (FUN_00529eb0): the null ID -> "None" (0x5d126c); else the level's words for the object ("<name> - <word>", "(null)" for an ID no object has)
		std::string TargetWords(Context const &c, char const *field)
		{
			uint8_t id[8];

			if (!c.r.Object(field, id))
			{
				return "None";
			}

			return c.world && c.world->targetWords ? c.world->targetWords(id) : std::string("(null)");
		}

		// ---- 53, the base class (FUN_0052b150, vtable 0x5ac400 +0x14; S-KC-11): the black screen with "%s : Strategy Type %d" (0x5ee7fc) at (200, 0) and nothing else
		void BaseLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			Put(lines, 200, 0, Format("%s : Strategy Type %d", c.name.c_str(), c.r.kind));
		}

		bool NoClick(Reader const &, int32_t, int32_t, StrategyViewClick *)
		{
			return false;
		}

		// ---- 62 Placed Object: 0x530ee0-0x53104d (vtable 0x5ac3cc +0x14; S-K62-1..4). The request word (+0x28), the flags (+0x24: 1 destroyed, 2 picked up), the target (+0x2c)
		void PlacedLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			uint32_t const request = (uint32_t)c.r.N("target_request");
			uint32_t const flags = (uint32_t)c.r.N("flags");
			std::string const target = TargetWords(c, "targetID");

			Put(lines, 200, 0, c.name + (c.r.N("Type") == 16 ? " : Light Object" : " : Placed Object"));   // 0x5ef358, 0x5ef36c
			Put(lines, 100, c.areaHeight - 20, "E : Edit extra message data for target");                    // 0x5eee60
			Put(lines, 16, 16, "Target request details :");                                                   // 0x5ef33c
			Put(lines, 32, 32, (request & 1u) != 0 ? "Turn Target On" : "Turn Target Off");                   // 0x5ef32c, 0x5ef31c
			Put(lines, 32, 48, (request & ~1u) != 0 ? Format("Event Target : %s    (Has extra message data)", target.c_str()) : Format("Event Target : %s", target.c_str()));   // 0x5ef2ec, 0x5ef2d8
			Put(lines, 16, 64, "Events that cause target request to be sent to target :");                   // 0x5ef2a0
			Put(lines, 16, 80, Box((flags & 1u) != 0));                                                       // glyph 1 (ticked) / 3 (empty)
			Put(lines, 32, 80, "Object destroyed");                                                           // 0x5ef28c
			Put(lines, 16, 96, Box((flags & 2u) != 0));
			Put(lines, 32, 96, "Object picked up");                                                           // 0x5ef278
		}

		bool PlacedClick(Reader const &, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (Row(y, 32))
			{
				*out = ToggleOf("target_request", 0x1);   // 0x5310dc: bit 1 flips, the higher bits kept
			}
			else if (Row(y, 48))
			{
				*out = PickOf("targetID", "Select target");   // 0x5e4048, FUN_00531160
				out->pickRow = kPickTarget;
				out->requestField = "target_request";
			}
			else if (Row(y, 80))
			{
				*out = ToggleOf("flags", 0x1);   // 0x5310a8
			}
			else if (Row(y, 96))
			{
				*out = ToggleOf("flags", 0x2);
			}
			else
			{
				return false;
			}

			return true;
		}

		// ---- 67 Teleport: 0x530b50-0x530c09 (vtable 0x5af588 +0x14; S-K67-1..3). inteng shows the frame first and writes these with its on-screen message routine (FUN_0057bd40, its own font): ours draws them as
		// the other views' lines (NAMED: one font)
		void TeleportLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			uint8_t to[8];
			std::string name;
			int32_t const type = c.r.N("Type");
			char const *const words[4] = { "Teleports all", "Teleports marine", "Teleports alien", "Teleports predator" };   // 0x5ef1fc, 0x5ef1e8, 0x5ef1d8, 0x5ef1c4

			Put(lines, 200, 0, c.name + " : Teleport");   // 0x5ef238
			Put(lines, 16, 16, !c.r.Object("TeleportTo", to) ? std::string("Teleport to :not selected") : "Teleport to :" + (c.names && c.names(to, &name) ? name : std::string("(null)")));   // 0x5ef20c, 0x5ef228

			if (type >= 0 && type < 4)
			{
				Put(lines, 16, 32, words[type]);   // any other Type: no line
			}
		}

		bool TeleportClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (Row(y, 16))
			{
				*out = PickOf("TeleportTo", "Select teleport position");   // 0x5ef25c, FUN_00530ce0
				out->pickRow = kPickTeleport;
				return true;
			}

			if (y >= 32 && y <= 48)
			{
				*out = SetOf("Type", (double)((r.N("Type") + 1) % 4));   // 0x530c85: 32 to 48, 48 included; C's remainder (a negative Type stays negative)
				return true;
			}

			return false;
		}

		// ---- 70 Mission Objective: 0x532820-0x532b6c (vtable 0x5af624 +0x14; S-K70-1). The texts of the two message numbers come from inteng's table at 0x913400, which FUN_0053bc30 fills from
		// "\\bob\vss\avp\avp_lang\english.txt" (0x6020b0: Rebellion's own network share; on any other machine the file is not there and every text is empty). Ours never reads a network path (security) and its
		// texts are empty too: an empty text draws nothing and ends at the y it was given, so the next row is that y / 16 + 1.
		struct MissionRows
		{
			int32_t completed, order, shown, possible, special, alterations;
		};

		MissionRows MissionRowsOf(Reader const &r)
		{
			int32_t const description = r.N("mission_description_string");
			int32_t const completed = r.N("mission_complete_string");
			MissionRows rows;

			rows.completed = description > 0 && completed <= 450 ? 32 / 16 + 1 : 2;   // 0x5328c4: the upper bound is read from the COMPLETED number (inteng's slip, kept)
			rows.order = rows.completed + 1 + (completed > 0 && completed <= 450 ? 1 : 0);
			rows.shown = rows.order + 1;
			rows.possible = rows.order + 2;
			rows.special = rows.order + 3;
			rows.alterations = rows.order + 4;
			return rows;
		}

		bool MissionTargetsOf(std::vector<uint8_t> const &payload, std::vector<MissionTarget> *targets)
		{
			targets->clear();

			if (payload.size() < 44)
			{
				return false;
			}

			int32_t count = 0;

			memcpy(&count, payload.data() + 40, 4);

			if (count < 0 || (size_t)count > (payload.size() - 44) / 12)
			{
				return false;   // a count the bytes do not hold (a hostile file): no list
			}

			for (int32_t i = 0; i < count; i++)
			{
				MissionTarget one;

				memcpy(one.id.data(), payload.data() + 44 + 12 * (size_t)i, 8);
				memcpy(&one.word, payload.data() + 52 + 12 * (size_t)i, 4);
				targets->push_back(one);
			}

			return true;
		}

		void MissionLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			MissionRows const rows = MissionRowsOf(c.r);
			uint32_t const flags = (uint32_t)c.r.N("flags");
			int32_t const special = c.r.N("mission_completion_effect");
			std::vector<MissionTarget> targets;

			MissionTargetsOf(c.r.payload, &targets);
			Put(lines, 200, 0, c.name + " : Mission Objective");                                       // 0x5ef844
			Put(lines, 100, c.areaHeight - 40, "P : Toggle 'Make Possible' setting for mission target");   // 0x5ef80c
			Put(lines, 100, c.areaHeight - 20, "V : Toggle 'Make Visible' setting for mission target");    // 0x5ef7d4
			Put(lines, 16, 16, Format("Description :%d", c.r.N("mission_description_string")));         // 0x5ef7c4
			Put(lines, 16, 16 * rows.completed, Format("Completed Message :%d", c.r.N("mission_complete_string")));   // 0x5ef7ac
			Put(lines, 16, 16 * rows.order, Format("Order Number :%d", c.r.N("mission_number")));       // 0x5ef798
			Put(lines, 16, 16 * rows.shown, (flags & 1u) != 0 ? "Shown at start" : "Hidden at start ");  // 0x5ef788, 0x5ef774 (its trailing space)
			Put(lines, 16, 16 * rows.possible, (flags & 2u) != 0 ? "Possible at start" : "Not possible at start");   // 0x5ef760, 0x5ef748
			Put(lines, 16, 16 * rows.special, special == 0 ? "Special :Nothing" : (special == 1 ? "Special :Complete level" : "Special :Unknown"));   // 0x5ef708, 0x5ef71c, 0x5ef734
			Put(lines, 16, 16 * rows.alterations, "Mission alterations upon completion :");             // 0x5ef6e0

			if (targets.empty())
			{
				Put(lines, 32, 16 * (rows.alterations + 1), "None");   // 0x5d126c
			}

			for (size_t i = 0; i < targets.size(); i++)
			{
				std::string name;
				std::string line = (c.names && c.names(targets[i].id.data(), &name) ? name : std::string("(null)")) + " ";   // "%s " (0x5ef6dc), FUN_005297f0

				line += (targets[i].word & 1) != 0 ? "(make visible)" : "";    // 0x5ef6cc
				line += (targets[i].word & 2) != 0 ? "(make possible)" : "";   // 0x5ef6bc
				Put(lines, 32, 16 * (rows.alterations + 1 + (int32_t)i), line);
			}
		}

		bool MissionClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			MissionRows const rows = MissionRowsOf(r);
			std::vector<MissionTarget> targets;

			MissionTargetsOf(r.payload, &targets);

			if (y >= 16 && y < 16 * rows.order)
			{
				// 0x532bd8 / 0x532c38: the Description rows, then the Completed rows: the text window (FUN_00532550)
				out->action = StrategyViewClick::Window;
				out->pickRow = kWindowMissionText;
				out->index = y < 16 * rows.completed ? 0 : 1;
				return true;
			}

			if (Row(y, 16 * rows.order))
			{
				*out = NumberOf("mission_number", "Mission Order Number", r.N("mission_number"), -2147483647 - 1, 2147483647);   // 0x5ef67c: no check
				return true;
			}

			if (Row(y, 16 * rows.shown))
			{
				*out = ToggleOf("flags", 0x1);
				return true;
			}

			if (Row(y, 16 * rows.possible))
			{
				*out = ToggleOf("flags", 0x2);
				return true;
			}

			if (Row(y, 16 * rows.special))
			{
				*out = SetOf("mission_completion_effect", (double)((r.N("mission_completion_effect") + 1) % 2));   // 0x532d46: C's remainder
				return true;
			}

			if (y >= 16 * rows.alterations)
			{
				int32_t const a = 16 * rows.alterations;
				int32_t const index = y >= a + 16 && !targets.empty() ? (y - a - 16) / 16 : -1;

				out->action = StrategyViewClick::Collect;
				out->pickRow = kCollectMissionTargets;
				out->index = index >= 0 && (size_t)index < targets.size() ? index : -1;
				return true;
			}

			return false;
		}

		// ---- 72 Track Object and 80 Track Object (Destructable): FUN_005345d0 (vtables 0x5af68c / 0x5af6c0 +0x14; S-K72-1..7, S-K80-1). The point effects in the file (the game's ReadPointEffects): the count at
		// +20, then per effect its point, a pad word, its target count and 16 bytes per target (ID, request word, direction word); then the kind's own words. The page shown is the editor's (world->page:
		// -1 the main page, else the point whose page is open).
		bool TrackEffectsOf(std::vector<uint8_t> const &payload, std::vector<TrackEffect> *effects, size_t *end)
		{
			effects->clear();

			if (payload.size() < 24)
			{
				return false;
			}

			int32_t count = 0;
			size_t at = 24;

			memcpy(&count, payload.data() + 20, 4);

			if (count < 0 || (size_t)count > (payload.size() - 24) / 12)
			{
				return false;   // a count the bytes do not hold (a hostile file)
			}

			for (int32_t i = 0; i < count; i++)
			{
				TrackEffect effect;
				int32_t targets = 0;

				if (payload.size() - at < 12)
				{
					return false;
				}

				memcpy(&effect.point, payload.data() + at, 4);
				memcpy(&effect.pad, payload.data() + at + 4, 4);
				memcpy(&targets, payload.data() + at + 8, 4);
				at += 12;

				if (targets < 0 || (size_t)targets > (payload.size() - at) / 16)
				{
					return false;
				}

				for (int32_t j = 0; j < targets; j++)
				{
					TrackTarget target;

					memcpy(target.id.data(), payload.data() + at, 8);
					memcpy(&target.request, payload.data() + at + 8, 4);
					memcpy(&target.direction, payload.data() + at + 12, 4);
					effect.targets.push_back(target);
					at += 16;
				}

				effects->push_back(effect);
			}

			*end = at;
			return true;
		}

		TrackEffect const *TrackEffectAt(std::vector<TrackEffect> const &effects, int32_t point)
		{
			for (TrackEffect const &one : effects)
			{
				if (one.point == point)
				{
					return &one;
				}
			}

			return nullptr;
		}

		std::string TrackTargetName(Context const &c, uint8_t const *id)
		{
			static uint8_t const none[8] = { 0 };

			if (memcmp(id, none, 8) == 0)
			{
				return "None";
			}

			return c.world && c.world->targetWords ? c.world->targetWords(id) : std::string("(null)");
		}

		void TrackLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			std::vector<TrackEffect> effects;
			size_t end = 0;
			int32_t const sections = c.world && c.world->trackSections && c.ownId ? c.world->trackSections(c.ownId) : 0;
			int32_t const page = c.world ? c.world->page : -1;

			TrackEffectsOf(c.r.payload, &effects, &end);
			Put(lines, 200, 0, c.name + (c.r.kind == 80 ? " : Track (Destructable)" : " : Track"));   // 0x5efc94, 0x5efc88
			Put(lines, 16, 16, Format("Track sections :%d", sections));                                // 0x5efc74
			Put(lines, 100, c.areaHeight - 40, "E : Edit extra message data for target");             // 0x5eee60
			Put(lines, 100, c.areaHeight - 20, "P : Add new special track point");                    // 0x5efc54

			TrackEffect const *const open = page >= 0 ? TrackEffectAt(effects, page) : nullptr;

			if (open)
			{
				// the point page (0x534ab7-0x534bcf)
				Put(lines, 16, 32, Format("Point %d targets :", open->point));   // 0x5efb6c

				if (open->targets.empty())
				{
					Put(lines, 32, 48, "None :");   // 0x5efa88
				}

				for (size_t j = 0; j < open->targets.size(); j++)
				{
					TrackTarget const &t = open->targets[j];
					int32_t const y = 48 + 16 * (int32_t)j;
					std::string const name = TrackTargetName(c, t.id.data());
					uint32_t const d = (uint32_t)t.direction;

					Put(lines, 0, y, (t.request & 1) != 0 ? "(on)" : "(off)");   // 0x5ef0d4, 0x5ef0cc
					Put(lines, 50, y, (d & 1u) != 0 && (d & 2u) != 0 ? ((d & 4u) != 0 ? "(Opposite back)" : "(Both directions)") : ((d & 1u) != 0 ? "(Forward only)" : "(Backward only)"));   // 0x5efac4, 0x5efab0, 0x5efaa0, 0x5efa90
					Put(lines, 190, y, ((uint32_t)t.request & ~1u) != 0 ? name + "  (Has extra message data)" : name);   // 0x5ef0ac
				}

				return;
			}

			// the main page (0x53460e-0x53481f)
			Put(lines, 16, 32, "Point Effects :");   // 0x5efc44

			if (effects.empty())
			{
				Put(lines, 32, 48, "None");   // 0x5d126c
			}

			for (size_t i = 0; i < effects.size(); i++)
			{
				Put(lines, 32, 48 + 16 * (int32_t)i, Format("Point %d : %d targets", effects[i].point, (int32_t)effects[i].targets.size()));   // 0x5efc2c
			}

			if (c.r.kind == 80)
			{
				int32_t const r = effects.empty() ? 4 : 3 + (int32_t)effects.size();
				uint32_t const request = (uint32_t)c.r.N("target_request");
				std::string const target = TargetWords(c, "targetID");

				Put(lines, 16, 16 * r, Format("Integrity : %d", c.r.N("integrity")));        // 0x5efc1c
				Put(lines, 16, 16 * (r + 1), "Destruct target request details :");             // 0x5efbf8
				Put(lines, 32, 16 * (r + 2), (request & 1u) != 0 ? "Turn Target On" : "Turn Target Off");
				Put(lines, 32, 16 * (r + 3), (request & ~1u) != 0 ? Format("Destruct Target : %s    (Has extra message data)", target.c_str()) : Format("Destruct Target : %s", target.c_str()));   // 0x5efbc4, 0x5efbac
			}
		}

		int32_t clickPage = -1;   // the page the click is on (StrategyView_Click's page)

		bool TrackClick(Reader const &r, int32_t x, int32_t y, StrategyViewClick *out)
		{
			std::vector<TrackEffect> effects;
			size_t end = 0;

			TrackEffectsOf(r.payload, &effects, &end);

			TrackEffect const *const open = clickPage >= 0 ? TrackEffectAt(effects, clickPage) : nullptr;

			if (open)
			{
				// 0x534c21-0x534ce9: y from the page's top row 48; below the last row (16 x (3 + count), 64 with none) nothing
				int32_t const count = (int32_t)open->targets.size();
				int32_t const limit = 16 * (count == 0 ? 4 : 3 + count);
				int32_t const j = y >= 48 ? (y - 48) / 16 : -1;

				if (y >= limit)
				{
					return false;
				}

				out->index = j;

				if (j >= 0 && j < count)
				{
					out->action = StrategyViewClick::Run;
					out->pickRow = x < 50 ? kRunTrackRequest : (x < 190 ? kRunTrackDirection : kCollectTrackTargets);
					out->action = out->pickRow == kCollectTrackTargets ? StrategyViewClick::Collect : StrategyViewClick::Run;
					return true;
				}

				out->action = StrategyViewClick::Collect;
				out->pickRow = kCollectTrackTargets;
				out->index = -1;
				return true;
			}

			int32_t const count = (int32_t)effects.size();
			int32_t const rows = count == 0 ? 4 : 3 + count;

			if (y >= 32 && y < 16 * rows)
			{
				int32_t const index = (y - 32) / 16;

				if (index == 0 || index > count)
				{
					// the label or past the effects ("None"): "Which track point?" (0x5efb98) holding 0
					out->action = StrategyViewClick::Window;
					out->pickRow = kWindowTrackPoint;
					out->index = -1;
					return true;
				}

				out->action = StrategyViewClick::Window;
				out->pickRow = kOpenTrackPage;
				out->index = effects[(size_t)index - 1].point;
				return true;
			}

			if (r.kind == 80)
			{
				if (Row(y, 16 * rows))
				{
					*out = NumberOf("integrity", "Integrity for object", r.N("integrity"), 0, 2147483647);   // 0x5efb80: a negative number is 0
					out->clampBelow = true;
					return true;
				}

				if (Row(y, 16 * (rows + 2)))
				{
					*out = ToggleOf("target_request", 0x1);
					return true;
				}

				if (Row(y, 16 * (rows + 3)))
				{
					*out = PickOf("targetID", "Select destruct target");   // 0x5efd08, FUN_005353c0
					out->pickRow = kPickTarget;
					out->requestField = "target_request";
					return true;
				}
			}

			return false;
		}

		// ---- 52 Binary Switch: FUN_0052d700 (vtable +0x14; strategy-data.md S-K52-1, S-K52-2, S-K52-4, S-K52-8; re-read 11-16 on dump-full-3:64723 and listing-all-3 0x52d7d5-0x52d8aa).
		// The words by FUN_0057bd90(x, y, 1, ...) each pass: the header, the editable foot (E, P, T at h - 0x3c / 0x28 / 0x14), Rest State, the mode, the switch-off line for modes 0-2, the target
		void BinaryLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			uint32_t const flags = (uint32_t)c.r.N("flags");                                             // +0x14
			int32_t const mode = c.r.N("Mode");                                                          // +0x18
			double const seconds = (double)((float)c.r.N("Time") * 1.5258789e-05f);                     // 0x52d802: (float)+0x1c times 1 / 65536
			uint32_t const request = (uint32_t)c.r.N("request");                                         // +0x2c
			std::string const target = TargetWords(c, "Target");                                         // FUN_00529eb0 (S-KC-12)

			Put(lines, 200, 0, c.name + " : Binary Switch");                                             // 0x5eee88
			Put(lines, 100, c.areaHeight - 0x3c, "E : Edit extra message data for target");              // 0x5eee60
			Put(lines, 100, c.areaHeight - 0x28, "P : Change Properties");                               // 0x5eee48
			Put(lines, 100, c.areaHeight - 0x14, "T : Change Target (+Ctrl to choose environment)");     // 0x5eee18
			Put(lines, 16, 0x20, (flags & 1u) != 0 ? "Rest State : On" : "Rest State : Off");            // 0x52d7d5: 0x5eedf8 / 0x5eede4

			switch (mode)                                                                                // 0x52d7f0
			{
				case 0: Put(lines, 16, 0x30, Format("Mode : Timer - reset after %.3f seconds", seconds)); break;                // 0x5eedbc
				case 1: Put(lines, 16, 0x30, "Mode : Wait"); break;                                                              // 0x5eedb0
				case 2: Put(lines, 16, 0x30, "Mode : Toggle"); break;                                                            // 0x5eeda0
				case 4: Put(lines, 16, 0x30, Format("Mode : Time Delay -%.3f seconds", seconds)); break;                        // 0x5eed80
				case 5: Put(lines, 16, 0x30, Format("Mode : Time Delay -%.3f seconds (starts immediately)", seconds)); break;   // 0x5eed48
				default: break;                                                                                                  // 3 (and any other): no line
			}

			if (mode == 0 || mode == 1 || mode == 2)                                                     // 0x52d87a-0x52d8aa: & 2 first, then & 4
			{
				Put(lines, 16, 0x40, (flags & 2u) != 0 ? "Switch off message : Same" : ((flags & 4u) != 0 ? "Switch off message : None" : "Switch off message : Reversed (default)"));
			}

			Put(lines, 16, 0x50, (request & ~1u) != 0 ? Format("Target : %s  (Has extra message data)", target.c_str()) : Format("Target : %s", target.c_str()));   // 0x5eecc0 / 0x5e4758
		}

		// 0x52d9ee: T (slot 0xd87a94) or a left press on y 0x50..0x5f: the target pick FUN_0052db10 (S-K52-4, S-K52-7). (P / a press on y 0x20..0x4f: the Properties form, S-K52-3; E: S-K52-5)
		bool BinaryClick(Reader const &, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (y >= 0x50 && y <= 0x5f)
			{
				*out = PickOf("Target", "Select target for switch");   // 0x5eeec0 at (200, h - 0x14)
				out->pickRow = kPickSwitchTarget;
				out->requestField = "request";
				return true;
			}

			return false;
		}

		// ---- 69 Enemy: FUN_005316a0 (S-K69-1..5). The species is the enemy's AVPGENER type (7 marine, 2 predator, the others one mission only); Mission +20, request +24, death target +28,
		// ExtraMissionData +36 (the path's number in its low 16 bits for a path follower)
		int32_t clickSpecies = -1;

		bool EnemyFollowsAPath(int32_t species, int32_t mission)
		{
			return (species == 7 && mission == 5) || (species == 2 && mission == 1);
		}

		std::string EnemyMissionWords(int32_t species, int32_t mission)
		{
			static char const *const marine[] = { "Mission : Wait then wander (default)", "Mission : Wander", "Mission : Guard", "Mission : Noncombatant", "Mission : Local Guard", "Mission : Path follower" };   // 0x5ef5a0 .. 0x5ef534

			if (species == 7)
			{
				return mission >= 0 && mission <= 5 ? marine[mission] : "Mission : Unknown Mission Type";   // 0x5ef514
			}

			if (species == 2)
			{
				return mission == 0 ? "Mission : Wait then wander (default)" : mission == 1 ? "Mission : Path follower" : "Mission : Unknown Mission Type";
			}

			return "Mission : Wait then wander (Only option)";   // 0x5ef4b0
		}

		// the path line (FUN_004fbaf0 finds the path whose number is n): "Path - %d  %s" (0x5ef504), "Path - %d" when it has no name (0x5ef4f8), "Path - %d  Doesn't exist" (0x5ef4dc)
		std::string PathWords(Context const &c, int32_t number)
		{
			std::string name;

			if (!c.world || !c.world->path || !c.world->path(number, &name))
			{
				return Format("Path - %d  Doesn't exist", number);
			}

			return name.empty() ? Format("Path - %d", number) : Format("Path - %d  %s", number, name.c_str());
		}

		void EnemyLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			int32_t const species = c.world && c.world->species && c.ownId ? c.world->species(c.ownId) : -1;
			int32_t const mission = c.r.N("MissionType");
			uint32_t const request = (uint32_t)c.r.N("request");
			std::string const target = TargetWords(c, "DeathTarget");
			int32_t r = 2;

			Put(lines, 200, 0, c.name + " : Enemy");                                                    // 0x5ef5c8
			Put(lines, 100, c.areaHeight - 20, "E : Edit extra message data for target");               // 0x5eee60
			Put(lines, 16, 16, EnemyMissionWords(species, mission));

			if (EnemyFollowsAPath(species, mission))
			{
				Put(lines, 16, 32, PathWords(c, c.r.N("ExtraMissionData") & 0xffff));
				r = 3;
			}

			Put(lines, 16, 16 * r, (request & 1u) != 0 ? "Turn Death Target On" : "Turn Death Target Off");   // 0x5ef498, 0x5ef480
			Put(lines, 16, 16 * (r + 1), (request & ~1u) != 0 ? Format("Death Target : %s  (Has extra message data)", target.c_str()) : Format("Death Target : %s", target.c_str()));   // 0x5ef454, 0x5ef440
		}

		bool EnemyClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			int32_t const mission = r.N("MissionType");
			int32_t const row = EnemyFollowsAPath(clickSpecies, mission) ? 3 : 2;

			if (Row(y, 16))
			{
				// 0x531989-0x5319fc: a marine's or a predator's window; any other species: nothing
				if (clickSpecies != 7 && clickSpecies != 2)
				{
					return false;
				}

				out->action = StrategyViewClick::Window;
				out->pickRow = kWindowEnemyMission;
			}
			else if (row == 3 && Row(y, 32))
			{
				out->action = StrategyViewClick::Window;   // 0x531a45: "Select Path"
				out->pickRow = kWindowSelectPath;
			}
			else if (Row(y, 16 * row))
			{
				*out = ToggleOf("request", 0x1);   // 0x531a0e
			}
			else if (Row(y, 16 * (row + 1)))
			{
				*out = PickOf("DeathTarget", "Select target");   // 0x531a38, FUN_00531b70, 0x5e4048
				out->pickRow = kPickTarget;
				out->requestField = "request";
			}
			else
			{
				return false;
			}

			return true;
		}

		// ---- 79 Generator: FUN_00531d40 (S-K79-1..3): Mission +20 (0 Hunt, 1 Path follower), ExtraMissionData +24; no E line and no target
		void GeneratorLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			int32_t const mission = c.r.N("MissionType");

			Put(lines, 200, 0, c.name + " : Generator");                                                // 0x5ef600
			Put(lines, 16, 16, mission == 0 ? "Mission : Hunt (default)" : mission == 1 ? "Mission : Path follower" : "Mission : Unknown Mission Type");   // 0x5ef5e4, 0x5ef534, 0x5ef514

			if (mission == 1)
			{
				Put(lines, 16, 32, PathWords(c, c.r.N("ExtraMissionData") & 0xffff));
			}
		}

		bool GeneratorClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (Row(y, 16))
			{
				out->action = StrategyViewClick::Window;   // 0x531e93: FUN_005323c0
				out->pickRow = kWindowGeneratorMission;
			}
			else if (r.N("MissionType") == 1 && Row(y, 32))
			{
				out->action = StrategyViewClick::Window;   // 0x531ecb: "Select Path"
				out->pickRow = kWindowSelectPath;
			}
			else
			{
				return false;
			}

			return true;
		}

		struct KindView
		{
			int32_t kind;
			void (*lines)(Context const &, std::vector<StrategyViewLine> *);
			bool (*click)(Reader const &, int32_t, int32_t, StrategyViewClick *);
		};

		KindView const kViews[] =
		{
			{ 52, BinaryLines, BinaryClick },
			{ 50, DoorLines, DoorClick },
			{ 51, LiftLines, LiftClick },
			{ 54, PlatformLines, PlatformClick },
			{ 59, SwitchDoorLinesAvp, SwitchDoorClickAvp },
			{ 62, PlacedLines, PlacedClick },
			{ 70, MissionLines, MissionClick },
			{ 72, TrackLines, TrackClick },
			{ 80, TrackLines, TrackClick },
			{ 67, TeleportLines, TeleportClick },
			{ 53, BaseLines, NoClick },
			{ 75, FanLines, FanClick },
			{ 73, MessageLines, MessageClick },
			{ 77, DeathLines, DeathClick },
			{ 78, SelfDestructLines, SelfDestructClick },
			{ 69, EnemyLines, EnemyClick },
			{ 79, GeneratorLines, GeneratorClick },
		};

		KindView const *ViewOf(int32_t kind)
		{
			for (KindView const &one : kViews)
			{
				if (one.kind == kind)
				{
					return &one;
				}
			}

			return nullptr;
		}
	}

	bool StrategyView_TrackEffects(std::vector<uint8_t> const &payload, std::vector<TrackEffect> *effects)
	{
		size_t end = 0;

		return TrackEffectsOf(payload, effects, &end);
	}

	bool StrategyView_TrackWith(std::vector<uint8_t> const &payload, std::vector<TrackEffect> const &effects, std::vector<uint8_t> *out)
	{
		std::vector<TrackEffect> old;
		size_t end = 0;

		if (!TrackEffectsOf(payload, &old, &end) || effects.size() > 4096)
		{
			return false;
		}

		int32_t const count = (int32_t)effects.size();

		out->assign(payload.begin(), payload.begin() + 20);
		out->insert(out->end(), (uint8_t const *)&count, (uint8_t const *)&count + 4);

		for (TrackEffect const &e : effects)
		{
			int32_t const targets = (int32_t)e.targets.size();

			out->insert(out->end(), (uint8_t const *)&e.point, (uint8_t const *)&e.point + 4);
			out->insert(out->end(), (uint8_t const *)&e.pad, (uint8_t const *)&e.pad + 4);
			out->insert(out->end(), (uint8_t const *)&targets, (uint8_t const *)&targets + 4);

			for (TrackTarget const &t : e.targets)
			{
				out->insert(out->end(), t.id.begin(), t.id.end());
				out->insert(out->end(), (uint8_t const *)&t.request, (uint8_t const *)&t.request + 4);
				out->insert(out->end(), (uint8_t const *)&t.direction, (uint8_t const *)&t.direction + 4);
			}
		}

		out->insert(out->end(), payload.begin() + (std::ptrdiff_t)end, payload.end());
		return true;
	}

	bool StrategyView_MissionTargets(std::vector<uint8_t> const &payload, std::vector<MissionTarget> *targets)
	{
		return MissionTargetsOf(payload, targets);
	}

	bool StrategyView_MissionTargetAt(std::vector<uint8_t> const &payload, int32_t y, size_t *index)
	{
		Reader const reader = { payload, 70 };
		MissionRows const rows = MissionRowsOf(reader);
		std::vector<MissionTarget> targets;
		int32_t const a = 16 * rows.alterations;

		if (!MissionTargetsOf(payload, &targets) || targets.empty() || y < a + 16)
		{
			return false;
		}

		size_t const at = (size_t)((y - a - 16) / 16);

		if (at >= targets.size())
		{
			return false;
		}

		*index = at;
		return true;
	}

	bool StrategyView_LiftPositions(std::vector<uint8_t> const &payload, std::vector<std::array<uint8_t, 8>> *ids)
	{
		int32_t count = 0;

		return LiftPositionsOf(payload, ids, &count);
	}

	bool StrategyView_FieldObject(int32_t kind, std::vector<uint8_t> const &payload, char const *field, uint8_t *id)
	{
		Reader const reader = { payload, kind };

		return reader.Object(field, id);
	}

	bool StrategyView_FieldNumber(int32_t kind, std::vector<uint8_t> const &payload, char const *field, int32_t *value)
	{
		Reader const reader = { payload, kind };

		return reader.Number(field, value);
	}

	bool StrategyView_Has(int32_t kind)
	{
		return ViewOf(kind) != nullptr;
	}

	bool StrategyView_Lines(int32_t kind, std::vector<uint8_t> const &payload, std::string const &name, int32_t areaHeight, StrategyViewNames const &names, uint8_t const *ownId, std::vector<StrategyViewLine> *lines,
	                        StrategyViewWorld const *world)
	{
		KindView const *const view = ViewOf(kind);

		if (!view || !lines)
		{
			return false;
		}

		Reader const reader = { payload, kind };
		Context const context = { reader, name, areaHeight, names, ownId, world };

		view->lines(context, lines);
		return true;
	}

	void StrategyView_OnOpen(int32_t kind, std::vector<uint8_t> const &payload, std::vector<StrategyViewFix> *fixes)
	{
		Reader const reader = { payload, kind };

		if (kind == 50)
		{
			// FUN_0052cbb0 0x52cbd1: a time of 0 is made 10 and 5 (tenths); the loop's first pass sets bit 2 on a door that is neither (0x52ce08)
			uint32_t const flags = (uint32_t)reader.N("DoorFlags");

			if (reader.N("TimeToOpen") == 0)
			{
				StrategyViewFix fix = { "TimeToOpen", 10.0 };

				fixes->push_back(fix);
			}

			if (reader.N("TimeToClose") == 0)
			{
				StrategyViewFix fix = { "TimeToClose", 5.0 };

				fixes->push_back(fix);
			}

			if ((flags & 2) == 0 && (flags & 8) == 0)
			{
				StrategyViewFix fix = { "DoorFlags", (double)(flags | 2u) };

				fixes->push_back(fix);
			}
		}
	}

	bool StrategyView_Click(int32_t kind, std::vector<uint8_t> const &payload, int32_t x, int32_t y, StrategyViewClick *out, int32_t page, int32_t species)
	{
		clickPage = page;
		clickSpecies = species;

		KindView const *const view = ViewOf(kind);

		if (!view || !out)
		{
			return false;
		}

		Reader const reader = { payload, kind };

		return view->click(reader, x, y, out);
	}
}
