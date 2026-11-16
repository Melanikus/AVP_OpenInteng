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
		};

		// ---- 75 Fan: FUN_005356b0 (vtable 0x5af728 +0x14): the lines at y 0, 16, 32, 48; a click on 16, 32, 48 asks for the number (a time is taken above 0 and below 1000, the wind from -1000 to 1000)
		void FanLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			Put(lines, 200, 0, c.name + " : Fan");                                                          // 0x5efe0c
			Put(lines, 16, 16, Format("speed up time : %d  seconds", c.r.Seconds("speed_up_time")));        // 0x5efdf0
			Put(lines, 16, 32, Format("slow down time :%d  seconds", c.r.Seconds("slow_down_time")));       // 0x5efdd4
			Put(lines, 16, 48, Format("Wind strength :%d  (default is 100)", c.r.N("fan_wind_strength")));  // 0x5efdb0
		}

		bool FanClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (Row(y, 16))
			{
				*out = NumberOf("speed_up_time", "Speed up time (seconds)", r.Seconds("speed_up_time"), 1, 999);   // 0x5efd98
			}
			else if (Row(y, 32))
			{
				*out = NumberOf("slow_down_time", "Slow down time (seconds)", r.Seconds("slow_down_time"), 1, 999);   // 0x5efd7c
			}
			else if (Row(y, 48))
			{
				*out = NumberOf("fan_wind_strength", "Wind strength (-1000 to 1000)", r.N("fan_wind_strength"), -1000, 1000);   // 0x5efd5c
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

			Put(lines, 200, 0, c.name + " : Volume of Death");                                                                            // 0x5efee8
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
				*out = NumberOf("damage", "Type in the damage per second.\nType 0 for infinite damage.", r.N("damage"), 0, 2147483647);   // 0x5efe30
			}
			else
			{
				return false;
			}

			return true;
		}

		// ---- 54 Platform Lift: FUN_0052eec0 (vtable 0x5af3b4 +0x14): two lines, each a bit of the flags word (obj +0x14) flipped by a click on it
		void PlatformLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			uint32_t const flags = (uint32_t)c.r.N("flags");

			Put(lines, 200, 0, c.name + " : Platform Lift");                                                                                     // 0x5ef040
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

			Put(lines, 200, 0, c.name + " : Door");                                                               // 0x5eeb6c

			if (DoorIsSwitch(flags))
			{
				Put(lines, 16, 16, "Switch activated door ('lift door')");                                        // 0x5eeacc
				Put(lines, 16, 32, (flags & 4) == 0 ? "Closed" : "Open");                                         // 0x5eeabc, 0x5eeac4
				row = 3;
			}
			else
			{
				Put(lines, 16, 16, "Proximity door");                                                             // 0x5eeb5c
				Put(lines, 16, 32, (flags & 1) == 0 ? "Door is not locked" : "Door is locked");                   // 0x5eeb38, 0x5eeb4c
				Put(lines, 16, 48, (flags & 0x10) == 0 ? "Door is vertical (standard)" : "Door is horizontal (ie. in floor/ceiling)");   // 0x5eeaf0, 0x5eeb0c
				row = 4;
			}

			Put(lines, 16, row * 16, Format("Time take to open : %.1f seconds", c.r.N("TimeToOpen") * 0.1));       // 0x5eea98
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
				*out = ToggleOf("DoorFlags", 0x1);
			}
			else if (sw && Row(y, 32))
			{
				*out = ToggleOf("DoorFlags", 0x4);
			}
			else if (!sw && Row(y, 48))
			{
				*out = ToggleOf("DoorFlags", 0x10);
			}
			else if (Row(y, 16))
			{
				// the first line switches the kind of door: a switch door becomes a proximity door (bit 8 out, bit 2 in), a proximity door a switch door (bit 2 out, bit 8 in)
				*out = SetOf("DoorFlags", (double)(sw ? ((flags & ~8u) | 2u) : ((flags & ~2u) | 8u)));
			}
			else if (Row(y, row * 16))
			{
				*out = DoorTime("TimeToOpen", "Time take to open (0.1 - 25 sec)", r.N("TimeToOpen"));    // 0x5eea50
			}
			else if (Row(y, (row + 1) * 16))
			{
				*out = DoorTime("TimeToClose", "Time take to close (0.1 - 25 sec)", r.N("TimeToClose"));   // 0x5eea2c
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

		StrategyViewClick PickOf(char const *field, char const *words)
		{
			StrategyViewClick click;

			click.action = StrategyViewClick::Pick;
			click.field = field;
			click.pickWords = words;
			return click;
		}

		// a time kept as tenths of a second in a masked field, typed in seconds (FUN_004747f0 and FUN_00474a10): from 0 up to below 6.35 seconds
		StrategyViewClick TenthsUpTo63(char const *field, char const *title, int32_t tenths)
		{
			StrategyViewClick click;

			click.action = StrategyViewClick::Number;
			click.field = field;
			click.title = title;
			click.initialText = Format("%g", tenths * 0.1);
			click.isFloat = true;
			click.factor = 10.0;
			click.bias = 0.01;       // 0x5ac438
			click.lowest = 0.0;      // 0x5ab360
			click.highest = 63.5;    // 0x5af918, below it
			click.highestIncluded = false;
			return click;
		}

		// ---- 59 Switch Door: FUN_0052d060 (vtable 0x5af41c +0x14, read from the dump and the listing). "Associated door :" and the door's name; a ticked box "Switch Operated" (a bit of the word at obj +0x1c); the open and close times (bits 20-25 and 26-31 of
		// that word, in tenths, 0 = Default). A click on the first two rows picks the door (FUN_0052d490: "Select associated door"), on the third flips the box, on the others asks the time.
		void SwitchDoorLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			int32_t const open = c.r.N("TimeToOpen"), close = c.r.N("TimeToClose");

			Put(lines, 200, 0, c.name + " : Switch door");                                                                   // 0x5eec6c
			Put(lines, 16, 16, "Associated door :");                                                                         // 0x5eec58
			Put(lines, 80, 32, ObjectName(c, "AssocDoor"));
			Put(lines, 0, 48, Box((c.r.N("SwitchOperated") & 1) != 0));                                                              // glyph 1 / 3
			Put(lines, 16, 48, "Switch Operated");                                                                           // 0x5eec48
			Put(lines, 0, 64, open == 0 ? std::string("Time taken to open : Default") : Format("Time taken to open : %.1f seconds", open * 0.1));     // 0x5eec04, 0x5eec24
			Put(lines, 0, 80, close == 0 ? std::string("Time taken to close : Default") : Format("Time taken to close : %.1f seconds", close * 0.1));   // 0x5eebc0, 0x5eebe0
		}

		bool SwitchDoorClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (y >= 16 && y < 48)
			{
				*out = PickOf("AssocDoor", "Select associated door");                                                       // 0x5eec90
			}
			else if (Row(y, 48))
			{
				*out = ToggleOf("SwitchOperated", 0x1);
			}
			else if (Row(y, 64))
			{
				*out = TenthsUpTo63("TimeToOpen", "Time take to open (0.0 - 6.3 sec)", r.N("TimeToOpen"));                  // 0x5eeb9c
			}
			else if (Row(y, 80))
			{
				*out = TenthsUpTo63("TimeToClose", "Time take to close (0.0 - 6.3 sec)", r.N("TimeToClose"));               // 0x5eeb78
			}
			else
			{
				return false;
			}

			return true;
		}

		// ---- 78 Self Destruct: FUN_00535cc0 (vtable 0x5af790 +0x14, from the listing): "Time : %d" (obj +0x14, seconds); a click on it asks "Time until destruction (seconds)" and keeps the whole number, a negative one as 0
		void SelfDestructLines(Context const &c, std::vector<StrategyViewLine> *lines)
		{
			Put(lines, 200, 0, c.name + " : Self Destruct");                      // 0x5eff30
			Put(lines, 16, 16, Format("Time : %d", c.r.N("timer")));              // 0x5eff24
		}

		bool SelfDestructClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (!Row(y, 16))
			{
				return false;
			}

			*out = NumberOf("timer", "Time until destruction (seconds)", r.N("timer"), 0, 32767);   // 0x5eff00
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
		}

		bool MessageClick(Reader const &r, int32_t, int32_t y, StrategyViewClick *out)
		{
			if (Row(y, 16))
			{
				*out = ToggleOf("flags", 0x1);
			}
			else if (y >= 32 && y < 48)
			{
				*out = NumberOf("message_string", "Message", r.N("message_string"), -2147483647 - 1, 2147483647);   // 0x5edb98
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
			{ 50, DoorLines, DoorClick },
			{ 54, PlatformLines, PlatformClick },
			{ 59, SwitchDoorLines, SwitchDoorClick },
			{ 75, FanLines, FanClick },
			{ 73, MessageLines, MessageClick },
			{ 77, DeathLines, DeathClick },
			{ 78, SelfDestructLines, SelfDestructClick },
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

	bool StrategyView_FieldNumber(int32_t kind, std::vector<uint8_t> const &payload, char const *field, int32_t *value)
	{
		Reader const reader = { payload, kind };

		return reader.Number(field, value);
	}

	bool StrategyView_Has(int32_t kind)
	{
		return ViewOf(kind) != nullptr;
	}

	bool StrategyView_Lines(int32_t kind, std::vector<uint8_t> const &payload, std::string const &name, int32_t areaHeight, StrategyViewNames const &names, uint8_t const *ownId, std::vector<StrategyViewLine> *lines)
	{
		KindView const *const view = ViewOf(kind);

		if (!view || !lines)
		{
			return false;
		}

		Reader const reader = { payload, kind };
		Context const context = { reader, name, areaHeight, names, ownId };

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

	bool StrategyView_Click(int32_t kind, std::vector<uint8_t> const &payload, int32_t x, int32_t y, StrategyViewClick *out)
	{
		KindView const *const view = ViewOf(kind);

		if (!view || !out)
		{
			return false;
		}

		Reader const reader = { payload, kind };

		return view->click(reader, x, y, out);
	}
}
