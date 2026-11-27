// export_films.cpp: see export_films.h. Written 2026-11-27 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include "export_films.h"

#include <windows.h>
#include <string.h>

#include <set>

#include "game_folder.h"         // GameFolder_WhyNotPlainBelow: every folder on the way a plain one
#include "level_chunks.h"        // FindEnvironmentChildren: the level's BMPNAMES
#include "level_file_name.h"     // LevelFileName_WhyRefused: the game's rule for a name from a level
#include "message_numbers.h"     // the stock films (MessageNumbers_IsStockFilm), the levels that play a number
#include "sha256_hex.h"          // Sha256Hex
#include "smk_header.h"          // MessageFmvName; SmkHeader_Read, SmkHeader_DiffersFromAvp: the format note
#include "strategy_lookup.h"     // FindObjectStrategy and the lists of everything that holds a strategy
#include "video_import.h"        // VideoImport_ReadPlainFile: the one reader for a file someone could have touched

#include "bitmap_names.hpp"      // Chunk_With_BMPs: BMPNAMES, the level's pictures
#include "chunk_object.hpp"      // Object_Chunk
#include "mishchnk.hpp"          // File_Chunk::list_objects

namespace host
{
	namespace
	{
		char const kWhat[] = "Export map";
		size_t const kKindAt = 8;           // in a strategy chunk's payload: its index and spare word, then the strategy's kind (strategy_chunk.cpp:41-70)
		size_t const kMessageNumberAt = 20; // a Message strategy's number (FMV-T-4: kind, Type, ExtraData, then message_string)
		int32_t const kMessageKind = 73;    // StratMessage (strategy_chunk.hpp:35)

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

		// Whether something has that name (a file, a folder or a link: the reader then decides what it is)
		bool There(std::string const &path)
		{
			std::wstring const wide = Wide(path);

			return !wide.empty() && GetFileAttributesW(wide.c_str()) != INVALID_FILE_ATTRIBUTES;
		}

		int32_t IntAt(std::vector<uint8_t> const &bytes, size_t at)
		{
			int32_t value = 0;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		char const *SmkReadWords(model::SmkRead read)
		{
			switch (read)
			{
			case model::SmkRead::TooShort:
				return "it is shorter than a Smacker film's header";
			case model::SmkRead::NotSmacker:
				return "it doesn't start as a Smacker film does (SMK2 or SMK4)";
			case model::SmkRead::Impossible:
				return "its header holds a size or a frame count no film has";
			default:
				return "its header can't be read";
			}
		}
	}

	std::vector<int32_t> ExportFilms_MessageNumbers(File_Chunk *level)
	{
		std::set<int32_t> found;

		if (!level)
		{
			return std::vector<int32_t>();
		}

		// every ID that holds a strategy, as strategy_lookup.h's ListStrategyRecords takes them: the level's objects, then its virtual objects, enemies
		// and generators, and placed hierarchies; each one's strategy as the game reads it (FindObjectStrategy: the one the game takes)
		std::vector<std::vector<uint8_t>> ids;
		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			if (each())
			{
				uint8_t const *const id = (uint8_t const *)&each()->object_data.ID;

				ids.push_back(std::vector<uint8_t>(id, id + 8));
			}
		}

		std::vector<VirtualObjectRef> virtuals;
		std::vector<GeneratorRef> generators;
		std::vector<PlacedHierarchyRef> hierarchies;

		ListVirtualObjects(level, &virtuals);
		ListGenerators(level, &generators);
		ListPlacedHierarchies(level, &hierarchies);

		for (VirtualObjectRef const &v : virtuals)
		{
			ids.push_back(std::vector<uint8_t>(v.id, v.id + 8));
		}

		for (GeneratorRef const &g : generators)
		{
			ids.push_back(std::vector<uint8_t>(g.id, g.id + 8));
		}

		for (PlacedHierarchyRef const &h : hierarchies)
		{
			ids.push_back(std::vector<uint8_t>(h.id, h.id + 8));
		}

		for (std::vector<uint8_t> const &id : ids)
		{
			ObjectStrategy strategy;

			if (FindObjectStrategy(level, id.data(), &strategy) == StrategyFound::Found && strategy.payload.size() >= kMessageNumberAt + 4 &&
				IntAt(strategy.payload, kKindAt) == kMessageKind)
			{
				int32_t const number = IntAt(strategy.payload, kMessageNumberAt);

				// 0 is "none"; the game's loader refuses more than 450 (strategy_chunk.cpp CheckMessageNumber)
				if (number >= 1 && number <= model::kMostMessageNumber)
				{
					found.insert(number);
				}
			}
		}

		return std::vector<int32_t>(found.begin(), found.end());
	}

	std::string ExportFilms_ScreenFilm(std::string const &picture)
	{
		size_t const offset1 = picture.find("FMVs");

		if (offset1 == std::string::npos)
		{
			return std::string();
		}

		// as ScanImagesForFMVs: the FIRST '.' of the whole name, and the count offset2 - offset1 in size_t, which wraps (to "the rest of the name") when
		// there is no '.' or it comes before "FMVs"
		size_t const offset2 = picture.find('.');

		return picture.substr(offset1, offset2 - offset1) + ".smk";
	}

	std::vector<ExportFilmScreen> ExportFilms_Screens(File_Chunk *level)
	{
		std::vector<ExportFilmScreen> screens;
		std::set<std::string> seen;
		std::vector<Chunk *> lists;

		if (!level)
		{
			return screens;
		}

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
				std::string const film = name ? ExportFilms_ScreenFilm(name) : std::string();

				if (!film.empty() && seen.insert(name).second)
				{
					screens.push_back({ name, film });
				}
			}
		}

		return screens;
	}

	bool ExportFilms_Gather(File_Chunk *level, std::string const &dataFolder, ExportFilms *out, std::string *why)
	{
		std::string scratch;
		std::string &said = why ? *why : scratch;

		said.clear();

		if (!out)
		{
			said = "no place for the films";
			return false;
		}

		*out = ExportFilms();

		std::string data = dataFolder;

		while (!data.empty() && (data.back() == '\\' || data.back() == '/'))
		{
			data.pop_back();
		}

		if (!level || data.empty())
		{
			said = "no level, or no data folder";
			return false;
		}

		out->numbers = ExportFilms_MessageNumbers(level);
		out->screens = ExportFilms_Screens(level);

		// THE FMVs FOLDER: none at all is no film at all; one that is there must be a plain folder (a link there could hand over another folder's films)
		std::string const fmvs = data + "\\FMVs";
		bool const haveFolder = There(fmvs);

		if (haveFolder)
		{
			std::string which;

			if (char const *const notPlain = GameFolder_WhyNotPlainBelow(data, fmvs + "\\message1.smk", &which))
			{
				said = "the data folder's " + which + ": " + notPlain;
				return false;
			}
		}

		// THE MESSAGE FILMS, by number: the name made from the number alone
		for (int32_t const number : out->numbers)
		{
			std::string const name = model::MessageFmvName(number);
			std::string const shown = "FMVs\\" + name;
			std::string const path = fmvs + "\\" + name;

			if (!haveFolder || !There(path))
			{
				out->textOnly.push_back(number);
				continue;
			}

			// read once here for its size, hash and header, then let go: ExportMap_Run reads it again, one film at a time, as it writes it
			std::vector<uint8_t> bytes;
			std::string readWhy;

			if (VideoImport_ReadPlainFile(Wide(path), &bytes, &readWhy, kExportFilmMostBytes, kWhat, "the film") != VideoImportResult::Done)
			{
				said = shown + " can't be read as a plain file: " + readWhy;
				return false;
			}

			ExportFilm own;

			own.path = shown;
			own.whole = path;
			own.size = bytes.size();
			own.sha256 = Sha256Hex(bytes.data(), bytes.size());

			if (model::MessageNumbers_IsStockFilm(number, own.size, own.sha256))
			{
				out->stock++;   // Rebellion's own bytes: every install has them
				continue;
			}

			// NOT A FILM the game's decoder can take: left out, never sent to the players' decoder (export_films.h)
			model::SmkHeader header;
			model::SmkRead const read = model::SmkHeader_Read(bytes.data(), bytes.size(), &header);

			if (read != model::SmkRead::Read)
			{
				out->unplayable++;
				out->warnings.push_back(shown + " is not exported: it isn't a film the game can play (" + SmkReadWords(read) +
					"), and the tree goes to every player's film decoder");
				continue;
			}

			if (model::MessageNumbers_HasStockFilm(number))
			{
				std::string const levels = model::MessageNumbers_StockLevels(number);

				out->replacing++;
				out->warnings.push_back(shown + " isn't Rebellion's film " + std::to_string(number) + ": pasting it replaces that stock film for every level that plays " +
					std::to_string(number) + (levels.empty() ? std::string(" (no stock level plays it)") : " (" + levels + ")") +
					"; Steam's \"Verify integrity of game files\" puts Rebellion's back");
			}

			std::string const differs = model::SmkHeader_DiffersFromAvp(header);

			if (!differs.empty())
			{
				out->notes.push_back(shown + " isn't AvP's usual film format: " + differs + " (it may still play)");
			}

			out->own.push_back(own);
		}

		// THE VIDEO SCREENS: never a film of their own in the tree; one found in the data folder freezes that screen in AvPx (export_films.h)
		// (the films' own loop above holds one film's bytes at a time)
		for (ExportFilmScreen const &screen : out->screens)
		{
			if (LevelFileName_WhyRefused(screen.film.c_str()))
			{
				continue;   // a name the game's rule refuses is one the game can't open either: nothing there to freeze the screen
			}

			if (There(data + "\\" + screen.film))
			{
				out->screenFilms++;
				out->warnings.push_back(screen.film + " is in the game folder: while it is there AvPx keeps the video screen " + screen.picture +
					" still (it plays the level's message films only when that film is missing); it is not exported");
			}
		}

		return true;
	}

	bool ExportFilms_Read(ExportFilm const &film, std::vector<uint8_t> *bytes, std::string *why)
	{
		std::string scratch;
		std::string &said = why ? *why : scratch;

		said.clear();

		if (!bytes || film.whole.empty())
		{
			said = "no film to read";
			return false;
		}

		bytes->clear();

		std::string readWhy;

		if (VideoImport_ReadPlainFile(Wide(film.whole), bytes, &readWhy, kExportFilmMostBytes, kWhat, "the film") != VideoImportResult::Done)
		{
			said = film.path + " can't be read as a plain file: " + readWhy;
			bytes->clear();
			return false;
		}

		if (bytes->size() != film.size || Sha256Hex(bytes->data(), bytes->size()) != film.sha256)
		{
			said = film.path + " changed while the map was being exported (it isn't the film that was checked); export again";
			bytes->clear();
			return false;
		}

		return true;
	}
}
