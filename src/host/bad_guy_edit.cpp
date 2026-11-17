// See bad_guy_edit.h. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "bad_guy_edit.h"

#include <stdlib.h>

namespace host
{
	namespace
	{
		uint32_t const kInactive = 0x80000000u;

		// the type combo (0x1001fc28)
		BadGuyChoices const kTypes = { { "Alien", 1 }, { "Marine", 7 }, { "Predator", 2 }, { "Facehugger", 3 }, { "Alien Queen", 6 }, { "Xenoborg", 4 }, { "Predator-Alien", 5 }, { "Pretorian", 8 },
		                               { "Sentry Gun", 9 } };

		// a Predator's two weapons (0x10020228)
		BadGuyChoices const kWeapons = { { "Pistol", 0 }, { "Wristblade", 1 }, { "Plasma Caster", 2 }, { "Staff", 3 } };

		// each type's subtypes (0x1001fbd8 + type x 0xa0)
		BadGuyChoices SubtypesOf(int32_t type)
		{
			switch (type)
			{
				case 1: return { { "Alien", 0 } };
				case 2: return { { "predator", 0 } };
				case 3: return { { "Facehugger", 0 } };
				case 4: return { { "Xenoborg", 0 } };
				case 5: return { { "Predator-Alien", 0 } };
				case 6: return { { "Alien Queen", 0 } };
				case 7: return { { "Pulse Rifle", 0 }, { "Pistol", 5 }, { "Smartgun", 10 }, { "Grenade Launcher", 20 }, { "Flamethrower", 50 }, { "Sadar", 60 }, { "Minigun", 70 }, { "Android", 120 },
				                 { "Civ - Shotgun 1", 30 }, { "Civ - Shotgun 2", 31 }, { "Civ - Shotgun 3", 32 }, { "Civ - Pistol 1", 40 }, { "Civ - Pistol 2", 41 }, { "Civ - Flamer", 100 },
				                 { "Civ - Molotov", 80 }, { "Civ - Unarmed", 110 }, { "Civ - Scientist 1", 90 }, { "Civ - Scientist 2", 91 } };
				case 8: return { { "Pretorian", 0 } };
				case 9: return { { "Sentry Gun", 0 } };
				default: return {};
			}
		}

		// FUN_100043b0: the first row standing for the number, -1 when none does (nothing is picked then)
		int32_t RowOf(BadGuyChoices const &choices, int32_t value)
		{
			for (size_t i = 0; i < choices.size(); i++)
			{
				if (choices[i].second == value)
				{
					return (int32_t)i;
				}
			}

			return -1;
		}

		// sscanf "%d" (FUN_100086e8 with 0x10020df0): false when no number is read
		bool ReadNumber(std::string const &text, long *value)
		{
			char *end = nullptr;
			char const *const start = text.c_str();
			long const read = strtol(start, &end, 10);

			if (!end || end == start)
			{
				// strtol reads nothing past leading spaces only when there is no number; sscanf "%d" alike
				return false;
			}

			*value = read;
			return true;
		}
	}

	BadGuyControls BadGuy_Controls(SpecialObject const &badGuy, int32_t type)
	{
		BadGuyControls c;

		c.types = kTypes;
		c.typePick = RowOf(kTypes, type);
		c.startsInactive = (badGuy.flags & kInactive) != 0;

		// the texture combo (FUN_10005400 0x10005490: "Random" alone, picked; a Marine's list instead, its row by the texture, 0x10005958-0x10005987)
		c.textures = { { "Random", 0 } };
		c.texturePick = 0;

		if (type == 2)
		{
			c.leftLabel = "Primary";      // 0x10020ed4
			c.rightLabel = "Secondary";   // 0x10020ec8
			c.leftCombo = c.rightCombo = true;
			c.leftChoices = c.rightChoices = kWeapons;
			c.leftPick = RowOf(kWeapons, badGuy.subType & 7);
			c.rightPick = RowOf(kWeapons, (badGuy.subType >> 3) & 7);
		}
		else if (type == 4)
		{
			c.leftLabel = "Active Time 0=default";      // 0x10020eb0
			c.rightLabel = "Module Range - 0=default";  // 0x10020e94
			c.leftEdit = c.rightEdit = true;
			c.leftText = std::to_string((long long)badGuy.extra1 * 5);
			c.rightText = std::to_string((long long)badGuy.extra2);
		}
		else if (type == 9)
		{
			c.leftLabel = "Ammo (30000+ =infinite)";    // 0x10020e7c
			c.leftEdit = true;
			c.leftText = std::to_string((long long)badGuy.extra1 * 256 + badGuy.extra2);
		}
		else
		{
			c.leftLabel = "Subtype";   // 0x10020e74
			c.leftCombo = true;
			c.leftChoices = SubtypesOf(type);
			c.leftPick = RowOf(c.leftChoices, badGuy.subType);

			if (type == 7)
			{
				// NAMED: hnpcmarine.txt / hnpc_civvie.txt are in no install: "Random" and "Original" (FUN_100063d0 0x100064a0-0x1000650c)
				c.textures = { { "Random", 0 }, { "Original", 1 } };
				c.texturePick = RowOf(c.textures, badGuy.texture);
				c.textureLive = true;
			}
		}

		return c;
	}

	SpecialObject BadGuy_Ok(SpecialObject const &badGuy, BadGuyTyped const &typed)
	{
		SpecialObject o = badGuy;

		o.type = typed.type;

		// 0x10005dfa: the texture, when a row is picked
		if (typed.texturePick >= 0)
		{
			o.texture = (uint8_t)typed.texturePick;
		}

		if (typed.type == 2)
		{
			// 0x10005e22-0x10005e89: none picked counts as 0
			int32_t const primary = typed.leftPick >= 0 ? typed.leftPick : 0;
			int32_t const secondary = typed.rightPick >= 0 ? typed.rightPick : 0;

			o.subType = (uint8_t)(((secondary & 7) << 3) | (primary & 7));
		}
		else if (typed.type == 4)
		{
			long seconds = 0, range = 0;

			// INTENG BUGS, FIXED: no number keeps the old value; below 0 is 0
			if (ReadNumber(typed.leftText, &seconds))
			{
				seconds = seconds < 0 ? 0 : seconds / 5;
				o.extra1 = (uint8_t)(seconds > 255 ? 255 : seconds);   // 0x10005ef5-0x10005f05
			}

			if (ReadNumber(typed.rightText, &range))
			{
				range = range < 0 ? 0 : range;
				o.extra2 = (uint8_t)(range > 255 ? 255 : range);   // 0x10005f50
			}
		}
		else if (typed.type == 9)
		{
			long ammo = 0;

			if (ReadNumber(typed.leftText, &ammo))
			{
				ammo = ammo < 0 ? 0 : (ammo > 30000 ? 30000 : ammo);   // 0x10005faf
				o.extra2 = (uint8_t)(ammo & 0xff);
				o.extra1 = (uint8_t)(ammo >> 8);
			}
		}
		else if (typed.leftPick >= 0)
		{
			o.subType = (uint8_t)typed.leftPick;   // FUN_10005a50: the picked row's number
		}

		o.flags = typed.startsInactive ? (o.flags | kInactive) : (o.flags & ~kInactive);
		return o;
	}
}
