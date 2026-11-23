// status_words_test.cpp: the status line's words are a copy the status line owns (src/host/status_words.h). Twice the status line read freed memory because it held a pointer
// into another string's buffer; these checks set it from strings that are then given new words, moved, cleared and freed, and read it back. Written 2026-11-18 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>

#include "status_words.h"

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

static bool Says(StatusWords const &status, char const *words)
{
	char const *const now = status;

	return now != nullptr && strcmp(now, words) == 0;
}

int main()
{
	printf("THE STATUS LINE'S WORDS\n");

	StatusWords status;

	Check((char const *)status == nullptr, "at the start nothing is said (null)");

	// the case that bit (RelightTheView): the status set from a string, the string then given longer words (a new buffer, the old one freed)
	std::string lightWords = "Picked: light 123, its marker under the pointer";

	status = lightWords.c_str();
	lightWords = "view re-lit: 12 modules, and enough more words that the string needs a buffer of its own";
	Check(Says(status, "Picked: light 123, its marker under the pointer"), "set from a string that then gets new, longer words: the status keeps its own words");
	Check((char const *)status != lightWords.c_str(), "and they are not the string's buffer");

	// a string moved away, cleared, and one that is freed
	std::string moved = "Light 123 copied (V pastes it at the camera)";

	status = moved.c_str();

	std::string const taken = std::move(moved);

	moved.clear();
	Check(Says(status, "Light 123 copied (V pastes it at the camera)"), "set from a string that is then moved away and cleared: the status keeps its own words");

	{
		std::unique_ptr<std::string> gone(new std::string("Doors closed: the doors are drawn shut and Save Intensities lights them so"));

		status = gone->c_str();
	}

	Check(Says(status, "Doors closed: the doors are drawn shut and Save Intensities lights them so"), "set from a string that is then freed: the status keeps its own words");

	// set from its own words (light_screen.cpp's keep, "statusWords = statusWords")
	status = (char const *)status;
	Check(Says(status, "Doors closed: the doors are drawn shut and Save Intensities lights them so"), "set from its own words: the same words");

	// a buffer written in place after it was set: the status keeps the words it was given
	char buffer[64] = "Camera speed: 6";

	status = buffer;
	strcpy_s(buffer, sizeof(buffer), "Camera speed: 9");
	Check(Says(status, "Camera speed: 6"), "a buffer written in place later: the status keeps the words it was given until it is set again");

	// From: which pointer the words were last set from (the bar's cut-off titles, a save's own words), compared, never read through
	char cutOff[16] = "Mode, Options";

	status = cutOff;
	Check(status.From(cutOff), "From says the words were set from that buffer");
	Check(!status.From(buffer) && !status.From(nullptr), "and from no other, nor from null");
	cutOff[0] = '\0';
	Check(status.From(cutOff) && Says(status, "Mode, Options"), "the buffer emptied later: still From it, and the words as they were set");

	// nothing said again
	status = nullptr;
	Check((char const *)status == nullptr && !status.From(cutOff), "set to null: nothing is said, and From is false");
	status = "";
	Check(Says(status, ""), "set to empty words: an empty line, not null");

	// WHO SAID THEM (Vasquez 11-19, the owner's status line call): Say keeps the sayer, any plain setting is Other's, and the screens inteng draws no status line on draw it only
	// while the words are their own mouse tool's
	Check(status.Sayer() == StatusSayer::Other, "a plain setting is Other's");
	status.Say("No waypoint under the pointer: none is current now", StatusSayer::WaypointMouse);
	Check(Says(status, "No waypoint under the pointer: none is current now") && status.Sayer() == StatusSayer::WaypointMouse, "Say sets the words (copied, as a plain setting) and the sayer");
	status = "Base is waypoint 0";
	Check(status.Sayer() == StatusSayer::Other, "the next plain setting (a menu item's words) is Other's again");
	status.Say("FMVs\\message47.smk: SMK2 128 x 96", StatusSayer::StrategyView);
	Check(status.Sayer() == StatusSayer::StrategyView, "the edit view's words are the StrategyView's");
	status.Say(nullptr, StatusSayer::StrategyView);
	Check((char const *)status == nullptr && status.Sayer() == StatusSayer::Other, "Say with nothing to say: nothing said, and no sayer");
	status.Say("C:\\Users\\someone\\Documents\\Genshd1.RIF opened", StatusSayer::WaypointMouse);
	Check(Says(status, "Genshd1.RIF opened"), "Say cuts a full path to its name too (the privacy rule)");

	Check(StatusWords_ShownWhereIntengHidesIt(StatusSayer::WaypointMouse, true, false), "the Waypoint screen draws its mouse tool's words");
	Check(!StatusWords_ShownWhereIntengHidesIt(StatusSayer::Other, true, false) && !StatusWords_ShownWhereIntengHidesIt(StatusSayer::StrategyView, true, false),
		"and no other words (inteng's: none)");
	Check(StatusWords_ShownWhereIntengHidesIt(StatusSayer::StrategyView, false, true), "Strategy's edit view draws its own words");
	Check(!StatusWords_ShownWhereIntengHidesIt(StatusSayer::Other, false, true) && !StatusWords_ShownWhereIntengHidesIt(StatusSayer::WaypointMouse, false, true),
		"and no other words (the view's own hint is not drawn)");
	Check(!StatusWords_ShownWhereIntengHidesIt(StatusSayer::WaypointMouse, false, false) && !StatusWords_ShownWhereIntengHidesIt(StatusSayer::StrategyView, false, false),
		"elsewhere it has no say");
	// THE AUTO-PLOT'S WORDS (the owner 11-21: "When I try to run alien auto link I don't see anything", "It takes forever?"): its progress and end line were plain settings,
	// so the Waypoint screen hid them all through a 112 s whole-level plot
	Check(StatusWords_ShownWhereIntengHidesIt(StatusSayer::WaypointPlot, true, false), "the Waypoint screen draws the auto-plot's progress and end line");
	Check(!StatusWords_ShownWhereIntengHidesIt(StatusSayer::WaypointPlot, false, true) && !StatusWords_ShownWhereIntengHidesIt(StatusSayer::WaypointPlot, false, false),
		"and only there");

	// A LEAD FOR ONE SAYER (Dietrich 2026-11-23, the alpha.6 cut: All stock maps' level in front of the plot's own progress, which it says later in the frame)
	{
		StatusWords led;
		char const *const plot = "Auto-plot: module 69 of 120 (CAVERNH-02), way 860 of 1418: searching";

		led.SetLead(StatusSayer::WaypointPlot, "All stock maps: level 17 of 22, battle.RIF: ");
		led.Say(plot, StatusSayer::WaypointPlot);
		Check(std::string(led) == std::string("All stock maps: level 17 of 22, battle.RIF: ") + plot && led.Sayer() == StatusSayer::WaypointPlot,
			"with a lead, the lead sayer's words are held with the lead in front: " + std::string(led));
		Check(led.LedWords() == plot, "and its own words, without the lead, are kept for the run's line: " + led.LedWords());
		Check(led.From(plot), "and From() still answers for the caller's own words");

		led.Say("All stock maps: level 17 of 22, battle.RIF: plotting", StatusSayer::WaypointPlot);
		Check(std::string(led) == "All stock maps: level 17 of 22, battle.RIF: plotting" && led.LedWords() == plot,
			"words that start with the lead already (the run's own line) are held as they are, and leave the led words alone: " + std::string(led));

		led.Say("Waypoint 3 made on the floor", StatusSayer::WaypointMouse);
		Check(std::string(led) == "Waypoint 3 made on the floor", "another sayer's words get no lead: " + std::string(led));
		led = "Saved a copy";
		Check(std::string(led) == "Saved a copy" && led.Sayer() == StatusSayer::Other, "nor a plain setting's");

		led.SetLead(StatusSayer::WaypointPlot, "All stock maps: level 18 of 22, invasion_p.RIF: ");
		Check(led.LedWords().empty(), "a new lead forgets the led words of the one before (the next level's line never shows the last level's plot)");

		led.ClearLead();
		led.Say(plot, StatusSayer::WaypointPlot);
		Check(std::string(led) == plot && led.LedWords().empty(), "with the lead cleared, the plot's words are its own again: " + std::string(led));
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
