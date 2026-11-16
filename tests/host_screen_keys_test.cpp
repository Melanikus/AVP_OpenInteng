// The test for the screen keys table (src/host/screen_keys.*): each screen's keys give the commands inteng's handlers read them as, a key that is not one of the screen gives none, the same key on another screen or
// in another Waypoint mode gives its own command. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include <string>

#include "screen_keys.h"

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
	using namespace host;

	Check(ScreenKeys_Command("0x5c3508", "", -1, 'N', false) == 0x32 && ScreenKeys_Command("0x5c3508", "", -1, 'D', false) == 0x34 && ScreenKeys_Command("0x5c3508", "", -1, 'S', false) == 0x64, "Adjacent Modules: N adds, D deletes, S sets current");
	Check(ScreenKeys_Command("0x5c3508", "", -1, '8', false) == 0xc8 && ScreenKeys_Command("0x5c3508", "", -1, '9', false) == 0xc9, "Adjacent Modules: 8 and 9 are its Polyline and Filled Polyline");
	Check(ScreenKeys_Command("0x5c3508", "", -1, 'B', false) == 0 && ScreenKeys_Command("0x5c3508", "", -1, 'L', false) == 0, "Adjacent Modules: the help's B and L are not keys of its handler");
	Check(ScreenKeys_Command("0x5e2e38", "", -1, 'N', false) == 0x32 && ScreenKeys_Command("0x5e2e38", "", -1, 'B', false) == 0x34 && ScreenKeys_Command("0x5e2e38", "", -1, 'D', false) == 0x36, "Module Linking: N, B, D are Normal, Branch, Delete");
	Check(ScreenKeys_Command("0x5e2e38", "", -1, 'S', false) == 0xc8 && ScreenKeys_Command("0x5e2e38", "", -1, 'S', true) == kScreenKeyLinkFollow, "Module Linking: S sets current, Shift+S follows the camera");
	Check(ScreenKeys_Command("0x5e2e38", "", -1, 'C', false) == 0x40 && ScreenKeys_Command("0x5e2e38", "", -1, 'O', false) == 0x42 && ScreenKeys_Command("0x5e2e38", "", -1, 'V', false) == 0x4b, "Module Linking: C closes, O opens a branch, V is Only Draw Linked");
	Check(ScreenKeys_Command("0x5e2e38", "", -1, 'H', false) == 0x96 && ScreenKeys_Command("0x5e2e38", "", -1, 'U', false) == 0x98 && ScreenKeys_Command("0x5e2e38", "", -1, 'L', false) == 0x9a, "Module Linking: H hides, U un-hides all, L un-hides the last");
	Check(ScreenKeys_Command("0x5e2e38", "", -1, '7', false) == 0x48 && ScreenKeys_Command("0x5e2e38", "", -1, '8', false) == 0x46 && ScreenKeys_Command("0x5e2e38", "", -1, '9', false) == 0x47 && ScreenKeys_Command("0x5e2e38", "", -1, '0', false) == 0x4a, "Module Linking: 7 8 9 0 are its view modes");
	Check(ScreenKeys_Command("0x5e1360", "", -1, 'S', false) == kScreenKeyLightOn && ScreenKeys_Command("0x5e1750", "", -1, 'X', false) == kScreenKeyLightOff, "Light Sourcing (both tables): S on, X off");
	Check(ScreenKeys_Command("0x5e1360", "", -1, 'N', false) == 0, "Light Sourcing has no N");
	Check(ScreenKeys_Command("0x5e1360", "", -1, 'B', false) == kScreenKeyLightHeadlight && ScreenKeys_Command("0x5e1750", "", -1, 'B', false) == kScreenKeyLightHeadlight, "Light Sourcing: B is the headlight");

	Check(ScreenKeys_Command("", "Waypoint", 0, 'S', false) == 0x19 && ScreenKeys_Command("", "Waypoint", 1, 'S', false) == 0x7, "Waypoint: S is Edit Module in mode 0 and Set Base in mode 1");
	Check(ScreenKeys_Command("", "Waypoint", 1, 'L', false) == 0x8 && ScreenKeys_Command("", "Waypoint", 1, 'L', true) == 0xa && ScreenKeys_Command("", "Waypoint", 0, 'L', false) == 0x1a, "Waypoint: L adds a link, Shift+L an alien link, in mode 0 a module");
	Check(ScreenKeys_Command("", "Waypoint", 2, 'R', false) == 0x1f && ScreenKeys_Command("", "Waypoint", 2, 'M', false) == 0x1c && ScreenKeys_Command("", "Waypoint", 1, 'R', false) == 0, "Waypoint: R is Centre Edit in mode 2 only");
	Check(ScreenKeys_Command("", "Waypoint", 1, 'W', false) == 0x27 && ScreenKeys_Command("", "Waypoint", 1, 'Y', false) == 0x28 && ScreenKeys_Command("", "Waypoint", 1, 'V', false) == 0xc, "Waypoint: W crawl, Y highlight cycle, V deviation link");
	Check(ScreenKeys_Command("", "Waypoint", 0, 0x2e, false) == 0x24 && ScreenKeys_Command("", "Waypoint", 0, 'F', false) == 0x25, "Waypoint mode 0: Delete deletes an entry point, F is Fly");
	Check(ScreenKeys_Command("", "Path", 1, 'S', false) == 0, "another code-built screen has none of these");
	Check(ScreenKeys_Command(nullptr, nullptr, -1, 'S', false) == 0, "no screen: none");

	for (size_t i = 0; i < ScreenKeys_Count(); i++)
	{
		ScreenKeyRow const &a = ScreenKeys_Row(i);

		for (size_t j = i + 1; j < ScreenKeys_Count(); j++)
		{
			ScreenKeyRow const &b = ScreenKeys_Row(j);

			if (strcmp(a.table, b.table) == 0 && strcmp(a.name, b.name) == 0 && (a.mode < 0 || b.mode < 0 || a.mode == b.mode) && a.key == b.key && a.shift == b.shift)
			{
				Check(false, std::string("two rows for the same key: ") + a.words + " / " + b.words);
			}
		}
	}

	Check(true, "no key is given twice on a screen");

	if (checksFailed)
	{
		printf("FAILED host_screen_keys_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_screen_keys_test\n");
	return 0;
}
