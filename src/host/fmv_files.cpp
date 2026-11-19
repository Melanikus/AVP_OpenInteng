// See fmv_files.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "fmv_files.h"

#include <windows.h>

#include <string.h>

#include <algorithm>

namespace host
{
	std::string FmvFiles_Folder(char const *gameFolder)
	{
		if (!gameFolder || !gameFolder[0])
		{
			return std::string();
		}

		std::string folder = gameFolder;

		while (!folder.empty() && (folder.back() == '\\' || folder.back() == '/'))
		{
			folder.pop_back();
		}

		return folder + "\\FMVs\\";
	}

	std::vector<std::string> FmvFiles_Names(std::string const &folder, size_t most)
	{
		std::vector<std::string> names;
		std::string root = folder;

		while (!root.empty() && (root.back() == '\\' || root.back() == '/'))
		{
			root.pop_back();
		}

		if (root.empty() || root.size() > MAX_PATH - 8)
		{
			return names;
		}

		WIN32_FIND_DATAA found;
		HANDLE const search = FindFirstFileA((root + "\\*.smk").c_str(), &found);

		if (search == INVALID_HANDLE_VALUE)
		{
			return names;
		}

		do
		{
			size_t const length = strnlen(found.cFileName, sizeof(found.cFileName));
			bool plain = length > 0 && length < 256;

			for (size_t i = 0; plain && i < length; i++)
			{
				plain = found.cFileName[i] >= ' ' && found.cFileName[i] <= '~';
			}

			if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && plain)
			{
				names.push_back(std::string(found.cFileName, length));
			}
		}
		while (names.size() < most && FindNextFileA(search, &found));

		FindClose(search);
		return names;
	}

	std::vector<int32_t> FmvFiles_MessageNumbers(std::string const &folder)
	{
		std::vector<int32_t> numbers;

		for (std::string const &name : FmvFiles_Names(folder))
		{
			int32_t number = 0;

			if (model::MessageFmvNumber(name, &number))
			{
				numbers.push_back(number);
			}
		}

		std::sort(numbers.begin(), numbers.end());
		numbers.erase(std::unique(numbers.begin(), numbers.end()), numbers.end());
		return numbers;
	}

	FmvFileRead FmvFiles_Header(std::string const &folder, std::string const &name, model::SmkHeader *header, std::string *why)
	{
		std::string root = folder;

		while (!root.empty() && (root.back() == '\\' || root.back() == '/'))
		{
			root.pop_back();
		}

		std::string const path = root + "\\" + name;

		if (root.empty() || name.empty() || path.size() >= MAX_PATH)
		{
			return FmvFileRead::Missing;
		}

		DWORD const attributes = GetFileAttributesA(path.c_str());

		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			return FmvFileRead::Missing;
		}

		if (attributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (why)
			{
				*why = "it is a folder";
			}

			return FmvFileRead::NotAFile;
		}

		HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			if (why)
			{
				*why = "Windows error " + std::to_string((unsigned long long)GetLastError());
			}

			return FmvFileRead::NotAFile;
		}

		uint8_t bytes[model::kSmkHeaderBytes];
		DWORD got = 0;
		BOOL const read = ReadFile(file, bytes, (DWORD)sizeof(bytes), &got, nullptr);

		CloseHandle(file);

		if (!read)
		{
			got = 0;
		}

		switch (model::SmkHeader_Read(bytes, got <= sizeof(bytes) ? (size_t)got : 0, header))
		{
		case model::SmkRead::Read:
			return FmvFileRead::Read;
		case model::SmkRead::NotSmacker:
			return FmvFileRead::NotSmacker;
		case model::SmkRead::Impossible:
			return FmvFileRead::Impossible;
		case model::SmkRead::TooShort:
		default:
			return FmvFileRead::TooShort;
		}
	}

	std::string FmvFiles_MessageWords(std::string const &folder, int32_t number)
	{
		if (number < 0 || number > model::kMostMessageNumber)
		{
			return "Message " + std::to_string((long long)number) + ": the game refuses a message number outside 0 to " + std::to_string((long long)model::kMostMessageNumber) + ", so it has no FMV";
		}

		std::string const name = model::MessageFmvName(number);
		std::string const shown = "FMVs\\" + name;
		model::SmkHeader header;
		std::string why;

		switch (FmvFiles_Header(folder, name, &header, &why))
		{
		case FmvFileRead::Read:
		{
			std::string const differs = model::SmkHeader_DiffersFromAvp(header);

			return shown + ": " + model::SmkHeader_Words(header) + (differs.empty() ? std::string() : "; not AvP's message format: " + differs);
		}
		case FmvFileRead::Missing:
			return "No " + shown + ": the game shows message " + std::to_string((long long)number) + " as text only";
		case FmvFileRead::NotAFile:
			return shown + " can't be read (" + why + "): the game can't play it";
		case FmvFileRead::TooShort:
			return shown + " is too short for a Smacker video: the game can't play it";
		case FmvFileRead::NotSmacker:
			return shown + " is not a Smacker video (no SMK2 or SMK4 at its start): the game can't play it";
		case FmvFileRead::Impossible:
		default:
			return shown + " is a Smacker file with impossible numbers (a side of 0 or above 4096, or no frames): the game can't play it";
		}
	}

	std::vector<FmvScreenRow> FmvFiles_JudgeScreens(std::vector<std::string> const &pictures, std::function<void(size_t, uint32_t *, uint32_t *)> const &size,
		std::function<bool(std::string const &)> const &smkThere)
	{
		std::vector<FmvScreenRow> rows;
		int32_t screens = 0;
		bool full = false;

		for (size_t i = 0; i < pictures.size(); i++)
		{
			std::string const &name = pictures[i];
			std::string::size_type const offset1 = name.find("FMVs");

			if (offset1 == std::string::npos)
			{
				continue;
			}

			// the game's own sums, std::string's substr included (a '.' before "FMVs", or none, takes the rest of the name)
			std::string::size_type const offset2 = name.find(".");
			FmvScreenRow row;

			row.picture = name;
			row.smk = name.substr(offset1, offset2 - offset1) + ".smk";
			row.width = row.height = 0;

			// the game BREAKS when its list is full (FmvCutscenes.cpp:385-389): this picture and every later one stay still
			if (full || screens >= kFmvScreensMost)
			{
				full = true;
				row.verdict = FmvScreen::StillPastTheTenth;
				rows.push_back(row);
				continue;
			}

			if (smkThere && smkThere(row.smk))
			{
				row.verdict = FmvScreen::StillItsSmkIsThere;
				rows.push_back(row);
				continue;
			}

			if (size)
			{
				size(i, &row.width, &row.height);
			}

			row.verdict = row.width == kFmvScreenSide && row.height == kFmvScreenSide ? FmvScreen::Screen : FmvScreen::StillWrongSize;
			screens += row.verdict == FmvScreen::Screen ? 1 : 0;
			rows.push_back(row);
		}

		return rows;
	}

	std::string FmvFiles_ScreensWords(std::vector<FmvScreenRow> const &rows)
	{
		std::vector<FmvScreenRow const *> still;

		for (FmvScreenRow const &row : rows)
		{
			if (row.verdict != FmvScreen::Screen)
			{
				still.push_back(&row);
			}
		}

		if (still.empty())
		{
			return std::string();
		}

		FmvScreenRow const &first = *still.front();
		std::string why;

		switch (first.verdict)
		{
		case FmvScreen::StillItsSmkIsThere:
			why = first.smk + " is in the game folder, and the game plays messages only on a screen whose own .smk is missing";
			break;
		case FmvScreen::StillWrongSize:
			why = "it is " + std::to_string(first.width) + " x " + std::to_string(first.height) + ", and a video screen must be 128 x 128";
			break;
		default:
			why = "the level has more than 10 video screens, the most the game plays on";
			break;
		}

		return (still.size() == 1 ? std::string("Video screen ") : std::to_string(still.size()) + " video screens stay still; the first, ") + first.picture +
			(still.size() == 1 ? " stays still: " : ": ") + why;
	}
}
