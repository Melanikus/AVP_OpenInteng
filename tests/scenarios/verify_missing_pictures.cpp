// verify_missing_pictures: the scenario harness's tool for levels whose PICTURES ARE MISSING (the owner's release check, 2026-11-17: "if someone gives you a map it is very likely
// it won't have a texture"; acceptance-missing-pictures.txt, acceptance-missing-picture-list.txt). Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE:
//   verify_missing_pictures.exe make <a level .rif> <the out file> [drop]
//       opens the level with the editor's own loader, takes its bytes as a save writes them (OpenLevelFile::BuildLevel, uncompressed), and either changes the FIRST picture name of
//       BMPNAMES to one no data folder has (its first three letters become "ZZQ": the same length, so no size changes), or with "drop" takes the whole BMPNAMES chunk out (the
//       sizes of the chunks holding it made smaller), and writes the bytes to the out file.
//   verify_missing_pictures.exe same <the made level> <a saved copy of it>
//       the copy keeps the level's picture references as they were: BMPNAMES there or not as in the made level, with the same entries (number, name and flags, in order), and
//       every polygon of every shape with the same colour word (its picture and UV list numbers).
// Gives back 0 when it did what it was asked, 1 otherwise, saying why.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "bitmap_names.hpp"
#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"

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
	printf("the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static int Fail(char const *why)
{
	printf("FAILED  %s\n", why);
	return 1;
}

// The BMPNAMES chunk's payload of a level's bytes, and where the chunk is (npos when none)
static size_t BmpNamesAt(std::vector<char> const &image)
{
	std::string const whole(image.begin(), image.end());

	return whole.find("BMPNAMES");
}

static int Make(char const *in, char const *out, bool drop)
{
	OpenLevelFile open;

	if (open.Open(in) != LevelOpenResult::Opened || !open.Level())
	{
		return Fail("the level does not open");
	}

	std::vector<char> image;

	if (!open.BuildLevel(image) || image.size() < 12)
	{
		return Fail("the level's bytes could not be built");
	}

	size_t const at = BmpNamesAt(image);

	if (at == std::string::npos || at + 12 > image.size())
	{
		return Fail("the level has no BMPNAMES");
	}

	int32_t size = 0;

	memcpy(&size, image.data() + at + 8, 4);

	if (size < 12 || at + (size_t)size > image.size())
	{
		return Fail("BMPNAMES's size is wrong");
	}

	if (drop)
	{
		// the file's heading (REBINFF2 at 0) and the environment chunk (REBENVDT) hold it: each made smaller by its size
		size_t fixed = 0;

		for (size_t h = 0; h + 12 <= at; h++)
		{
			if (!(h == 0 || memcmp(image.data() + h, "REBENVDT", 8) == 0))
			{
				continue;
			}

			int32_t hs = 0;

			memcpy(&hs, image.data() + h + 8, 4);

			if (hs > size && h + (size_t)hs >= at + (size_t)size && h + (size_t)hs <= image.size())
			{
				int32_t const smaller = hs - size;

				memcpy(image.data() + h + 8, &smaller, 4);
				fixed++;
			}
		}

		if (fixed != 2)
		{
			return Fail("the chunks holding BMPNAMES were not the file's heading and REBENVDT");
		}

		image.erase(image.begin() + (std::ptrdiff_t)at, image.begin() + (std::ptrdiff_t)(at + (size_t)size));
		printf("BMPNAMES (%d bytes) taken out\n", size);
	}
	else
	{
		// the first name: the first run of four letters (or path signs) after the chunk's heading
		size_t name = std::string::npos;

		for (size_t p = at + 12; p + 4 < at + (size_t)size; p++)
		{
			bool run = true;

			for (size_t k = 0; k < 4 && run; k++)
			{
				char const c = image[p + k];

				run = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '\\' || c == '/';
			}

			if (run)
			{
				name = p;
				break;
			}
		}

		if (name == std::string::npos)
		{
			return Fail("no picture name found in BMPNAMES");
		}

		printf("the first picture name was: %.60s\n", image.data() + name);
		image[name] = 'Z';
		image[name + 1] = 'Z';
		image[name + 2] = 'Q';
		printf("it is now: %.60s\n", image.data() + name);
	}

	FILE *file = nullptr;

	if (fopen_s(&file, out, "wb") != 0 || !file)
	{
		return Fail("the out file could not be written");
	}

	bool const written = fwrite(image.data(), 1, image.size(), file) == image.size();

	fclose(file);

	if (!written)
	{
		return Fail("the out file could not be written");
	}

	printf("written: %s (%zu bytes)\n", out, image.size());
	return 0;
}

// One entry of a level's list of pictures (BMPNAMES): what a polygon's picture number finds
struct PictureName
{
	int index;
	std::string name;
	int flags;
};

// Every polygon's colour word of every shape, in order, and BMPNAMES's entries (none when it isn't there)
static bool ReferencesOf(char const *path, std::vector<unsigned int> *colours, std::vector<PictureName> *names, bool *hasNames)
{
	OpenLevelFile open;

	if (open.Open(path) != LevelOpenResult::Opened || !open.Level())
	{
		return false;
	}

	List<Shape_Chunk *> shapes;

	open.Level()->list_shapes(&shapes);

	for (LIF<Shape_Chunk *> each(&shapes); !each.done(); each.next())
	{
		ChunkShape const &data = each()->shape_data;

		for (int p = 0; p < data.num_polys; p++)
		{
			colours->push_back(data.poly_list[p].colour);
		}
	}

	names->clear();
	*hasNames = false;

	Chunk_With_Children *const environment = (Chunk_With_Children *)open.Level()->lookup_single_child("REBENVDT");
	Chunk *const list = environment ? environment->lookup_single_child("BMPNAMES") : nullptr;

	if (list)
	{
		*hasNames = true;

		for (LIF<BMP_Name> each(&((Global_BMP_Name_Chunk *)list)->bmps); !each.done(); each.next())
		{
			names->push_back({each().index, each().filename ? each().filename : "", (int)each().flags});
		}
	}

	return true;
}

static int Same(char const *made, char const *saved)
{
	std::vector<unsigned int> coloursMade, coloursSaved;
	std::vector<PictureName> namesMade, namesSaved;
	bool hasMade = false, hasSaved = false;

	if (!ReferencesOf(made, &coloursMade, &namesMade, &hasMade))
	{
		return Fail("the made level does not open");
	}

	if (!ReferencesOf(saved, &coloursSaved, &namesSaved, &hasSaved))
	{
		return Fail("the saved copy does not open");
	}

	printf("the made level: %zu polygons, BMPNAMES %s (%zu names); the copy: %zu polygons, BMPNAMES %s (%zu names)\n", coloursMade.size(), hasMade ? "there" : "none",
		namesMade.size(), coloursSaved.size(), hasSaved ? "there" : "none", namesSaved.size());

	if (hasMade != hasSaved || namesMade.size() != namesSaved.size())
	{
		return Fail("the copy's list of pictures is not the made level's");
	}

	for (size_t n = 0; n < namesMade.size(); n++)
	{
		PictureName const &a = namesMade[n];
		PictureName const &b = namesSaved[n];

		if (a.index != b.index || a.name != b.name || a.flags != b.flags)
		{
			printf("entry %zu: the made level has %d %.80s (flags 0x%x), the copy %d %.80s (flags 0x%x)\n", n, a.index, a.name.c_str(), (unsigned)a.flags, b.index, b.name.c_str(),
				(unsigned)b.flags);
			return Fail("the copy's list of pictures is not the made level's");
		}
	}

	if (coloursMade != coloursSaved)
	{
		return Fail("a polygon's picture reference changed in the copy");
	}

	printf("the copy keeps every picture reference as it was\n");
	return 0;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc >= 4 && strcmp(argv[1], "make") == 0)
	{
		return Make(argv[2], argv[3], argc >= 5 && strcmp(argv[4], "drop") == 0);
	}

	if (argc >= 4 && strcmp(argv[1], "same") == 0)
	{
		return Same(argv[2], argv[3]);
	}

	printf("verify_missing_pictures make <a level .rif> <the out file> [drop] | same <the made level> <a saved copy>\n");
	return 1;
}
