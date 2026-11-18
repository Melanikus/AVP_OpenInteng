// open_at.cpp: the editor opened at a place, -at and -shot (open_at.h says what and why). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "open_at.h"

#include <string.h>
#include <wchar.h>

#include <vector>

#include <windows.h>

#include "editor_shared.h"   // theCamera
#include "inteng_menus.h"    // IntengMenus_ScreenFromCommandWord
#include "level_view.h"      // FlyCamera, FlyCamera_Hold
#include "level_objects.h"   // LevelObjects_Read
#include "object_list.h"     // host::LevelObject
#include "selection.h"

namespace host
{
	namespace
	{
		// A command-line word as plain ASCII of at most most bytes; false for a word with anything past ASCII, an empty one, or a longer one
		bool Plain(wchar_t const *word, size_t most, std::string *out)
		{
			out->clear();

			for (size_t k = 0; word && word[k] != L'\0'; k++)
			{
				if (word[k] < 0x20 || word[k] > 0x7e || out->size() >= most)
				{
					return false;
				}

				out->push_back((char)word[k]);
			}

			return !out->empty();
		}

		size_t const kMostLevelBytes = 256u << 20;   // a level read whole for its sha256: far past any level (the largest stock one is a few MB)
	}

	bool OpenAt_Argument(wchar_t **args, int count, int *i, OpenAt *out, std::string *why)
	{
		bool const at = wcscmp(args[*i], L"-at") == 0;
		bool const shot = wcscmp(args[*i], L"-shot") == 0;

		if (!at && !shot)
		{
			return false;
		}

		why->clear();

		if (out->at || out->shot)
		{
			*why = "-at and -shot: one of them, once";
			return true;
		}

		if (at)
		{
			if (*i + 5 >= count)
			{
				*why = "-at takes x y z turn look: <level> -at <x> <y> <z> <turn> <look> [<screen>]";
				return true;
			}

			std::string words[5];
			char const *pointers[5] = {};

			for (int k = 0; k < 5; k++)
			{
				if (!Plain(args[*i + 1 + k], 16, &words[k]))
				{
					*why = "-at takes x y z turn look as whole numbers";
					return true;
				}

				pointers[k] = words[k].c_str();
			}

			if (!ShotRead_Numbers(pointers, &out->place, why))
			{
				return true;
			}

			*i += 5;

			// a screen after the numbers: the next word, when it does not start with '-' (-screen's own reader checks it)
			if (*i + 1 < count && args[*i + 1][0] != L'-' && args[*i + 1][0] != L'\0')
			{
				std::string screen;

				if (!Plain(args[*i + 1], 64, &screen))
				{
					*why = "-at's screen is 1 to 64 plain letters, as -screen names it";
					return true;
				}

				out->place.screen = screen;
				(*i)++;
			}

			out->at = true;
			return true;
		}

		if (*i + 1 >= count)
		{
			*why = "-shot takes a screenshot's path: -shot <the .png>";
			return true;
		}

		char path[MAX_PATH] = "";
		BOOL lost = FALSE;
		int const length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, args[*i + 1], -1, path, sizeof(path), NULL, &lost);

		if (length <= 1 || lost)
		{
			*why = "-shot takes a picture whose path this program can read (shorter than 260 letters)";
			return true;
		}

		if (!ShotRead_File(path, &out->place, why))
		{
			*why = "-shot " + std::string(path) + ": " + *why;
			return true;
		}

		out->shotPath = path;
		out->shot = true;
		(*i)++;
		return true;
	}

	bool OpenAt_Level(OpenAt const &at, std::wstring const &levelWord, char const *dataFolder, char *levelName, size_t size, std::string (*sha256)(uint8_t const *, size_t), std::string *why)
	{
		std::string name;

		if (at.shot)
		{
			if (!levelWord.empty())
			{
				*why = "-shot opens the shot's own level: give no level with it";
				return false;
			}

			name = at.place.map;
		}
		else if (levelWord.empty())
		{
			*why = "-at needs the level: AVP_OpenInteng.exe <level> -at <x> <y> <z> <turn> <look> [<screen>]";
			return false;
		}
		else if (!Plain(levelWord.c_str(), 128, &name) || !ShotRead_PlainName(name))
		{
			*why = "with -at the level is a NAME of the data folder's levels (avp_rifs), not a path";
			return false;
		}

		std::string base = name;

		if (base.size() > 4 && _stricmp(base.c_str() + base.size() - 4, ".rif") == 0)
		{
			base.resize(base.size() - 4);
		}

		std::string const path = std::string(dataFolder ? dataFolder : "") + "\\avp_rifs\\" + base + ".rif";
		DWORD const kind = GetFileAttributesA(path.c_str());

		if (base.empty() || base.size() + 1 > size || !dataFolder || path.size() >= MAX_PATH || kind == INVALID_FILE_ATTRIBUTES ||
			(kind & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
		{
			*why = "the level " + name + " is not one of the data folder's levels (" + path + ")";
			return false;
		}

		if (at.shot)
		{
			HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
			LARGE_INTEGER bytes = {};
			std::vector<uint8_t> data;
			bool read = file != INVALID_HANDLE_VALUE && GetFileSizeEx(file, &bytes) && bytes.QuadPart >= 0 && (unsigned long long)bytes.QuadPart <= kMostLevelBytes;

			if (read)
			{
				data.resize((size_t)bytes.QuadPart);

				size_t done = 0;

				while (read && done < data.size())
				{
					DWORD got = 0;
					DWORD const ask = (DWORD)(data.size() - done < (1u << 20) ? data.size() - done : (1u << 20));

					read = ReadFile(file, data.data() + done, ask, &got, nullptr) && got > 0;
					done += got;
				}
			}

			if (file != INVALID_HANDLE_VALUE)
			{
				CloseHandle(file);
			}

			if (!read)
			{
				*why = "the level " + path + " can't be read whole (at most 256 MB) for the shot's sha256";
				return false;
			}

			std::string const have = sha256(data.data(), data.size());

			if (have != at.place.sha256)
			{
				*why = "the level " + path + " is not the shot's: its sha256 is " + have + ", the shot's " + at.place.sha256;
				return false;
			}
		}

		strcpy_s(levelName, size, base.c_str());
		return true;
	}

	bool OpenAt_Screen(OpenAt const &at, bool screenGiven, size_t *startScreen, std::string *why)
	{
		if (screenGiven)
		{
			*why = "the screen is given twice: by -screen and by " + std::string(at.shot ? "the shot" : "-at");
			return false;
		}

		std::wstring const word(at.place.screen.begin(), at.place.screen.end());   // plain ASCII (shot_read.h, OpenAt_Argument)
		std::string screenWhy;

		if (!IntengMenus_ScreenFromCommandWord(word.c_str(), startScreen, &screenWhy))
		{
			*why = "the screen \"" + at.place.screen + "\": " + screenWhy;
			return false;
		}

		return true;
	}

	void OpenAt_Apply(OpenAt *at, std::string *words)
	{
		words->clear();

		if (!at || at->applied || (!at->at && !at->shot) || !theCamera)
		{
			return;
		}

		at->applied = true;

		std::string selected = "nothing selected";

		if (!at->place.selected.empty())
		{
			std::vector<LevelObject> objects;
			bool found = false;

			if (LevelObjects_Read(&objects))
			{
				for (LevelObject const &one : objects)
				{
					if (one.name && at->place.selected == one.name)
					{
						Selected picked = {};

						picked.kind = SelectedKind::Object;
						memcpy(picked.objectId, one.id, kSelectionIdBytes);
						found = Selection_Set(picked);
						break;
					}
				}
			}

			selected = found ? "selected " + at->place.selected : "no object of the level is named " + at->place.selected + " (a special object or a light is not selected this way)";
		}

		theCamera->x = at->place.pos[0];
		theCamera->y = at->place.pos[1];
		theCamera->z = at->place.pos[2];
		theCamera->turn = at->place.turn;
		theCamera->look = at->place.look;
		theCamera->roll = 0;
		FlyCamera_Hold(theCamera);

		*words = std::string(at->shot ? "Opened at the shot's place: " : "Opened at ") + std::to_string(theCamera->x) + " " + std::to_string(theCamera->y) + " " + std::to_string(theCamera->z) + " turn " +
			std::to_string(theCamera->turn) + " look " + std::to_string(theCamera->look) + ", " + selected;
	}
}
