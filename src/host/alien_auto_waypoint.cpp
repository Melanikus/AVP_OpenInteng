// alien_auto_waypoint.cpp: the "Alien auto waypoint" menu's typed words (alien_auto_waypoint.h). Written 2026-11-20 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
#include "alien_auto_waypoint.h"

#include <set>

namespace host
{
	namespace
	{
		char Lower(char c)
		{
			return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
		}

		bool SameIgnoringCase(std::string const &a, std::string const &b)
		{
			if (a.size() != b.size())
			{
				return false;
			}

			for (size_t i = 0; i < a.size(); i++)
			{
				if (Lower(a[i]) != Lower(b[i]))
				{
					return false;
				}
			}

			return true;
		}

		bool IsGap(char c)
		{
			return c == ' ' || c == '\t' || c == ',' || c == ';' || c == '\r' || c == '\n';
		}

		// The line's words: gaps between them, a double-quoted word may hold gaps (an open quote runs to the end)
		std::vector<std::string> WordsOf(std::string const &typed)
		{
			std::vector<std::string> words;
			size_t const end = typed.size() < kAlienAutoWaypointMostLetters ? typed.size() : kAlienAutoWaypointMostLetters;
			size_t i = 0;

			while (i < end)
			{
				if (IsGap(typed[i]))
				{
					i++;
					continue;
				}

				std::string word;

				if (typed[i] == '"')
				{
					i++;

					while (i < end && typed[i] != '"')
					{
						word += typed[i++];
					}

					i++;   // past the closing quote (or the end)
				}
				else
				{
					while (i < end && !IsGap(typed[i]) && typed[i] != '"')
					{
						word += typed[i++];
					}
				}

				if (!word.empty())
				{
					words.push_back(word);
				}
			}

			return words;
		}

		// a word as the status line shows it: its printable letters, cut with "..."
		std::string Shown(std::string const &word)
		{
			std::string shown;

			for (char const c : word)
			{
				if (shown.size() >= 24)
				{
					return shown + "...";
				}

				shown += (unsigned char)c >= 0x20 && (unsigned char)c < 0x7f ? c : '?';
			}

			return shown;
		}

		bool NeedsQuotes(std::string const &name)
		{
			for (char const c : name)
			{
				if (IsGap(c))
				{
					return true;
				}
			}

			return false;
		}
	}

	bool AlienAutoWaypoint_Parse(std::string const &typed, std::vector<AlienAutoWaypointObject> const &objects, std::vector<int32_t> *modules, std::string *refusal)
	{
		if (!modules || !refusal)
		{
			return false;
		}

		modules->clear();
		refusal->clear();

		std::vector<std::string> const words = WordsOf(typed);

		if (words.empty())
		{
			*refusal = "no module names typed";
			return false;
		}

		std::vector<std::string> refused;
		std::vector<int32_t> found;
		std::set<int32_t> seen;

		for (std::string const &word : words)
		{
			if (word.size() > kAlienAutoWaypointMostWordLetters)
			{
				refused.push_back(Shown(word) + " is too long");
				continue;
			}

			AlienAutoWaypointObject const *exact = nullptr;
			AlienAutoWaypointObject const *loose = nullptr;
			size_t exactCount = 0, looseCount = 0;

			for (AlienAutoWaypointObject const &one : objects)
			{
				if (one.name.empty())
				{
					continue;
				}

				if (one.name == word)
				{
					exact = exactCount == 0 ? &one : exact;
					exactCount++;
				}
				else if (SameIgnoringCase(one.name, word))
				{
					loose = looseCount == 0 ? &one : loose;
					looseCount++;
				}
			}

			// exactly one object of that name, else (none exactly) exactly one with the case ignored; two or more of either: refused, named, never a guess
			size_t const many = exactCount ? exactCount : looseCount;
			AlienAutoWaypointObject const *const it = exactCount == 1 ? exact : exactCount == 0 && looseCount == 1 ? loose : nullptr;

			if (!it)
			{
				refused.push_back(many > 1 ? Shown(word) + " names " + std::to_string(many) + " objects" : "no module named " + Shown(word));
				continue;
			}

			if (!it->module)
			{
				refused.push_back(Shown(word) + " is not a module");
				continue;
			}

			if (seen.insert(it->index).second)
			{
				found.push_back(it->index);
			}
		}

		if (!refused.empty())
		{
			std::string words2;

			for (size_t i = 0; i < refused.size() && i < kAlienAutoWaypointMostRefusalsNamed; i++)
			{
				words2 += (i ? "; " : "") + refused[i];
			}

			if (refused.size() > kAlienAutoWaypointMostRefusalsNamed)
			{
				words2 += "; and " + std::to_string(refused.size() - kAlienAutoWaypointMostRefusalsNamed) + " more";
			}

			*refusal = words2;
			return false;
		}

		*modules = found;
		return true;
	}

	bool AlienAutoWaypoint_ParseCount(std::string const &typed, int32_t least, int32_t most, int32_t fallback, char const *label, int32_t *value, std::string *why)
	{
		if (!value || !why)
		{
			return false;
		}

		why->clear();

		std::string const name = label ? label : "a count";
		size_t first = 0, last = typed.size();

		while (first < last && (typed[first] == ' ' || typed[first] == '\t'))
		{
			first++;
		}

		while (last > first && (typed[last - 1] == ' ' || typed[last - 1] == '\t'))
		{
			last--;
		}

		if (first == last)
		{
			*value = fallback;
			return true;
		}

		std::string const digits = typed.substr(first, last - first);
		bool whole = digits.size() <= 9;

		for (char const c : digits)
		{
			whole = whole && c >= '0' && c <= '9';
		}

		long long const number = whole ? std::stoll(digits) : -1;

		if (!whole || number < least || number > most)
		{
			*why = name + ": '" + Shown(digits) + "' is not a whole number from " + std::to_string((long long)least) + " to " + std::to_string((long long)most);
			return false;
		}

		*value = (int32_t)number;
		return true;
	}

	std::string AlienAutoWaypoint_StartWords(std::vector<int32_t> const &modules, std::map<int32_t, std::string> const &names)
	{
		std::string words;

		for (int32_t const index : modules)
		{
			auto const found = names.find(index);

			if (found == names.end() || found->second.empty())
			{
				continue;
			}

			std::string name;

			for (char const c : found->second)
			{
				if (c != '"')
				{
					name += c;
				}
			}

			if (name.empty())
			{
				continue;
			}

			std::string const one = NeedsQuotes(name) ? "\"" + name + "\"" : name;
			size_t const grown = words.size() + (words.empty() ? 0 : 1) + one.size();

			if (grown > kAlienAutoWaypointMostLetters)
			{
				break;
			}

			words += (words.empty() ? "" : " ") + one;
		}

		return words;
	}

	namespace
	{
		DialogWidget Part(DialogWidgetType type, DialogRect box, std::string const &text, size_t most = 10)
		{
			DialogWidget w;
			w.type = type;
			w.box = box;
			w.text = text;
			w.most = most;
			return w;
		}

		// One count or size of the dialog: its label's name, its range and default, the help lines under it (plain words, each at most kAlienAutoWaypointHelpLetters
		// letters: what it is, what more and what less do, with the scored runs' numbers), and the reason a refusal of it gives (none for the counts)
		struct PlotField
		{
			char const *name;
			int32_t least, most, fallback;
			bool size;                         // a size in mm (its label "name: least to most, default d"); a count's is "name (least to most, default d)"
			std::vector<char const *> help;
			char const *refusalReason;
		};

		// THE FIELDS, in the dialog's order. The sizes' help and reasons are Vasquez's dialog-knobs-2026-11-21.md, part A (AvP-docs/tools/vasquez/gridgaps/out), from the
		// scored runs: whole batch 2 (29 AI modules) for along; candidate 4's pair against every whole plot's for up and under
		std::vector<PlotField> const &PlotFields()
		{
			static std::vector<PlotField> const fields = {
				{ "Climb-ins", kAutoPlotLeastClimbIns, kAutoPlotMostClimbIns, kAutoPlotDefaultClimbIns, false,
					{ "The most floor-to-ceiling climbs per AI module; 0: each floor goes straight up." }, nullptr },
				{ "Get-outs", kAutoPlotLeastGetOuts, kAutoPlotMostGetOuts, kAutoPlotDefaultGetOuts, false,
					{ "The most ceiling-to-floor attack drops per AI module; 0: drop where lanes end." }, nullptr },
				{ "Corridor aim along (mm)", kAutoPlotLeastHighAlong, kAutoPlotMostHighAlong, kAutoPlotVolumeHighAlong, true,
					{ "How far along a corridor a climber aims after a sloped side. Less: it wedges in",
						"the crease (600: 1,636 stalled frames, 2500: 26); more slides it to the far end." }, "under 2500 every value tried broke the climb" },
				{ "Upper-face aim, up (mm)", kAutoPlotLeastHighUp, kAutoPlotMostHighUp, kAutoPlotVolumeHighUp, true,
					{ "How far up the sloped ceiling face the aim sits. More turns the climber into",
						"the face (it backs off). Less, or a face shorter than up + under: the edge.",
						"Scored only as 1000 with 300 and 1200 with 550: better on stretch 1, worse on 2." }, "only 1000 with 300 and 1200 with 550 were tested" },
				{ "Upper-face aim, under (mm)", kAutoPlotLeastHighUnder, kAutoPlotMostHighUnder, kAutoPlotVolumeHighUnder, true,
					{ "How far under that face the aim sits (tested only with up). More: the body may",
						"never hold it; less nears the face, where no box holds it (the gencor01 loop)." }, "only 1000 with 300 and 1200 with 550 were tested" },
			};

			return fields;
		}

		std::string FieldLabel(PlotField const &f)
		{
			std::string const range = std::to_string((long long)f.least) + " to " + std::to_string((long long)f.most) + ", default " + std::to_string((long long)f.fallback);

			return f.size ? std::string(f.name) + ": " + range : std::string(f.name) + " (" + range + ")";
		}

		// one field at y: its TEXT field (0x96 x 0x12) started with its default, its label to its right, its help lines under it 0x12 apart; the y after it
		int32_t AddField(DialogFormSpec *spec, PlotField const &f, int32_t y)
		{
			spec->widgets.push_back(Part(DialogWidgetType::Edit, { 0x1e, y, 0x96, 0x12 }, std::to_string((long long)f.fallback)));
			spec->widgets.push_back(Part(DialogWidgetType::Label, { 0xbe, y, 0x190, 0x12 }, FieldLabel(f)));
			y += 0x14;

			for (char const *line : f.help)
			{
				spec->widgets.push_back(Part(DialogWidgetType::Label, { 0x1e, y, 0x230, 0x12 }, line));
				y += 0x12;
			}

			return y + 6;
		}
	}

	DialogFormSpec AlienAutoWaypoint_PlotForm(bool withNames, std::string const &names)
	{
		std::vector<PlotField> const &fields = PlotFields();
		DialogFormSpec spec;

		spec.title = "Alien auto waypoint";
		spec.width = 620;   // (wider than inteng's TEXT dialog's 400: the help lines, kAlienAutoWaypointHelpLetters letters at its ~7 px a letter, inside inteng's 640 x 480 screen)
		spec.widgets.push_back(Part(DialogWidgetType::Label, { 0x12, 0x12, 0x230, 0x24 }, withNames ?
			"Type the modules' names (spaces between them), then the counts and sizes\neach AI module is plotted with (each field starts with its default)" :
			"Every AI module of the level, plotted with these counts and sizes\n(each field starts with its default)"));

		int32_t y = 0x3c;

		if (withNames)
		{
			spec.widgets.push_back(Part(DialogWidgetType::Edit, { 0x1e, y, 300, 0x12 }, names, kAlienAutoWaypointMostLetters));
			y += 0x18;
		}

		y = AddField(&spec, fields[0], y);
		y = AddField(&spec, fields[1], y);

		// the Airduct box under the two counts, its label 16 to its right (path_screen.cpp's Loop box: 0x10 and 0x20)
		DialogWidget airduct = Part(DialogWidgetType::Check, { 0x1e, y, 12, 12 }, std::string());

		airduct.checked = AutoPlotVolumeCounts().airduct;   // unticked as it opens: the planner's default (the owner 11-20 19:06)
		spec.widgets.push_back(airduct);
		spec.widgets.push_back(Part(DialogWidgetType::Label, { 0x1e + 0x10, y, 0x150, 0x12 }, "Airduct on the modules it plots (aliens crawl there)"));
		y += 0x12;

		// THE OVERWRITE BOX under it (the owner 11-21), as the Airduct box, UNTICKED as it opens; its plain help on its own row, right of its label (a line under it would
		// take the Modules dialog past inteng's 480: the two box rows are a line each, 0x12, where the Airduct row alone was 0x18)
		DialogWidget overwrite = Part(DialogWidgetType::Check, { 0x1e, y, 12, 12 }, std::string());

		overwrite.checked = false;   // off: the modules that have alien waypoints, most likely Rebellion's own, are kept
		spec.widgets.push_back(overwrite);
		spec.widgets.push_back(Part(DialogWidgetType::Label, { 0x1e + 0x10, y, 0xe0, 0x12 }, "Overwrite already linked modules"));
		spec.widgets.push_back(Part(DialogWidgetType::Label, { 0x118, y, 0x150, 0x12 }, "unticked: kept, most likely Rebellion's own"));
		y += 0x12;

		// THE OWNER'S SIZES (autoplot_volumes.h): where a climber aims after a sloped side
		for (size_t f = 2; f < fields.size(); f++)
		{
			y = AddField(&spec, fields[f], y);
		}

		// Ok and Cancel a quarter and a half across, as inteng's 400-wide TEXT dialog has them at 100 and 200; the window 0x3c under them, as that dialog's 0xbe in 0xfa
		DialogWidget ok = Part(DialogWidgetType::Button, { spec.width / 4, y + 6, 0x32, 0x10 }, "Ok");

		ok.answer = kDialogOk;
		spec.widgets.push_back(ok);

		DialogWidget cancel = Part(DialogWidgetType::Button, { spec.width / 2, y + 6, 0x32, 0x10 }, "Cancel");

		cancel.answer = kDialogEscape;
		spec.widgets.push_back(cancel);
		spec.height = y + 6 + 0x3c;
		return spec;
	}

	bool AlienAutoWaypoint_ReadPlotForm(bool withNames, std::vector<DialogWidget> const &widgets, AlienAutoWaypointPlotAnswers *answers)
	{
		if (!answers)
		{
			return false;
		}

		std::vector<std::string> edits;
		std::vector<bool> ticks;   // the check boxes in order: Airduct, then Overwrite

		for (DialogWidget const &w : widgets)
		{
			if (w.type == DialogWidgetType::Edit)
			{
				edits.push_back(w.text);
			}
			else if (w.type == DialogWidgetType::Check)
			{
				ticks.push_back(w.checked);
			}
		}

		size_t const first = withNames ? 1 : 0;

		if (edits.size() != first + PlotFields().size() || ticks.size() != 2)
		{
			return false;
		}

		answers->names = withNames ? edits[0] : std::string();
		answers->climbIns = edits[first];
		answers->getOuts = edits[first + 1];
		answers->highAlong = edits[first + 2];
		answers->highUp = edits[first + 3];
		answers->highUnder = edits[first + 4];
		answers->airduct = ticks[0];
		answers->overwrite = ticks[1];
		return true;
	}

	void AlienAutoWaypoint_SplitLinked(std::vector<int32_t> const &heads, std::map<int32_t, size_t> const &alienCounts, bool overwrite, std::vector<int32_t> *plot,
		std::vector<int32_t> *kept)
	{
		std::vector<int32_t> toPlot, toKeep;

		for (int32_t const head : heads)
		{
			auto const found = alienCounts.find(head);
			bool const linked = found != alienCounts.end() && found->second > 0;

			(linked && !overwrite ? toKeep : toPlot).push_back(head);
		}

		if (plot)
		{
			*plot = toPlot;
		}

		if (kept)
		{
			*kept = toKeep;
		}
	}

	std::string AlienAutoWaypoint_LinkedKeptWords(size_t kept)
	{
		return kept == 0 ? std::string() : ", " + std::to_string((unsigned long long)kept) + " kept, already linked";
	}

	bool AlienAutoWaypoint_PlotCounts(AlienAutoWaypointPlotAnswers const &answers, AutoPlotVolumeCounts *counts, std::string *why)
	{
		if (!counts)
		{
			return false;
		}

		AutoPlotVolumeCounts read;
		std::vector<PlotField> const &fields = PlotFields();
		std::string const *const typed[] = { &answers.climbIns, &answers.getOuts, &answers.highAlong, &answers.highUp, &answers.highUnder };
		int32_t *const into[] = { &read.climbIns, &read.getOuts, &read.highAlong, &read.highUp, &read.highUnder };

		// each field in its range (nothing typed: -1, the default); a refusal names the field, says what was typed, and gives the reason its range is what it is
		for (size_t f = 0; f < fields.size() && f < sizeof(into) / sizeof(into[0]); f++)
		{
			if (!AlienAutoWaypoint_ParseCount(*typed[f], fields[f].least, fields[f].most, -1, fields[f].name, into[f], why))
			{
				if (why && fields[f].refusalReason)
				{
					*why += std::string(" (") + fields[f].refusalReason + ")";
				}

				return false;
			}
		}

		read.airduct = answers.airduct;
		*counts = read;
		return true;
	}
}
