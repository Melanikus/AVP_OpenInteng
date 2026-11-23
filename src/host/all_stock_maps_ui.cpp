// all_stock_maps_ui.cpp: "All stock maps", its words, its form, its progress line and its report (all_stock_maps_ui.h says what and why). Written
// 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "all_stock_maps_ui.h"

#include <string.h>

#include "panel_paint.h"   // PanelPaint_SafeText: the editor's one rule for safe text
#include "screen_names.h"  // ScreenNames_Cut: a full path shown as its last part, as the status line shows it

namespace host
{
	char const kAllStockMapsTitle[] = "All stock maps";

	// BISHOP 2'S SHORT DRAFT for the owner ("make our disclaimer shorter"), with the run's time from Vasquez's plan (plan-all-stock-maps.md), and what the
	// Overwrite tick does to every level (Dietrich 11-22: the tick is the plot's answer, its own question never opens during the run)
	char const kAllStockMapsWords[] =
		"All stock maps adds alien wall and ceiling waypoints to every Marine and Predator campaign level, one after another, with the options "
		"below. It takes a while: about 30 to 45 minutes.\n\n"
		"Each level is backed up first as <level>.rif.bak, and an existing backup is never replaced. With Overwrite ticked, the alien "
		"waypoints already in every level are replaced. Not tested on every map: aliens may sometimes get stuck.";

	namespace
	{
		int32_t const kMargin = 12;   // the words' room from the window's left and right edges
		int32_t const kGap = 12;      // between the words and the options under them

		int64_t WidthOf(std::string const &text, std::function<int32_t(unsigned char)> const &widthOf)
		{
			int64_t sum = 0;

			for (char const letter : text)
			{
				sum += widthOf((unsigned char)letter);
			}

			return sum;
		}

		size_t LinesIn(std::string const &text)
		{
			size_t lines = 1;

			for (char const letter : text)
			{
				lines += letter == '\n';
			}

			return lines;
		}

		// A NAME OR A REASON from the engine, the plot or the file system, as it is shown and logged: the editor's one rule for safe text
		// (panel_paint.h PanelPaint_SafeText: the space to the tilde as they are, every other byte a '?', at most 255 letters), so no byte in it
		// moves, hides or ends the text on the screen or in the log (input is hostile: a level's file name or a system error is not ours)
		std::string Safe(std::string const &text)
		{
			return PanelPaint_SafeText(text);
		}

		char const *Heading(StockMapOutcome outcome)
		{
			switch (outcome)
			{
				case StockMapOutcome::Changed: return "CHANGED";
				case StockMapOutcome::Skipped: return "SKIPPED";
				case StockMapOutcome::Refused: return "REFUSED";
				case StockMapOutcome::Failed: return "FAILED";
				default: return "NOT STARTED";
			}
		}

		char const *OutcomeWords(StockMapOutcome outcome)
		{
			switch (outcome)
			{
				case StockMapOutcome::Changed: return "changed";
				case StockMapOutcome::Skipped: return "skipped";
				case StockMapOutcome::Refused: return "refused";
				case StockMapOutcome::Failed: return "failed";
				default: return "not started";
			}
		}

		size_t Count(std::vector<AllStockMapsLevel> const &report, StockMapOutcome outcome)
		{
			size_t count = 0;

			for (AllStockMapsLevel const &level : report)
			{
				count += level.outcome == outcome;
			}

			return count;
		}

		// "21 of 22 levels changed, 1 failed": the run's count, as the status line and the log say it
		std::string CountWords(std::vector<AllStockMapsLevel> const &report)
		{
			std::string words = std::to_string((unsigned long long)Count(report, StockMapOutcome::Changed)) + " of " +
				std::to_string((unsigned long long)report.size()) + " levels changed";

			for (StockMapOutcome const outcome : { StockMapOutcome::Skipped, StockMapOutcome::Refused, StockMapOutcome::Failed, StockMapOutcome::NotStarted })
			{
				size_t const count = Count(report, outcome);

				if (count)
				{
					words += ", " + std::to_string((unsigned long long)count) + " " + OutcomeWords(outcome);
				}
			}

			return words;
		}

		// A REASON ON THE SCREEN (Vasquez's review, SHOULD): the engine's words can end with a level's whole path ("...: C:\Users\<name>\...\derelict.RIF"),
		// and inteng's text screen, unlike the status line, shows a path as it is, so the user's own folder would be on the owner's recording. On the
		// screen every full path is cut to its last part (screen_names.h ScreenNames_Cut, the status line's own rule); the log keeps the whole path
		std::string Shown(std::string const &text, bool onScreen)
		{
			return onScreen ? ScreenNames_Cut(Safe(text)) : Safe(text);
		}

		// The end stage worth saying: anything but plain "done" (or nothing), as the safe text rule makes it; empty when there is nothing to add
		std::string EndNote(std::string const &endStage, bool onScreen)
		{
			return endStage.empty() || endStage == "done" ? std::string() : Shown(endStage, onScreen);
		}

		// One level as the report and the log say it: "derelict.RIF: <why>; backup derelict.RIF.bak made; NOT STEAM'S OWN before the run ..."
		std::string LevelWords(AllStockMapsLevel const &level, bool onScreen)
		{
			std::string const file = Shown(level.file, onScreen);
			std::string line = file + (level.why.empty() ? std::string() : ": " + Shown(level.why, onScreen));

			if (level.outcome == StockMapOutcome::Changed)
			{
				line += level.backupMade ? "; backup " + file + ".bak made" : (level.backupKept ? "; the older " + file + ".bak kept as it was" : std::string());

				if (level.steamsOwn == 0)
				{
					line += "; NOT STEAM'S OWN before the run (its backup holds it as it was)";
				}
			}

			return line;
		}
	}

	std::string AllStockMaps_Unmarked(std::string const &words)
	{
		std::string out;

		out.reserve(words.size());

		for (char const letter : words)
		{
			if (letter != kWrapPathStart && letter != kWrapPathEnd)
			{
				out += letter;
			}
		}

		return out;
	}

	std::string AllStockMaps_Wrapped(std::string const &words, std::function<int32_t(unsigned char)> const &widthOf, int32_t width)
	{
		std::string out;
		size_t at = 0;

		while (at <= words.size())
		{
			size_t const end = words.find('\n', at);
			std::string const marked = words.substr(at, end == std::string::npos ? std::string::npos : end - at);

			// the line's letters with the marks taken out, and for each letter whether it is inside a marked path (all_stock_maps_ui.h kWrapPathStart)
			std::string rest;
			std::vector<bool> inPath;
			bool path = false;

			for (char const letter : marked)
			{
				if (letter == kWrapPathStart || letter == kWrapPathEnd)
				{
					path = letter == kWrapPathStart;
					continue;
				}

				rest += letter;
				inPath.push_back(path);
			}

			if (rest.empty())
			{
				out += "\n";   // an empty line: a paragraph's break kept
			}

			size_t from = 0;   // the first letter not yet on a line

			while (from < rest.size())
			{
				// the longest piece that fits; at least one letter, so a narrow window never loops
				size_t const left = rest.size() - from;
				size_t fit = 0;

				while (fit < left && WidthOf(rest.substr(from, fit + 1), widthOf) <= width)
				{
					fit++;
				}

				size_t cut = fit == 0 ? 1 : fit;

				// Where the line may end, the latest such place that fits: before a space outside a path (the space is dropped), or right after a path's \ or /
				// (the next line goes on with the path's next part). A space inside a path, and the middle of a name, never: without such a place, the
				// line ends where the letters ran out, so only a word, or a path's part, wider than the whole line is cut inside. With no marks this is
				// the last space at or before the cut, but not the line's first letter, as it always was.
				if (cut < left)
				{
					for (size_t b = cut; b > 0; b--)
					{
						size_t const next = from + b;   // the letter the next line would start with
						bool const atSpace = rest[next] == ' ' && !inPath[next];
						bool const afterSeparator = inPath[next - 1] && (rest[next - 1] == '\\' || rest[next - 1] == '/');

						if (atSpace || afterSeparator)
						{
							cut = b;
							break;
						}
					}
				}

				out += rest.substr(from, cut) + "\n";
				from += cut;

				while (from < rest.size() && rest[from] == ' ' && !inPath[from])
				{
					from++;
				}
			}

			if (end == std::string::npos)
			{
				break;
			}

			at = end + 1;
		}

		while (!out.empty() && out.back() == '\n')
		{
			out.pop_back();
		}

		return out;
	}

	DialogFormSpec AllStockMaps_Form(DialogFormSpec const &options, std::function<int32_t(unsigned char)> const &widthOf, int32_t fontHeight)
	{
		DialogFormSpec spec = options;
		int32_t const lineHeight = fontHeight > 0 ? fontHeight : 15;

		// the words go where Environment's own first part was; every part of it moves down under them
		int32_t top = spec.height;

		for (DialogWidget const &part : spec.widgets)
		{
			top = part.box.y < top ? part.box.y : top;
		}

		top = top > 0 && top < spec.height ? top : 18;

		DialogWidget words;
		std::string const wrapped = AllStockMaps_Wrapped(kAllStockMapsWords, widthOf, spec.width - 2 * kMargin - 1);   // drawn from the box's x + 1
		int32_t const wordsHeight = (int32_t)LinesIn(wrapped) * lineHeight;

		words.type = DialogWidgetType::Label;
		words.text = wrapped;
		words.box = { kMargin, top, spec.width - 2 * kMargin, wordsHeight };

		for (DialogWidget &part : spec.widgets)
		{
			part.box.y += wordsHeight + kGap;
		}

		spec.title = kAllStockMapsTitle;
		spec.height += wordsHeight + kGap;
		spec.widgets.insert(spec.widgets.begin(), words);
		return spec;
	}

	// the owner's rule for questions (2026-11-23): short, plain, one question; what each answer does, so No never loses his work unsaid
	char const kAllStockMapsSaveFirstWords[] =
		"Save your level first?\n\n"
		"It has changes not saved. Yes saves it, then runs. No runs without saving it: its changes are lost. Cancel runs nothing.";

	DialogFormSpec AllStockMaps_SaveFirstForm(std::function<int32_t(unsigned char)> const &widthOf, int32_t fontHeight)
	{
		// inteng's Yes / No box (dialog_box.cpp OpenConfirm, FUN_00474a40): 400 x 200, the words at 0x12, 0x12 in 0x172 across, buttons
		// 0x32 x 0x10 at 0x96 down; its Yes and No 100 apart, and Cancel a third, so the three sit 75, 175 and 275 across, centred
		int32_t const lineHeight = fontHeight > 0 ? fontHeight : 15;
		DialogFormSpec spec;

		spec.title = kAllStockMapsTitle;
		spec.width = 400;

		DialogWidget words;
		std::string const wrapped = AllStockMaps_Wrapped(kAllStockMapsSaveFirstWords, widthOf, 0x172 - 1);   // drawn from the box's x + 1
		int32_t const wordsHeight = (int32_t)LinesIn(wrapped) * lineHeight;

		words.type = DialogWidgetType::Label;
		words.text = wrapped;
		words.box = { 0x12, 0x12, 0x172, wordsHeight };
		spec.widgets.push_back(words);

		int32_t const under = 0x12 + wordsHeight + kGap;
		int32_t const buttonsAt = under > 0x96 ? under : 0x96;

		struct Press
		{
			char const *text;
			int32_t x;
			int32_t answer;
		};

		for (Press const &press : { Press{ "Yes", 75, kDialogOk }, Press{ "No", 175, kDialogNo }, Press{ "Cancel", 275, kDialogEscape } })
		{
			DialogWidget button;

			button.type = DialogWidgetType::Button;
			button.text = press.text;
			button.answer = press.answer;
			button.box = { press.x, buttonsAt, 0x32, 0x10 };
			spec.widgets.push_back(button);
		}

		spec.height = buttonsAt + 0x32 > 200 ? buttonsAt + 0x32 : 200;
		return spec;
	}

	std::vector<DialogWidget> AllStockMaps_OptionsOf(std::vector<DialogWidget> const &widgets)
	{
		size_t const lead = strlen("All stock maps adds");

		if (widgets.empty() || widgets[0].type != DialogWidgetType::Label || widgets[0].text.compare(0, lead, kAllStockMapsWords, lead) != 0)
		{
			return std::vector<DialogWidget>();
		}

		return std::vector<DialogWidget>(widgets.begin() + 1, widgets.end());
	}

	std::string AllStockMaps_ProgressLead(AllStockMapsProgress const &progress)
	{
		return "All stock maps: level " + std::to_string((unsigned long long)progress.level) + " of " + std::to_string((unsigned long long)progress.levels) +
			(progress.file.empty() ? std::string() : ", " + Safe(progress.file)) + ": ";
	}

	std::string AllStockMaps_ProgressWords(AllStockMapsProgress const &progress)
	{
		std::string const lead = AllStockMaps_ProgressLead(progress);
		std::string const words = progress.stage.empty() ? lead.substr(0, lead.size() - 2) : lead + Safe(progress.stage);

		return progress.stopping ? words + "; stopping after this level (Esc)" : words;
	}

	std::string AllStockMaps_EndWords(std::vector<AllStockMapsLevel> const &report, std::string const &endStage)
	{
		std::string const note = EndNote(endStage, true);

		return "All stock maps: " + CountWords(report) + (note.empty() ? std::string() : "; " + note) + " (the report is open)";
	}

	std::vector<std::string> AllStockMaps_LogLines(std::vector<AllStockMapsLevel> const &report, std::string const &endStage)
	{
		std::vector<std::string> lines;

		lines.push_back("All stock maps: the run ended: " + CountWords(report));

		for (AllStockMapsLevel const &level : report)
		{
			lines.push_back(std::string("All stock maps: ") + Heading(level.outcome) + " " + LevelWords(level, false));
		}

		if (!EndNote(endStage, false).empty())
		{
			lines.push_back("All stock maps: " + EndNote(endStage, false));
		}

		return lines;
	}

	std::vector<std::string> AllStockMaps_ReportPages(std::vector<AllStockMapsLevel> const &report, std::function<int32_t(unsigned char)> const &widthOf,
		int32_t screenWidth, size_t linesPerPage, std::string const &endStage)
	{
		std::vector<std::string> said;
		size_t const notStarted = Count(report, StockMapOutcome::NotStarted);

		said.push_back("ALL STOCK MAPS: " + std::to_string((unsigned long long)Count(report, StockMapOutcome::Changed)) + " of " +
			std::to_string((unsigned long long)report.size()) + " levels changed.");

		if (!EndNote(endStage, true).empty())
		{
			said.push_back("AT THE END: " + EndNote(endStage, true) + ".");
		}

		if (notStarted && notStarted < report.size())
		{
			said.push_back("STOPPED (Esc): " + std::to_string((unsigned long long)notStarted) + " levels were not started, and are as they were.");
		}

		struct Part
		{
			StockMapOutcome outcome;
			char const *heading;
		};

		Part const parts[] = {
			{ StockMapOutcome::Changed, "CHANGED, each backed up first:" },
			{ StockMapOutcome::Skipped, "SKIPPED, not touched:" },
			{ StockMapOutcome::Refused, "REFUSED, not touched:" },
			{ StockMapOutcome::Failed, "FAILED, not saved (a backup made for it is still beside it):" },
		};

		for (Part const &part : parts)
		{
			if (!Count(report, part.outcome))
			{
				continue;
			}

			said.push_back("");
			said.push_back(part.heading);

			for (AllStockMapsLevel const &level : report)
			{
				if (level.outcome != part.outcome)
				{
					continue;
				}

				said.push_back("    " + LevelWords(level, true));
			}
		}

		if (notStarted)
		{
			std::string names;

			for (AllStockMapsLevel const &level : report)
			{
				if (level.outcome == StockMapOutcome::NotStarted)
				{
					names += (names.empty() ? "" : ", ") + Shown(level.file, true);
				}
			}

			said.push_back("");
			said.push_back("NOT STARTED: " + names + ".");
		}

		said.push_back("");
		said.push_back("TO UNDO A LEVEL: put its .rif.bak back in place of its .rif (for example, copy derelict.RIF.bak over derelict.RIF). A .rif.bak holds the "
			"level as it was before the first run.");

		// wrapped to the screen, then cut into pages; a page that isn't the last says so on its last line
		std::vector<std::string> lines;

		for (std::string const &line : said)
		{
			std::string const wrapped = line.empty() ? std::string() : AllStockMaps_Wrapped(line, widthOf, screenWidth);
			size_t from = 0;

			for (;;)
			{
				size_t const end = wrapped.find('\n', from);
				lines.push_back(wrapped.substr(from, end == std::string::npos ? std::string::npos : end - from));

				if (end == std::string::npos)
				{
					break;
				}

				from = end + 1;
			}
		}

		std::vector<std::string> pages;
		size_t const room = linesPerPage > 2 ? linesPerPage - 1 : 1;

		for (size_t at = 0; at < lines.size();)
		{
			bool const last = lines.size() - at <= (linesPerPage > 1 ? linesPerPage : 1);
			size_t const take = last ? lines.size() - at : room;
			std::string page;

			for (size_t k = 0; k < take; k++)
			{
				page += (k == 0 ? "" : "\n") + lines[at + k];
			}

			pages.push_back(last ? page : page + "\n(more on the next page)");
			at += take;
		}

		return pages;
	}
}
