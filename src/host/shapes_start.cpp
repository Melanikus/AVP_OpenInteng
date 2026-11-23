// shapes_start.cpp: the player's maps found and searched for object shapes, on the disk (shapes_start.h says what and why). Written 2026-11-23 by Melanikus
// (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shapes_start.h"

#include <ctype.h>

#include <algorithm>
#include <set>

#include <windows.h>

#include "game_folder.h"            // GameFolder_WhyNotPlainBelow: every folder below the data folder on the way, plain
#include "shape_extract_files.h"    // ShapeExtract_ReadMapFile, ShapeExtract_BytesToFolder, ShapeExtract_StockCheck
#include "shape_identity.h"         // the identity table's names (every stock outside shape)

namespace host
{
	namespace
	{
		// the folders searched, in this order, below the data folder (the levels, a player's own, the copies Open with works on, the HUD files)
		char const *const kMapFolders[] = { "avp_rifs", "avp_rifs\\Custom", "avp_rifs\\OpenedByEditor", "avp_huds" };

		std::string Lower(std::string text)
		{
			for (char &letter : text)
			{
				letter = (char)tolower((unsigned char)letter);
			}

			return text;
		}

		std::wstring Wide(std::string const &text)
		{
			int const length = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, nullptr, 0);
			std::wstring wide(length > 1 ? (size_t)length - 1 : 0, L'\0');

			if (length > 1)
			{
				MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, &wide[0], length);
			}

			return wide;
		}

		// A long name as the editor's paths are (the code page), or false when the code page can't say a letter of it (WC_NO_BEST_FIT_CHARS: a letter it
		// lacks is never "best fitted" onto another, so one name can't stand for another)
		bool Narrow(wchar_t const *name, std::string *out)
		{
			BOOL unsaid = FALSE;
			int const length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, name, -1, nullptr, 0, nullptr, &unsaid);

			if (length <= 1 || unsaid)
			{
				return false;
			}

			out->assign((size_t)length - 1, '\0');
			unsaid = FALSE;
			return WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, name, -1, &(*out)[0], length, nullptr, &unsaid) == length && !unsaid;
		}

		bool EndsWithRif(std::string const &name)
		{
			return name.size() > 4 && Lower(name.substr(name.size() - 4)) == ".rif";
		}

		// Each plain file's long name in folder (FindExInfoBasic: no short 8.3 name is even looked up), the ones a test keeps, at most `most`, sorted (any
		// case, then as written, so the order is fixed). False with *why when the folder can't be listed; a folder that isn't there gives back true, empty
		bool PlainFiles(std::string const &folder, bool (*keep)(std::string const &), size_t most, std::vector<std::string> *names, size_t *more, size_t *unsaid,
			std::string *why)
		{
			WIN32_FIND_DATAW found;
			HANDLE const search = FindFirstFileExW(Wide(folder + "\\*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

			if (search == INVALID_HANDLE_VALUE)
			{
				DWORD const error = GetLastError();

				if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
				{
					return true;
				}

				*why = "it can't be listed (Windows error " + std::to_string((unsigned long)error) + ")";
				return false;
			}

			do
			{
				if (found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
				{
					continue;   // a folder (".", ".." among them), or a link of any kind: never a map
				}

				std::string name;

				if (!Narrow(found.cFileName, &name))
				{
					(*unsaid)++;
					continue;
				}

				if (!keep(name))
				{
					continue;
				}

				if (names->size() < most)
				{
					names->push_back(name);
				}
				else
				{
					(*more)++;
				}
			} while (FindNextFileW(search, &found));

			FindClose(search);
			std::sort(names->begin(), names->end(), [](std::string const &a, std::string const &b)
			{
				std::string const la = Lower(a), lb = Lower(b);
				return la != lb ? la < lb : a < b;
			});
			return true;
		}

		bool AnyName(std::string const &)
		{
			return true;
		}

		// the data folder as the editor may have it from DataFolder= ("C:\Games\AvP\"): without its last slashes, so no path made from it has an empty part
		std::string WithoutLastSlash(std::string folder)
		{
			while (folder.size() > 3 && (folder.back() == '\\' || folder.back() == '/'))
			{
				folder.pop_back();
			}

			return folder;
		}

		// "C:\Games\AvP\avp_rifs\Genshd1.RIF" -> "Genshd1.RIF"
		std::string FileOf(std::string const &path)
		{
			size_t const cut = path.find_last_of("\\/");

			return cut == std::string::npos ? path : path.substr(cut + 1);
		}
	}

	std::vector<std::string> ShapesStart_MapsFound(std::string const &dataFolderAsGiven, std::vector<std::string> *refused)
	{
		std::string const dataFolder = WithoutLastSlash(dataFolderAsGiven);
		std::vector<std::string> maps;

		for (char const *const below : kMapFolders)
		{
			std::string const folder = dataFolder + "\\" + below;
			DWORD const kind = GetFileAttributesW(Wide(folder).c_str());   // (of the folder's own name: a link isn't followed by this)

			if (kind == INVALID_FILE_ATTRIBUTES)
			{
				DWORD const error = GetLastError();

				if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
				{
					refused->push_back(std::string(below) + ": it can't be looked at (Windows error " + std::to_string((unsigned long)error) + ")");
				}

				continue;   // not there: nothing in it to search
			}

			// every folder from the data folder down to this one plain, this one too (asked of a file name inside it)
			std::string which;

			if (char const *const notPlain = GameFolder_WhyNotPlainBelow(dataFolder, folder + "\\x.rif", &which))
			{
				refused->push_back((which.empty() ? std::string(below) : which) + ": " + notPlain + ", so it isn't looked in");
				continue;
			}

			std::vector<std::string> names;
			size_t more = 0, unsaid = 0;
			std::string why;

			if (!PlainFiles(folder, EndsWithRif, kShapesStartMostAFolder, &names, &more, &unsaid, &why))
			{
				refused->push_back(std::string(below) + ": " + why);
				continue;
			}

			for (std::string const &name : names)
			{
				maps.push_back(folder + "\\" + name);
			}

			if (more)
			{
				refused->push_back(std::string(below) + ": " + std::to_string((unsigned long long)more) + " more .rif files not taken (at most " +
					std::to_string((unsigned long long)kShapesStartMostAFolder) + " a folder)");
			}

			if (unsaid)
			{
				refused->push_back(std::string(below) + ": " + std::to_string((unsigned long long)unsaid) + " file names Windows' code page can't say, not taken");
			}
		}

		return maps;
	}

	size_t ShapesStart_StockShapesMissing(std::string const &dataFolderAsGiven)
	{
		std::string const dataFolder = WithoutLastSlash(dataFolderAsGiven);
		std::vector<std::string> names;
		size_t more = 0, unsaid = 0;
		std::string why;
		std::set<std::string> there;

		// (a shape_rifs holding more than its most is listed only to that most: a name past it counts as missing, so the form may ask; the search decides)
		if (PlainFiles(dataFolder + "\\shape_rifs", AnyName, 65536, &names, &more, &unsaid, &why))
		{
			for (std::string const &name : names)
			{
				there.insert(Lower(name));
			}
		}

		size_t missing = 0;

		for (size_t k = 0; k < kShapeIdentityRowCount; k++)
		{
			missing += there.count(Lower(kShapeIdentityRows[k].fileName)) ? 0 : 1;
		}

		return missing;
	}

	FirstLaunchFacts ShapesStart_Facts(std::string const &programFolder, std::string const &dataFolderAsGiven, bool fromIni, std::vector<std::string> const &maps)
	{
		std::string const dataFolder = WithoutLastSlash(dataFolderAsGiven);
		ShapeStockCheck const check = ShapeExtract_StockCheck(dataFolder);
		FirstLaunchFacts facts;

		facts.programFolder = programFolder;
		facts.found = true;
		facts.fromIni = fromIni;
		facts.gameFolder = dataFolder;
		facts.levels = check.levels;
		facts.huds = check.huds;
		facts.notOwn = check.notOwn;
		facts.missing = check.missing;

		size_t const own = check.levelsOwn + check.hudsOwn;
		size_t const notSteams = maps.size() > own ? maps.size() - own : 0;   // every map found but Steam's own bytes: its shapes are known once it is searched

		facts.shapesToWrite = ShapesStart_StockShapesMissing(dataFolder) + notSteams;
		return facts;
	}

	bool ShapesStart_ShouldAsk(bool listKept, FirstLaunchFacts const &facts)
	{
		return !listKept && facts.found && facts.shapesToWrite > 0;
	}

	ShapesStartSearch::ShapesStartSearch(std::string const &folder) : shapeFolder(folder)
	{
	}

	ShapesTriggerHooks ShapesStartSearch::Hooks()
	{
		ShapesTriggerHooks hooks;

		hooks.read = [this](const std::string &path, std::string *sha256)
		{
			held.clear();
			heldPath.clear();
			sha256->clear();

			std::vector<char> bytes;
			std::string sum;
			std::string const why = ShapeExtract_ReadMapFile(path, &bytes, &sum);

			if (why.empty())
			{
				held.swap(bytes);
				heldPath = path;
				*sha256 = sum;
			}

			return why;
		};

		hooks.search = [this](const std::string &path)
		{
			ShapesMapSearch result;

			if (path != heldPath)
			{
				result.failed = true;
				result.why = "it wasn't the map just read, so it isn't searched";
				return result;
			}

			ShapeLevelReport report;
			ShapeExtract_BytesToFolder(FileOf(path), held, shapeFolder, &report);

			std::vector<char>().swap(held);   // the map's bytes let go at once (a map can be 16 MB)
			heldPath.clear();
			result.written = report.written.size();
			result.failed = report.failed;
			result.why = report.why;
			return result;
		};

		return hooks;
	}
}
