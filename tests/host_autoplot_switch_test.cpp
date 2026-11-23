// host_autoplot_switch_test.cpp: the owner's "hide auto pilot" rule (src/host/autoplot_switch.h): the two Auto-plot commands are hidden while AutoPlot= is not 1, and shown with it;
// no other command is ever hidden, the Waypoint commands round them included; the switch holds what was set. Written 2026-11-19 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>

#include "autoplot_switch.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

int main()
{
	Check(host::AutoPlot_CommandHidden(0x7330, false), "Auto-plot AI module (0x7330) hidden while the switch is off");
	Check(host::AutoPlot_CommandHidden(0x7331, false), "Auto-plot all AI modules (0x7331) hidden while the switch is off");
	Check(!host::AutoPlot_CommandHidden(0x7330, true), "Auto-plot AI module shown with AutoPlot=1");
	Check(!host::AutoPlot_CommandHidden(0x7331, true), "Auto-plot all AI modules shown with AutoPlot=1");
	Check(host::AutoPlot_CommandHidden(0x7332, false), "Auto-plot selected AI modules (0x7332) hidden while the switch is off");
	Check(!host::AutoPlot_CommandHidden(0x7332, true), "Auto-plot selected AI modules shown with AutoPlot=1");
	Check(host::AutoPlot_CommandHidden(0x7334, false), "Alien auto waypoint > Modules (0x7334, the owner's 11-20) hidden while the switch is off");
	Check(!host::AutoPlot_CommandHidden(0x7334, true), "Alien auto waypoint > Modules shown with AutoPlot=1");
	Check(host::AutoPlot_CommandHidden(0x7335, false), "Alien auto waypoint > Environment (0x7335, the counts asked) hidden while the switch is off");
	Check(!host::AutoPlot_CommandHidden(0x7335, true), "Alien auto waypoint > Environment shown with AutoPlot=1");
	Check(host::AutoPlot_CommandHidden(0x7338, false), "Alien auto waypoint > All stock maps (0x7338, the owner's 11-22) hidden while the switch is off, so the whole menu is "
		"(Frost's acceptance-all-stock-maps-autoplot-off)");
	Check(!host::AutoPlot_CommandHidden(0x7338, true), "Alien auto waypoint > All stock maps shown with AutoPlot=1");

	// inteng's own Waypoint commands (Fly 0x25, Delete 0x24, Recalculate all 0x26, Help 0x15), commands next to the four, 0 and a negative: never hidden, either way
	int32_t const others[] = { 0x25, 0x24, 0x26, 0x15, 0x732f, 0x7333, 0x7336, 0, -1 };

	for (int32_t const command : others)
	{
		char words[96];

		sprintf_s(words, sizeof(words), "command 0x%x is never hidden (switch off, then on)", (unsigned)command);
		Check(!host::AutoPlot_CommandHidden(command, false) && !host::AutoPlot_CommandHidden(command, true), words);
	}

	Check(!host::AutoPlot_Shown(), "the switch starts off (the shipped ini has no AutoPlot=)");
	host::AutoPlot_SetShown(true);
	Check(host::AutoPlot_Shown(), "the switch holds AutoPlot=1");
	host::AutoPlot_SetShown(false);
	Check(!host::AutoPlot_Shown(), "and off again");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
