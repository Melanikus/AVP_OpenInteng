// The test for the tool row and action buttons (tool_buttons.h). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (A8).
// AVP_OpenInteng's own code: see License.txt. No window, none of the game.

#include <stdio.h>
#include <string.h>

#include <string>

#include "tool_buttons.h"
#include "inteng_menus.h"
#include "editor_actions.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static int Ten(int) { return 10; }

int main()
{
	puts("The tool row and the action buttons:");

	ViewToggles now = { DrawMode::Polyline, Gouraud::On, false, true, 128, true };
	ToolButton tools[kToolButtonMost];
	size_t const count = ToolButtons_Tools(now, true, tools, kToolButtonMost);

	Check(count == 7 && std::string(tools[0].label) == "Polygon" && tools[0].action == kActionViewPolygon && std::string(tools[5].label) == "Textures"
		&& tools[5].action == kActionViewTextures && std::string(tools[6].label) == "Headlight",
		"the tool row is the four draw modes, Gouraud, Textures and Headlight, each with its menu item's number");
	Check(!tools[0].lit && tools[1].lit && !tools[2].lit && !tools[3].lit && tools[4].lit && tools[5].lit && tools[6].lit,
		"exactly the draw mode in use is lit, and Gouraud, Textures and Headlight with their switches");
	ViewToggles untextured = now;
	untextured.textured = false;
	ToolButton plain[kToolButtonMost];
	ToolButtons_Tools(untextured, true, plain, kToolButtonMost);
	Check(!plain[5].lit && plain[4].lit, "Textures is not lit while the textures are off, and nothing else changes");
	Check(!tools[1].off && ToolButtons_Tools(now, false, tools, kToolButtonMost) == 7 && tools[1].off && tools[2].off && !tools[0].off,
		"Polyline and Filled are off while the game can't show their lines");
	Check(ToolButtons_Tools(now, true, tools, 2) == 2 && ToolButtons_Tools(now, true, nullptr, 5) == 0, "never more than the room given, and none for no room");

	size_t placement = 0;

	for (size_t s = 0; s < kIntengScreenCount; s++)
	{
		if (strcmp(kIntengScreens[s].table, "0x5e51b8") == 0)
		{
			placement = s;
		}
	}

	ToolButton panel[kToolButtonMost];
	size_t const hub = ToolButtons_Panel(kIntengHub, false, panel, kToolButtonMost);
	Check(hub == 2 && std::string(panel[0].label) == "Open" && panel[0].action == kActionOpenLevel && panel[1].action == kActionSaveCopy,
		"a screen with no Add or Delete has Open and Save copy");

	size_t const place = ToolButtons_Panel(placement, false, panel, kToolButtonMost);
	// Create like the selected is [wip] again (it does not match inteng yet), so no item does it and there is no Add: only Delete
	Check(place >= 3 && std::string(panel[place - 1].label) == "Delete" && panel[place - 1].off,
		"Object Placement has Delete last (Create like the selected works now: Add may come before it), off with nothing selected");
	ToolButtons_Panel(placement, true, panel, kToolButtonMost);
	Check(!panel[place - 1].off && IntengMenus_Find(panel[place - 1].action) && IntengMenus_Find(panel[place - 1].action)->work == IntengWork::DeleteObject,
		"and on with an object selected, holding inteng's own item's number");

	ui::Box boxes[kToolButtonMost];
	ui::Box const room = { 100, 30, 200, 20 };
	size_t const placed = ToolButtons_LayOut(tools, 6, room, 4, 20, Ten, boxes);
	Check(placed >= 2 && boxes[0].x == 100 && boxes[0].y == 30 && boxes[1].x > boxes[0].x + boxes[0].width && boxes[placed - 1].x + boxes[placed - 1].width <= 300,
		"they go left to right inside the room, and the ones that don't fit whole are left out");
	Check(ToolButtons_At(boxes, placed, boxes[1].x + 1, 35) == 1 && ToolButtons_At(boxes, placed, 5, 5) == -1, "a press finds its button, or none");
	Check(ToolButtons_LayOut(tools, 6, ui::Box{ 0, 0, 200, 10 }, 4, 20, Ten, boxes) == 0, "a room too short for a button places none");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks ok");
	return checksFailed ? 1 : 0;
}
