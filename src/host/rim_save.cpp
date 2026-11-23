// rim_save.cpp: an imported texture saved as a new RIM file of the mapper's own (rim_save.h says where, the rules and why). Written 2026-11-23 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "rim_save.h"

#include <string.h>

#include <windows.h>

#include "game_folder.h"
#include "level_file_name.h"   // LevelFileName_WhyRefused: the game's own rule for a name from a level
#include "video_import.h"      // VideoImport_ReadPlainFile: the editor's one reader for a file someone could have touched

namespace host
{
	namespace
	{
		char const kWhat[] = "Import texture";
		size_t const kSuggestedMost = 32;

		RimSaveResult Refuse(RimSaveResult result, std::string const &words, std::string *why)
		{
			*why = std::string(kWhat) + ": " + words;
			return result;
		}

		bool EndsWithCaseBlind(std::string const &text, char const *ending)
		{
			size_t const length = strlen(ending);

			return text.size() >= length && _strnicmp(text.c_str() + text.size() - length, ending, length) == 0;
		}

		// The last part of a path: what follows its last slash of either kind, or all of it
		std::string LastPart(std::string const &path)
		{
			size_t const slash = path.find_last_of("\\/");

			return slash == std::string::npos ? path : path.substr(slash + 1);
		}

		// A name for the words: the game's own way of showing a name it refused (cut short, any letter it shouldn't have shown as "?")
		std::string Shown(std::string const &name)
		{
			char line[kLevelFileNameMost + 8];

			LevelFileName_ForTheLog(name.c_str(), line, sizeof(line));
			return line;
		}

		std::wstring Wide(std::string const &narrow)
		{
			int const letters = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, narrow.c_str(), (int)narrow.size(), nullptr, 0);
			std::wstring wide(letters > 0 ? (size_t)letters : 0, L'\0');

			if (letters > 0 && MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, narrow.c_str(), (int)narrow.size(), &wide[0], letters) != letters)
			{
				wide.clear();
			}

			return wide;
		}

		std::string Error(DWORD error)
		{
			return "Windows error " + std::to_string(error);
		}
	}

	bool RimSave_EntryName(std::string const &levelFile, std::string const &texture, std::string *entry, std::string *why)
	{
		std::string notSaid;

		why = why ? why : &notSaid;
		why->clear();

		if (!entry)
		{
			*why = "no place for the name";
			return false;
		}

		entry->clear();

		// the level's part: its file's own name, without ".rif"; the texture's: the name given, without ".rim"
		std::string level = LastPart(levelFile);
		std::string name = texture;

		if (EndsWithCaseBlind(level, ".rif"))
		{
			level.resize(level.size() - 4);
		}

		if (EndsWithCaseBlind(name, ".rim"))
		{
			name.resize(name.size() - 4);
		}

		if (level.empty())
		{
			*why = "the level's file has no name before its .rif";
			return false;
		}

		if (name.empty())
		{
			*why = "the texture has no name";
			return false;
		}

		if (name.find_first_of("\\/") != std::string::npos)
		{
			*why = "a texture's name is one name, with no slash in it";
			return false;
		}

		std::string const whole = "Custom\\" + level + "\\" + name + ".RIM";

		// a zero byte would end the name early for the rule (and for the game), so the rule would pass less than the whole
		if (whole.find('\0') != std::string::npos)
		{
			*why = "the name has a zero byte in it";
			return false;
		}

		char const *const refused = LevelFileName_WhyRefused(whole.c_str());

		if (refused)
		{
			*why = std::string("the game won't open ") + Shown(whole) + ": " + refused;
			return false;
		}

		*entry = whole;
		return true;
	}

	std::string RimSave_SuggestedName(std::string const &pictureFile)
	{
		std::string name = LastPart(pictureFile);
		size_t const dot = name.rfind('.');

		if (dot != std::string::npos)
		{
			name.resize(dot);   // the picture's ending (.png, .bmp)
		}

		// each letter the game's rule refuses made "_" (a slash can't be here: LastPart took what came after the last one)
		for (char &c : name)
		{
			c = LevelFileName_CharacterIsAllowed(c) ? c : '_';
		}

		if (name.size() > kSuggestedMost)
		{
			name.resize(kSuggestedMost);
		}

		// Windows drops dots and spaces at a name's end, and the rule refuses them
		while (!name.empty() && (name.back() == '.' || name.back() == ' '))
		{
			name.pop_back();
		}

		// two dots together go up a folder: the second made "_" (that never makes a dot or a space at the end)
		for (size_t i = 1; i < name.size(); i++)
		{
			if (name[i] == '.' && name[i - 1] == '.')
			{
				name[i] = '_';
			}
		}

		if (name.empty())
		{
			return "texture";
		}

		if (LevelFileName_PartIsDevice(name.c_str(), name.size()))
		{
			name = "tex_" + name;
		}

		return name;
	}

	RimSaveResult RimSave_New(std::string const &dataFolder, std::string const &entry, std::vector<uint8_t> const &bytes, std::string *savedPath, std::string *why)
	{
		std::string notSaid;

		why = why ? why : &notSaid;
		why->clear();

		if (savedPath)
		{
			savedPath->clear();
		}

		if (bytes.empty() || bytes.size() > kRimSaveMostBytes)
		{
			return Refuse(RimSaveResult::BadCall, "the texture's file would be " + std::to_string(bytes.size()) + " bytes, not 1 to " +
				std::to_string(kRimSaveMostBytes), why);
		}

		std::string data = dataFolder;

		while (!data.empty() && (data.back() == '\\' || data.back() == '/'))
		{
			data.pop_back();
		}

		if (data.empty())
		{
			return Refuse(RimSaveResult::BadCall, "no game folder is set to save the texture in", why);
		}

		// THE ENTRY'S SHAPE, held again here: Custom\<level>\<texture>.RIM exactly (three parts, backslashes only, a texture name before its ending), and the game's rule
		size_t const first = entry.find('\\');
		size_t const second = first == std::string::npos ? std::string::npos : entry.find('\\', first + 1);
		bool const shaped = entry.find('\0') == std::string::npos && entry.find('/') == std::string::npos && entry.compare(0, 7, "Custom\\") == 0 &&
			first == 6 && second != std::string::npos && second > first + 1 && entry.find('\\', second + 1) == std::string::npos &&
			entry.size() > second + 1 + 4 && EndsWithCaseBlind(entry, ".rim") && !LevelFileName_WhyRefused(entry.c_str());

		if (!shaped)
		{
			return Refuse(RimSaveResult::BadCall, Shown(entry) + " isn't a name of the shape Custom\\<level>\\<texture>.RIM that the game opens", why);
		}

		std::string const graphics = data + "\\graphics";
		std::string const custom = graphics + "\\Custom";
		std::string const levelFolder = graphics + "\\" + entry.substr(0, second);
		std::string const full = graphics + "\\" + entry;

		// the whole path, as the game and Windows open it (MAX_PATH holds 259 letters and the zero)
		if (full.size() >= MAX_PATH)
		{
			return Refuse(RimSaveResult::BadName, "the file's whole path would be " + std::to_string(full.size()) + " letters, more than the " +
				std::to_string(MAX_PATH - 1) + " Windows opens: give the texture a shorter name", why);
		}

		std::wstring const wide = Wide(full);

		if (wide.empty())
		{
			return Refuse(RimSaveResult::BadCall, "the game folder's name can't be read as a path", why);
		}

		// THE FOLDERS, one at a time: each made when missing, then it and every folder above it down from the game folder held plain before anything is made in it
		for (std::string const *folder : { &graphics, &custom, &levelFolder })
		{
			if (!CreateDirectoryA(folder->c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
			{
				return Refuse(RimSaveResult::NotPlain, "couldn't make the folder " + folder->substr(data.size() + 1) + " (" + Error(GetLastError()) + ")", why);
			}

			std::string which;
			char const *const notPlain = GameFolder_WhyNotPlainBelow(data, *folder + "\\" + LastPart(entry), &which);

			if (notPlain)
			{
				return Refuse(RimSaveResult::NotPlain, which + ": " + notPlain, why);
			}
		}

		// THE FILE: CREATE_NEW, so one there already (or a link of that name) is never written over or through. DELETE, so a half-written one is deleted through
		// this very handle
		HANDLE const file = CreateFileW(wide.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			DWORD const error = GetLastError();

			if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
			{
				return Refuse(RimSaveResult::NameTaken, "graphics\\" + entry + " is there already, and is never written over: give the texture another name", why);
			}

			return Refuse(RimSaveResult::CantWrite, "couldn't make graphics\\" + entry + " (" + Error(error) + ")", why);
		}

		size_t done = 0;
		bool written = true;

		while (written && done < bytes.size())
		{
			DWORD wrote = 0;
			size_t const left = bytes.size() - done;

			written = WriteFile(file, bytes.data() + done, (DWORD)(left < 65536 ? left : 65536), &wrote, nullptr) && wrote > 0;
			done += wrote;
		}

		written = written && FlushFileBuffers(file);

		if (!written)
		{
			DWORD const error = GetLastError();
			FILE_DISPOSITION_INFO gone = {};

			gone.DeleteFile = TRUE;
			SetFileInformationByHandle(file, FileDispositionInfo, &gone, sizeof(gone));
			CloseHandle(file);
			return Refuse(RimSaveResult::CantWrite, "couldn't write graphics\\" + entry + " (" + Error(error) + "); the half-written file is deleted", why);
		}

		if (!CloseHandle(file))
		{
			return Refuse(RimSaveResult::NotReadBack, "graphics\\" + entry + " was written but couldn't be closed (" + Error(GetLastError()) + "): it isn't used", why);
		}

		// READ BACK by its name, as the game will open it
		std::vector<uint8_t> back;
		std::string readWhy;
		VideoImportResult const read = VideoImport_ReadPlainFile(wide, &back, &readWhy, bytes.size(), kWhat, "the texture's file");

		if (read != VideoImportResult::Done || back != bytes)
		{
			return Refuse(RimSaveResult::NotReadBack, "graphics\\" + entry + " was written, but reading it back " +
				(read != VideoImportResult::Done ? "was refused (" + readWhy + ")" : std::string("gave other bytes")) + ": it isn't used, and is left there to look at",
				why);
		}

		if (savedPath)
		{
			*savedPath = full;
		}

		return RimSaveResult::Saved;
	}

	char const *RimSaveResultWords(RimSaveResult result)
	{
		switch (result)
		{
		case RimSaveResult::Saved:
			return "saved";
		case RimSaveResult::BadCall:
			return "not a call it takes";
		case RimSaveResult::BadName:
			return "the path is too long";
		case RimSaveResult::NotPlain:
			return "a folder on the way isn't a plain folder";
		case RimSaveResult::NameTaken:
			return "the name is taken";
		case RimSaveResult::CantWrite:
			return "the disk refused it";
		case RimSaveResult::NotReadBack:
			return "it didn't read back";
		}

		return "unknown";
	}
}
