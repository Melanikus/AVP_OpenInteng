// make_shape_game.cpp: a scenario's own small game folder for the shape lookup order (src/host/shape_search.h): one level, the files the editor needs to open
// it, and a shape's file put where the scenario says. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// USAGE: make_shape_game <game folder> <data folder to copy from> <level> <level the shape is taken from> <shape> <place> [<place> ...]
//   <game folder>   made when missing, inside the run's own shard ({datadir}-shapefind-<case>): never a shared folder, never the owner's
//   <data folder>   read only: its fastfile, shaders, avp_huds, language.txt and default.cfg, and avp_rifs\<level>, are copied into the game folder
//   <level>         the level the scenario opens ("Invasion.RIF")
//   <level the shape is taken from>  a level of the data folder that places the shape ("Statue.RIF"): the shape's file is made from it by the editor's own
//                   extractor (shape_extract.h ShapeExtract_File), so the file is what Option C writes. The extractor makes only a copy the level didn't
//                   rescale, so the level must hold one: Lab14, Statue and Sulaco rescale the Quick-create weapons 1.5, but Lockdown4 holds sadar, pulse,
//                   smart, flame, grenade and minigun at 1 (measured 11-23, after Frost's identity table; co-op_meat_factory's one skeeter is 1.5). The
//                   acceptances use pulam from Statue (the ammo ones there are at 1)
//   <shape>         its name without ".rif" ("pulam")
//   <place>         where the file goes, each one:
//                     shape_rifs        <game>\shape_rifs\<shape>.rif   (where Option C writes)
//                     tools             <game>\Tools\shape_rifs\<shape>.rif   (the old tools' package)
//                     own               <game>\own-shapes\<shape>.rif   (a folder for ShapeFolder=)
//                     junction          <game>\shape_rifs made a junction (mklink /J, no privilege needed) to <game>\elsewhere, which holds <shape>.rif
//                     cut               <game>\shape_rifs\<shape>_cut.rif: a HOSTILE file, the first half of the shape's file only, so its chunks run
//                                       past its end (added 2026-11-23 by Vasquez: the placement picker must show such a file by its name, with why)
// FRESH EVERY RUN, NOTHING DELETED: each file is made when it is missing, kept when it is there with the same bytes, and the run is refused (exit 1) when
// it is there with other bytes; so the same scenario run twice in one shard (a re-run) finds its folder as it left it. It prints what it made.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <windows.h>

#include "huffman.hpp"
#include "make_junction.h"   // a junction made without a shell
#include "shape_extract.h"
#include "shape_import.h"

// the game's memory code (our_mem.cpp, for the decoder) calls these: here they only say what happened
void avp_MessageBox(const char *message, int)
{
	printf("the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("FAILED: the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

namespace fs = std::filesystem;

typedef std::vector<char> Bytes;

static uint64_t const kMostBytes = 64ull * 1024 * 1024;

static int Refuse(std::string const &why)
{
	printf("make_shape_game: REFUSED: %s\n", why.c_str());
	return 1;
}

static bool ReadAll(fs::path const &file, Bytes *bytes)
{
	std::error_code e;

	if (!fs::is_regular_file(fs::symlink_status(file, e)) || e || fs::file_size(file, e) > kMostBytes || e)
	{
		return false;
	}

	std::ifstream in(file, std::ios::binary);

	bytes->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return !in.bad();
}

// The file made with these bytes when it is missing, kept when it holds them already; false when it holds other bytes or can't be made
static bool PutFile(fs::path const &file, Bytes const &bytes, std::string *why)
{
	std::error_code e;

	if (fs::exists(fs::symlink_status(file, e)))
	{
		Bytes there;

		if (!ReadAll(file, &there) || there != bytes)
		{
			*why = file.string() + " is there already with other bytes";
			return false;
		}

		return true;
	}

	HANDLE const out = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (out == INVALID_HANDLE_VALUE)
	{
		*why = file.string() + " can't be made (Windows error " + std::to_string(GetLastError()) + ")";
		return false;
	}

	DWORD wrote = 0;
	bool const written = bytes.empty() || (WriteFile(out, bytes.data(), (DWORD)bytes.size(), &wrote, nullptr) && wrote == bytes.size());

	CloseHandle(out);

	if (!written)
	{
		*why = file.string() + " couldn't be written";
		return false;
	}

	printf("made %s (%zu bytes)\n", file.string().c_str(), bytes.size());
	return true;
}

static bool PutFolder(fs::path const &folder, std::string *why)
{
	std::error_code e;

	fs::create_directories(folder, e);

	if (!fs::is_directory(fs::symlink_status(folder, e)))
	{
		*why = folder.string() + " isn't a folder and can't be made one";
		return false;
	}

	return true;
}

// Every plain file of a folder (and its folders), copied as PutFile does
static bool PutTree(fs::path const &from, fs::path const &to, std::string *why)
{
	std::error_code e;

	if (!PutFolder(to, why))
	{
		return false;
	}

	for (fs::directory_entry const &entry : fs::directory_iterator(from, e))
	{
		fs::file_status const kind = entry.symlink_status(e);

		if (fs::is_directory(kind))
		{
			if (!PutTree(entry.path(), to / entry.path().filename(), why))
			{
				return false;
			}
		}
		else if (fs::is_regular_file(kind))
		{
			Bytes bytes;

			if (!ReadAll(entry.path(), &bytes) || !PutFile(to / entry.path().filename(), bytes, why))
			{
				*why = why->empty() ? entry.path().string() + " can't be read" : *why;
				return false;
			}
		}
	}

	return !e;
}

// A level's bytes as the game reads them: a packed one (REBCRIF1) unpacked by the game's own decoder, with its own checks
static bool Unpacked(Bytes const &raw, Bytes *out)
{
	if (raw.size() >= 8 && memcmp(raw.data(), "REBINFF2", 8) == 0)
	{
		*out = raw;
		return true;
	}

	if (raw.size() < sizeof(HuffmanPackage) || memcmp(raw.data(), COMPRESSED_RIF_IDENTIFIER, 8) != 0)
	{
		return false;
	}

	int size = 0;

	memcpy(&size, raw.data() + offsetof(HuffmanPackage, UncompressedDataSize), sizeof(size));

	if (size < 12 || (uint64_t)size > 4 * kMostBytes)
	{
		return false;
	}

	std::unique_ptr<char, void (*)(void *)> const unpacked(HuffmanDecompressChecked((HuffmanPackage const *)raw.data(), (int)raw.size()), free);

	if (!unpacked)
	{
		return false;
	}

	out->assign(unpacked.get(), unpacked.get() + size);
	return true;
}

static std::string Lower(std::string text)
{
	for (char &c : text)
	{
		c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
	}

	return text;
}

int main(int argc, char **argv)
{
	if (argc < 7)
	{
		return Refuse("usage: make_shape_game <game folder> <data folder> <level> <shape's level> <shape> <place> [<place> ...]");
	}

	fs::path const game = fs::u8path(argv[1]);
	fs::path const data = fs::u8path(argv[2]);
	std::string const level = argv[3];
	std::string const shapeLevel = argv[4];
	std::string const shape = argv[5];
	std::string why;

	// only plain names for the level and the shape: they become parts of paths
	for (std::string const &name : { level, shapeLevel, shape })
	{
		if (name.empty() || name.find_first_of("\\/:") != std::string::npos || name.find("..") != std::string::npos)
		{
			return Refuse("\"" + name + "\" isn't a plain name");
		}
	}

	// THE GAME: what the editor reads to open the level, and the level
	if (!PutFolder(game, &why) || !PutFolder(game / "avp_rifs", &why))
	{
		return Refuse(why);
	}

	for (char const *folder : { "fastfile", "shaders", "avp_huds" })
	{
		std::error_code e;

		if (fs::is_directory(fs::symlink_status(data / folder, e)) && !PutTree(data / folder, game / folder, &why))
		{
			return Refuse(why);
		}
	}

	for (char const *file : { "language.txt", "default.cfg" })
	{
		Bytes bytes;
		std::error_code e;

		if (fs::exists(fs::symlink_status(data / file, e)) && (!ReadAll(data / file, &bytes) || !PutFile(game / file, bytes, &why)))
		{
			return Refuse(why.empty() ? std::string(file) + " can't be read" : why);
		}
	}

	Bytes levelBytes;

	if (!ReadAll(data / "avp_rifs" / level, &levelBytes) || !PutFile(game / "avp_rifs" / level, levelBytes, &why))
	{
		return Refuse(why.empty() ? "the level " + level + " can't be read" : why);
	}

	// THE SHAPE'S FILE, made by the editor's own extractor from a level that places it
	Bytes raw, unpacked;

	if (!ReadAll(data / "avp_rifs" / shapeLevel, &raw) || !Unpacked(raw, &unpacked))
	{
		return Refuse("the level " + shapeLevel + " can't be read or unpacked");
	}

	std::vector<host::ShapeExtractPlaced> placed;
	Bytes file;
	std::string fileName;

	if (!host::ShapeExtract_Placed(unpacked, &placed))
	{
		return Refuse("the level " + shapeLevel + "'s chunks can't be walked");
	}

	// the first copy the extractor makes (one rescaled in the level is refused by it: its points aren't the file's), each refusal said
	std::string refusals;

	for (host::ShapeExtractPlaced const &one : placed)
	{
		if (!file.empty() || Lower(one.fileName) != Lower(shape + ".rif"))
		{
			continue;
		}

		std::string oneWhy;
		host::ShapeExtractResult const made = host::ShapeExtract_File(unpacked, one, &file, &fileName, &oneWhy);

		if (made != host::ShapeExtractResult::Done)
		{
			file.clear();
			refusals += " [result " + std::to_string((int)made) + ", rescale " + std::to_string(one.rescale) + (oneWhy.empty() ? std::string() : ": " + oneWhy) + "]";
		}
	}

	if (file.empty())
	{
		return Refuse(shapeLevel + " has no copy of " + shape + ".rif the extractor makes" + (refusals.empty() ? std::string(" (it places none)") : refusals));
	}

	// THE PLACES
	for (int i = 6; i < argc; i++)
	{
		std::string const place = argv[i];
		fs::path folder;

		if (place == "shape_rifs")
		{
			folder = game / "shape_rifs";
		}
		else if (place == "tools")
		{
			folder = game / "Tools" / "shape_rifs";
		}
		else if (place == "own")
		{
			folder = game / "own-shapes";
		}
		else if (place == "junction")
		{
			fs::path const link = game / "shape_rifs";
			fs::path const target = game / "elsewhere";
			std::error_code e;

			if (!PutFolder(target, &why) || !PutFile(target / (shape + ".rif"), file, &why))
			{
				return Refuse(why);
			}

			// made without a shell (make_junction.h: no command line, so the folders from this tool's own command line reach none)
			if (!fs::exists(fs::symlink_status(link, e)))
			{
				if (!MakeJunction(link.wstring(), target.wstring()))
				{
					return Refuse("the junction " + link.string() + " couldn't be made");
				}

				printf("made %s, a junction to %s\n", link.string().c_str(), target.string().c_str());
			}

			DWORD const kind = GetFileAttributesW(link.c_str());

			if (kind == INVALID_FILE_ATTRIBUTES || (kind & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
			{
				return Refuse(link.string() + " is there already and isn't a junction");
			}

			continue;
		}
		else if (place == "cut")
		{
			// a hostile file: the shape's file cut to its first half, so the sizes its chunks say run past its end
			Bytes const half(file.begin(), file.begin() + (std::ptrdiff_t)(file.size() / 2));

			if (!PutFolder(game / "shape_rifs", &why) || !PutFile(game / "shape_rifs" / (shape + "_cut.rif"), half, &why))
			{
				return Refuse(why);
			}

			continue;
		}
		else
		{
			return Refuse("\"" + place + "\" isn't a place (shape_rifs, tools, own, junction or cut)");
		}

		if (!PutFolder(folder, &why) || !PutFile(folder / (shape + ".rif"), file, &why))
		{
			return Refuse(why);
		}
	}

	printf("make_shape_game: %s ready, %s.rif (%zu bytes, from %s) in %d place(s)\n", game.string().c_str(), shape.c_str(), file.size(), shapeLevel.c_str(), argc - 6);
	return 0;
}
