// See quick_create.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng (P3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "quick_create.h"

#include <stddef.h>

namespace host
{
	namespace
	{
		struct Entry
		{
			int32_t command;
			char const *shape;
		};

		// Every case of FUN_004d9af0 that calls FUN_004de260 (map-1-strings.txt:4078-4262). The strings at 0x5e84b0 and 0x5e84a0 (Light > Bulb and
		// Mine) are "Bulb" and "Mine" (wip-answers-quickcreate.md). The commands 0x44e (atsham01), 0x454 (plasam02) and 0x4b2 (sentry) are in the
		// switch with no item of the menu: they are not here.
		Entry const kEntries[] =
		{
			{ 0x3e9, "pulse" },       // Weapon > Pulse Rifle
			{ 0x3eb, "smart" },       // Smartgun
			{ 0x3ec, "flame" },       // Flame Thrower
			{ 0x3ed, "sadar" },       // Sadar
			{ 0x3ee, "grenade" },     // Grenade Launcher
			{ 0x3ef, "minigun" },     // Minigun
			{ 0x3f8, "pistol" },      // Pistol
			{ 0x3f9, "skeeter" },     // Skeeter
			{ 0x44d, "pulam" },       // Ammo > Pulse Rifle
			{ 0x44f, "smrtam01" },    // Smartgun
			{ 0x450, "flam01" },      // Flame Thrower
			{ 0x451, "rocam01" },     // Sadar
			{ 0x452, "grenbx01" },    // Grenade Launcher
			{ 0x453, "miniamo" },     // Minigun
			{ 0x455, "Grenband" },    // Pulse Rifle Grenade
			{ 0x456, "6spears" },     // Speargun
			{ 0x460, "Bulb" },        // Light > Bulb
			{ 0x461, "Simple" },      // Simple
			{ 0x462, "Mine" },        // Mine
			{ 0x4b1, "armour01" },    // Other > Armour (small)
			{ 0x4b3, "medbox01" },    // Stimpack
			{ 0x4b4, "Pboost" },      // Predator Field Charge
		};
	}

	char const *QuickCreate_ShapeName(int32_t command)
	{
		for (size_t i = 0; i < sizeof(kEntries) / sizeof(kEntries[0]); i++)
		{
			if (kEntries[i].command == command)
			{
				return kEntries[i].shape;
			}
		}

		return nullptr;
	}
}
