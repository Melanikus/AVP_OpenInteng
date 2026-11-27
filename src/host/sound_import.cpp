// sound_import.cpp: see sound_import.h. Written 2026-11-27 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include "sound_import.h"

#include <windows.h>
#include <commdlg.h>

#include "editor_shared.h"       // statusWords, editedLevel, openLevelName, theGameFolder
#include "game_folder.h"         // GameFolder_WhyNotPlainBelow: every folder on the way a plain one
#include "level_file.h"          // OpenLevelFile: BuildLevel, ReplaceLevel
#include "level_file_name.h"     // LevelFileName_WhyRefused: the game's rule for a name from a level
#include "placed_sounds_game.h"  // SoundGame_FastFileNames: the stock names the game's fastfiles hold
#include "sound_dir_add.h"       // a level with no sound folder given one
#include "track_edit.h"          // TrackLevelSoundDirectory: the level's own sound folder
#include "video_drop.h"          // the path rules a dropped file is checked by
#include "video_import.h"        // VideoImport_ReadPlainFile: the one reader for a file someone could have touched
#include "wav_check.h"           // what both games play

namespace host
{
	namespace
	{
		char const kWhat[] = "Load sound";
		size_t const kMostRead = 16;                                            // files read from one drop or one Open dialog at most (as Video mode's drop)
		uint64_t const kMostFileBytes = (uint64_t)model::kWavMostDataBytes + 1024 * 1024;   // the sound's 64 MB, and room for its headings
		size_t const kMostPathLetters = 199;                                    // "sound\<folder>\<name>": the game's 200-byte buffer (object_setup.cpp:130-155)

		std::string LastPart(std::string const &path)
		{
			size_t const slash = path.find_last_of("\\/");

			return slash == std::string::npos ? path : path.substr(slash + 1);
		}

		bool EndsWithCaseBlind(std::string const &text, char const *ending)
		{
			size_t const n = strlen(ending);

			return text.size() >= n && _stricmp(text.c_str() + text.size() - n, ending) == 0;
		}

		std::wstring Wide(std::string const &narrow)
		{
			int const letters = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrow.c_str(), (int)narrow.size(), nullptr, 0);
			std::wstring wide(letters > 0 ? (size_t)letters : 0, L'\0');

			if (letters > 0)
			{
				MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrow.c_str(), (int)narrow.size(), &wide[0], letters);
			}

			return wide;
		}

		std::string Error(DWORD error)
		{
			return "Windows error " + std::to_string((unsigned long long)error);
		}

		// The words a path check gave, without its own "Not taken: " in front (ours start "Not loaded: ")
		std::string Reason(std::string const &why)
		{
			char const kFront[] = "Not taken: ";

			return why.compare(0, sizeof(kFront) - 1, kFront) == 0 ? why.substr(sizeof(kFront) - 1) : why;
		}

		enum class Saved
		{
			Made,          // written, flushed and read back exactly
			AlreadyThere,  // a file of that name holds exactly these bytes: taken as it is
			Refused,       // *why says why: nothing was written over, and no half-written file is left
		};

		// THE FILE, saved as rim_save.h's RimSave_New saves a texture: the folders sound\<folder parts> made one at a time when missing, each held plain before
		// anything is made in it; CREATE_NEW (never over or through another file); a half-written file deleted through the very handle that made it; flushed,
		// closed, then read back by its name as the game will open it, and compared byte for byte. shown: "sound\<folder>\<name>", for the words
		Saved SaveNew(std::string const &data, std::string const &folder, std::string const &name, std::vector<uint8_t> const &bytes, std::string const &shown,
			std::string *why)
		{
			std::vector<std::string> folders;
			std::string at = data + "\\sound";

			folders.push_back(at);

			for (size_t from = 0; from <= folder.size(); )
			{
				size_t const to = folder.find('\\', from);
				std::string const part = folder.substr(from, to == std::string::npos ? std::string::npos : to - from);

				at += "\\" + part;
				folders.push_back(at);

				if (to == std::string::npos)
				{
					break;
				}

				from = to + 1;
			}

			std::string const full = at + "\\" + name;

			if (full.size() >= MAX_PATH)
			{
				*why = shown + "'s whole path would be " + std::to_string((unsigned long long)full.size()) + " letters, more than the " +
					std::to_string((unsigned long long)(MAX_PATH - 1)) + " Windows opens";
				return Saved::Refused;
			}

			for (std::string const &each : folders)
			{
				if (!CreateDirectoryA(each.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
				{
					*why = "couldn't make the folder " + each.substr(data.size() + 1) + " (" + Error(GetLastError()) + ")";
					return Saved::Refused;
				}

				std::string which;
				char const *const notPlain = GameFolder_WhyNotPlainBelow(data, each + "\\" + name, &which);

				if (notPlain)
				{
					*why = which + ": " + notPlain;
					return Saved::Refused;
				}
			}

			std::wstring const wide = Wide(full);

			if (wide.empty())
			{
				*why = "the data folder's name can't be read as a path";
				return Saved::Refused;
			}

			HANDLE const file = CreateFileW(wide.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

			if (file == INVALID_HANDLE_VALUE)
			{
				DWORD const error = GetLastError();

				if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
				{
					// ALREADY THERE: read back as a plain file (a link of that name is refused, never followed); the very same bytes are this sound already
					std::vector<uint8_t> there;
					std::string thereWhy;
					VideoImportResult const read = VideoImport_ReadPlainFile(wide, &there, &thereWhy, kMostFileBytes, kWhat, "the sound");

					if (read == VideoImportResult::Done && there == bytes)
					{
						return Saved::AlreadyThere;
					}

					*why = shown + " is there already, " + (read == VideoImportResult::Done ? std::string("with other bytes,") :
						"and can't be read as a plain file (" + thereWhy + "),") + " and is never written over: give the file you're loading another name (or move "
						"that one away first)";
					return Saved::Refused;
				}

				*why = "couldn't make " + shown + " (" + Error(error) + ")";
				return Saved::Refused;
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
				*why = "couldn't write " + shown + " (" + Error(error) + "); the half-written file is deleted";
				return Saved::Refused;
			}

			if (!CloseHandle(file))
			{
				*why = shown + " was written but couldn't be closed (" + Error(GetLastError()) + ")";
				return Saved::Refused;
			}

			std::vector<uint8_t> back;
			std::string readWhy;
			VideoImportResult const read = VideoImport_ReadPlainFile(wide, &back, &readWhy, kMostFileBytes, kWhat, "the sound");

			if (read != VideoImportResult::Done || back != bytes)
			{
				*why = shown + " was written, but reading it back " + (read != VideoImportResult::Done ? "was refused (" + readWhy + ")" :
					std::string("gave other bytes")) + ": it is left there to look at";
				return Saved::Refused;
			}

			return Saved::Made;
		}

		// THE LEVEL GIVEN ITS OWN SOUND FOLDER: its bytes with a SOUNDDIR (sound_dir_add.h), swapped in by the game's own loader. False with *why when anything
		// refused it: then the level is as it was
		bool GiveTheLevelItsFolder(OpenLevelFile *level, std::string const &folder, std::string *why)
		{
			std::vector<char> image, withFolder;
			std::string addWhy;

			if (!level->BuildLevel(image))
			{
				*why = "the level's bytes couldn't be built to give it a sound folder";
				return false;
			}

			if (model::SoundDirAdd_ToImage(image, folder, &withFolder, &addWhy) != model::SoundDirAddResult::Added)
			{
				*why = "the level couldn't be given the sound folder " + folder + ": " + addWhy;
				return false;
			}

			if (level->ReplaceLevel(withFolder) != LevelOpenResult::Opened)
			{
				*why = "the game's own loader wouldn't take the level with the sound folder " + folder + ", so the level is as it was";
				return false;
			}

			return true;
		}

		bool InStock(std::string const &folder, std::string const &name)
		{
			std::string prefix = "sound/" + folder + "/";

			for (char &c : prefix)
			{
				c = c == '\\' ? '/' : c;
			}

			for (std::string const &stock : SoundGame_FastFileNames(prefix))
			{
				if (_stricmp(LastPart(stock).c_str(), name.c_str()) == 0 && stock.size() == prefix.size() + name.size())
				{
					return true;
				}
			}

			return false;
		}
	}

	bool SoundImport_AllSounds(std::vector<std::wstring> const &paths)
	{
		for (std::wstring const &path : paths)
		{
			if (!EndsWithCaseBlind(VideoDrop_Words(path), ".wav"))
			{
				return false;
			}
		}

		return !paths.empty();
	}

	std::string SoundImport_FolderFor(std::string const &levelFile)
	{
		std::string name = LastPart(levelFile);

		if (EndsWithCaseBlind(name, ".rif"))
		{
			name.resize(name.size() - 4);
		}

		std::string const folder = "Custom\\" + name;

		return name.empty() || name.find_first_of("\\/") != std::string::npos || LevelFileName_WhyRefused(folder.c_str()) ? std::string() : folder;
	}

	void SoundImport_Dropped(std::vector<std::wstring> const &paths, size_t listed)
	{
		if (!editedLevel || openLevelName.empty())
		{
			statusWords = "Load sound: open a level first (the sound goes in the open level's sound folder)";
			return;
		}

		std::string const data = theGameFolder;

		if (data.empty())
		{
			statusWords = "Load sound: no data folder is set to load the sound into";
			return;
		}

		std::string folder = TrackLevelSoundDirectory(editedLevel);
		bool const needsFolder = folder.empty();

		if (needsFolder)
		{
			folder = SoundImport_FolderFor(openLevelName);

			if (folder.empty())
			{
				statusWords = "Load sound: the level has no sound folder, and its name can't make one (Custom\\<level>)";
				return;
			}
		}

		bool folderGiven = !needsFolder;
		std::vector<std::string> loaded;   // each loaded file's words, as one file's drop says them
		std::vector<std::string> loadedAlone;   // the same without the folder, for a drop of several
		std::vector<std::string> refused;  // each refused file's words
		size_t const reading = paths.size() < kMostRead ? paths.size() : kMostRead;

		for (size_t i = 0; i < reading; i++)
		{
			std::wstring path;
			std::string why;
			std::string const name = LastPart(VideoDrop_Words(paths[i]));

			// THE PATH, as a dropped film's is held (video_drop.h)
			if (!VideoDrop_CheckPath({ paths[i] }, 1, "sound", &path, &why))
			{
				refused.push_back("Not loaded: " + Reason(why));
				continue;
			}

			// THE BYTES, read by the one reader for a file someone could have touched, then held to what both games play
			std::vector<uint8_t> bytes;

			if (VideoImport_ReadPlainFile(path, &bytes, &why, kMostFileBytes, kWhat, "the sound") != VideoImportResult::Done)
			{
				refused.push_back("Not loaded: " + name + ": " + why);
				continue;
			}

			model::WavFacts facts;

			if (!model::WavCheck(bytes.data(), bytes.size(), &facts, &why))
			{
				refused.push_back("Not loaded: " + name + ": " + why);
				continue;
			}

			// THE NAME, as the game will build it: "sound\<folder>\<name>", by the game's rule and within its buffer
			std::string const shown = "sound\\" + folder + "\\" + name;

			if (char const *const ruled = LevelFileName_WhyRefused(shown.c_str()))
			{
				refused.push_back("Not loaded: " + name + ": the game's rule for a name refuses " + shown + " (" + ruled + ")");
				continue;
			}

			if (shown.size() > kMostPathLetters)
			{
				refused.push_back("Not loaded: " + name + ": " + shown + " is " + std::to_string((unsigned long long)shown.size()) + " letters; the game opens " +
					std::to_string((unsigned long long)kMostPathLetters) + " at most");
				continue;
			}

			// THE LEVEL'S OWN FOLDER, given before the first file is written into it (a file that then can't be saved leaves only the folder's name in the level)
			if (!folderGiven)
			{
				std::string folderWhy;

				if (!GiveTheLevelItsFolder(editedLevel, folder, &folderWhy))
				{
					statusWords = ("Not loaded: " + folderWhy).c_str();
					return;
				}

				folderGiven = true;
			}

			Saved const saved = SaveNew(data, folder, name, bytes, shown, &why);

			if (saved == Saved::Refused)
			{
				refused.push_back("Not loaded: " + why);
				continue;
			}

			// its words alone (for several files: the folder said once) and whole (for one)
			std::string alone = saved == Saved::AlreadyThere ? name + " (there already, the very same bytes)" : name;

			if (InStock(folder, name))
			{
				alone += "; WARNING: Steam AvP plays the stock " + name + ", not this one (it opens the game's pack before a loose file; AvPx plays this one)";
			}

			if (facts.overSteamLimit)
			{
				alone += "; WARNING: Steam AvP plays nothing for a sound of more than " + std::to_string((unsigned long long)model::kWavSteamMostDataBytes) +
					" bytes (this one holds " + std::to_string((unsigned long long)facts.dataBytes) + "; Rebellion's SOUND_MAXSIZE)";
			}

			std::string const warnings = alone.substr(alone.find(';') == std::string::npos ? alone.size() : alone.find(';'));

			loaded.push_back((saved == Saved::AlreadyThere ? name + " is in sound\\" + folder + " already (the very same bytes)" : "Loaded " + name + " into sound\\" + folder) +
				warnings);
			loadedAlone.push_back(alone);
		}

		std::string said;
		std::string const newFolder = needsFolder && !loaded.empty() ? " (the level's own sound folder now: save the level to keep it)" : "";

		if (loaded.size() + refused.size() == 1)
		{
			said = loaded.empty() ? refused[0] : loaded[0] + newFolder + ": pick it in a sound's Edit (E)";
		}
		else
		{
			// SEVERAL: the folder once, then each file's own words without it, the hint once (Vasquez's review NIT, 11-27)
			said = std::to_string((unsigned long long)loaded.size()) + " of " + std::to_string((unsigned long long)(loaded.size() + refused.size())) +
				" sounds loaded into sound\\" + folder + newFolder + (loaded.empty() ? std::string() : ": pick them in a sound's Edit (E)");

			for (std::string const &each : loadedAlone)
			{
				said += "; " + each;
			}

			for (std::string const &each : refused)
			{
				said += "; " + each;
			}
		}

		if (listed > reading)
		{
			said += "; " + std::to_string((unsigned long long)(listed - reading)) + " more not read (" + std::to_string((unsigned long long)kMostRead) +
				" at a time)";
		}

		statusWords = said.c_str();
	}

	bool SoundImport_AnySound(std::vector<std::wstring> const &paths)
	{
		for (std::wstring const &path : paths)
		{
			if (EndsWithCaseBlind(VideoDrop_Words(path), ".wav"))
			{
				return true;
			}
		}

		return false;
	}

	void SoundImport_DroppedMixed()
	{
		statusWords = "Not taken: sounds and other files dropped together; drop the WAVs on Object Placement (sounds), and a picture on its own (a texture)";
	}

	void SoundImport_DroppedElsewhere(std::vector<std::wstring> const &paths)
	{
		std::string const first = paths.empty() ? std::string("the file") : LastPart(VideoDrop_Words(paths[0]));

		statusWords = ("Not taken: " + first + " is a sound; a sound is loaded in Object Placement (Mode > Object Placement), into the level's sound folder").c_str();
	}

	void SoundImport_Browse(void *owner)
	{
		wchar_t const filter[] = L"Sounds (*.wav)\0*.wav\0\0";
		std::vector<wchar_t> picked(65536, L'\0');
		OPENFILENAMEW open = {};

		open.lStructSize = sizeof(open);
		open.hwndOwner = (HWND)owner;
		open.lpstrFilter = filter;
		open.lpstrFile = picked.data();
		open.nMaxFile = (DWORD)picked.size();
		open.lpstrTitle = L"Load sound";
		// as Video mode's (video_mode.cpp VideoMode_Browse): a shortcut comes back as itself, never its target, and is refused by the drop's own checks
		open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT | OFN_NONETWORKBUTTON | OFN_HIDEREADONLY | OFN_EXPLORER |
			OFN_NODEREFERENCELINKS | OFN_ALLOWMULTISELECT;

		if (!GetOpenFileNameW(&open))
		{
			// cancelled, or more names than the box's room (Vasquez's review NIT, 11-27)
			statusWords = CommDlgExtendedError() == FNERR_BUFFERTOOSMALL ? "Load sound: too many files picked at once; pick fewer" : "Load sound: nothing picked";
			return;
		}

		// ONE FILE: its whole path. SEVERAL: the folder, then each name, each ending in a zero, and two zeros at the end (a drive's root comes back with its own
		// "\", so none is added after it)
		std::vector<std::wstring> paths;
		std::wstring const first(picked.data());
		std::wstring const between = !first.empty() && first.back() == L'\\' ? L"" : L"\\";
		size_t at = first.size() + 1;

		if (at >= picked.size() || picked[at] == L'\0')
		{
			paths.push_back(first);
		}
		else
		{
			while (at < picked.size() && picked[at] != L'\0')
			{
				std::wstring const name(picked.data() + at);

				paths.push_back(first + between + name);
				at += name.size() + 1;
			}
		}

		// the picked files are hostile all the same (a name can be typed in that box): the drop's own checks
		SoundImport_Dropped(paths, paths.size());
	}
}
