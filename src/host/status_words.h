// status_words.h: the status line's words, OWNED by the status line. Setting them copies the text, so the status line never points at a string that is changed, moved or freed
// later. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng, after twice the status line read freed memory: it held a pointer into another string's buffer
// (lightWords, given new words by RelightTheView). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
//   statusWords = text;        copies text (a null pointer: nothing is said); text may be the status line's own words
//   (char const *)statusWords  the words, or null; they stay as they are until the next setting
//   statusWords.From(p)        true when the words were last set from that very pointer: the address is kept only to be compared, never read through (the two places that ask
//                              which buffer the words came from: the bar's cut-off titles, a save's own words)
// NO FULL PATH (2026-11-18, Drake, Bishop's privacy item for alpha.1): the status line is on the screen, and so in screenshots: the copy has every full path in the words cut to
// its file's or folder's name (screen_names.h), "Saved a copy: C:\Users\<name>\...\Invasion.RIF" says "Saved a copy: Invasion.RIF". The log keeps its own full words.
// WHO SAID THEM (2026-11-19, Vasquez, THE OWNER'S CALL through Bishop, alpha.3 "Status line call"): inteng draws no status line on the Waypoint screen or in Strategy's edit view;
// ours draws one there WHILE the mouse tools have something to say (a named difference). The mouse tools set their words with Say(text, sayer); any plain setting is Other's, so
// the next menu item, key or screen's words hide the line again, as inteng's.
//   statusWords.Say(text, s)   as statusWords = text, said by s
//   statusWords.Sayer()        who set the words last (Other when nothing is said)
// A LEAD FOR ONE SAYER (2026-11-23, Dietrich, Bishop 2's order for the alpha.6 cut; the owner records All stock maps' run): while a run of many levels plots each
// one, the plot says its own progress every frame of its search, AFTER the run's line was drawn, so the held words were the plot's alone (no level) and the
// screen said "plotting" (all_stock_maps_ui.h). With a lead set for a sayer, every Say by it that doesn't start with the lead is held as lead + its words, and
// those words, without the lead, are kept for the run to show in its own line:
//   statusWords.SetLead(s, lead)   from now on s's words are led by lead (a new lead forgets the led words of the one before)
//   statusWords.ClearLead()        no lead, and no led words
//   statusWords.LedWords()         the last words s said under the lead, without it ("" none since the lead was set)

#pragma once

#include <string>

#include "screen_names.h"

enum class StatusSayer
{
	Other,           // any plain setting (statusWords = text)
	WaypointMouse,   // Waypoint mode by mouse: a pick, a link, a refusal with its reason (waypoint_mouse.cpp)
	StrategyView,    // Strategy's edit view: a Message's FMV, an edit refused with its reason (strategy_screen.cpp)
	WaypointPlot,    // Waypoint mode's auto-plot: its progress while it plots and its end line (autoplot_command.cpp; the owner 11-21, "When I try to run alien auto link I
	                 // don't see anything")
};

class StatusWords
{
public:
	StatusWords() = default;
	StatusWords(StatusWords const &) = delete;
	StatusWords &operator=(StatusWords const &) = delete;

	StatusWords &operator=(char const *text)
	{
		Set(text);
		sayer = StatusSayer::Other;
		return *this;
	}

	void Say(char const *text, StatusSayer who)
	{
		if (text && !leadWords.empty() && who == leadSayer && std::string(text).compare(0, leadWords.size(), leadWords) != 0)
		{
			led = text;
			std::string const both = leadWords + text;

			Set(both.c_str());
			source = text;   // From() answers for the caller's own words, as without a lead
		}
		else
		{
			Set(text);
		}

		sayer = text ? who : StatusSayer::Other;
	}

	void SetLead(StatusSayer who, std::string const &lead)
	{
		if (who != leadSayer || lead != leadWords)
		{
			led.clear();
		}

		leadSayer = who;
		leadWords = lead;
	}

	void ClearLead()
	{
		leadWords.clear();
		led.clear();
	}

	std::string const &LedWords() const
	{
		return led;
	}

	operator char const *() const
	{
		return held ? words.c_str() : nullptr;
	}

	bool From(char const *pointer) const
	{
		return held && pointer != nullptr && pointer == source;
	}

	StatusSayer Sayer() const
	{
		return held ? sayer : StatusSayer::Other;
	}

private:
	void Set(char const *text)
	{
		source = text;

		if (!text)
		{
			held = false;
			words.clear();
			return;
		}

		std::string copy(host::ScreenNames_Cut(text));   // copied first (text may point into words itself), every full path cut to its name

		words.swap(copy);
		held = true;
	}

	std::string words;
	char const *source = nullptr;
	bool held = false;
	StatusSayer sayer = StatusSayer::Other;
	StatusSayer leadSayer = StatusSayer::Other;
	std::string leadWords;   // "": no lead
	std::string led;         // the lead sayer's last words under the lead, without it
};

// The status line on a screen where inteng draws none: drawn there only while its words are that screen's own tools' (the Waypoint screen: Waypoint by mouse, and its auto-plot's
// progress and end line, the owner 11-21; Strategy's edit view: the view's FMV and refusal words). Elsewhere this has no say (false)
inline bool StatusWords_ShownWhereIntengHidesIt(StatusSayer sayer, bool onWaypointScreen, bool inStrategyEditView)
{
	return (onWaypointScreen && (sayer == StatusSayer::WaypointMouse || sayer == StatusSayer::WaypointPlot)) || (inStrategyEditView && sayer == StatusSayer::StrategyView);
}
