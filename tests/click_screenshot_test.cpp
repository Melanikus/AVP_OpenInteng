// The test for src/host's command-then-click (click_mode) and the screenshot's name (screenshot_name). Written 2026-11-15 by Melanikus
// (Dietrich) for AVP_OpenInteng (P4, P6). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". It needs no window and none
// of the game. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "click_mode.h"
#include "screenshot_name.h"

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

static PickReport Report(PickFound found, int32_t polygon, bool corner)
{
	PickReport report;

	memset(&report, 0, sizeof(report));
	report.found = found;
	report.hasHit = polygon >= 0;
	report.polygonNumber = polygon;
	report.hasCorner = corner;
	return report;
}

static void TestClickMode()
{
	puts("Command, then click:");

	ClickMode_Cancel();
	Check(!ClickMode_Waiting() && ClickMode_Wants() == ClickWants::Nothing, "nothing waits at the start");
	Check(!ClickMode_Click(Report(PickFound::Object, 0, false)).done, "with nothing waiting, a click is not taken");

	Check(!ClickMode_Start(1, ClickWants::Nothing, "Set") && !ClickMode_Start(1, ClickWants::Polygon, "") && !ClickMode_Start(1, ClickWants::Polygon, nullptr)
		&& !ClickMode_Waiting(), "a start that waits for nothing, or has no name, starts nothing");

	Check(ClickMode_Start(7, ClickWants::Polygon, "Select by polygons") && ClickMode_Waiting() && ClickMode_Wants() == ClickWants::Polygon,
		"a start waits");
	Check(std::string(ClickMode_Words()) == "Select by polygons - click a polygon in the view (Esc cancels)", "and the status line says what it waits for");

	ClickAnswer const wrong = ClickMode_Click(Report(PickFound::Nothing, -1, false));
	Check(!wrong.done && ClickMode_Waiting() && std::string(ClickMode_Words()).find("nothing, not a polygon") != std::string::npos,
		"a click that found nothing keeps waiting and says so");

	ClickAnswer const noPolygon = ClickMode_Click(Report(PickFound::NotInTheFile, 3, true));
	Check(!noPolygon.done && ClickMode_Waiting(), "a hit on something the game made isn't a polygon of the level: it keeps waiting");

	ClickAnswer const right = ClickMode_Click(Report(PickFound::Module, 12, false));
	Check(right.done && right.command == 7 && right.wants == ClickWants::Polygon && right.report.polygonNumber == 12 && !ClickMode_Waiting()
		&& std::string(ClickMode_Words()).empty(), "a polygon of a module is what it waited for: done, with the command, and no longer waiting");

	ClickMode_Start(2, ClickWants::Corner, "Snap To Vertex");
	Check(!ClickMode_Click(Report(PickFound::Module, 5, false)).done && ClickMode_Click(Report(PickFound::Object, 5, true)).done,
		"a corner needs the nearest corner read: a polygon without one is refused, with one it's taken");

	ClickMode_Start(3, ClickWants::Object, "Pick");
	Check(!ClickMode_Click(Report(PickFound::Module, 5, true)).done && ClickMode_Click(Report(PickFound::Object, -1, false)).done,
		"an object is only an object, not a module");

	ClickMode_Start(4, ClickWants::Module, "Set base");
	Check(ClickMode_Cancel() && !ClickMode_Waiting() && !ClickMode_Cancel() && std::string(ClickMode_Words()).empty(),
		"Esc (cancel) stops it, once, and the words go");

	char longName[200];

	memset(longName, 'x', sizeof(longName));
	longName[199] = '\0';
	longName[3] = '\x01';
	ClickMode_Start(5, ClickWants::Module, longName);
	Check(strlen(ClickMode_Words()) < kClickModeMostWords && std::string(ClickMode_Words()).find("xxx?") == 0, "a long name is cut, and a byte outside space to ~ shows as ?");
	ClickMode_Cancel();
}

static bool Taken(std::string const &name, void *context)
{
	std::string const *const used = (std::string const *)context;

	return used->find("|" + name + "|") != std::string::npos;
}

static void TestScreenshotName()
{
	puts("The screenshot's name:");

	Check(ScreenshotName("C:\\game\\avp_rifs\\Invasion.RIF", 0) == "InvasionP00.bmp", "the level's name, P, two digits, .bmp");
	Check(ScreenshotName("C:\\game\\avp_rifs\\Invasion.RIF", 99) == "InvasionP99.bmp" && ScreenshotName("x", 7) == "xP07.bmp", "up to 99, two digits");
	Check(ScreenshotName("x", -1).empty() && ScreenshotName("x", 100).empty(), "a number outside 0 to 99 is no name");
	Check(ScreenshotName("C:\\a\\we ird;name.rif", 1) == "we_ird_nameP01.bmp", "anything but letters, digits, _ and - becomes _");
	Check(ScreenshotName("", 2) == "levelP02.bmp" && ScreenshotName(nullptr, 2) == "levelP02.bmp", "no usable name gives 'level'");

	std::string used = "|InvasionP00.bmp|InvasionP01.bmp|InvasionP03.bmp|";

	Check(ScreenshotFirstFree("Invasion.rif", Taken, &used) == 2, "the first number not taken is used, as inteng's loop does");

	used = "";

	for (int n = 0; n < kScreenshotNumbers; n++)
	{
		used += "|" + ScreenshotName("Invasion.rif", n) + "|";
	}

	Check(ScreenshotFirstFree("Invasion.rif", Taken, &used) == -1, "with all 100 taken, nothing is saved (-1)");
	Check(ScreenshotFirstFree("Invasion.rif", nullptr, nullptr) == -1, "no way to ask is -1, never a guess");

	Check(ScreenshotFolderFromCopyFolder("C:\\editor\\edited\\") == "C:\\editor\\pictures\\" && ScreenshotFolderFromCopyFolder("C:\\editor\\other\\").empty(),
		"the pictures' folder is the copies' folder's sibling, and nothing else is accepted");
}

int main()
{
	TestClickMode();
	TestScreenshotName();
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks ok");
	return checksFailed ? 1 : 0;
}
