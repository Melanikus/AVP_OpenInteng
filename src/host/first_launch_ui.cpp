// first_launch_ui.cpp: the first-launch setup form's words and parts, and the ini line it writes (first_launch_ui.h says what and why). Written 2026-11-23 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "first_launch_ui.h"

#include <ctype.h>

#include "all_stock_maps_ui.h"   // AllStockMaps_Wrapped: words wrapped to a width by the letters' own widths (the same wrapping as All stock maps' form)
#include "panel_paint.h"         // PanelPaint_SafeText: the editor's one rule for safe text

namespace host
{
	char const kFirstLaunchTitle[] = "Open Inteng";

	namespace
	{
		int32_t const kMargin = 12;         // the words' room from the window's left and right edges
		int32_t const kGap = 12;            // between the words and the buttons
		int32_t const kButtonHeight = 0x10; // Environment's and inteng's TEXT dialog's buttons
		int32_t const kButtonLeast = 0x32;

		std::string Safe(std::string const &text)
		{
			return PanelPaint_SafeText(text);
		}

		// "derelict.RIF, genshd1.RIF and 3 more": at most kFirstLaunchMostNames names, each made safe
		std::string Names(std::vector<std::string> const &names, char const *between)
		{
			size_t const shown = names.size() < kFirstLaunchMostNames ? names.size() : kFirstLaunchMostNames;
			std::string out;

			for (size_t k = 0; k < shown; k++)
			{
				out += (k ? between : "") + Safe(names[k]);
			}

			if (names.size() > shown)
			{
				out += " and " + std::to_string((unsigned long long)(names.size() - shown)) + " more";
			}

			return out;
		}

		std::string Counted(size_t count, char const *one, char const *many)
		{
			return std::to_string((unsigned long long)count) + " " + (count == 1 ? one : many);
		}

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

		std::string Trimmed(std::string const &text)
		{
			size_t from = 0;
			size_t to = text.size();

			while (from < to && (text[from] == ' ' || text[from] == '\t'))
			{
				from++;
			}

			while (to > from && (text[to - 1] == ' ' || text[to - 1] == '\t'))
			{
				to--;
			}

			return text.substr(from, to - from);
		}

		bool SameName(std::string const &a, std::string const &b)
		{
			if (a.size() != b.size())
			{
				return false;
			}

			for (size_t k = 0; k < a.size(); k++)
			{
				if (tolower((unsigned char)a[k]) != tolower((unsigned char)b[k]))
				{
					return false;
				}
			}

			return true;
		}

		// a new line, a 0, or one of `also`: never written into an ini line (it would make another line, end the text, or change what the line says)
		bool Unwritable(std::string const &text, char const *also)
		{
			for (char const letter : text)
			{
				if (letter == '\r' || letter == '\n' || letter == '\0')
				{
					return true;
				}

				for (char const *a = also; *a; a++)
				{
					if (letter == *a)
					{
						return true;
					}
				}
			}

			return false;
		}
	}

	std::string FirstLaunch_Words(FirstLaunchFacts const &facts)
	{
		// 1 and 2: where it is, where it belongs (the owner: "Open inteng is in <...> expected to reside in ..steamapps or ...\Aliens Versus Predator\Tools or
		// Program files..\Fox\AVP..")
		std::string words = "Open Inteng is in: " + Safe(facts.programFolder) + "\n\n"
			"It belongs in your game's Tools folder: Steam's ...\\steamapps\\common\\Aliens versus Predator Classic\\Tools, or the original install's "
			"C:\\Program Files (x86)\\Fox\\Aliens versus Predator\\Tools (C:\\Program Files\\... on 32-bit Windows).\n\n";

		// 3: what it found around it, then the question
		if (!facts.found)
		{
			words += "It found no levels around it.";

			if (!facts.looked.empty())
			{
				words += " It looked in: " + Names(facts.looked, "; ") + ".";
			}

			return words + "\n\nMove the Open Inteng folder into your game's Tools folder, or tell it where your game is (the folder that holds avp_rifs). Set "
				"the game's folder now?";
		}

		words += (facts.fromIni ? "In the game's folder that DataFolder= names, " : "Around it, in ") + Safe(facts.gameFolder) + ", it found " +
			Counted(facts.levels, "level", "levels") + " and " + Counted(facts.huds, "HUD file", "HUD files") + ".";

		if (!facts.notOwn.empty())
		{
			words += " " + std::to_string((unsigned long long)facts.notOwn.size()) + (facts.notOwn.size() == 1 ? " isn't" : " aren't") + " Steam's own: " +
				Names(facts.notOwn, ", ") + ".";
		}

		if (!facts.missing.empty())
		{
			words += " Missing: " + Names(facts.missing, ", ") + ".";
		}

		if (facts.shapesToWrite)
		{
			words += "\n\nOpen Inteng can make the object shapes from your game's " + Counted(facts.levels, "level", "levels") + ", into " +
				Safe(facts.gameFolder) + "\\shape_rifs, once. Nothing there is replaced. Make them now?";
		}

		return words;
	}

	DialogFormSpec FirstLaunch_Form(FirstLaunchFacts const &facts, std::function<int32_t(unsigned char)> const &widthOf, int32_t fontHeight, int32_t screenWidth)
	{
		DialogFormSpec spec;
		int32_t const lineHeight = fontHeight > 0 ? fontHeight : 15;
		int32_t const room = screenWidth - 64;

		spec.title = kFirstLaunchTitle;
		spec.width = room > 760 ? 760 : (room < 400 ? 400 : room);

		// the words, wrapped to the window (drawn from the box's x + 1, as dialogs.cpp draws a label)
		std::string const wrapped = AllStockMaps_Wrapped(FirstLaunch_Words(facts), widthOf, spec.width - 2 * kMargin - 1);
		int32_t const wordsHeight = (int32_t)LinesIn(wrapped) * lineHeight;
		DialogWidget words;

		words.type = DialogWidgetType::Label;
		words.text = wrapped;
		words.box = { kMargin, 0x12, spec.width - 2 * kMargin, wordsHeight };
		spec.widgets.push_back(words);

		// the two buttons under the words, a quarter and a half across as Environment's are, each wide enough for its word
		struct Press
		{
			char const *text;
			int32_t answer;
		};

		std::vector<Press> presses;

		if (!facts.found)
		{
			presses = { { "Browse...", kFirstLaunchBrowse }, { "Not now", kFirstLaunchNotNow } };
		}
		else if (facts.shapesToWrite)
		{
			presses = { { "Yes", kFirstLaunchYes }, { "Not now", kFirstLaunchNotNow } };
		}
		else
		{
			presses = { { "Ok", kDialogOk } };
		}

		int32_t const y = 0x12 + wordsHeight + kGap;
		int32_t x = spec.width / 4;

		for (Press const &press : presses)
		{
			int64_t const textWidth = WidthOf(press.text, widthOf) + 16;
			int32_t const width = textWidth > kButtonLeast ? (int32_t)textWidth : kButtonLeast;
			DialogWidget button;

			button.type = DialogWidgetType::Button;
			button.text = press.text;
			button.answer = press.answer;
			button.box = { x, y + 6, width, kButtonHeight };
			spec.widgets.push_back(button);

			int32_t const next = x + width + 16;
			x = next > spec.width / 2 ? next : spec.width / 2;
		}

		spec.height = y + 6 + 0x3c;
		return spec;
	}

	bool FirstLaunch_IniWithLine(std::string const &ini, std::string const &section, std::string const &key, std::string const &value, std::string *out)
	{
		std::string const sectionName = Trimmed(section);
		std::string const keyName = Trimmed(key);

		if (!out || sectionName.empty() || keyName.empty() || Unwritable(value, "") || Unwritable(key, "=") || Unwritable(section, "[]"))
		{
			return false;
		}

		std::string const newLine = ini.find("\r\n") != std::string::npos ? "\r\n" : (ini.find('\n') != std::string::npos ? "\n" : "\r\n");
		std::string const line = key + "=" + value;
		bool inSection = false;
		bool sectionFound = false;
		size_t insertAt = 0;          // after the last line of the section that isn't empty
		bool insertNeedsNewLine = false;

		for (size_t start = 0; start < ini.size();)
		{
			size_t const lineFeed = ini.find('\n', start);
			size_t const next = lineFeed == std::string::npos ? ini.size() : lineFeed + 1;
			size_t end = lineFeed == std::string::npos ? ini.size() : lineFeed;

			if (end > start && ini[end - 1] == '\r')
			{
				end--;
			}

			// a UTF-8 mark at the very start isn't part of the first line's words
			size_t const wordsFrom = start == 0 && ini.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : start;
			std::string const words = Trimmed(ini.substr(wordsFrom, end > wordsFrom ? end - wordsFrom : 0));

			if (!words.empty() && words[0] == '[')
			{
				size_t const close = words.find(']');

				inSection = close != std::string::npos && SameName(Trimmed(words.substr(1, close - 1)), sectionName);

				if (inSection && !sectionFound)
				{
					sectionFound = true;
					insertAt = next;
					insertNeedsNewLine = lineFeed == std::string::npos;
				}
				else if (inSection)
				{
					inSection = false;   // a second [section] of the same name: Windows reads the first, so only the first is written
				}
			}
			else if (inSection && !words.empty())
			{
				size_t const equals = words.find('=');

				if (words[0] != ';' && equals != std::string::npos && SameName(Trimmed(words.substr(0, equals)), keyName))
				{
					*out = ini.substr(0, start) + line + ini.substr(end);   // the line's own ending, and every other byte, kept
					return true;
				}

				insertAt = next;
				insertNeedsNewLine = lineFeed == std::string::npos;
			}

			start = next;
		}

		if (sectionFound)
		{
			*out = ini.substr(0, insertAt) + (insertNeedsNewLine ? newLine : std::string()) + line + newLine + ini.substr(insertAt);
			return true;
		}

		bool const endsLine = ini.empty() || ini.back() == '\n';

		*out = ini + (endsLine ? std::string() : newLine) + "[" + section + "]" + newLine + line + newLine;
		return true;
	}

	bool FirstLaunch_IniWithSection(std::string const &ini, std::string const &section, std::vector<std::string> const &lines, std::string *out)
	{
		std::string const sectionName = Trimmed(section);

		if (!out || sectionName.empty() || Unwritable(section, "[]"))
		{
			return false;
		}

		for (std::string const &line : lines)
		{
			if (Unwritable(line, ""))
			{
				return false;
			}
		}

		std::string const newLine = ini.find("\r\n") != std::string::npos ? "\r\n" : (ini.find('\n') != std::string::npos ? "\n" : "\r\n");
		std::string body;

		for (std::string const &line : lines)
		{
			body += line + newLine;
		}

		bool found = false;
		bool inSection = false;
		size_t bodyFrom = 0;       // just after the section's header line
		size_t bodyTo = 0;         // just after its last line that isn't empty (bodyFrom when it has none)
		bool headerNeedsNewLine = false;

		for (size_t start = 0; start < ini.size();)
		{
			size_t const lineFeed = ini.find('\n', start);
			size_t const next = lineFeed == std::string::npos ? ini.size() : lineFeed + 1;
			size_t end = lineFeed == std::string::npos ? ini.size() : lineFeed;

			if (end > start && ini[end - 1] == '\r')
			{
				end--;
			}

			size_t const wordsFrom = start == 0 && ini.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : start;
			std::string const words = Trimmed(ini.substr(wordsFrom, end > wordsFrom ? end - wordsFrom : 0));

			if (!words.empty() && words[0] == '[')
			{
				if (inSection)
				{
					break;   // the next section: the body ended before it
				}

				size_t const close = words.find(']');

				if (!found && close != std::string::npos && SameName(Trimmed(words.substr(1, close - 1)), sectionName))
				{
					found = true;
					inSection = true;
					bodyFrom = next;
					bodyTo = next;
					headerNeedsNewLine = lineFeed == std::string::npos;
				}
			}
			else if (inSection && !words.empty())
			{
				bodyTo = next;
			}

			start = next;
		}

		if (found)
		{
			*out = ini.substr(0, bodyFrom) + (headerNeedsNewLine ? newLine : std::string()) + body + ini.substr(bodyTo);
			return true;
		}

		bool const endsLine = ini.empty() || ini.back() == '\n';

		*out = ini + (endsLine ? std::string() : newLine) + "[" + section + "]" + newLine + body;
		return true;
	}
}
