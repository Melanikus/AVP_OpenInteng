// See screen_keys.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "screen_keys.h"

#include <string.h>

namespace host
{
	namespace
	{
		ScreenKeyRow const kRows[] =
		{
			// ADJACENT MODULES (FUN_00404a20, inteng-menus.md 5)
			{ "0x5c3508", "", -1, 'N', false, 0x32, "Link > Add" },
			{ "0x5c3508", "", -1, 'D', false, 0x34, "Link > Delete" },
			{ "0x5c3508", "", -1, 'S', false, 0x64, "Base > Set Current" },
			{ "0x5c3508", "", -1, '8', false, 0xc8, "View Mode > Polyline" },
			{ "0x5c3508", "", -1, '9', false, 0xc9, "View Mode > Filled Polyline" },

			// MODULE LINKING (FUN_004c1a90, inteng-menus.md 7), both of its tables
			{ "0x5e2e38", "", -1, 'H', false, 0x96, "Object > Hide" },
			{ "0x5e2e38", "", -1, 'U', false, 0x98, "Object > Un-hide all" },
			{ "0x5e2e38", "", -1, 'L', false, 0x9a, "Object > Un-hide last" },
			{ "0x5e2e38", "", -1, 'S', false, 0xc8, "Base > Set Current" },
			{ "0x5e2e38", "", -1, 'S', true, kScreenKeyLinkFollow, "Shift+S: the base follows the module the camera is in" },
			{ "0x5e2e38", "", -1, 'N', false, 0x32, "Link > Normal" },
			{ "0x5e2e38", "", -1, 'B', false, 0x34, "Link > Branch" },
			{ "0x5e2e38", "", -1, 'C', false, 0x40, "Branch > Close Branch" },
			{ "0x5e2e38", "", -1, 'O', false, 0x42, "Branch > Open Branch" },
			{ "0x5e2e38", "", -1, 'D', false, 0x36, "Link > Delete" },
			{ "0x5e2e38", "", -1, '7', false, 0x48, "View Mode > Filled" },
			{ "0x5e2e38", "", -1, '8', false, 0x46, "View Mode > Polyline" },
			{ "0x5e2e38", "", -1, '9', false, 0x47, "View Mode > Filled Polyline" },
			{ "0x5e2e38", "", -1, '0', false, 0x4a, "View Mode > Mixed" },
			{ "0x5e2e38", "", -1, 'V', false, 0x4b, "Only Draw Linked Modules" },

			// WAYPOINT (the code-built screen; the three handlers FUN_00583d00 mode 0, FUN_00584ee0 mode 1, FUN_00585750 mode 2: their key-to-command rows are the help texts' words)
			{ "", "Waypoint", 0, 'S', false, 0x19, "Edit selected AI module" },
			{ "", "Waypoint", 0, 'L', false, 0x1a, "Add module to AI module" },
			{ "", "Waypoint", 0, 'R', false, 0x1b, "Remove module from AI module" },
			{ "", "Waypoint", 0, 'F', false, 0x25, "Toggle fly mode for the selected entry point" },
			{ "", "Waypoint", 0, 0x2e, false, 0x24, "Delete the selected entry point" },
			{ "", "Waypoint", 1, 'C', false, 0x5, "Create waypoint" },
			{ "", "Waypoint", 1, 'S', false, 0x7, "Set base waypoint" },
			{ "", "Waypoint", 1, 'D', false, 0x10, "Delete link" },
			{ "", "Waypoint", 1, 'M', false, 0x1c, "Toggle waypoint minimise mode" },
			{ "", "Waypoint", 1, 'L', false, 0x8, "Add link" },
			{ "", "Waypoint", 1, 'L', true, 0xa, "Add alien link" },
			{ "", "Waypoint", 1, 'V', false, 0xc, "Add deviation allowed link" },
			{ "", "Waypoint", 1, 'W', false, 0x27, "Toggle can crawl flag" },
			{ "", "Waypoint", 1, 'Y', false, 0x28, "Cycle the link flag highlight (yellow links)" },
			{ "", "Waypoint", 2, 'C', false, 0x5, "Create waypoint" },
			{ "", "Waypoint", 2, 'R', false, 0x1f, "Toggle edit waypoint centre" },
			{ "", "Waypoint", 2, 'M', false, 0x1c, "Toggle waypoint minimise mode" },

			// LIGHT SOURCING (FUN_004aab60), both tables: S and X read their key flags 0xd87a93 and 0xd87a98
			{ "0x5e1360", "", -1, 'S', false, kScreenKeyLightOn, "Switch the selected light on" },
			{ "0x5e1360", "", -1, 'X', false, kScreenKeyLightOff, "Switch the selected light off" },
			{ "0x5e1750", "", -1, 'S', false, kScreenKeyLightOn, "Switch the selected light on" },
			{ "0x5e1750", "", -1, 'X', false, kScreenKeyLightOff, "Switch the selected light off" },
			{ "0x5e1360", "", -1, 'B', false, kScreenKeyLightHeadlight, "Headlight on and off" },
			{ "0x5e1750", "", -1, 'B', false, kScreenKeyLightHeadlight, "Headlight on and off" },
			{ "0x5e1360", "", -1, 'C', false, kScreenKeyLightCopy, "Copy light" },
			{ "0x5e1360", "", -1, 'D', false, kScreenKeyLightDrop, "Drop light at the camera" },
			{ "0x5e1360", "", -1, 'V', false, kScreenKeyLightPaste, "Paste the copied light at the camera" },
			{ "0x5e1360", "", -1, 'R', false, kScreenKeyLightRemove, "Remove light" },
			{ "0x5e1360", "", -1, 'O', false, kScreenKeyLightOverwrite, "Copy the clipboard light's data over the selected light" },
			{ "0x5e1360", "", -1, 'I', false, kScreenKeyLightInvisible, "Toggle the selected light's visibility" },
			{ "0x5e1360", "", -1, 'U', false, kScreenKeyLightUpdate, "Update the selected light (the changed flag taken out)" },
			{ "0x5e1360", "", -1, 0xbc, false, kScreenKeyLightAttrPrev, "Select the light attribute before (<)" },
			{ "0x5e1360", "", -1, 0xbc, true, kScreenKeyLightAttrPrev, "Select the light attribute before (<)" },
			{ "0x5e1360", "", -1, 0xbe, false, kScreenKeyLightAttrNext, "Select the light attribute after (>)" },
			{ "0x5e1360", "", -1, 0xbe, true, kScreenKeyLightAttrNext, "Select the light attribute after (>)" },
			{ "0x5e1360", "", -1, 'F', false, kScreenKeyLightRelight, "Work the lighting out again" },
			{ "0x5e1360", "", -1, 0x0d, false, kScreenKeyLightReturn, "Return: the write-back of Exit, the copy saved when changed" },
			{ "0x5e1360", "", -1, 'L', false, kScreenKeyLightFly, "Fly light on / off (not built)" },
			{ "0x5e1360", "", -1, 'T', false, kScreenKeyLightPlacedColour, "Toggle up / down colour of a placed-object light" },
			{ "0x5e1360", "", -1, 'P', false, kScreenKeyLightPlacedProperties, "Edit a placed-object light's properties" },
			{ "0x5e1750", "", -1, 'C', false, kScreenKeyLightCopy, "Copy light" },
			{ "0x5e1750", "", -1, 'D', false, kScreenKeyLightDrop, "Drop light at the camera" },
			{ "0x5e1750", "", -1, 'V', false, kScreenKeyLightPaste, "Paste the copied light at the camera" },
			{ "0x5e1750", "", -1, 'R', false, kScreenKeyLightRemove, "Remove light" },
			{ "0x5e1750", "", -1, 'O', false, kScreenKeyLightOverwrite, "Copy the clipboard light's data over the selected light" },
			{ "0x5e1750", "", -1, 'I', false, kScreenKeyLightInvisible, "Toggle the selected light's visibility" },
			{ "0x5e1750", "", -1, 'U', false, kScreenKeyLightUpdate, "Update the selected light (the changed flag taken out)" },
			{ "0x5e1750", "", -1, 0xbc, false, kScreenKeyLightAttrPrev, "Select the light attribute before (<)" },
			{ "0x5e1750", "", -1, 0xbc, true, kScreenKeyLightAttrPrev, "Select the light attribute before (<)" },
			{ "0x5e1750", "", -1, 0xbe, false, kScreenKeyLightAttrNext, "Select the light attribute after (>)" },
			{ "0x5e1750", "", -1, 0xbe, true, kScreenKeyLightAttrNext, "Select the light attribute after (>)" },
			{ "0x5e1750", "", -1, 'F', false, kScreenKeyLightRelight, "Work the lighting out again" },
			{ "0x5e1750", "", -1, 0x0d, false, kScreenKeyLightReturn, "Return: the write-back of Exit, the copy saved when changed" },
			{ "0x5e1750", "", -1, 'L', false, kScreenKeyLightFly, "Fly light on / off (not built)" },
			{ "0x5e1750", "", -1, 'T', false, kScreenKeyLightPlacedColour, "Toggle up / down colour of a placed-object light" },
			{ "0x5e1750", "", -1, 'P', false, kScreenKeyLightPlacedProperties, "Edit a placed-object light's properties" },
		};

		size_t const kRowCount = sizeof(kRows) / sizeof(kRows[0]);
	}

	size_t ScreenKeys_Count()
	{
		return kRowCount;
	}

	ScreenKeyRow const &ScreenKeys_Row(size_t index)
	{
		return kRows[index < kRowCount ? index : 0];
	}

	int32_t ScreenKeys_Command(char const *table, char const *name, int mode, uint8_t key, bool shift)
	{
		for (size_t i = 0; i < kRowCount; i++)
		{
			ScreenKeyRow const &row = kRows[i];
			bool const same = row.table[0] != 0 ? (table && strcmp(row.table, table) == 0) : (name && strcmp(row.name, name) == 0);

			if (same && (row.mode < 0 || row.mode == mode) && row.key == key && row.shift == shift)
			{
				return row.command;
			}
		}

		return 0;
	}
}
