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

		std::string CountLabel(char const *what, int32_t least, int32_t most)
		{
			return std::string(what) + " (" + std::to_string((long long)least) + " to " + std::to_string((long long)most) + ")";
		}
	}

	DialogFormSpec AlienAutoWaypoint_PlotForm(bool withNames, std::string const &names)
	{
		DialogFormSpec spec;
		spec.title = "Alien auto waypoint";
		spec.width = 400;
		spec.height = 0xfa;
		spec.widgets.push_back(Part(DialogWidgetType::Label, { 0x12, 0x12, 0x172, 0x50 }, withNames ?
			"Type the modules' names (spaces between them),\nand how many climb-ins and get-outs each AI module gets" :
			"Every AI module of the level:\nhow many climb-ins and get-outs each one gets"));
		int32_t y = 100;
		if (withNames)
		{
			spec.widgets.push_back(Part(DialogWidgetType::Edit, { 0x1e, 100, 300, 0x12 }, names, kAlienAutoWaypointMostLetters));
			y = 0x7c;
		}
		spec.widgets.push_back(Part(DialogWidgetType::Edit, { 0x1e, y, 0x96, 0x12 }, std::to_string((long long)kAutoPlotDefaultClimbIns)));
		spec.widgets.push_back(Part(DialogWidgetType::Label, { 0xbe, y, 0xb4, 0x12 }, CountLabel("Climb-ins", kAutoPlotLeastClimbIns, kAutoPlotMostClimbIns)));
		y += 0x18;
		spec.widgets.push_back(Part(DialogWidgetType::Edit, { 0x1e, y, 0x96, 0x12 }, std::to_string((long long)kAutoPlotDefaultGetOuts)));
		spec.widgets.push_back(Part(DialogWidgetType::Label, { 0xbe, y, 0xb4, 0x12 }, CountLabel("Get-outs", kAutoPlotLeastGetOuts, kAutoPlotMostGetOuts)));
		// the Airduct box, one row under the get-outs field (the TEXT dialog's rows are 0x18 apart), its label 16 to its right (path_screen.cpp's Loop box: 0x10 and 0x20)
		y += 0x18;
		DialogWidget airduct = Part(DialogWidgetType::Check, { 0x1e, y, 12, 12 }, std::string());
		airduct.checked = AutoPlotVolumeCounts().airduct;   // unticked as it opens: the planner's default (the owner 11-20 19:06)
		spec.widgets.push_back(airduct);
		spec.widgets.push_back(Part(DialogWidgetType::Label, { 0x1e + 0x10, y, 0x150, 0x12 }, "Airduct on the modules it plots (aliens crawl there)"));
		DialogWidget ok = Part(DialogWidgetType::Button, { 100, 0xbe, 0x32, 0x10 }, "Ok");
		ok.answer = kDialogOk;
		spec.widgets.push_back(ok);
		DialogWidget cancel = Part(DialogWidgetType::Button, { 200, 0xbe, 0x32, 0x10 }, "Cancel");
		cancel.answer = kDialogEscape;
		spec.widgets.push_back(cancel);
		return spec;
	}

	bool AlienAutoWaypoint_ReadPlotForm(bool withNames, std::vector<DialogWidget> const &widgets, AlienAutoWaypointPlotAnswers *answers)
	{
		if (!answers)
		{
			return false;
		}

		std::vector<std::string> edits;
		int boxes = 0;
		bool ticked = false;

		for (DialogWidget const &w : widgets)
		{
			if (w.type == DialogWidgetType::Edit)
			{
				edits.push_back(w.text);
			}
			else if (w.type == DialogWidgetType::Check)
			{
				boxes++;
				ticked = w.checked;
			}
		}

		size_t const want = withNames ? 3 : 2;

		if (edits.size() != want || boxes != 1)
		{
			return false;
		}

		answers->names = withNames ? edits[0] : std::string();
		answers->climbIns = edits[want - 2];
		answers->getOuts = edits[want - 1];
		answers->airduct = ticked;
		return true;
	}

	bool AlienAutoWaypoint_PlotCounts(AlienAutoWaypointPlotAnswers const &answers, AutoPlotVolumeCounts *counts, std::string *why)
	{
		if (!counts)
		{
			return false;
		}

		AutoPlotVolumeCounts read;

		if (!AlienAutoWaypoint_ParseCount(answers.climbIns, kAutoPlotLeastClimbIns, kAutoPlotMostClimbIns, -1, "Climb-ins", &read.climbIns, why) ||
			!AlienAutoWaypoint_ParseCount(answers.getOuts, kAutoPlotLeastGetOuts, kAutoPlotMostGetOuts, -1, "Get-outs", &read.getOuts, why))
		{
			return false;
		}

		read.airduct = answers.airduct;
		*counts = read;
		return true;
	}
}
