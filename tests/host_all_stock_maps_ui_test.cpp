// The test for All stock maps' words, form, progress line and report (src/host/all_stock_maps_ui.h). Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng
// (the owner's wishlist item 5: "Give them the full options bellow our disclaimer, make our disclaimer shorter"). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// THE FORM: the short disclaimer on top, then Environment's OWN form (alien_auto_waypoint.h AlienAutoWaypoint_PlotForm(false, "")) part for part, moved down under
// the words and nothing else changed, every part inside the window, and its options read back as Environment reads them (defaults, and a tick that was changed).
// THE PROGRESS LINE, THE END WORDS AND THE REPORT PAGES: what each says, every line inside the screen, every page inside its lines, nothing lost between pages.
// THE END STAGE: plain "done" adds nothing; "done, but the level open before ... could not be opened again" is said on the status line, in the pages
// (under the count) and in the log (last), as the safe text rule makes it.
// "SAVE YOUR LEVEL FIRST?" (Dietrich 2026-11-23, Bishop 2's order after the owner's first real test): the question first, what each answer does, Yes / No /
// Cancel answering Ok / No / Escape, every part inside inteng's box, the buttons under the words and apart, and a taller box when the letters are taller.
// THE LOG LINES: the count, then a line a level, each "All stock maps: <HEADING> <file>...". A HOSTILE NAME (control bytes, a new line, "%s"): every byte
// outside the space to the tilde shown and logged as '?', "%s" as the letters it is.
//
// Pure: every letter 8 pixels wide, a line 16 high. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "alien_auto_waypoint.h"
#include "all_stock_maps_ui.h"

using namespace host;

static int checksFailed = 0;
static int checksRun = 0;

static void Check(bool good, std::string const &what)
{
	checksRun++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static int32_t Eight(unsigned char)
{
	return 8;
}

static std::vector<std::string> Lines(std::string const &text)
{
	std::vector<std::string> lines;
	size_t from = 0;

	for (;;)
	{
		size_t const end = text.find('\n', from);
		lines.push_back(text.substr(from, end == std::string::npos ? std::string::npos : end - from));

		if (end == std::string::npos)
		{
			return lines;
		}

		from = end + 1;
	}
}

static bool Inside(DialogRect const &part, int32_t width, int32_t height)
{
	return part.x >= 0 && part.y >= 0 && part.x + part.width <= width && part.y + part.height <= height;
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

static void TheWords()
{
	std::string const words = kAllStockMapsWords;

	Check(words.find("All stock maps adds alien wall and ceiling waypoints to every Marine and Predator campaign level") == 0,
		"the disclaimer says first what it does, and to which levels");
	Check(Has(words, "30 to 45 minutes"), "it says how long it takes (Vasquez's plan: 30 to 45 minutes)");
	Check(Has(words, "<level>.rif.bak") && Has(words, "an existing backup is never replaced"), "it says each level is backed up first, and an existing backup is never replaced");
	Check(Has(words, "With Overwrite ticked, the alien waypoints already in every level are replaced"),
		"it says what the Overwrite tick does to every level (Dietrich: the tick is the plot's answer, no question opens during the run)");
	Check(Has(words, "aliens may sometimes get stuck"), "it says it isn't tested on every map");
	Check(Lines(words).size() <= 4, "it is short (the owner: \"make our disclaimer shorter\"): " + std::to_string(Lines(words).size()) + " paragraphs' lines");
}

static void TheForm()
{
	DialogFormSpec const environment = AlienAutoWaypoint_PlotForm(false, std::string());
	DialogFormSpec const spec = AllStockMaps_Form(environment, Eight, 16);

	Check(spec.title == "All stock maps", "the form's title is the menu item's");
	Check(spec.width == environment.width, "the window is Environment's own width (" + std::to_string(spec.width) + ")");
	Check(spec.widgets.size() == environment.widgets.size() + 1 && !spec.widgets.empty() && spec.widgets[0].type == DialogWidgetType::Label,
		"the words, then Environment's parts (" + std::to_string(environment.widgets.size()) + ")");

	if (spec.widgets.size() != environment.widgets.size() + 1 || environment.widgets.empty())
	{
		return;
	}

	DialogWidget const &words = spec.widgets[0];
	std::vector<std::string> const lines = Lines(words.text);
	bool fits = true;

	for (std::string const &line : lines)
	{
		fits = fits && 1 + (int32_t)line.size() * 8 <= words.box.width;   // drawn from the box's x + 1 (dialogs.cpp)
	}

	Check(fits, "every line of the words is drawn inside the words' box (" + std::to_string(lines.size()) + " lines)");
	Check(words.box.height == (int32_t)lines.size() * 16 && Inside(words.box, spec.width, spec.height), "the words' box is inside the window, a line 16 high");

	// Environment's parts, each as it was, only lower; none of them over the words
	int32_t const down = spec.widgets[1].box.y - environment.widgets[0].box.y;
	bool same = down > 0;
	bool under = true;
	bool inside = true;

	for (size_t k = 0; k < environment.widgets.size(); k++)
	{
		DialogWidget const &was = environment.widgets[k];
		DialogWidget const &now = spec.widgets[k + 1];

		same = same && now.type == was.type && now.text == was.text && now.checked == was.checked && now.answer == was.answer && now.most == was.most &&
			now.box.x == was.box.x && now.box.width == was.box.width && now.box.height == was.box.height && now.box.y == was.box.y + down;
		under = under && now.box.y >= words.box.y + words.box.height;
		inside = inside && Inside(now.box, spec.width, spec.height);
	}

	Check(same, "Environment's parts are all there, in order, each as it was, " + std::to_string(down) + " pixels lower");
	Check(under, "none of them is over the words");
	Check(inside && spec.height == environment.height + down, "every part is inside the window, and the window grew by the words (" + std::to_string(spec.height) +
		" high, Environment's " + std::to_string(environment.height) + ")");
	Check(spec.height <= 600, "the form fits a window 600 pixels tall, its known limit (all_stock_maps_ui.h; Bishop 2 11-22): " + std::to_string(spec.height));

	// Ok reads the options as Environment reads them
	AlienAutoWaypointPlotAnswers mine;
	AlienAutoWaypointPlotAnswers theirs;
	bool const readMine = AlienAutoWaypoint_ReadPlotForm(false, AllStockMaps_OptionsOf(spec.widgets), &mine);
	bool const readTheirs = AlienAutoWaypoint_ReadPlotForm(false, environment.widgets, &theirs);

	Check(readMine && readTheirs && mine.climbIns == theirs.climbIns && mine.getOuts == theirs.getOuts && mine.airduct == theirs.airduct &&
		mine.overwrite == theirs.overwrite, "Ok with nothing changed reads Environment's own defaults (climb-ins \"" + mine.climbIns + "\", get-outs \"" + mine.getOuts + "\")");

	std::vector<DialogWidget> ticked = spec.widgets;
	size_t checks = 0;

	for (DialogWidget &part : ticked)
	{
		if (part.type == DialogWidgetType::Check && ++checks == 2)
		{
			part.checked = true;   // the second box: Overwrite already linked modules
		}
	}

	AlienAutoWaypointPlotAnswers tickedAnswers;
	Check(AlienAutoWaypoint_ReadPlotForm(false, AllStockMaps_OptionsOf(ticked), &tickedAnswers) && tickedAnswers.overwrite && !tickedAnswers.airduct,
		"a box ticked in the form is read as ticked (Overwrite already linked modules)");

	std::vector<DialogWidget> wordless(spec.widgets.begin() + 1, spec.widgets.end());
	Check(AllStockMaps_OptionsOf(wordless).empty() && AllStockMaps_OptionsOf(std::vector<DialogWidget>()).empty(),
		"parts that don't start with the disclaimer give no options (then nothing is read, and nothing runs)");
}

static void TheProgressLine()
{
	AllStockMapsProgress progress;

	progress.level = 7;
	progress.levels = 22;
	progress.file = "genshd1.RIF";
	progress.stage = "plotting AI module 3 of 40";

	std::string const words = AllStockMaps_ProgressWords(progress);
	Check(words == "All stock maps: level 7 of 22, genshd1.RIF: plotting AI module 3 of 40", "the progress line: \"" + words + "\"");

	progress.stopping = true;
	Check(AllStockMaps_ProgressWords(progress) == words + "; stopping after this level (Esc)", "once Esc was pressed it says the run stops after this level");

	AllStockMapsProgress before;
	before.levels = 22;
	Check(AllStockMaps_ProgressWords(before) == "All stock maps: level 0 of 22", "before the first level: \"" + AllStockMaps_ProgressWords(before) + "\"");

	// THE LEAD (the alpha.6 cut): the words before the stage, put in front of the plot's own lines while the run goes; the progress line always starts with it, so
	// the run's own line is never led twice
	progress.stopping = false;
	Check(AllStockMaps_ProgressLead(progress) == "All stock maps: level 7 of 22, genshd1.RIF: ", "the lead: \"" + AllStockMaps_ProgressLead(progress) + "\"");
	Check(words.compare(0, AllStockMaps_ProgressLead(progress).size(), AllStockMaps_ProgressLead(progress)) == 0, "and the progress line starts with it");

	AllStockMapsProgress ending;
	ending.level = 16;
	ending.levels = 22;
	ending.stage = "opening the level before";
	Check(AllStockMaps_ProgressLead(ending) == "All stock maps: level 16 of 22: " &&
		AllStockMaps_ProgressWords(ending) == "All stock maps: level 16 of 22: opening the level before", "with no file (the run's end) too: \"" +
		AllStockMaps_ProgressWords(ending) + "\"");

	AllStockMapsProgress hostile;
	hostile.level = 1;
	hostile.levels = 22;
	hostile.file = "a\nb.RIF";
	Check(AllStockMaps_ProgressLead(hostile) == "All stock maps: level 1 of 22, a?b.RIF: ", "a file name's control bytes come out as '?' in the lead too");
}

static AllStockMapsLevel Level(char const *file, StockMapOutcome outcome, char const *why, bool made, bool kept, int32_t steams)
{
	AllStockMapsLevel level;

	level.file = file;
	level.outcome = outcome;
	level.why = why;
	level.backupMade = made;
	level.backupKept = kept;
	level.steamsOwn = steams;
	return level;
}

static void TheReport()
{
	std::vector<AllStockMapsLevel> report;

	report.push_back(Level("derelict.RIF", StockMapOutcome::Changed, "40 AI modules written, 2 kept, already linked", true, false, 1));
	report.push_back(Level("genshd1.RIF", StockMapOutcome::Changed, "31 AI modules written", false, true, 0));
	report.push_back(Level("hadleyshope.RIF", StockMapOutcome::Skipped, "missing", false, false, -1));
	report.push_back(Level("leadworks.RIF", StockMapOutcome::Refused, "the file changed on disk while open", true, false, 1));
	report.push_back(Level("invasion.RIF", StockMapOutcome::Failed, "the plot found no AI modules", true, false, 1));
	report.push_back(Level("furyall.RIF", StockMapOutcome::NotStarted, "", false, false, -1));
	report.push_back(Level("temple.RIF", StockMapOutcome::NotStarted, "", false, false, -1));

	std::string const end = AllStockMaps_EndWords(report);
	Check(end == "All stock maps: 2 of 7 levels changed, 1 skipped, 1 refused, 1 failed, 2 not started (the report is open)", "the end words: \"" + end + "\"");

	int32_t const screenWidth = 600;
	size_t const linesPerPage = 8;
	std::vector<std::string> const pages = AllStockMaps_ReportPages(report, Eight, screenWidth, linesPerPage);
	std::string all;
	bool wide = false;
	bool tall = false;
	bool said = true;

	for (size_t p = 0; p < pages.size(); p++)
	{
		std::vector<std::string> const lines = Lines(pages[p]);
		bool const last = p + 1 == pages.size();

		tall = tall || lines.size() > linesPerPage;
		said = said && (last ? lines.back() != "(more on the next page)" : lines.back() == "(more on the next page)");

		for (size_t k = 0; k < lines.size(); k++)
		{
			wide = wide || (int32_t)lines[k].size() * 8 > screenWidth;

			if (last || k + 1 < lines.size())
			{
				all += lines[k] + "\n";
			}
		}
	}

	Check(pages.size() > 1, "a long report takes more than one page (" + std::to_string(pages.size()) + ")");
	Check(!wide && !tall, "every line is inside the screen's width, every page inside its lines");
	Check(said, "every page but the last ends \"(more on the next page)\", and the last doesn't");
	Check(all.find("ALL STOCK MAPS: 2 of 7 levels changed.") == 0, "the report starts with its count");
	Check(Has(all, "STOPPED (Esc): 2 levels were not started"), "a stopped run says how many levels weren't started");

	size_t const changed = all.find("CHANGED");
	size_t const skipped = all.find("SKIPPED");
	size_t const refused = all.find("REFUSED");
	size_t const failed = all.find("FAILED");
	size_t const notStarted = all.find("NOT STARTED: furyall.RIF, temple.RIF.");
	size_t const undo = all.find("TO UNDO A LEVEL");

	Check(changed < skipped && skipped < refused && refused < failed && failed < notStarted && notStarted < undo && undo != std::string::npos,
		"CHANGED, SKIPPED, REFUSED, FAILED, NOT STARTED and how to undo a level, in that order");

	// a level's own line may wrap; read it with its wrap undone
	std::string flat = all;

	for (size_t at = flat.find('\n'); at != std::string::npos; at = flat.find('\n', at + 1))
	{
		flat[at] = ' ';
	}

	Check(Has(flat, "derelict.RIF: 40 AI modules written, 2 kept, already linked; backup derelict.RIF.bak made"), "a changed level: what the plot did, and its new backup");
	Check(Has(flat, "genshd1.RIF: 31 AI modules written; the older genshd1.RIF.bak kept as it was; NOT STEAM'S OWN before the run"),
		"a changed level that had a backup: the older one kept; and one that wasn't Steam's own says so");
	Check(!Has(flat, "derelict.RIF: 40 AI modules written, 2 kept, already linked; backup derelict.RIF.bak made; NOT STEAM'S OWN"),
		"a level that was Steam's own doesn't say it wasn't");
	Check(Has(flat, "hadleyshope.RIF: missing") && Has(flat, "leadworks.RIF: the file changed on disk while open") && Has(flat, "invasion.RIF: the plot found no AI modules"),
		"every level not changed says why");

	std::vector<AllStockMapsLevel> whole;
	whole.push_back(Level("derelict.RIF", StockMapOutcome::Changed, "40 AI modules written", true, false, 1));
	std::vector<std::string> const one = AllStockMaps_ReportPages(whole, Eight, screenWidth, 40);
	Check(one.size() == 1 && !Has(one[0], "STOPPED") && !Has(one[0], "NOT STARTED") && !Has(one[0], "SKIPPED"), "a whole run on one page: no STOPPED, and no empty sections");

	std::vector<AllStockMapsLevel> none(3);
	none[0].file = "A.RIF";
	none[1].file = "B.RIF";
	none[2].file = "C.RIF";
	std::vector<std::string> const stoppedFirst = AllStockMaps_ReportPages(none, Eight, screenWidth, 40);
	Check(stoppedFirst.size() == 1 && Has(stoppedFirst[0], "NOT STARTED: A.RIF, B.RIF, C.RIF.") && !Has(stoppedFirst[0], "STOPPED (Esc)"),
		"stopped before its first level: every level named not started");

	Check(!AllStockMaps_ReportPages(report, Eight, 16, 1).empty(), "a screen narrower than a word and one line a page still gives pages, never an endless loop");

	// THE LOG: the count, then every level in the run's order, each line carrying "All stock maps: " and its heading
	std::vector<std::string> const log = AllStockMaps_LogLines(report);
	bool marked = true;

	for (std::string const &line : log)
	{
		marked = marked && line.find("All stock maps: ") == 0 && line.find('\n') == std::string::npos;
	}

	Check(log.size() == report.size() + 1 && marked, "the log: a count line and a line a level (" + std::to_string(log.size()) + "), each starting \"All stock maps: \", none broken");
	Check(log.size() == 8 && log[0] == "All stock maps: the run ended: 2 of 7 levels changed, 1 skipped, 1 refused, 1 failed, 2 not started", "the log's count: \"" +
		(log.empty() ? std::string() : log[0]) + "\"");
	Check(log.size() == 8 && log[1] == "All stock maps: CHANGED derelict.RIF: 40 AI modules written, 2 kept, already linked; backup derelict.RIF.bak made" &&
		log[2] == "All stock maps: CHANGED genshd1.RIF: 31 AI modules written; the older genshd1.RIF.bak kept as it was; NOT STEAM'S OWN before the run (its backup holds it as it was)" &&
		log[3] == "All stock maps: SKIPPED hadleyshope.RIF: missing" && log[4] == "All stock maps: REFUSED leadworks.RIF: the file changed on disk while open" &&
		log[5] == "All stock maps: FAILED invasion.RIF: the plot found no AI modules" && log[6] == "All stock maps: NOT STARTED furyall.RIF" &&
		log[7] == "All stock maps: NOT STARTED temple.RIF", "the log names every level with its heading, why, and its backup, as the report does");

	// A HOSTILE NAME: a level file or a reason with control bytes, a new line, a high byte and a format
	std::vector<AllStockMapsLevel> hostile;
	hostile.push_back(Level("evil\x1b[2J\n\xff%s.RIF", StockMapOutcome::Failed, "bad\r\nline %n\x07", true, false, 1));

	std::vector<std::string> const hostileLog = AllStockMaps_LogLines(hostile);
	std::vector<std::string> const hostilePages = AllStockMaps_ReportPages(hostile, Eight, screenWidth, 40);
	std::string shown = hostilePages.empty() ? std::string() : hostilePages[0];
	bool clean = hostileLog.size() == 2;

	for (std::string const &line : hostileLog)
	{
		for (char const letter : line)
		{
			clean = clean && (unsigned char)letter >= 32 && (unsigned char)letter <= 126;
		}
	}

	for (char const letter : shown)
	{
		clean = clean && (letter == '\n' || ((unsigned char)letter >= 32 && (unsigned char)letter <= 126));
	}

	Check(clean, "a hostile name or reason: every byte outside the space to the tilde is gone from the log and the pages");
	Check(hostileLog.size() == 2 && hostileLog[1] == "All stock maps: FAILED evil?[2J??%s.RIF: bad??line %n?",
		"... each one a '?', and \"%s\" is the letters it is: \"" + (hostileLog.size() == 2 ? hostileLog[1] : std::string()) + "\"");

	// THE END STAGE
	std::string const endFailed = "done, but the level open before, genshd1.RIF, could not be opened again: the file is missing";

	Check(AllStockMaps_EndWords(report, "done") == end && AllStockMaps_EndWords(report, "") == end && AllStockMaps_LogLines(report, "done") == log &&
		AllStockMaps_ReportPages(report, Eight, screenWidth, linesPerPage, "done") == pages, "a plain \"done\" end adds nothing anywhere");
	Check(AllStockMaps_EndWords(report, endFailed) == "All stock maps: 2 of 7 levels changed, 1 skipped, 1 refused, 1 failed, 2 not started; " + endFailed +
		" (the report is open)", "the status line says when the level open before couldn't be opened again: \"" + AllStockMaps_EndWords(report, endFailed) + "\"");

	std::vector<std::string> const failedLog = AllStockMaps_LogLines(report, endFailed);
	Check(failedLog.size() == log.size() + 1 && failedLog.back() == "All stock maps: " + endFailed, "... and the log, last: \"" + failedLog.back() + "\"");

	std::vector<std::string> const failedPages = AllStockMaps_ReportPages(whole, Eight, screenWidth, 40, endFailed);
	std::vector<std::string> const failedLines = failedPages.empty() ? std::vector<std::string>() : Lines(failedPages[0]);
	std::string flatPage = failedPages.empty() ? std::string() : failedPages[0];

	for (size_t at = flatPage.find('\n'); at != std::string::npos; at = flatPage.find('\n', at + 1))
	{
		flatPage[at] = ' ';
	}

	Check(failedLines.size() > 1 && failedLines[1].find("AT THE END: done, but the level open before") == 0 &&
		Has(flatPage, "could not be opened again: the file is missing."), "... and the report, right under its count");

	std::string const hostileEnd = AllStockMaps_EndWords(whole, "done, but \x1b[2J\nX%s");
	Check(hostileEnd == "All stock maps: 1 of 1 levels changed; done, but ?[2J?X%s (the report is open)", "a hostile end stage as the safe text rule makes it: \"" +
		hostileEnd + "\"");

	// A FULL PATH IN A REASON (Vasquez's review, SHOULD: LevelSave_Over's and the reopen's words end with the target's whole path): the pages (inteng's
	// text screen, which the owner records) show only the file's name; the log keeps the path
	std::vector<AllStockMapsLevel> pathy;
	pathy.push_back(Level("derelict.RIF", StockMapOutcome::Refused, "the save refused it: C:\\Users\\someone\\Games\\avp_rifs\\derelict.RIF", true, false, 1));
	std::string const pathyEnd = "done, but the level open before, genshd1.RIF, could not be opened again: C:\\Users\\someone\\Games\\avp_rifs\\genshd1.RIF";
	std::vector<std::string> const pathyPages = AllStockMaps_ReportPages(pathy, Eight, screenWidth, 40, pathyEnd);
	std::vector<std::string> const pathyLog = AllStockMaps_LogLines(pathy, pathyEnd);
	std::string pathyPage = pathyPages.empty() ? std::string() : pathyPages[0];

	for (size_t at = pathyPage.find('\n'); at != std::string::npos; at = pathyPage.find('\n', at + 1))
	{
		pathyPage[at] = ' ';
	}

	Check(!Has(pathyPage, "someone") && !Has(pathyPage, "C:\\") && Has(pathyPage, "derelict.RIF: the save refused it: derelict.RIF") &&
		Has(pathyPage, "could not be opened again: genshd1.RIF."), "a full path in a reason or the end stage: the pages show only its file's name");
	Check(!Has(AllStockMaps_EndWords(pathy, pathyEnd), "someone"), "... and so does the status line's end: \"" + AllStockMaps_EndWords(pathy, pathyEnd) + "\"");
	Check(pathyLog.size() == 3 && Has(pathyLog[1], "C:\\Users\\someone\\Games\\avp_rifs\\derelict.RIF") && Has(pathyLog[2], "avp_rifs\\genshd1.RIF"),
		"... while the log keeps the whole path");

	AllStockMapsProgress hostileProgress;
	hostileProgress.level = 1;
	hostileProgress.levels = 22;
	hostileProgress.file = "a\nb.RIF";
	hostileProgress.stage = "plot\x1b";
	Check(AllStockMaps_ProgressWords(hostileProgress) == "All stock maps: level 1 of 22, a?b.RIF: plot?", "the progress line too: \"" +
		AllStockMaps_ProgressWords(hostileProgress) + "\"");
}

static std::string OnOneLine(std::string text)
{
	for (char &letter : text)
	{
		letter = letter == '\n' ? ' ' : letter;
	}

	return text;
}

// the question asked after Ok when the open level has changes not saved (all_stock_maps_ui.h AllStockMaps_SaveFirstForm)
static void TheSaveFirstQuestion()
{
	puts("Save your level first?");

	for (int32_t const lineHeight : { 16, 40 })
	{
		DialogFormSpec const spec = AllStockMaps_SaveFirstForm(Eight, lineHeight);
		std::string const at = " (lines " + std::to_string(lineHeight) + " high)";

		Check(spec.widgets.size() == 4 && spec.widgets[0].type == DialogWidgetType::Label, "the words, then three buttons" + at);

		if (spec.widgets.size() != 4)
		{
			continue;
		}

		DialogWidget const &words = spec.widgets[0];
		std::string const said = OnOneLine(words.text);

		Check(spec.title == "All stock maps" && spec.width == 400 && spec.height >= 200, "inteng's Yes / No box, 400 across and at least 200 down, titled " +
			spec.title + ": " + std::to_string(spec.width) + " x " + std::to_string(spec.height) + at);
		Check(Lines(words.text)[0] == "Save your level first?", "the question comes first, on its own line: \"" + Lines(words.text)[0] + "\"");
		Check(said.find("Yes saves it, then runs.") != std::string::npos && said.find("No runs without saving it: its changes are lost.") != std::string::npos &&
			said.find("Cancel runs nothing.") != std::string::npos, "and what each answer does, in plain words: \"" + said + "\"");

		bool wrappedInside = true;

		for (std::string const &line : Lines(words.text))
		{
			wrappedInside = wrappedInside && (int32_t)line.size() * 8 <= words.box.width;
		}

		Check(wrappedInside && words.box.height == (int32_t)Lines(words.text).size() * lineHeight, "every line of the words fits their box, a line " +
			std::to_string(lineHeight) + " high" + at);

		static char const *const names[] = { "Yes", "No", "Cancel" };
		static int32_t const answers[] = { kDialogOk, kDialogNo, kDialogEscape };
		bool inside = words.box.x >= 0 && words.box.x + words.box.width <= spec.width && words.box.y >= 0 && words.box.y + words.box.height <= spec.height;
		bool named = true;
		bool under = true;
		bool apart = true;

		for (size_t b = 0; b < 3; b++)
		{
			DialogWidget const &button = spec.widgets[1 + b];

			named = named && button.type == DialogWidgetType::Button && button.text == names[b] && button.answer == answers[b] &&
				(int32_t)button.text.size() * 8 <= button.box.width;
			inside = inside && button.box.x >= 0 && button.box.x + button.box.width <= spec.width && button.box.y + button.box.height <= spec.height;
			under = under && button.box.y >= words.box.y + words.box.height;
			apart = apart && (b == 0 || spec.widgets[b].box.x + spec.widgets[b].box.width < button.box.x);
		}

		Check(named, "Yes, No and Cancel, in that order, answering Ok, No and Escape, each wide enough for its word" + at);
		Check(inside, "every part inside the window" + at);
		Check(under && apart, "the buttons under the words, side by side and apart" + at);
	}
}

int main()
{
	TheWords();
	TheForm();
	TheProgressLine();
	TheReport();
	TheSaveFirstQuestion();

	std::string const wrapped = AllStockMaps_Wrapped("abc def\n\nghi", Eight, 40);
	Check(wrapped == "abc\ndef\n\nghi", "words wrap at spaces, and a paragraph's empty line is kept: \"" + wrapped + "\"");
	Check(Lines(AllStockMaps_Wrapped(std::string(30, 'x'), Eight, 4)).size() == 30, "a line narrower than a letter gives a letter a line, never an endless loop");

	printf("checks %d; failed %d\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
