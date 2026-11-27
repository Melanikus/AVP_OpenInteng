// export_map.cpp: see export_map.h. Written 2026-11-27 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include "export_map.h"

#include <windows.h>
#include <shlobj.h>   // SHGetFolderPathA: the user's Documents folder, for the status line's words

#include <set>
#include <vector>

#include "editor_shared.h"       // statusWords, editedLevel, openLevelName, theGameFolder, theCopiesFolder
#include "export_films.h"        // ExportFilms_Gather: the level's own message films (Vasquez)
#include "level_chunks.h"        // FindEnvironmentChildren, FindChildren: the level's chunks by name
#include "level_file.h"          // OpenLevelFile: Level, BuildLevel
#include "level_file_name.h"     // LevelFileName_WhyRefused: the game's rule for a name from a level
#include "rim_save.h"            // kRimSaveMostBytes: the most a picture file may hold
#include "new_file_save.h"       // NewFile_Save: the one safe way a new file is written
#include "placed_sounds_game.h"  // SoundGame_FastFileNames: the names the game's fastfiles hold (stock)
#include "sha256_hex.h"          // Sha256Hex: the README's sums
#include "track_edit.h"          // TrackLevelSoundDirectory: the level's own sound folder
#include "video_import.h"        // VideoImport_ReadPlainFile: the one reader for a file someone could have touched
#include "wav_check.h"           // kWavMostDataBytes: the most a sound file may hold

#include "bitmap_names.hpp"      // Chunk_With_BMPs: BMPNAMES, the level's pictures
#include "chunk_fragments.hpp"   // Fragment_Type_Sound_Chunk: FRGSOUND, a break sound
#include "chunk_object.hpp"      // Object_Track_Sound_Chunk: TRAKSOUN, a track's sound
#include "chunk_sound.hpp"       // Sound_Object_Chunk: SOUNDOB2, a placed sound; Indexed_Sound_Chunk: INDSOUND
#include "mishchnk.hpp"          // File_Chunk: the level

namespace host
{
	namespace
	{
		char const kWhat[] = "Export map";

		uint64_t const kMostSoundBytes = (uint64_t)model::kWavMostDataBytes + 1024 * 1024;   // a sound, as the sound import reads one
		uint64_t const kMostPictureBytes = kRimSaveMostBytes;   // a picture, as the RIM import saves one (rim_save.h)
		size_t const kMostPathLetters = 199;   // "sound\<folder>\<name>", "graphics\<entry>": the game's 200-byte buffers (object_setup.cpp:130-155)

		std::string Lower(std::string text)
		{
			for (char &c : text)
			{
				c = c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
			}

			return text;
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

		std::vector<std::string> Parts(std::string const &path)
		{
			std::vector<std::string> parts;

			for (size_t from = 0; from <= path.size(); )
			{
				size_t const to = path.find('\\', from);

				parts.push_back(path.substr(from, to == std::string::npos ? std::string::npos : to - from));

				if (to == std::string::npos)
				{
					break;
				}

				from = to + 1;
			}

			return parts;
		}

		// EVERY SOUND NAME THE LEVEL HOLDS, once each (capitals aside), in the level's order, read where the game itself reads each (export_map.h WHICH FILES):
		// REBENVDT > SPECLOBJ > SOUNDOB2 (object_setup.cpp:3850); SPECLOBJ > PARGENER > INDSOUND (avp_chunk.cpp:760) and SPECLOBJ > PLACHIER > INDSOUND
		// (object_setup.cpp:1007); REBENVDT > FRAGTYPE > FRGSOUND and REBSHAPE > SHPFRAGS > FRGSOUND (chunk_load.cpp:184, :1042); RBOBJECT > TRAKSOUN
		// (object_setup.cpp:195). The 63 stock levels hold 4,223 of them, all in these six places (AvP-docs tools/dietrich/ccmain-reads)
		std::vector<std::string> LevelSoundNames(File_Chunk *level)
		{
			std::vector<std::string> names;
			std::set<std::string> seen;
			auto const add = [&](char const *name)
			{
				if (name && *name && seen.insert(Lower(name)).second)
				{
					names.push_back(name);
				}
			};

			std::vector<Chunk *> special, holders, sounds, shapes, fragments, objects;

			FindEnvironmentChildren(level, "SPECLOBJ", &special);

			for (Chunk *s : special)
			{
				FindChildren(s, "SOUNDOB2", &sounds);

				for (Chunk *c : sounds)
				{
					if (Sound_Object_Chunk const *const sound = dynamic_cast<Sound_Object_Chunk const *>(c))
					{
						add(sound->wav_name);
					}
				}

				for (char const *holder : { "PARGENER", "PLACHIER" })
				{
					FindChildren(s, holder, &holders);

					for (Chunk *h : holders)
					{
						FindChildren(h, "INDSOUND", &sounds);

						for (Chunk *c : sounds)
						{
							if (Indexed_Sound_Chunk const *const sound = dynamic_cast<Indexed_Sound_Chunk const *>(c))
							{
								add(sound->wav_name);
							}
						}
					}
				}
			}

			FindEnvironmentChildren(level, "FRAGTYPE", &holders);

			for (Chunk *h : holders)
			{
				FindChildren(h, "FRGSOUND", &sounds);

				for (Chunk *c : sounds)
				{
					if (Fragment_Type_Sound_Chunk const *const sound = dynamic_cast<Fragment_Type_Sound_Chunk const *>(c))
					{
						add(sound->wav_name);
					}
				}
			}

			FindChildren(level, "REBSHAPE", &shapes);

			for (Chunk *shape : shapes)
			{
				FindChildren(shape, "SHPFRAGS", &fragments);

				for (Chunk *f : fragments)
				{
					FindChildren(f, "FRGSOUND", &sounds);

					for (Chunk *c : sounds)
					{
						if (Fragment_Type_Sound_Chunk const *const sound = dynamic_cast<Fragment_Type_Sound_Chunk const *>(c))
						{
							add(sound->wav_name);
						}
					}
				}
			}

			FindChildren(level, "RBOBJECT", &objects);

			for (Chunk *o : objects)
			{
				FindChildren(o, "TRAKSOUN", &sounds);

				for (Chunk *c : sounds)
				{
					if (Object_Track_Sound_Chunk const *const sound = dynamic_cast<Object_Track_Sound_Chunk const *>(c))
					{
						add(sound->wav_name);
					}
				}
			}

			return names;
		}

		// EVERY PICTURE THE LEVEL NAMES: REBENVDT > BMPNAMES (the game opens "graphics\" + each, projload.cpp load_rif_bitmap), once each, in the level's order
		std::vector<std::string> LevelPictureNames(File_Chunk *level)
		{
			std::vector<std::string> names;
			std::set<std::string> seen;
			std::vector<Chunk *> lists;

			FindEnvironmentChildren(level, "BMPNAMES", &lists);

			for (Chunk *c : lists)
			{
				Chunk_With_BMPs *const list = dynamic_cast<Chunk_With_BMPs *>(c);

				if (!list)
				{
					continue;
				}

				for (LIF<BMP_Name> i(&list->bmps); !i.done(); i.next())
				{
					char const *const name = i().filename;

					if (name && *name && seen.insert(Lower(name)).second)
					{
						names.push_back(name);
					}
				}
			}

			return names;
		}

		// True when the game's fastfiles hold <path> (spelled with '\', any capitals): a stock file, which every install has
		bool InTheFastfiles(std::string const &path)
		{
			std::string asked = Lower(path);

			for (char &c : asked)
			{
				c = c == '\\' ? '/' : c;
			}

			size_t const slash = asked.find_last_of('/');

			for (std::string const &held : SoundGame_FastFileNames(slash == std::string::npos ? std::string() : asked.substr(0, slash + 1)))
			{
				if (Lower(held) == asked)
				{
					return true;
				}
			}

			return false;
		}

		enum class Found
		{
			Own,
			Stock,
			NotFound,
			Unreadable,
		};

		// What the level's name for a file is: a loose file in the data folder is its own (read in *own, at most mostBytes: over that, Unreadable with the reader's
		// words saying so); one only the fastfiles hold is stock; else not there. A name the game's rule refuses, or too long for its buffer, is not there (the
		// game can't open it either)
		Found FindFile(std::string const &path, uint64_t mostBytes, char const *noun, OwnFile *own, std::string *why)
		{
			if (path.size() > kMostPathLetters || path.find('/') != std::string::npos || LevelFileName_WhyRefused(path.c_str()))
			{
				return Found::NotFound;
			}

			std::string const loose = std::string(theGameFolder) + "\\" + path;

			if (GetFileAttributesW(Wide(loose).c_str()) != INVALID_FILE_ATTRIBUTES)
			{
				own->path = path;
				own->stockName = InTheFastfiles(path);
				return VideoImport_ReadPlainFile(Wide(loose), &own->bytes, why, mostBytes, kWhat, noun) == VideoImportResult::Done ? Found::Own : Found::Unreadable;
			}

			return InTheFastfiles(path) ? Found::Stock : Found::NotFound;
		}

		std::string Plural(size_t n, char const *one, char const *many)
		{
			return std::to_string((unsigned long long)n) + " " + (n == 1 ? one : many);
		}

	}

	std::string ExportMap_LevelPart(std::string const &levelFile)
	{
		size_t const slash = levelFile.find_last_of("\\/");
		std::string name = slash == std::string::npos ? levelFile : levelFile.substr(slash + 1);

		if (name.size() >= 4 && _stricmp(name.c_str() + name.size() - 4, ".rif") == 0)
		{
			name.resize(name.size() - 4);
		}

		std::string part;

		for (char c : name)
		{
			if (part.size() == 25)
			{
				break;
			}

			bool const plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';

			part += plain ? c : '_';
		}

		return part;
	}

	void ExportMap_Run()
	{
		if (!editedLevel || !editedLevel->Level() || openLevelName.empty())
		{
			statusWords = "Export map: open a level first";
			return;
		}

		std::string copies = theCopiesFolder;

		while (!copies.empty() && (copies.back() == '\\' || copies.back() == '/'))
		{
			copies.pop_back();
		}

		if (copies.empty())
		{
			statusWords = "Export map: Windows gave no usable Documents folder, so there is no folder for exports (the folder for copies)";
			return;
		}

		std::string const level = ExportMap_LevelPart(openLevelName);

		if (level.empty())
		{
			statusWords = "Export map: the level's file name leaves nothing to name its folder by";
			return;
		}

		// THE TREE: never written over. Its folder mustn't be there at all
		std::string const tree = copies + "\\Export\\" + level;

		// WHERE, ON THE STATUS LINE: the status line cuts every full path to its last part (status_words.h NO FULL PATH: it is in screenshots), so the folder is
		// named from the user's Documents folder on, which holds no name of theirs. The README says it the same way: it travels with the map to every player, and a
		// whole path would give them the mapper's Windows name (Vasquez's review MUST, 11-27)
		std::string where = "AVP_OpenInteng\\Export\\" + level;
		char documents[MAX_PATH] = "";

		if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, documents)) && *documents && _strnicmp(copies.c_str(), documents, strlen(documents)) == 0)
		{
			where = "your Documents folder's " + where;
		}

		if (GetFileAttributesW(Wide(tree).c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			statusWords = ("Not exported: " + where + " is there already, and is never written over: move it away (or rename it) first").c_str();
			return;
		}

		// THE LEVEL'S OWN FILES, loose in the data folder; stock ones left out; the rest named
		std::string const folder = TrackLevelSoundDirectory(editedLevel);
		std::vector<OwnFile> sounds, pictures;
		std::vector<std::string> notFound;
		size_t stockSounds = 0, stockPictures = 0;
		std::string why;

		for (std::string const &name : LevelSoundNames(editedLevel->Level()))
		{
			OwnFile own;
			Found const found = folder.empty() ? Found::NotFound : FindFile("sound\\" + folder + "\\" + name, kMostSoundBytes, "the sound", &own, &why);

			if (found == Found::Unreadable)
			{
				statusWords = ("Not exported: sound\\" + folder + "\\" + name + ": " + why).c_str();
				return;
			}

			if (found == Found::Own)
			{
				sounds.push_back(own);
			}
			else if (found == Found::Stock)
			{
				stockSounds++;
			}
			else
			{
				notFound.push_back(folder.empty() ? name + " (the level has no sound folder)" : "sound\\" + folder + "\\" + name);
			}
		}

		for (std::string const &entry : LevelPictureNames(editedLevel->Level()))
		{
			OwnFile own;
			Found const found = FindFile("graphics\\" + entry, kMostPictureBytes, "the picture", &own, &why);

			if (found == Found::Unreadable)
			{
				statusWords = ("Not exported: graphics\\" + entry + ": " + why).c_str();
				return;
			}

			if (found == Found::Own)
			{
				pictures.push_back(own);
			}
			else if (found == Found::Stock)
			{
				stockPictures++;
			}
			else
			{
				notFound.push_back("graphics\\" + entry);
			}
		}

		// THE LEVEL'S OWN FILMS (export_films.h, Vasquez 2026-11-27): the message films its Message strategies play that aren't Rebellion's own bytes; the video
		// screens named, never a film of theirs; a film there that can't be read stops the export, as a sound or a picture does
		ExportFilms films;

		if (!ExportFilms_Gather(editedLevel->Level(), theGameFolder, &films, &why))
		{
			statusWords = ("Not exported: " + why).c_str();
			return;
		}

		// THE LEVEL, as the next save would write it (unpacked, REBINFF2: both games open that as well as a packed one)
		std::vector<char> image;

		if (!editedLevel->BuildLevel(image) || image.empty())
		{
			statusWords = "Not exported: the level's bytes couldn't be built";
			return;
		}

		std::vector<uint8_t> const levelBytes(image.begin(), image.end());
		std::string const rifName = level + ".RIF";

		// THE FILES, each new, read back (new_file_save.h). A refusal leaves the files made so far, and says so
		size_t made = 0;
		auto const save = [&](std::vector<std::string> const &parts, std::vector<uint8_t> const &bytes, std::string const &shown) -> bool
		{
			std::vector<std::string> folders = { "Export", level };

			folders.insert(folders.end(), parts.begin(), parts.end() - 1);

			if (NewFile_Save(copies, folders, parts.back(), bytes, shown, bytes.size() + 1, kWhat, &why) != NewFileSaved::Made)
			{
				statusWords = ("Not exported: " + why + (made ? "; made before it, in " + where + ": " + Plural(made, "file", "files") : std::string())).c_str();
				return false;
			}

			made++;
			return true;
		};

		if (!save({ "avp_rifs", "Custom", rifName }, levelBytes, "avp_rifs\\Custom\\" + rifName))
		{
			return;
		}

		for (std::vector<OwnFile> const *list : { &sounds, &pictures })
		{
			for (OwnFile const &f : *list)
			{
				if (!save(Parts(f.path), f.bytes, f.path))
				{
					return;
				}
			}
		}

		// the films, ONE AT A TIME (each up to 256 MB, in a 32-bit editor): read again just before it's written, and only if it's still the film gathered
		for (ExportFilm const &film : films.own)
		{
			std::vector<uint8_t> bytes;

			if (!ExportFilms_Read(film, &bytes, &why))
			{
				statusWords = ("Not exported: " + why + (made ? "; made before it, in " + where + ": " + Plural(made, "file", "files") : std::string())).c_str();
				return;
			}

			if (!save(Parts(film.path), bytes, film.path))
			{
				return;
			}
		}

		// THE README: where it is, what to paste, every file with its size and sha256
		SYSTEMTIME now = {};

		GetLocalTime(&now);

		char date[32];

		_snprintf_s(date, sizeof(date), _TRUNCATE, "%04u-%02u-%02u %02u:%02u", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute);

		std::string readme = level + ", exported by AVP_OpenInteng's Export map, " + date + "\r\n\r\nThis folder: " + where + "\r\n\r\n" +
			"TO PLAY IT IN STEAM AvP: copy everything in this folder into the Aliens versus Predator Classic folder, beside avp_rifs (paste and play: the level's own "
			"sounds and pictures are loose files where the game looks for them, as the Universal Map Pack ships them; nothing in fastfile is changed).\r\n" +
			"The level is avp_rifs\\Custom\\" + rifName + ": Steam AvP lists the levels in avp_rifs\\Custom among its custom levels (multiplayer and skirmish).\r\n\r\n" +
			"avp_rifs\\Custom\\" + rifName + "  " + std::to_string((unsigned long long)levelBytes.size()) + " bytes  sha256 " + Sha256Hex(levelBytes.data(), levelBytes.size()) +
			"\r\n";

		std::vector<std::string> warnings;

		for (std::vector<OwnFile> const *list : { &sounds, &pictures })
		{
			for (OwnFile const &f : *list)
			{
				readme += f.path + "  " + std::to_string((unsigned long long)f.bytes.size()) + " bytes  sha256 " + Sha256Hex(f.bytes.data(), f.bytes.size()) + "\r\n";

				if (f.stockName)
				{
					warnings.push_back(f.path);
				}
			}
		}

		readme += "\r\nLeft out: " + Plural(stockSounds, "stock sound", "stock sounds") + " and " + Plural(stockPictures, "stock picture", "stock pictures") +
			" (every install has them).\r\n";

		for (std::string const &w : warnings)
		{
			readme += "WARNING: " + w + " has a stock file's name: Steam AvP opens the game's pack before a loose file, so it plays the stock one (AvPx plays this one).\r\n";
		}

		// THE FILMS' LINES (export_films.h): each film written, with the size and SHA-256 it was gathered (and written) with; Rebellion's own left out; each
		// warning and note whole; the video screens; the numbers that play no film
		for (ExportFilm const &film : films.own)
		{
			readme += film.path + "  " + std::to_string((unsigned long long)film.size) + " bytes  sha256 " + film.sha256 + "\r\n";
		}

		if (films.stock)
		{
			readme += "Left out too: " + Plural(films.stock, "film of Rebellion's own", "films of Rebellion's own") + " (every install has them, byte for byte).\r\n";
		}

		for (std::string const &w : films.warnings)
		{
			readme += "WARNING: " + w + ".\r\n";
		}

		for (std::string const &n : films.notes)
		{
			readme += "NOTE: " + n + ".\r\n";
		}

		for (ExportFilmScreen const &s : films.screens)
		{
			readme += "VIDEO SCREEN: " + s.picture + " plays the level's message films; a film of its own, " + s.film +
				", is never exported (while one is there, AvPx keeps the screen still).\r\n";
		}

		if (!films.textOnly.empty())
		{
			readme += "Message numbers that play no film (the console prints their line only):";

			for (int32_t n : films.textOnly)
			{
				readme += " " + std::to_string(n);
			}

			readme += "\r\n";
		}

		if (!notFound.empty())
		{
			readme += "NOT FOUND (the game shows or plays nothing for them):";

			for (std::string const &n : notFound)
			{
				readme += " " + n;
			}

			readme += "\r\n";
		}

		std::string const readmeName = "README-" + level + ".txt";   // pasted into the AvP folder beside other maps' (Vasquez's review SHOULD): each its own

		if (!save({ readmeName }, std::vector<uint8_t>(readme.begin(), readme.end()), readmeName))
		{
			return;
		}

		// the films are said only when there are some, so a level with none reads as before ("1 sound and 0 textures of its own")
		std::string const owned = films.own.empty() ? Plural(sounds.size(), "sound", "sounds") + " and " + Plural(pictures.size(), "texture", "textures") :
			Plural(sounds.size(), "sound", "sounds") + ", " + Plural(pictures.size(), "texture", "textures") + " and " + Plural(films.own.size(), "film", "films");
		std::string said = "Exported " + level + " to " + where + ": the level, " + owned + " of its own; paste the folder's contents into the AvP folder (" + readmeName + ")";

		if (stockSounds + stockPictures + films.stock)
		{
			said += "; " + Plural(stockSounds + stockPictures + films.stock, "stock file", "stock files") + " left out";
		}

		if (!warnings.empty())
		{
			said += "; WARNING: " + Plural(warnings.size(), "file has", "files have") + " a stock name (" + readmeName + ")";
		}

		if (films.replacing)
		{
			said += "; WARNING: " + Plural(films.replacing, "film replaces Rebellion's", "films replace Rebellion's") + " (" + readmeName + ")";
		}

		if (films.unplayable)
		{
			said += "; WARNING: " + Plural(films.unplayable, "film isn't", "films aren't") + " one the game can play, not exported (" + readmeName + ")";
		}

		if (films.screenFilms)
		{
			said += "; WARNING: " + Plural(films.screenFilms, "video screen's film is", "video screens' films are") + " in the game folder, not exported (" + readmeName + ")";
		}

		if (!notFound.empty())
		{
			said += "; NOT FOUND: " + Plural(notFound.size(), "file", "files") + " (" + readmeName + " names them)";
		}

		statusWords = said.c_str();
	}
}
