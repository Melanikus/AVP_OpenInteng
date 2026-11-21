// The test for the "Alien auto waypoint" menu's typed words (host::AlienAutoWaypoint_Parse / _StartWords, src/host/alien_auto_waypoint.cpp). Written 2026-11-20 by Melanikus (Vasquez) for
// AVP_OpenInteng (the owner's ask through Bishop: "a drop down with environment or modules and accept a string of multiple modules"). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// CHECKS: several names (spaces, commas, semicolons, tabs between), each to its module's object index in the order typed, once; a name with the case changed when only one object
// has it so; an exact name beats a near one; an unknown name, an object that is not a module, a name two objects share, a too-long word: each NAMED, and nothing plotted (the list
// empty) even when the other words are good; nothing typed; quoted names with a space; an open quote; the refusals named at most 8 and counted after; the start words (in order,
// quoted when needed, names missing or empty left out, the letters' limit kept whole). HOSTILE: bytes that are not letters, a line over the limit.
// THE AIRDUCT TICK BOX (11-20 night): in both dialogs one check box, UNTICKED as it opens (the owner 11-20 19:06 through Bishop, after playing his gold path with it off ("It was perfect"): "Let's not auto set it anymore"), under the two counts with its label to its right, above Ok; the form read on Ok
// (the tick both ways, a form that lost a part refused); the tick and the counts to the planner's AutoPlotVolumeCounts (a count out of range named).

#include <stdio.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "alien_auto_waypoint.h"

static int checks = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static host::AlienAutoWaypointObject One(char const *name, int32_t index, bool module)
{
	host::AlienAutoWaypointObject o;
	o.name = name;
	o.index = index;
	o.module = module;
	return o;
}

int main()
{
	std::vector<host::AlienAutoWaypointObject> const level = { One("SHED05", 212, true), One("SHED01", 213, true), One("gencor01", 228, true), One("Object 16", 286, true),
		One("chair1", 900, false), One("twin", 40, true), One("TWIN", 41, true), One("Same", 50, true), One("Same", 51, true), One("near", 60, true), One("Near", 61, true),
		One("", 70, true) };
	std::vector<int32_t> got;
	std::string why;

	// several names, any gap, the order kept, each once
	bool ok = host::AlienAutoWaypoint_Parse("SHED05 gencor01,SHED01;\tSHED05", level, &got, &why);
	Check(ok && got == std::vector<int32_t>({ 212, 228, 213 }) && why.empty(), "several names, spaces / commas / semicolons / tabs between, in the order typed, each once");

	ok = host::AlienAutoWaypoint_Parse("  shed05  ", level, &got, &why);
	Check(ok && got == std::vector<int32_t>({ 212 }), "a name with its case changed, when one object has it so");

	ok = host::AlienAutoWaypoint_Parse("near", level, &got, &why);
	Check(ok && got == std::vector<int32_t>({ 60 }), "an exact name beats a near one (near, not Near)");

	ok = host::AlienAutoWaypoint_Parse("Twin", level, &got, &why);
	Check(!ok && got.empty() && why == "Twin names 2 objects", "two objects with the case ignored and none exact: named, nothing plotted: " + why);

	ok = host::AlienAutoWaypoint_Parse("Same", level, &got, &why);
	Check(!ok && got.empty() && why == "Same names 2 objects", "two objects of the very same name: named, never a guess: " + why);

	// refusals: named, and nothing plotted though other words are good
	ok = host::AlienAutoWaypoint_Parse("SHED05 nosuchmodule gencor01", level, &got, &why);
	Check(!ok && got.empty() && why == "no module named nosuchmodule", "an unknown name among good ones: named, NOTHING plotted: " + why);

	ok = host::AlienAutoWaypoint_Parse("chair1 SHED01", level, &got, &why);
	Check(!ok && got.empty() && why == "chair1 is not a module", "an object that is not a module: named, nothing plotted: " + why);

	ok = host::AlienAutoWaypoint_Parse("bad1 chair1 bad2", level, &got, &why);
	Check(!ok && why == "no module named bad1; chair1 is not a module; no module named bad2", "every refused word named, in order: " + why);

	ok = host::AlienAutoWaypoint_Parse("", level, &got, &why);
	Check(!ok && got.empty() && why == "no module names typed", "nothing typed: said: " + why);

	ok = host::AlienAutoWaypoint_Parse(" ,; \t", level, &got, &why);
	Check(!ok && why == "no module names typed", "only gaps typed: said");

	// quotes
	ok = host::AlienAutoWaypoint_Parse("\"Object 16\" SHED05", level, &got, &why);
	Check(ok && got == std::vector<int32_t>({ 286, 212 }), "a quoted name with a space");

	ok = host::AlienAutoWaypoint_Parse("SHED05 \"Object 16", level, &got, &why);
	Check(ok && got == std::vector<int32_t>({ 212, 286 }), "an open quote runs to the end of the line");

	ok = host::AlienAutoWaypoint_Parse("Object 16", level, &got, &why);
	Check(!ok && why == "no module named Object; no module named 16", "a name with a space, not quoted: its words refused by name: " + why);

	// HOSTILE
	std::string const longWord(200, 'x');
	ok = host::AlienAutoWaypoint_Parse(longWord, level, &got, &why);
	Check(!ok && got.empty() && why == std::string(24, 'x') + "... is too long", "a word over 64 letters: refused, cut in the words: " + why);

	ok = host::AlienAutoWaypoint_Parse(std::string("SHED05 \x01\x7f\xff"), level, &got, &why);
	Check(!ok && why == "no module named ???", "bytes that are not letters are shown as ?: " + why);

	std::string many;
	for (int i = 0; i < 11; i++)
	{
		many += "bad" + std::to_string(i) + " ";
	}
	ok = host::AlienAutoWaypoint_Parse(many, level, &got, &why);
	Check(!ok && why.find("no module named bad7") != std::string::npos && why.find("bad8") == std::string::npos && why.size() > 9 &&
			why.compare(why.size() - 10, 10, "and 3 more") == 0, "at most 8 refusals named, the rest counted: " + why);

	std::string longLine(host::kAlienAutoWaypointMostLetters, ' ');
	longLine += "SHED05";
	ok = host::AlienAutoWaypoint_Parse(longLine, level, &got, &why);
	Check(!ok && why == "no module names typed", "the line is read up to its 2000 letters only (a name past them is not read)");

	ok = host::AlienAutoWaypoint_Parse("SHED05", level, nullptr, &why);
	Check(!ok, "no list given: false");

	// the start words
	std::map<int32_t, std::string> const names = { { 212, "SHED05" }, { 286, "Object 16" }, { 70, "" }, { 300, "say\"q\"" }, { 301, "a,b" } };
	Check(host::AlienAutoWaypoint_StartWords({ 212, 286, 70, 999 }, names) == "SHED05 \"Object 16\"", "the start words: in order, a name with a space quoted, empty and unknown ones left out");
	Check(host::AlienAutoWaypoint_StartWords({ 300, 301 }, names) == "sayq \"a,b\"", "a double quote in a name is left out, a comma quoted");
	Check(host::AlienAutoWaypoint_StartWords({}, names).empty(), "no modules: no words");

	std::map<int32_t, std::string> big;
	std::vector<int32_t> all;
	for (int32_t i = 0; i < 400; i++)
	{
		big[i] = "module" + std::to_string(1000 + i);
		all.push_back(i);
	}
	std::string const start = host::AlienAutoWaypoint_StartWords(all, big);
	Check(start.size() <= host::kAlienAutoWaypointMostLetters && start.size() > host::kAlienAutoWaypointMostLetters - 12 && start.compare(start.size() - 4, 4, start.substr(start.size() - 4)) == 0 &&
			start[start.size() - 1] != ' ', "the start words keep to the 2000 letters, whole names only (" + std::to_string(start.size()) + " letters)");

	// the round trip: the start words parse back to the same modules
	std::vector<host::AlienAutoWaypointObject> const named = { One("SHED05", 212, true), One("Object 16", 286, true), One("a,b", 301, true) };
	ok = host::AlienAutoWaypoint_Parse(host::AlienAutoWaypoint_StartWords({ 286, 301, 212 }, { { 212, "SHED05" }, { 286, "Object 16" }, { 301, "a,b" } }), named, &got, &why);
	Check(ok && got == std::vector<int32_t>({ 286, 301, 212 }), "the start words parse back to the same modules, in order");

	// the counts (the owner's "climb-ins and get-outs", the plot dialog's tunable counts): a whole number in its range, empty is the default, anything else NAMED
	int32_t n = -7;
	Check(host::AlienAutoWaypoint_ParseCount("3", 1, 16, 2, "Climb-ins", &n, &why) && n == 3 && why.empty(), "a count typed: 3");
	n = -7;
	Check(host::AlienAutoWaypoint_ParseCount("  12 \t", 1, 16, 2, "Climb-ins", &n, &why) && n == 12, "spaces round a count are fine");
	n = -7;
	Check(host::AlienAutoWaypoint_ParseCount("", 1, 16, 2, "Climb-ins", &n, &why) && n == 2, "nothing typed: the planner's default");
	n = -7;
	Check(host::AlienAutoWaypoint_ParseCount("   ", 0, 16, 4, "Get-outs", &n, &why) && n == 4, "only spaces: the default");
	Check(host::AlienAutoWaypoint_ParseCount("1", 1, 16, 2, "Climb-ins", &n, &why) && n == 1 && host::AlienAutoWaypoint_ParseCount("16", 1, 16, 2, "Climb-ins", &n, &why) && n == 16,
		"the range's two ends are taken");
	n = -7;
	ok = host::AlienAutoWaypoint_ParseCount("0", 1, 16, 2, "Climb-ins", &n, &why);
	Check(!ok && n == -7 && why == "Climb-ins: '0' is not a whole number from 1 to 16", "under the range: named, nothing set: " + why);
	ok = host::AlienAutoWaypoint_ParseCount("17", 1, 16, 2, "Climb-ins", &n, &why);
	Check(!ok && why == "Climb-ins: '17' is not a whole number from 1 to 16", "over the range: named: " + why);
	ok = host::AlienAutoWaypoint_ParseCount("-3", 0, 16, 2, "Get-outs", &n, &why);
	Check(!ok && why == "Get-outs: '-3' is not a whole number from 0 to 16", "a sign: refused, named: " + why);
	ok = host::AlienAutoWaypoint_ParseCount("2.5", 0, 16, 2, "Get-outs", &n, &why);
	Check(!ok && why.find("'2.5'") != std::string::npos, "a fraction: refused");
	ok = host::AlienAutoWaypoint_ParseCount("three", 0, 16, 2, "Get-outs", &n, &why);
	Check(!ok && why.find("'three'") != std::string::npos, "letters: refused");
	ok = host::AlienAutoWaypoint_ParseCount("1 2", 0, 16, 2, "Get-outs", &n, &why);
	Check(!ok, "two numbers: refused");
	ok = host::AlienAutoWaypoint_ParseCount("99999999999999999999", 0, 16, 2, "Get-outs", &n, &why);
	Check(!ok && why == "Get-outs: '99999999999999999999' is not a whole number from 0 to 16", "HOSTILE: twenty digits refused (more than 9: never read as a number, no overflow): " + why);
	ok = host::AlienAutoWaypoint_ParseCount(std::string(30, '9'), 0, 16, 2, "Get-outs", &n, &why);
	Check(!ok && why == "Get-outs: '" + std::string(24, '9') + "...' is not a whole number from 0 to 16", "HOSTILE: thirty digits: the words cut at 24: " + why);
	ok = host::AlienAutoWaypoint_ParseCount("000000004", 0, 16, 2, "Get-outs", &n, &why);
	Check(ok && n == 4, "nine digits with leading zeros: read");
	ok = host::AlienAutoWaypoint_ParseCount(std::string("4\x01"), 0, 16, 2, "Get-outs", &n, &why);
	Check(!ok && why == "Get-outs: '4?' is not a whole number from 0 to 16", "a byte that is no letter: shown as ?, refused: " + why);
	Check(!host::AlienAutoWaypoint_ParseCount("3", 0, 16, 2, "Get-outs", nullptr, &why), "no value given: false");

	// THE AIRDUCT TICK BOX (the owner 11-20 night; unticked by default since the owner 11-20 19:06 through Bishop, after playing his gold path with it off ("It was perfect"): "Let's not auto set it anymore"): inteng's own check box (FUN_0057d670, 12 x 12) with its label to its right, under
	// the two counts, in both dialogs, unticked as it opens; its tick is the planner's counts.airduct
	for (int withNames = 0; withNames < 2; withNames++)
	{
		std::string const which = withNames ? "Modules" : "Environment";
		host::DialogFormSpec const form = host::AlienAutoWaypoint_PlotForm(withNames == 1, "SHED05 gencor01");
		int boxes = 0, edits = 0;
		bool ticked = false;
		int32_t group = 0;
		host::DialogRect lastEdit, box, label, ok;
		std::string labelWords;

		for (size_t i = 0; i < form.widgets.size(); i++)
		{
			host::DialogWidget const &w = form.widgets[i];

			if (w.type == host::DialogWidgetType::Edit)
			{
				edits++;
				lastEdit = w.box;
			}
			else if (w.type == host::DialogWidgetType::Check)
			{
				boxes++;
				box = w.box;
				ticked = w.checked;
				group = w.group;

				if (i + 1 < form.widgets.size() && form.widgets[i + 1].type == host::DialogWidgetType::Label)
				{
					label = form.widgets[i + 1].box;
					labelWords = form.widgets[i + 1].text;
				}
			}
			else if (w.type == host::DialogWidgetType::Button && w.text == "Ok")
			{
				ok = w.box;
			}
		}

		Check(boxes == 1 && !ticked && group < 0, which + ": one check box, unticked as it opens, in no radio group");
		Check(edits == (withNames ? 3 : 2), which + ": its fields as before (" + std::to_string(edits) + ")");
		Check(box.x == 0x1e && box.y == lastEdit.y + 0x18 && box.width == 12 && box.height == 12, which + ": the box under the two counts, at the fields' x, 12 x 12 (at " +
			std::to_string(box.x) + ", " + std::to_string(box.y) + ")");
		Check(labelWords.compare(0, 7, "Airduct") == 0 && label.x == box.x + 0x10 && label.y == box.y, which + ": its label to its right: '" + labelWords + "'");
		Check(box.y + box.height <= ok.y, which + ": above Ok");
	}

	// the form read on Ok: the typed words and the tick; the tick to the planner's counts
	host::DialogFormSpec form = host::AlienAutoWaypoint_PlotForm(true, "SHED05 gencor01");
	host::AlienAutoWaypointPlotAnswers answers;
	ok = host::AlienAutoWaypoint_ReadPlotForm(true, form.widgets, &answers);
	Check(ok && answers.names == "SHED05 gencor01" && answers.climbIns == "1" && answers.getOuts == "1" && !answers.airduct, "Modules' form read as it opens: its names, 1 and 1, Airduct unticked");
	host::AutoPlotVolumeCounts counts;
	Check(!counts.airduct, "the planner's own default: no Airduct (set only when the box is ticked)");
	counts.airduct = true;
	Check(host::AlienAutoWaypoint_PlotCounts(answers, &counts, &why) && !counts.airduct && counts.climbIns == 1 && counts.getOuts == 1, "unticked: the planner's counts have no Airduct");

	for (host::DialogWidget &w : form.widgets)
	{
		if (w.type == host::DialogWidgetType::Check)
		{
			w.checked = true;
		}
	}

	ok = host::AlienAutoWaypoint_ReadPlotForm(true, form.widgets, &answers);
	Check(ok && answers.airduct, "ticked: read as Airduct on");
	counts = host::AutoPlotVolumeCounts();
	Check(host::AlienAutoWaypoint_PlotCounts(answers, &counts, &why) && counts.airduct, "ticked: the planner's counts have Airduct");

	host::DialogFormSpec const environment = host::AlienAutoWaypoint_PlotForm(false, "SHED05");
	ok = host::AlienAutoWaypoint_ReadPlotForm(false, environment.widgets, &answers);
	Check(ok && answers.names.empty() && answers.climbIns == "1" && answers.getOuts == "1" && !answers.airduct, "Environment's form read: no names, 1 and 1, Airduct unticked");

	std::vector<host::DialogWidget> lost;

	for (host::DialogWidget const &w : environment.widgets)
	{
		if (w.type != host::DialogWidgetType::Check)
		{
			lost.push_back(w);
		}
	}

	Check(!host::AlienAutoWaypoint_ReadPlotForm(false, lost, &answers), "a form that lost its check box: not read (nothing plotted)");
	Check(!host::AlienAutoWaypoint_ReadPlotForm(true, environment.widgets, &answers), "Environment's parts read as Modules' (a field short): not read");
	Check(!host::AlienAutoWaypoint_ReadPlotForm(true, form.widgets, nullptr), "no answers given: false");

	answers.climbIns = "19";
	counts = host::AutoPlotVolumeCounts();
	Check(!host::AlienAutoWaypoint_PlotCounts(answers, &counts, &why) && why == "Climb-ins: '19' is not a whole number from 0 to 8", "a count out of range: named: " + why);
	answers.climbIns = "2";
	answers.getOuts = "x";
	Check(!host::AlienAutoWaypoint_PlotCounts(answers, &counts, &why) && why == "Get-outs: 'x' is not a whole number from 0 to 8", "a get-outs count refused: named: " + why);
	Check(!host::AlienAutoWaypoint_PlotCounts(answers, nullptr, &why), "no counts given: false");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
