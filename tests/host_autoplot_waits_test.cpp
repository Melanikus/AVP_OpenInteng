// host_autoplot_waits_test.cpp: AN EDIT WAITS FOR THE PLOT (src/host/autoplot_waits.h; the owner 11-21, "Full map + inset on top", and Bishop's spec: "Edits that
// would touch the level being written wait until the plot ends, and the status line says so"). Which menu items wait while Alien auto waypoint plots: the ones
// that change the level or leave the Waypoint screen wait, the ones that only look are done, Quit is done, and a kind the rule does not know waits.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>

#include "autoplot_waits.h"

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
	using host::IntengWork;

	// ---- WHAT CHANGES THE LEVEL, OR LEAVES THE WAYPOINT SCREEN, WAITS
	Check(host::AutoPlot_ItemWaits(IntengWork::SaveCopy), "Save a copy waits (it would write a level half plotted)");
	Check(host::AutoPlot_ItemWaits(IntengWork::DeleteObject), "Delete object waits");
	Check(host::AutoPlot_ItemWaits(IntengWork::WaypointCommand), "a Waypoint command waits (its Exit writes the waypoints)");
	Check(host::AutoPlot_ItemWaits(IntengWork::AutoLinkEnvironment), "Auto link waits");
	Check(host::AutoPlot_ItemWaits(IntengWork::ToScreen) && host::AutoPlot_ItemWaits(IntengWork::ToHub),
		"going to another screen waits (the plot goes on only on the Waypoint screen: on another, the level could be edited under it)");
	Check(host::AutoPlot_ItemWaits(IntengWork::None), "None (\"the editor does it\": many items, edits among them) waits");
	Check(host::AutoPlot_ItemWaits((IntengWork)0x7fff), "a kind the rule does not know waits");

	// ---- WHAT ONLY LOOKS IS DONE
	Check(!host::AutoPlot_ItemWaits(IntengWork::ViewPolygon) && !host::AutoPlot_ItemWaits(IntengWork::ViewPolyline) &&
		!host::AutoPlot_ItemWaits(IntengWork::ViewFilledPolyline), "the draw modes are done (Polygon, Polyline, Filled Polyline)");
	Check(!host::AutoPlot_ItemWaits(IntengWork::Keys) && !host::AutoPlot_ItemWaits(IntengWork::LinkHelp), "the keys' help is done");
	Check(!host::AutoPlot_ItemWaits(IntengWork::Screenshot), "a picture is done");
	Check(!host::AutoPlot_ItemWaits(IntengWork::TextScale), "the text size is done");
	Check(!host::AutoPlot_ItemWaits(IntengWork::Quit), "Quit is done (the plot is stopped and waited for first)");

	// ---- THE WORDS
	std::string const words = host::AutoPlot_WaitWords("Save a copy");

	Check(words.find("Save a copy") != std::string::npos && words.find("waits") != std::string::npos && words.find("auto-plot") != std::string::npos,
		"the status line names the item and says it waits for the auto-plot: \"" + words + "\"");
	Check(host::AutoPlot_WaitWords(nullptr).find("waits") != std::string::npos, "an item with no name still gets words");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
