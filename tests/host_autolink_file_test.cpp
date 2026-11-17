// The test of Load from file's records turned into AutoLink's views (src/host/autolink_mlf.cpp MlfFindModule / MlfView; inteng's FUN_004c8be0, read on dump-full-2 64700).
// Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_autolink_file_test.exe <Invasion .rif> <a folder it may write into>. The level is only read; the folder gets a copy of it and a module file beside it.
// THE CASES: (1) MlfView: the camera's place is the record's view x 100 (0x4c8dc3-0x4c8dd8) and its matrix the record's TRANSPOSED (FUN_004b3710, 0x4c8dd4); (2) on Invasion, a module file
// of ONE record (gARAGDOR, at its place, with the first view AutoLink itself draws for it, written back the way FUN_004c8be0 reads it) beside a copy of the level, for
// acceptance-autolink-file; read back through MlfReadFile / MlfParse, MlfFindModule finds gARAGDOR and MlfView gives the view again; (3) a record whose module MOVED is found and marked so.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "autolink_mlf.h"
#include "autolink_records.h"
#include "autolink_views.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// what the game's chunk code calls
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

static void PutInt(std::vector<uint8_t> *bytes, int32_t value)
{
	for (int i = 0; i < 4; i++)
	{
		bytes->push_back((uint8_t)(((uint32_t)value >> (8 * i)) & 0xff));
	}
}

int main(int argc, char **argv)
{
	// ---- (1) the view of a record
	{
		host::MlfRecord record;
		host::AutoLinkView view;

		record.name = "x";

		for (int i = 0; i < 3; i++)
		{
			record.place[i] = 0;
			record.view[i] = (i + 1) * 7;
		}

		for (int i = 0; i < 9; i++)
		{
			record.turn[i] = i + 1;   // rows 1 2 3 / 4 5 6 / 7 8 9
		}

		bool const made = host::MlfView(record, &view);

		Check(made && view.place[0] == 700 && view.place[1] == 1400 && view.place[2] == 2100, "the camera's place is the view x 100");
		Check(view.turn[0] == 1 && view.turn[1] == 4 && view.turn[2] == 7 && view.turn[3] == 2 && view.turn[5] == 8 && view.turn[6] == 3 && view.turn[7] == 6 && view.turn[8] == 9,
			"the matrix is the record's transposed (FUN_004b3710 swaps 1/3, 2/6, 5/7)");

		record.view[0] = 30000000;   // x 100 does not fit 32 bits
		Check(!host::MlfView(record, &view), "a view whose place x 100 does not fit is refused");
	}

	if (argc < 3)
	{
		printf("  FAILED  usage: host_autolink_file_test <Invasion .rif> <folder>\n");
		return 1;
	}

	// ---- (2) a module file for gARAGDOR beside a copy of Invasion
	OpenLevelFile level;

	Check(level.Open(argv[1]) == LevelOpenResult::Opened && level.Level(), std::string("the level opens: ") + argv[1]);

	if (checksFailed)
	{
		printf("%d checks failed\n", checksFailed);
		return 1;
	}

	host::AutoLinkRecords records;
	int32_t garage = -1, record = -1;

	host::AutoLink_Records(&level, 1.0, &records);

	for (size_t n = 0; n < records.scene.list.size() && garage < 0; n++)
	{
		int32_t const object = records.scene.list[n].number;

		if (records.scene.names[object] == "gARAGDOR")
		{
			garage = object;
			record = (int32_t)n;
		}
	}

	host::AutoLinkView first;
	bool haveView = false;

	for (size_t b = 0; garage >= 0 && b < records.scene.list.size() && !haveView; b++)
	{
		host::Portal portal;
		std::vector<host::AutoLinkView> views;

		if ((int32_t)b == record || !host::AutoLink_ModulesMeet(records.records[(size_t)record], records.records[b], &portal))
		{
			continue;
		}

		host::AutoLink_ViewsFor(portal, &views);

		if (!views.empty())
		{
			first = views[0];
			haveView = true;
		}
	}

	Check(garage >= 0 && haveView, "the scene: gARAGDOR (object " + std::to_string(garage) + ") and the first view AutoLink draws for it");

	// the record as FUN_004c8be0 reads it: the name, NUL-padded to 4; the place (the file's numbers); the nine matrix numbers (the camera's, transposed back); the view's place / 100
	std::vector<int32_t> const &file = records.scene.fileLocations[garage];
	std::vector<uint8_t> bytes;
	std::string const name = "gARAGDOR";

	for (char const letter : name)
	{
		bytes.push_back((uint8_t)letter);
	}

	do
	{
		bytes.push_back(0);
	} while (bytes.size() % 4 != 0);

	for (int i = 0; i < 3; i++)
	{
		PutInt(&bytes, file.size() >= 3 ? file[(size_t)i] : 0);
	}

	for (int r = 0; r < 3; r++)
	{
		for (int c = 0; c < 3; c++)
		{
			PutInt(&bytes, first.turn[c * 3 + r]);
		}
	}

	for (int i = 0; i < 3; i++)
	{
		PutInt(&bytes, first.place[i] / 100);
	}

	std::string const folder = argv[2];
	std::string const copy = folder + "\\Invasion-mlf.rif", mlf = folder + "\\Invasion-mlf.mlf";

	CreateDirectoryA(folder.c_str(), nullptr);
	DeleteFileA(copy.c_str());
	Check(level.SaveAs(copy.c_str()) == LevelSaveResult::Saved, "a copy of the level: " + copy);

	FILE *out = fopen(mlf.c_str(), "wb");
	bool const written = out && fwrite(bytes.data(), 1, bytes.size(), out) == bytes.size();

	if (out)
	{
		fclose(out);
	}

	Check(written && host::MlfPathFor(copy.c_str()) == mlf, "the module file beside it, where Load from file looks: " + mlf);

	std::vector<uint8_t> read;
	std::vector<host::MlfRecord> parsed;
	std::string why;

	Check(host::MlfReadFile(mlf.c_str(), &read, &why) && host::MlfParse(read.data(), read.size(), &parsed, &why) && parsed.size() == 1, "it reads back as one record: " + why);

	int32_t found = -1;
	bool moved = true;
	host::AutoLinkView again;

	Check(!parsed.empty() && host::MlfFindModule(records.scene, parsed[0], &found, &moved) && found == garage && !moved, "the record names gARAGDOR, where it is");
	Check(!parsed.empty() && host::MlfView(parsed[0], &again) && memcmp(again.turn, first.turn, sizeof(first.turn)) == 0 && again.place[0] == first.place[0] / 100 * 100,
		"and gives the view again (the matrix exactly, the place to the hundred)");

	// ---- (3) the module moved 5 units since the file was made: found, marked moved
	if (!parsed.empty())
	{
		host::MlfRecord shifted = parsed[0];

		shifted.place[0] += 5;
		Check(host::MlfFindModule(records.scene, shifted, &found, &moved) && moved && found < 0, "a record whose module moved is found and marked moved (\"%s has moved since module file created\")");
	}

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
