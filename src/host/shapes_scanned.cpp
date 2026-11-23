// shapes_scanned.cpp: the maps already searched for object shapes, a list kept in AVP_OpenInteng.ini (shapes_scanned.h says what and why). Written 2026-11-23
// by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shapes_scanned.h"

#include <ctype.h>

#include <set>

#include "first_launch_ui.h"   // FirstLaunch_IniWithSection: the section rewritten, every other byte kept
#include "panel_paint.h"       // PanelPaint_SafeText: the editor's one rule for safe text

namespace host
{
	char const kShapesScannedSection[] = "ShapesScanned";

	namespace
	{
		std::string Trimmed(std::string const &text)
		{
			size_t from = 0;
			size_t to = text.size();

			while (from < to && (text[from] == ' ' || text[from] == '\t' || text[from] == '\r'))
			{
				from++;
			}

			while (to > from && (text[to - 1] == ' ' || text[to - 1] == '\t' || text[to - 1] == '\r'))
			{
				to--;
			}

			return text.substr(from, to - from);
		}

		std::string Lower(std::string text)
		{
			for (char &letter : text)
			{
				letter = (char)tolower((unsigned char)letter);
			}

			return text;
		}

		// 64 hex letters (any case), nothing else
		bool IsSha256(std::string const &text)
		{
			if (text.size() != 64)
			{
				return false;
			}

			for (char const letter : text)
			{
				if (!isxdigit((unsigned char)letter))
				{
					return false;
				}
			}

			return true;
		}
	}

	std::string ShapesScanned_Name(std::string const &name)
	{
		std::string const safe = PanelPaint_SafeText(name);

		return safe.size() > kShapesScannedMostName ? safe.substr(0, kShapesScannedMostName) : safe;
	}

	ShapesScannedList ShapesScanned_Read(std::string const &ini)
	{
		ShapesScannedList list;
		std::set<std::string> seen;
		bool inSection = false;

		for (size_t start = 0; start < ini.size();)
		{
			size_t const lineFeed = ini.find('\n', start);
			size_t const next = lineFeed == std::string::npos ? ini.size() : lineFeed + 1;
			size_t const wordsFrom = start == 0 && ini.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : start;
			size_t const end = lineFeed == std::string::npos ? ini.size() : lineFeed;
			std::string const words = Trimmed(ini.substr(wordsFrom, end > wordsFrom ? end - wordsFrom : 0));

			start = next;

			if (!words.empty() && words[0] == '[')
			{
				if (inSection)
				{
					break;   // the first [ShapesScanned] is the one read, as Windows reads the first
				}

				size_t const close = words.find(']');

				inSection = close != std::string::npos && Lower(Trimmed(words.substr(1, close - 1))) == Lower(kShapesScannedSection);
				list.present = list.present || inSection;
				continue;
			}

			if (!inSection || words.empty() || words[0] == ';')
			{
				continue;
			}

			size_t const equals = words.find('=');
			std::string const key = Lower(Trimmed(words.substr(0, equals)));
			std::string const name = equals == std::string::npos ? std::string() : Trimmed(words.substr(equals + 1));

			if (!IsSha256(key) || seen.count(key) || list.entries.size() >= kShapesScannedMost)
			{
				if (list.refused.size() < kShapesScannedMostRefused)
				{
					list.refused.push_back(PanelPaint_SafeText(words.substr(0, 120)));
				}
				else
				{
					list.refusedMore++;
				}

				continue;
			}

			seen.insert(key);
			list.entries.push_back({ key, ShapesScanned_Name(name) });
		}

		return list;
	}

	bool ShapesScanned_Has(ShapesScannedList const &list, std::string const &sha256)
	{
		std::string const key = Lower(sha256);

		for (ShapesScannedEntry const &entry : list.entries)
		{
			if (entry.sha256 == key)
			{
				return true;
			}
		}

		return false;
	}

	bool ShapesScanned_Write(std::string const &ini, std::vector<ShapesScannedEntry> const &entries, std::string *out)
	{
		if (!out || entries.size() > kShapesScannedMost)
		{
			return false;
		}

		std::vector<std::string> lines;

		lines.push_back("; the maps Open Inteng has searched for object shapes (it writes this list; a map not on it is searched at the next start)");

		for (ShapesScannedEntry const &entry : entries)
		{
			if (!IsSha256(entry.sha256))
			{
				return false;
			}

			lines.push_back(Lower(entry.sha256) + "=" + ShapesScanned_Name(entry.name));
		}

		return FirstLaunch_IniWithSection(ini, kShapesScannedSection, lines, out);
	}
}
