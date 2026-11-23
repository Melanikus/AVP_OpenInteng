// The test for a picture made a texture of the open level, end to end (src/host/texture_import.h: the RIM import, Stage 1). Written 2026-11-23 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// It runs on a stock level, opened read-only (it is never saved), with a scratch folder as the game's folder:   <the level> <a scratch folder>
//   (1) a BMP of 64 x 32, red on the left and blue on the right, imported as Wall01 at 128, Fit: Imported, the name Custom\<level>\Wall01.RIM, the file in
//       the scratch folder's graphics\Custom\<level>\, the index the list's largest + 1, and the level swapped (Generation one more)
//   (2) the game's own library reads the new list: one name more, the last one the new name, its index, flags 0x0100010C, priority 6, version the list's old
//       version << 8, its size word 128 << 16 | 128; and the list's version one more
//   (3) the game's own RimLoader reads the file: 128 x 128, the picture in rows 32 to 95 (red on the left, blue on the right), the bars black
//   (4) the same name again: the list names it already, so nothing is saved, the level and the file as they were
//   (5) a name whose file is there already: refused, the level as it was, that file's bytes unchanged
//   (6) refused before anything is made: a text file named .png, a device's name, no level open
//   (7) the words for the status line name the texture and its index
// RED FIRST: on a stand-in that refuses every import (as no level open), 12 of the 16 checks fail: all but the setup's and "no level open".

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "bitmap_names.hpp"
#include "chunk.hpp"
#include "given_paths.h"   // the level and the scratch folder from the command line, made whole and checked
#include "level_file.h"
#include "mishchnk.hpp"
#include "rim_save.h"
#include "texture_import.h"
#include "FastFile.h"
#include "RimLoader.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls (as host_acoustic_mode_test's): files are read by Windows' own, the log goes nowhere
HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

// ---- What the game's FileStream asks of the rest of the game: these pictures are plain files, never in a fastfile
FastFileHandle *FF_Find(const std::string &)
{
	return 0;
}

const std::string &FF_GetFastFileName(int32_t)
{
	static std::string const none;
	return none;
}

namespace Util
{
	void FtoBslash(std::string &text)
	{
		for (char &letter : text)
		{
			letter = letter == '/' ? '\\' : letter;
		}
	}
}

static std::vector<uint8_t> ReadAll(std::string const &path)
{
	std::vector<uint8_t> bytes;
	FILE *file = fopen(path.c_str(), "rb");

	if (file)
	{
		int c;

		while ((c = fgetc(file)) != EOF)
		{
			bytes.push_back((uint8_t)c);
		}

		fclose(file);
	}

	return bytes;
}

static bool WriteAll(std::string const &path, std::vector<uint8_t> const &bytes)
{
	FILE *file = fopen(path.c_str(), "wb");

	if (!file)
	{
		return false;
	}

	bool const written = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();

	return fclose(file) == 0 && written;
}

static void Put(std::vector<uint8_t> *bytes, uint32_t value, int count)
{
	for (int i = 0; i < count; i++)
	{
		bytes->push_back((uint8_t)(value >> (8 * i)));
	}
}

// A 24-bit BMP, bottom row first as BMPs are: the left half red, the right half blue
static std::vector<uint8_t> HalvesBmp(uint32_t width, uint32_t height)
{
	uint32_t const row = (width * 3 + 3) & ~3u;
	std::vector<uint8_t> bmp;

	bmp.push_back('B');
	bmp.push_back('M');
	Put(&bmp, 54 + row * height, 4);
	Put(&bmp, 0, 4);
	Put(&bmp, 54, 4);
	Put(&bmp, 40, 4);
	Put(&bmp, width, 4);
	Put(&bmp, height, 4);
	Put(&bmp, 1, 2);
	Put(&bmp, 24, 2);
	Put(&bmp, 0, 4);
	Put(&bmp, row * height, 4);
	Put(&bmp, 2835, 4);
	Put(&bmp, 2835, 4);
	Put(&bmp, 0, 4);
	Put(&bmp, 0, 4);

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			bool const left = x < width / 2;

			bmp.push_back(left ? 0 : 255);   // blue
			bmp.push_back(0);                // green
			bmp.push_back(left ? 255 : 0);   // red
		}

		bmp.insert(bmp.end(), row - width * 3, 0);
	}

	return bmp;
}

// The level's BMPNAMES as the game's library reads it, and its version
struct ListNow
{
	bool found = false;
	size_t count = 0;
	BMP_Name last;
	int32_t version = 0;
};

static ListNow ReadList(OpenLevelFile &open)
{
	ListNow now;
	File_Chunk *const level = open.Level();
	Chunk_With_Children *const environment = level ? dynamic_cast<Chunk_With_Children *>(level->lookup_single_child("REBENVDT")) : nullptr;
	Chunk_With_BMPs *const names = environment ? dynamic_cast<Chunk_With_BMPs *>(environment->lookup_single_child("BMPNAMES")) : nullptr;

	if (names)
	{
		now.found = true;
		now.count = (size_t)names->bmps.size();

		for (LIF<BMP_Name> each(&names->bmps); !each.done(); each.next())
		{
			now.last = each();
		}

		now.version = names->get_version_num();
	}

	return now;
}

int main(int argc, char **argv)
{
	std::string levelPath, levelName, scratch;

	// both made whole and checked before any path is made from them (given_paths.h: the Snyk gate on 41e310df, cpp/PT and cpp/IntegerOverflow on paths and
	// a name cut from argv): the level a plain file ending in .rif, its name the letters before that; the scratch folder a plain one, made when missing
	if (argc < 3 || !GivenFile(argv[1], ".rif", &levelPath, &levelName) || !GivenScratchFolder(argv[2], &scratch))
	{
		printf("  FAILED  give a stock level (a .rif file that is there) and a scratch folder (a plain folder, made when missing): nothing was tested\n");
		return 1;
	}

	std::string const data = scratch + "\\game";
	bool const made = CreateDirectoryA(data.c_str(), nullptr) != FALSE;

	Check(made, "the scratch game folder is made: " + data);

	std::string const picture = scratch + "\\halves.bmp";

	Check(WriteAll(picture, HalvesBmp(64, 32)), "a 64 x 32 BMP is written, red on the left and blue on the right");

	OpenLevelFile open;

	Check(open.Open(levelPath.c_str()) == LevelOpenResult::Opened, "the stock level opens: " + levelPath);

	ListNow const before = ReadList(open);
	int32_t mostIndex = 0;

	if (before.found)
	{
		Chunk_With_Children *const environment = dynamic_cast<Chunk_With_Children *>(open.Level()->lookup_single_child("REBENVDT"));
		Chunk_With_BMPs *const names = dynamic_cast<Chunk_With_BMPs *>(environment->lookup_single_child("BMPNAMES"));

		for (LIF<BMP_Name> each(&names->bmps); !each.done(); each.next())
		{
			mostIndex = each().index > mostIndex ? each().index : mostIndex;
		}
	}

	Check(before.found, "its list of pictures (BMPNAMES) is read: " + std::to_string(before.count) + " names, the largest index " + std::to_string(mostIndex) +
		", version " + std::to_string(before.version));

	// ---- (1) imported
	puts("Imported:");
	TextureImportChoice choice;
	TextureImportDone done;
	std::string why;

	choice.picture = std::wstring(picture.begin(), picture.end());
	choice.name = "Wall01";
	choice.side = 128;
	choice.mode = model::ImageFitMode::Fit;

	unsigned const generation = open.Generation();
	TextureImportResult const imported = TextureImport_Make(&open, levelPath, data, choice, &done, &why);
	std::string const entry = "Custom\\" + levelName + "\\Wall01.RIM";
	std::string const file = data + "\\graphics\\" + entry;

	Check(imported == TextureImportResult::Imported && done.entry == entry && done.savedPath == file,
		std::string("(1) imported as ") + done.entry + " in " + done.savedPath + " (" + TextureImportResultWords(imported) + " " + why + ")");
	Check(done.index == mostIndex + 1 && open.Generation() == generation + 1, "(1) index " + std::to_string(done.index) + " (the largest + 1), and the level swapped");

	// ---- (2) the game's library reads the new list
	ListNow const after = ReadList(open);

	Check(after.found && after.count == before.count + 1 && after.last.filename && entry == after.last.filename && after.last.index == done.index,
		"(2) the library's list: one name more, the last " + std::string(after.last.filename ? after.last.filename : "(none)") + " at index " + std::to_string(after.last.index));
	// priority and the size word through the class's own readers (the fields are its own): an IFF name's width and height are its size word's halves
	Check((int32_t)after.last.flags == 0x0100010C && after.last.get_pc_priority() == 6 && after.last.version_num == (before.version << 8) &&
		after.last.GetWidth() == 128 && after.last.GetHeight() == 128 && after.version == before.version + 1,
		"(2) flags 0x0100010C, priority 6, version " + std::to_string(after.last.version_num) + " (the old version << 8), size word 128 x 128, the list's version " +
		std::to_string(after.version));

	// ---- (3) the game's RimLoader reads the file
	RimLoader_NewLevel();
	RimLoader loader;
	uint32_t width = 0, height = 0;
	std::vector<uint8_t> pixels(128 * 128 * 4, 0xcd);
	bool const opened = loader.Open(file);

	if (opened)
	{
		loader.GetDimensions(width, height);
	}

	bool const decoded = opened && width == 128 && height == 128 && loader.Decode(pixels.data(), 128 * 4);
	auto pixel = [&](uint32_t x, uint32_t y, uint8_t b, uint8_t g, uint8_t r)
	{
		size_t const at = ((size_t)y * 128 + x) * 4;

		return pixels[at] == b && pixels[at + 1] == g && pixels[at + 2] == r && pixels[at + 3] == 255;
	};

	Check(decoded, "(3) RimLoader opens and decodes it, 128 x 128 (" + std::to_string(width) + " x " + std::to_string(height) + ")");
	Check(decoded && pixel(10, 40, 0, 0, 255) && pixel(10, 90, 0, 0, 255) && pixel(117, 40, 255, 0, 0) && pixel(117, 90, 255, 0, 0) && pixel(64, 5, 0, 0, 0) &&
		pixel(64, 122, 0, 0, 0), "(3) the picture in rows 32 to 95, red on the left and blue on the right; the bars black");

	// ---- (4) the same name again
	puts("Refused:");
	std::vector<uint8_t> const savedBytes = ReadAll(file);
	TextureImportResult const twice = TextureImport_Make(&open, levelPath, data, choice, &done, &why);

	Check(twice == TextureImportResult::ListRefused && open.Generation() == generation + 1 && ReadAll(file) == savedBytes && ReadList(open).count == after.count,
		std::string("(4) the same name again: the list names it, nothing saved, the level and the file as they were (") + TextureImportResultWords(twice) + ": " + why + ")");

	// ---- (5) a name whose file is there already
	std::string const planted = data + "\\graphics\\Custom\\" + levelName + "\\Wall02.RIM";
	std::vector<uint8_t> const plantedBytes = { 'n', 'o', 't', ' ', 'm', 'i', 'n', 'e' };
	bool const plantedMade = WriteAll(planted, plantedBytes);

	choice.name = "Wall02";
	TextureImportResult const taken = TextureImport_Make(&open, levelPath, data, choice, &done, &why);

	Check(plantedMade && taken == TextureImportResult::CantSave && open.Generation() == generation + 1 && ReadAll(planted) == plantedBytes &&
		ReadList(open).count == after.count, std::string("(5) a file there already: refused, its bytes unchanged, the level as it was (") + TextureImportResultWords(taken) + ": " +
		why + ")");

	// ---- (6) refused before anything is made
	std::string const notPicture = scratch + "\\words.png";

	WriteAll(notPicture, { 'h', 'e', 'l', 'l', 'o' });
	choice.picture = std::wstring(notPicture.begin(), notPicture.end());
	choice.name = "Wall03";

	TextureImportResult const text = TextureImport_Make(&open, levelPath, data, choice, &done, &why);

	Check(text == TextureImportResult::BadPicture && GetFileAttributesA((data + "\\graphics\\Custom\\" + levelName + "\\Wall03.RIM").c_str()) == INVALID_FILE_ATTRIBUTES,
		std::string("(6) a text file named .png: refused, no file made (") + TextureImportResultWords(text) + ": " + why + ")");

	choice.picture = std::wstring(picture.begin(), picture.end());
	choice.name = "con";

	TextureImportResult const device = TextureImport_Make(&open, levelPath, data, choice, &done, &why);

	Check(device == TextureImportResult::BadName, std::string("(6) a device's name: refused (") + TextureImportResultWords(device) + ": " + why + ")");

	OpenLevelFile none;
	TextureImportResult const noLevel = TextureImport_Make(&none, levelPath, data, choice, &done, &why);

	Check(noLevel == TextureImportResult::NoLevel && open.Generation() == generation + 1, std::string("(6) no level open: refused (") + TextureImportResultWords(noLevel) + ")");

	// ---- (7) the words
	TextureImportDone words;

	words.entry = entry;
	words.index = 18;
	words.pictureWidth = 64;
	words.pictureHeight = 32;
	words.side = 128;
	words.coloursIn = 3;
	words.coloursOut = 3;

	std::string const said = TextureImport_DoneWords(words);

	Check(said.find(entry) != std::string::npos && said.find("texture 18") != std::string::npos && said.find("Save the level") != std::string::npos, "(7) the words: " + said);

	// ---- our own files away (the level was never saved)
	DeleteFileA(file.c_str());
	DeleteFileA(planted.c_str());
	RemoveDirectoryA((data + "\\graphics\\Custom\\" + levelName).c_str());
	RemoveDirectoryA((data + "\\graphics\\Custom").c_str());
	RemoveDirectoryA((data + "\\graphics").c_str());
	RemoveDirectoryA(data.c_str());
	DeleteFileA(picture.c_str());
	DeleteFileA(notPicture.c_str());

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
