// shapes_start_screen.cpp: shapes part 3 in the editor (shapes_start_screen.h says what and why). Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shapes_start_screen.h"

#include <memory>
#include <vector>

#include "dialogs.h"
#include "first_launch_screen.h"   // FirstLaunch_ReadIni / FirstLaunch_WriteIni: the ini read whole and written back in one step, as DataFolder= is
#include "first_launch_ui.h"       // the FOUND form
#include "logString.h"             // LogString: the editor's log (settings\avp_log.txt)
#include "panel_pass.h"            // the letters the dialogs draw with
#include "shapes_scanned.h"
#include "shapes_start.h"
#include "shapes_trigger.h"

namespace
{
	enum class Stage
	{
		Off,         // nothing at all (a hidden run without ShapesInTests=1, a start with no level, or not begun)
		Nothing,     // no list kept and nothing to search: no form, no search
		Asking,      // the FOUND form waits for the first frame it can open in
		FormOpen,    // it is open
		NotNow,      // the player said Not now: nothing is searched in this run
		Searching,   // one map a frame
		Done         // every map taken (a level opened later starts it again)
	};

	struct Run
	{
		host::ShapesStartSetting setting;
		Stage stage = Stage::Off;
		bool consented = false;                          // a list is kept (Yes was said once) or Yes was said now: only then is any map searched
		bool firstSearch = false;                        // Yes was said in this run: the first search's end opens the level again if it wrote a shape
		bool reopened = false;
		host::FirstLaunchFacts facts;
		host::ShapesScannedList kept;                    // the list as the ini held it at the start
		std::vector<std::string> maps;                   // the maps found at the start
		std::vector<std::string> waiting;                // levels opened while the form waited, searched after Yes
		std::unique_ptr<host::ShapesStartSearch> search;
		host::ShapesTriggerHooks hooks;
		host::ShapesTrigger trigger;
		size_t taken = 0, found = 0;                     // for the state line: maps taken, of the maps found (and opened since)
		size_t writtenSize = 0;                          // the list's length when it was last written (or as read)
		std::string said;                                // the last end words said, so a later end with nothing new says nothing again
		bool searchedAgain = false;                      // a list was kept but shape_rifs held no shape: every map is searched again (shapes_start.h)
	};

	// why every map is searched again, in the words the status line and the "shapes" field say
	char const kSearchedAgainWords[] = "every map searched again: shape_rifs held no shape";

	Run run;

	void Say(std::string const &words)
	{
		LogString("shapes: " + words);

		if (run.setting.say)
		{
			run.setting.say(words);
		}
	}

	// the list, written into the ini as the trigger holds it (every other byte of the file kept)
	bool WriteList(std::string *why)
	{
		std::string text, written;
		bool there = false;

		if (!host::FirstLaunch_ReadIni(run.setting.settingsFile.c_str(), &text, &there, why))
		{
			return false;
		}

		if (!host::ShapesScanned_Write(text, run.trigger.List(), &written))
		{
			*why = "the list couldn't be made into the ini's lines";
			return false;
		}

		if (!host::FirstLaunch_WriteIni(run.setting.settingsFile.c_str(), written, there, why))
		{
			return false;
		}

		run.writtenSize = run.trigger.List().size();
		return true;
	}

	void BeginSearch()
	{
		run.search.reset(new host::ShapesStartSearch(run.setting.dataFolder + "\\shape_rifs"));
		run.hooks = run.search->Hooks();
		run.trigger.Begin(run.kept, run.maps);

		for (std::string const &path : run.waiting)
		{
			run.trigger.Add(path);
		}

		run.found = run.maps.size() + run.waiting.size();
		run.taken = 0;
		run.waiting.clear();
		run.consented = true;
		run.stage = Stage::Searching;
	}

	void Finish()
	{
		run.stage = Stage::Done;
		std::string why;

		if (run.trigger.List().size() != run.writtenSize && !WriteList(&why))
		{
			Say("Shapes: the list of maps searched couldn't be written into AVP_OpenInteng.ini (" + why + "), so they are searched again at the next start");
		}

		// Drake's condition: the level was opened before its shapes were written, so after the first search that wrote any, it is opened again to draw them.
		// FIRST, then the words: the open says its own words on the status line, and said before it the search's would be gone at once (acceptance-shapes-found-1,
		// 11-23: the status read the level's picture words, never "Shapes: ...")
		if (run.firstSearch && !run.reopened && run.trigger.Written() > 0 && run.setting.reopen)
		{
			run.reopened = true;
			run.setting.reopen();
		}

		run.firstSearch = false;
		std::string words = run.trigger.Words();

		// when the list was let go at the start, the end words say why, for the whole sitting: their counts are the sitting's, the maps searched again among
		// them (the level opened again at the first end searches once more, and its end, saying the same, must not take the reason off the status line)
		if (!words.empty() && run.searchedAgain)
		{
			words += std::string(" (") + kSearchedAgainWords + ")";
		}

		if (!words.empty() && words != run.said)
		{
			run.said = words;
			Say(words);
		}
	}

	void OpenTheForm()
	{
		bool const inteng = PanelPass_HasMenuFont();
		int32_t const fontHeight = inteng ? PanelPass_MenuFontHeight() : kPanelPassLineHeight;
		host::DialogFormSpec const spec = host::FirstLaunch_Form(run.facts, [](unsigned char letter)
		{
			return (int32_t)PanelPass_MenuWidth(letter);
		}, fontHeight, host::Dialogs_ScreenWidth());

		run.stage = Stage::FormOpen;
		host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &)
		{
			if (answer != host::kFirstLaunchYes)
			{
				run.stage = Stage::NotNow;
				run.waiting.clear();
				Say("Shapes: not now; nothing was searched, and Open Inteng asks again at its next start");
				return;
			}

			run.firstSearch = true;
			BeginSearch();
			Say("Shapes: searching " + std::to_string((unsigned long long)run.found) + " maps for object shapes, one a frame");
		});
	}
}

void host::ShapesStartScreen_Begin(ShapesStartSetting const &setting)
{
	run = Run();   // (one start a sitting; a fresh one, should it ever be asked again)
	run.setting = setting;

	if (!setting.on)
	{
		return;
	}

	std::string text, why;
	bool there = false;

	if (!FirstLaunch_ReadIni(setting.settingsFile.c_str(), &text, &there, &why))
	{
		Say("Shapes: AVP_OpenInteng.ini can't be read (" + why + "), so no map is searched for shapes");
		return;
	}

	run.kept = ShapesScanned_Read(text);
	run.writtenSize = run.kept.entries.size();

	// the list's lines not taken (anyone can write the ini), and the folders not looked in: each in the log, the first on the status line, once
	std::vector<std::string> notTaken = run.kept.refused;
	std::vector<std::string> folders;
	run.maps = ShapesStart_MapsFound(setting.dataFolder, &folders);

	for (std::string const &line : notTaken)
	{
		LogString("shapes: a line of [ShapesScanned] in AVP_OpenInteng.ini not taken: " + line);
	}

	for (std::string const &folder : folders)
	{
		LogString("shapes: " + folder);
	}

	size_t const lines = notTaken.size() + run.kept.refusedMore;

	if (lines)
	{
		Say("Shapes: " + std::to_string((unsigned long long)lines) + (lines == 1 ? " line" : " lines") + " of [ShapesScanned] in AVP_OpenInteng.ini not taken, as \"" +
			(notTaken.empty() ? std::string() : notTaken[0]) + "\" (the log has them all)");
	}
	else if (!folders.empty())
	{
		Say("Shapes: " + folders[0] + (folders.size() > 1 ? " (and " + std::to_string((unsigned long long)(folders.size() - 1)) + " more in the log)" : std::string()));
	}

	// THE LIST WITH NOTHING BEHIND IT (Drake 2026-11-23, Bishop's card for beta.0, the owner testing "like a fresh player would"): the list says which maps
	// were searched, but what they gave lives in shape_rifs. With no shape file there (the player deleted the folder, or every write was refused), the list
	// stands for nothing, and trusted it would keep every map from ever being searched again: the placement picker empty for good. So it is let go for this
	// start: every map is searched again, still on the player's Yes (a list was kept), as a first search is (the level opened again at its end if it wrote
	// a shape, so the picker has them), and the list is written again at the end
	if (run.kept.present && !run.kept.entries.empty())
	{
		std::string notListed;

		if (!ShapesStart_ShapeFilesThere(setting.dataFolder, &notListed))
		{
			LogString("shapes: the list of maps searched in AVP_OpenInteng.ini holds " + std::to_string((unsigned long long)run.kept.entries.size()) +
				" maps, but shape_rifs holds no shape file, so every map is searched again");
			run.kept.entries.clear();
			run.writtenSize = (size_t)-1;   // (so the list is written again at the end, whatever its length)
			run.searchedAgain = true;
			run.firstSearch = true;
			Say(std::string("Shapes: ") + kSearchedAgainWords + "; searching the maps for object shapes, one a frame");
		}
		else if (!notListed.empty())
		{
			LogString("shapes: " + notListed + ", so the list of maps searched is kept as it is");
		}
	}

	// a list kept: the player said Yes once, so every map not on it is searched now, without asking
	if (run.kept.present)
	{
		BeginSearch();
		return;
	}

	run.facts = ShapesStart_Facts(setting.programFolder, setting.dataFolder, setting.fromIni, run.maps);
	run.stage = ShapesStart_ShouldAsk(false, run.facts) ? Stage::Asking : Stage::Nothing;
}

void host::ShapesStartScreen_Frame()
{
	if (run.stage == Stage::Asking && !Dialogs_IsOpen() && Dialogs_ScreenWidth() > 0)
	{
		OpenTheForm();
		return;
	}

	if (run.stage != Stage::Searching)
	{
		return;
	}

	if (run.trigger.Step(run.hooks))
	{
		run.taken++;
	}

	if (run.trigger.Done())
	{
		Finish();
	}
}

void host::ShapesStartScreen_LevelOpened(std::string const &path)
{
	if (path.empty())
	{
		return;
	}

	if (run.stage == Stage::Asking || run.stage == Stage::FormOpen)
	{
		run.waiting.push_back(path);   // searched if Yes is said
		return;
	}

	if (!run.consented || (run.stage != Stage::Searching && run.stage != Stage::Done))
	{
		return;   // never searched without the player's Yes (now or kept)
	}

	run.trigger.Add(path);
	run.found++;
	run.stage = Stage::Searching;
}

void host::ShapesStartScreen_AtClose()
{
	if (!run.consented || run.trigger.List().size() == run.writtenSize)
	{
		return;
	}

	std::string why;

	if (!WriteList(&why))
	{
		LogString("shapes: at the close, the list of maps searched couldn't be written into AVP_OpenInteng.ini (" + why + ")");
	}
}

std::string host::ShapesStartScreen_StateWords()
{
	// (when the list was let go at the start, the searching and done words say so, for the whole sitting: acceptance-shapes-gone-2)
	std::string const again = run.searchedAgain ? std::string("; ") + kSearchedAgainWords : std::string();

	switch (run.stage)
	{
		case Stage::Off: return "off";
		case Stage::Nothing: return "nothing to search";
		case Stage::Asking: return "asking";
		case Stage::FormOpen: return "asking (the form is open)";
		case Stage::NotNow: return "not now";
		case Stage::Searching: return "searching " + std::to_string((unsigned long long)run.taken) + " of " + std::to_string((unsigned long long)run.found) + again;
		case Stage::Done: return "done; " + std::to_string((unsigned long long)run.trigger.Scanned()) + " new maps scanned, " +
			std::to_string((unsigned long long)run.trigger.Written()) + " new shapes in shape_rifs" + again;   // (this run's counts; the status line has the words, any map not searched named)
	}

	return "off";
}
