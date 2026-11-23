// all_stock_maps_screen.cpp: Waypoint mode's "Alien auto waypoint" > "All stock maps" in the editor (all_stock_maps_screen.h says what and why).
// Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "all_stock_maps_screen.h"

#include <string>
#include <vector>

#include "alien_auto_waypoint.h"   // Environment's own form, its reader and its counts
#include "all_stock_maps.h"        // Dietrich's run
#include "all_stock_maps_ui.h"
#include "autoplot_waits.h"        // what waits while a run goes, and its words
#include "dialogs.h"
#include "editor_shared.h"         // statusWords, textPanelText / textPanelNext (inteng's text screen)
#include "inteng_menus.h"
#include "logString.h"              // LogString: the editor's log (settings\avp_log.txt)
#include "panel_pass.h"            // the letters the dialogs and the text screen draw with

namespace
{
	bool runWasGoing = false;               // a run was going last frame (its end is taken in the frame it ends); not the editor's own "running" (editor_shared.h)
	std::vector<std::string> pages;     // the last run's report pages; textPanelText / textPanelNext point into them while they are shown

	// every line of ours with the plot's own sayer: drawn on the Waypoint screen, where a plain status line isn't (status_words.h), so the owner's recording shows it
	void Say(std::string const &words)
	{
		statusWords.Say(words.c_str(), StatusSayer::WaypointPlot);   // drawn on the Waypoint screen, as the plot's own line is
	}

	bool ShowingOurs()
	{
		for (std::string const &page : pages)
		{
			if (textPanelText == page.c_str() || textPanelNext == page.c_str())
			{
				return true;
			}
		}

		return false;
	}

	// the run started, and its first words said; refused, why
	void StartTheRun(host::AutoPlotVolumeCounts const &counts, bool overwrite, bool changesAllowed)
	{
		std::string why;

		// the Overwrite tick IS the answer (Dietrich's engine never opens the plot's own replace question during the run)
		if (!host::AllStockMaps_Start(counts, overwrite, changesAllowed, &why))
		{
			Say("All stock maps: not started: " + why);
			return;
		}

		runWasGoing = true;
		Say(overwrite ? "All stock maps: starting; the alien waypoints already in every level are replaced (each level's .rif.bak keeps them)" :
			"All stock maps: starting; the alien waypoints already in a level are kept");
	}
}

void host::AllStockMaps_Open()
{
	if (AllStockMaps_Busy())
	{
		Say("All stock maps: the run is going (Esc stops it after this level)");
		return;
	}

	// measured in the letters dialogs.cpp draws a form's words with: inteng's own when its font was found, else the game's small font
	bool const inteng = PanelPass_HasMenuFont();
	int32_t const fontHeight = inteng ? PanelPass_MenuFontHeight() : kPanelPassLineHeight;
	DialogFormSpec const spec = AllStockMaps_Form(AlienAutoWaypoint_PlotForm(false, std::string()), [](unsigned char letter)
	{
		return (int32_t)PanelPass_MenuWidth(letter);
	}, fontHeight);

	Dialogs_Form(spec, [](int32_t answer, std::vector<DialogWidget> const &widgets)
	{
		if (answer != kDialogOk)
		{
			Say("All stock maps: nothing changed; every level is as it was");
			return;
		}

		// Environment's options, read as Environment reads them
		AlienAutoWaypointPlotAnswers answers;
		AutoPlotVolumeCounts counts;
		std::string why;

		if (!AlienAutoWaypoint_ReadPlotForm(false, AllStockMaps_OptionsOf(widgets), &answers))
		{
			Say("All stock maps: nothing changed: the form lost its fields");
			return;
		}

		if (!AlienAutoWaypoint_PlotCounts(answers, &counts, &why))
		{
			Say("All stock maps: nothing changed: " + why);
			return;
		}

		// every other reason not to run is said first, so the question below is never asked for a run that couldn't start anyway
		std::string const whyNot = AllStockMaps_WhyNot(true);

		if (!whyNot.empty())
		{
			Say("All stock maps: not started: " + whyNot);
			return;
		}

		if (!AllStockMaps_ChangesNotSaved())
		{
			StartTheRun(counts, answers.overwrite, false);
			return;
		}

		// CHANGES NOT SAVED: "Save your level first?" Yes / No / Cancel (all_stock_maps_ui.h), never a dead end (Bishop 2, 2026-11-23). Yes is
		// the editor's own Save (SaveAtIntengsPoint: where his level really is, an Open-with level's own file too); a save that didn't go through
		// runs nothing and says why
		bool const overwrite = answers.overwrite;
		bool const ownLetters = PanelPass_HasMenuFont();
		int32_t const letterHeight = ownLetters ? PanelPass_MenuFontHeight() : kPanelPassLineHeight;

		Dialogs_Form(AllStockMaps_SaveFirstForm([](unsigned char letter) { return (int32_t)PanelPass_MenuWidth(letter); }, letterHeight),
			[counts, overwrite](int32_t saveAnswer, std::vector<DialogWidget> const &)
		{
			if (saveAnswer == kDialogOk)
			{
				const std::string saved = SaveAtIntengsPoint();   // const first: Snyk Code's parser skips the whole file on "std::string const" in a lambda given to a call

				if (AllStockMaps_ChangesNotSaved())
				{
					Say("All stock maps: not started: your level was not saved (" + saved + ")");
					return;
				}

				StartTheRun(counts, overwrite, false);
			}
			else if (saveAnswer == kDialogNo)
			{
				StartTheRun(counts, overwrite, true);
			}
			else
			{
				Say("All stock maps: nothing changed; every level is as it was");
			}
		});
	});
}

void host::AllStockMapsScreen_Frame(int32_t screenWidth, int32_t screenHeight)
{
	if (AllStockMaps_Busy())
	{
		runWasGoing = true;
		AllStockMaps_Frame();
	}

	if (AllStockMaps_Busy())
	{
		// after the step, so the plot's own words are in it this frame. THE LEAD (status_words.h; the alpha.6 cut): the plot says its progress later in the
		// frame than this line is drawn (editor_panels.cpp), so every line it says is held with the run's words in front, and its words come back into
		// this line's stage the next frame (all_stock_maps_host.cpp, the Plotting step)
		AllStockMapsProgress const progress = AllStockMaps_Progress();

		statusWords.SetLead(StatusSayer::WaypointPlot, AllStockMaps_ProgressLead(progress));
		Say(AllStockMaps_ProgressWords(progress));
	}
	else if (runWasGoing)
	{
		statusWords.ClearLead();   // the run's over: the plot's words are its own again

		// THE END: the report as inteng's text pages, laid out as its Help pages are (editor_panels.cpp: inteng's letters, a line the letters' height
		// down, "Click to continue." 0x14 above the screen's foot); never over pages of ours still shown
		runWasGoing = false;

		std::vector<AllStockMapsLevel> const report = AllStockMaps_Report();
		std::string const endStage = AllStockMaps_Progress().stage;   // "done", or what went wrong at the end (the level open before not opened again)

		// the same report in the editor's log, a line a level (all_stock_maps_ui.h AllStockMaps_LogLines)
		for (std::string const &line : AllStockMaps_LogLines(report, endStage))
		{
			LogString(line);
		}

		if (!ShowingOurs())
		{
			int32_t const letterHeight = PanelPass_MenuFontHeight() > 0 ? PanelPass_MenuFontHeight() : kPanelPassLineHeight;
			int32_t const room = screenHeight - 0x14 - letterHeight;
			size_t const lines = room > letterHeight * 3 ? (size_t)(room / letterHeight) : 3;

			pages = AllStockMaps_ReportPages(report, [](unsigned char letter) { return (int32_t)PanelPass_MenuWidth(letter); },
				screenWidth > 64 ? screenWidth - 8 : 64, lines, endStage);

			if (!pages.empty())
			{
				textPanelText = pages[0].c_str();
				textPanelNext = pages.size() > 1 ? pages[1].c_str() : nullptr;
			}
		}

		Say(AllStockMaps_EndWords(report, endStage));
	}

	// THE NEXT PAGE: inteng's text screen shows a text and the one after it (textPanelNext); when a press has moved on to a page of ours, the page
	// after that one is put next, so every page is shown in turn
	if (!pages.empty() && textPanelText && !textPanelNext)
	{
		for (size_t k = 0; k + 1 < pages.size(); k++)
		{
			if (textPanelText == pages[k].c_str())
			{
				textPanelNext = pages[k + 1].c_str();
				break;
			}
		}
	}
}

bool host::AllStockMapsScreen_ItemWaits(int32_t action)
{
	if (!AllStockMaps_Busy())
	{
		return false;
	}

	IntengItem const *const item = IntengMenus_Find(action);

	if (item && !AutoPlot_ItemWaits(item->work))
	{
		return false;   // (an item that only looks, or Quit)
	}

	Say(AutoPlot_WaitWords(item ? item->name : nullptr) + " (All stock maps is running; Esc stops it after this level)");
	return true;
}

bool host::AllStockMapsScreen_Waits(char const *what)
{
	if (!AllStockMaps_Busy())
	{
		return false;
	}

	Say(AutoPlot_WaitWords(what) + " (All stock maps is running; Esc stops it after this level)");
	return true;
}

bool host::AllStockMapsScreen_Esc()
{
	if (!AllStockMaps_Busy())
	{
		return false;
	}

	AllStockMaps_Stop();
	Say(AllStockMaps_ProgressWords(AllStockMaps_Progress()));
	return true;
}
