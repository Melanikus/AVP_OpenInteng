// shape_search.cpp: the lookup order for a shape's file (shape_search.h says the order, the checks and why). Written 2026-11-23 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shape_search.h"

#include <string.h>

#include <windows.h>

#include "game_folder.h"    // GameFolder_WhyNotPlainBelow: every folder below the game's folder on the way, plain
#include "save_folder.h"    // SaveFolder_WhyNotUsable: the data folder's own rule (a whole path, the folder itself plain)
#include "shape_import.h"   // ShapeImport_Read: the file on one handle

namespace host
{
	namespace
	{
		std::string WithoutLastSlashes(std::string path)
		{
			while (path.size() > 3 && (path.back() == '\\' || path.back() == '/'))
			{
				path.pop_back();
			}

			return path;
		}

		// A path in Windows' wide letters (the code page's, as the A functions read it), its slashes made one kind; empty when it can't be read
		std::wstring WideFolder(std::string const &path)
		{
			int const letters = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path.c_str(), (int)path.size(), nullptr, 0);
			std::wstring wide(letters > 0 ? (size_t)letters : 0, L'\0');

			if (letters <= 0 || MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path.c_str(), (int)path.size(), &wide[0], letters) != letters)
			{
				return std::wstring();
			}

			for (wchar_t &letter : wide)
			{
				letter = letter == L'/' ? L'\\' : letter;
			}

			return wide;
		}

		// The same folder as Windows names it: either slash, and any capitals as the file system compares them (CompareStringOrdinal ignoring case: the
		// upcase table Windows uses for file names, every letter of it, not only A to Z). Two paths that can't be read are never the same
		bool SameFolder(std::string const &a, std::string const &b)
		{
			std::wstring const x = WideFolder(a);
			std::wstring const y = WideFolder(b);

			return !x.empty() && !y.empty() && CompareStringOrdinal(x.c_str(), (int)x.size(), y.c_str(), (int)y.size(), TRUE) == CSTR_EQUAL;
		}
	}

	std::vector<ShapeSearchFolder> ShapeSearch_Order(std::string const &setting, std::string const &gameFolder)
	{
		std::vector<ShapeSearchFolder> order;
		std::string const game = WithoutLastSlashes(gameFolder);
		auto const add = [&order](ShapeSearchFolder const &folder)
		{
			for (ShapeSearchFolder const &before : order)
			{
				if (SameFolder(WithoutLastSlashes(before.path), WithoutLastSlashes(folder.path)))
				{
					return;
				}
			}

			order.push_back(folder);
		};

		if (!setting.empty())
		{
			add({ WithoutLastSlashes(setting), "ShapeFolder=", "" });
		}

		if (!game.empty())
		{
			add({ game + "\\Tools\\shape_rifs", "the game's Tools\\shape_rifs", game });
			add({ game + "\\shape_rifs", "the game's shape_rifs", game });
		}

		return order;
	}

	std::vector<ShapeSearchFolder> ShapeSearch_RefreshOrder(std::string const &setting, std::string const &gameFolder)
	{
		// the order without the game's shape_rifs (shape_search.h says why). A ShapeFolder= the user set to that very folder stays: it is the user's own choice
		std::vector<ShapeSearchFolder> order = ShapeSearch_Order(setting, gameFolder);
		std::string const game = WithoutLastSlashes(gameFolder);
		std::vector<ShapeSearchFolder> kept;

		for (ShapeSearchFolder const &folder : order)
		{
			if (folder.under.empty() || !SameFolder(folder.path, game + "\\shape_rifs"))
			{
				kept.push_back(folder);
			}
		}

		return kept;
	}

	std::string ShapeSearch_FolderWhyNot(ShapeSearchFolder const &folder, bool *missing)
	{
		bool notSaid = false;

		missing = missing ? missing : &notSaid;
		*missing = false;

		// a whole path on a drive, the folder itself there and plain (not a link, a junction or a file)
		char checked[MAX_PATH];
		char const *const notUsable = SaveFolder_WhyNotUsable(folder.path.c_str(), checked, sizeof(checked));

		if (notUsable && strcmp(notUsable, kSaveFolderGivenNotThere) == 0)
		{
			*missing = true;
			return std::string();
		}

		if (notUsable)
		{
			return notUsable;
		}

		// a local drive: opening a folder on a network drive hands that computer the user's login
		char const root[4] = { checked[0], ':', '\\', 0 };
		UINT const drive = GetDriveTypeA(root);

		if (drive == DRIVE_REMOTE || drive == DRIVE_UNKNOWN || drive == DRIVE_NO_ROOT_DIR)
		{
			return "it is on a network drive, or on a drive Windows can't tell";
		}

		// below the game's folder: every folder on the way down plain (the game's folder itself was held to its rule at the start)
		if (!folder.under.empty())
		{
			std::string which;
			char const *const notPlain = GameFolder_WhyNotPlainBelow(folder.under, std::string(checked) + "x.rif", &which);

			if (notPlain)
			{
				return which + ": " + notPlain;
			}
		}

		return std::string();
	}

	ShapeSearchResult ShapeSearch_Find(std::vector<ShapeSearchFolder> const &order, std::string const &name, ShapeSearchFolder *found, std::string *why)
	{
		std::string notSaid;
		ShapeSearchFolder none;

		why = why ? why : &notSaid;
		found = found ? found : &none;
		why->clear();
		*found = ShapeSearchFolder();

		std::vector<char> bytes;

		// the name first: a name that isn't plain is never looked for, in any folder (shape_import.h)
		if (!ShapeImport_NameIsPlain(name))
		{
			*why = ShapeImportWords(ShapeImportResult::BadName);
			return ShapeSearchResult::BadName;
		}

		if (order.empty())
		{
			*why = "no shape folder: no ShapeFolder= in AVP_OpenInteng.ini, and no game folder";
			return ShapeSearchResult::NothingToLook;
		}

		for (ShapeSearchFolder const &folder : order)
		{
			bool missing = false;
			std::string const whyNot = ShapeSearch_FolderWhyNot(folder, &missing);

			if (missing)
			{
				continue;
			}

			if (!whyNot.empty())
			{
				*found = folder;
				*why = folder.label + " can't be used (" + whyNot + "), so the shape isn't looked for past it";
				return ShapeSearchResult::FolderRefused;
			}

			ShapeImportResult const read = ShapeImport_Read(folder.path.c_str(), name, &bytes, nullptr);

			if (read == ShapeImportResult::NotFound)
			{
				continue;
			}

			*found = folder;

			if (read != ShapeImportResult::Done)
			{
				*why = name + ".rif in " + folder.label + ": " + ShapeImportWords(read) + ", so the shape isn't looked for past it";
				return ShapeSearchResult::FileRefused;
			}

			return ShapeSearchResult::Found;
		}

		*why = "no " + name + ".rif in " + ShapeSearch_Labels(order);
		return ShapeSearchResult::NotFound;
	}

	std::string ShapeSearch_Labels(std::vector<ShapeSearchFolder> const &order)
	{
		std::string words;

		for (size_t i = 0; i < order.size(); i++)
		{
			words += (i == 0 ? "" : (i + 1 == order.size() ? " or " : ", ")) + order[i].label;
		}

		return words;
	}
}
