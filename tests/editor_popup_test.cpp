// The test for the right-click pop-up's table (editor_bar.h): which screens have one and what it holds. Written 2026-11-15 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt. No window, none of the game.

#include <stdio.h>
#include <string.h>

#include <string>

#include "editor_bar.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

int main()
{
	puts("The right-click pop-up:");

	size_t linking = kEditorNoPopup;
	size_t links = kEditorNoPopup;

	for (size_t s = 0; s < host::kIntengScreenCount; s++)
	{
		if (strcmp(host::kIntengScreens[s].table, "0x5e2e38") == 0)
		{
			linking = s;
		}

		if (strcmp(host::kIntengScreens[s].table, "0x5e2c88") == 0)
		{
			links = s;
		}
	}

	Check(linking != kEditorNoPopup && links != kEditorNoPopup && EditorBar_PopupScreen(linking) == links, "Module Linking's pop-up is its second table, 0x5e2c88");
	Check(EditorBar_PopupScreen(host::kIntengHub) == kEditorNoPopup && EditorBar_PopupScreen(links) == kEditorNoPopup
		&& EditorBar_PopupScreen(host::kIntengScreenCount) == kEditorNoPopup, "the other screens have none, and a screen that isn't one has none");

	static EditorBarRoom room;
	Check(EditorBar_MakePopup(linking, &room) && room.menuCount == 1 && room.menus[0].count == 8, "it makes one menu with the table's eight link commands");
	Check(!EditorBar_MakePopup(host::kIntengHub, &room) && !EditorBar_MakePopup(linking, nullptr), "no pop-up for a screen without one, or without a room");

	bool allMake = true;

	for (size_t i = 0; i < room.menus[0].count; i++)
	{
		allMake = allMake && room.menus[0].items[i].action > 0 && room.menus[0].items[i].name && host::IntengMenus_Find(room.menus[0].items[i].action);
	}

	Check(allMake, "each item has its words and an action the bar's own table knows, so a pick runs through the same door");

	// Construction's Mode > Normal (0x39) and No Textures (0x3a): both enter the Strategy Data screen; only 0x3a paints every polygon white (flag 0x93 = 0x13 | 0x80)
	int32_t normal = 0;
	int32_t noTextures = 0;

	for (size_t m = 0; m < host::kIntengScreens[host::kIntengHub].count; m++)
	{
		for (size_t i = 0; i < host::kIntengScreens[host::kIntengHub].menus[m].count; i++)
		{
			host::IntengItem const &item = host::kIntengScreens[host::kIntengHub].menus[m].items[i];

			normal = item.command == 0x39 ? item.action : normal;
			noTextures = item.command == 0x3a ? item.action : noTextures;
		}
	}

	EditorHostState hostNormal = {};
	EditorHostState hostPlain = {};

	Check(normal && noTextures && EditorBar_Run(normal, &hostNormal) && EditorBar_Run(noTextures, &hostPlain), "Construction has Normal (0x39) and No Textures (0x3a), and both run");
	Check(strcmp(host::kIntengScreens[hostNormal.screen].name, "Strategy Data") == 0 && strcmp(host::kIntengScreens[hostPlain.screen].name, "Strategy Data") == 0,
		"both enter the Strategy Data screen");
	Check(!hostNormal.paintWhite && hostPlain.paintWhite, "only No Textures paints every polygon white (inteng's flag 0x80)");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks ok");
	return checksFailed ? 1 : 0;
}
