// shapes_trigger.cpp: the search of new maps for object shapes, one map a frame (shapes_trigger.h says what and why). Written 2026-11-23 by Melanikus (Drake)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shapes_trigger.h"

#include <ctype.h>

#include "panel_paint.h"   // PanelPaint_SafeText: the editor's one rule for safe text

namespace host
{
	namespace
	{
		std::string Lower(std::string text)
		{
			for (char &letter : text)
			{
				letter = (char)tolower((unsigned char)letter);
			}

			return text;
		}

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

		// "Genshd1.RIF" from "C:\...\avp_rifs\Genshd1.RIF": the name the list and the words keep (a path stays out of the ini and off the screen)
		std::string FileOf(std::string const &path)
		{
			size_t const cut = path.find_last_of("\\/");

			return ShapesScanned_Name(cut == std::string::npos ? path : path.substr(cut + 1));
		}
	}

	void ShapesTrigger::Begin(ShapesScannedList const &kept, std::vector<std::string> const &found)
	{
		*this = ShapesTrigger();
		list = kept.entries;
		paths = found;

		for (ShapesScannedEntry const &entry : list)
		{
			known.insert(Lower(entry.sha256));
		}
	}

	void ShapesTrigger::Add(std::string const &path)
	{
		for (size_t k = next; k < paths.size(); k++)
		{
			if (paths[k] == path)
			{
				return;   // still waiting to be taken
			}
		}

		paths.push_back(path);
	}

	bool ShapesTrigger::Step(ShapesTriggerHooks const &hooks)
	{
		if (next >= paths.size())
		{
			return false;
		}

		std::string const path = paths[next++];
		std::string const file = FileOf(path);
		std::string sha256;
		std::string const why = hooks.read ? hooks.read(path, &sha256) : std::string("nothing reads it");

		if (!why.empty() || !IsSha256(sha256))
		{
			named.push_back(file + ": " + PanelPaint_SafeText(why.empty() ? "no SHA-256" : why));
			return true;
		}

		std::string const key = Lower(sha256);

		if (known.count(key))
		{
			return true;   // searched before (on the list), or a copy of a map met earlier in this run
		}

		if (list.size() >= kShapesScannedMost)
		{
			named.push_back(file + ": the list of maps searched is full (" + std::to_string(kShapesScannedMost) + ")");
			return true;
		}

		ShapesMapSearch const search = hooks.search ? hooks.search(path) : ShapesMapSearch{ 0, true, "nothing searches it" };

		known.insert(key);
		list.push_back({ key, file });
		changed = true;
		scanned++;
		written += search.written;

		if (search.failed)
		{
			named.push_back(file + ": " + PanelPaint_SafeText(search.why.empty() ? "the search failed" : search.why));
		}

		return true;
	}

	bool ShapesTrigger::Done() const
	{
		return next >= paths.size();
	}

	bool ShapesTrigger::Changed() const
	{
		return changed;
	}

	std::vector<ShapesScannedEntry> const &ShapesTrigger::List() const
	{
		return list;
	}

	size_t ShapesTrigger::Scanned() const
	{
		return scanned;
	}

	size_t ShapesTrigger::Written() const
	{
		return written;
	}

	std::string ShapesTrigger::Words() const
	{
		if (scanned == 0 && named.empty())
		{
			return std::string();
		}

		std::string words = "Shapes: " + std::to_string((unsigned long long)scanned) + (scanned == 1 ? " new map" : " new maps") + " scanned, " +
			std::to_string((unsigned long long)written) + (written == 1 ? " new shape" : " new shapes") + " in shape_rifs";

		// the maps it couldn't search, or that failed: the first three named, the rest counted (the log has them all from the host)
		for (size_t k = 0; k < named.size() && k < 3; k++)
		{
			words += (k == 0 ? "; " : ", ") + named[k];
		}

		if (named.size() > 3)
		{
			words += " and " + std::to_string((unsigned long long)(named.size() - 3)) + " more";
		}

		return words;
	}
}
