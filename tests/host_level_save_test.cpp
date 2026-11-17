// The test of Save over the opened file with a backup (src/host/level_save.cpp; THE OWNER'S RULING, Melanikus 2026-11-16: "Overwrite, with backup"). Written 2026-11-16 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_level_save_test.exe <Invasion .rif> <a folder it may write into>. The stock level is only read: every file written is in the folder (a COPY of the level is the "opened" file).
// THE CASES: (1) gARAGDOR's links cut to 6, saved over the copy: the copy loads with 6, its .bak is the stock file byte for byte, no temporary file is left; (2) cut to 5 and saved again: the
// .bak now holds the 6 (the one before), the copy 5; (3) a READ-ONLY file is refused and left as it was; (4) a file that is not there yet is made, with no .bak; (5) not a .rif: refused.

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
#include "level_chunks.h"
#include "level_save.h"
#include "module_links.h"
#include "object_edit.h"

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

static std::vector<uint8_t> Bytes(std::string const &path)
{
	std::vector<uint8_t> out;
	FILE *in = fopen(path.c_str(), "rb");

	if (in)
	{
		uint8_t block[65536];
		size_t got;

		while ((got = fread(block, 1, sizeof(block), in)) > 0)
		{
			out.insert(out.end(), block, block + got);
		}

		fclose(in);
	}

	return out;
}

// gARAGDOR's link count in a level file, -1 when it does not load
static int LinksIn(std::string const &path, int32_t garage)
{
	OpenLevelFile level;
	std::vector<host::LinkItem> items;

	if (level.Open(path.c_str()) != LevelOpenResult::Opened || !level.Level() || !host::ReadModuleLinks(level.Level(), garage, &items))
	{
		return -1;
	}

	return (int)items.size();
}

// gARAGDOR's links cut to their first `keep` items (no branch reaches past them), through the editor's own SetModuleLinks
static bool CutLinks(OpenLevelFile *open, int32_t garage, size_t keep)
{
	std::vector<host::LinkItem> items;
	uint8_t id[8];

	if (!host::ReadModuleLinks(open->Level(), garage, &items) || items.size() < keep || !host::ModuleIdOfIndex(open->Level(), garage, id))
	{
		return false;
	}

	std::vector<host::ModuleLinkItem> kept;

	for (size_t i = 0; i < keep; i++)
	{
		host::ModuleLinkItem const one = { items[i].objectIndex, items[i].branchNo > (int32_t)keep ? (int32_t)keep : items[i].branchNo, items[i].flags, items[i].spare };

		kept.push_back(one);
	}

	return host::SetModuleLinks(open, id, kept.data(), kept.size()) == host::ObjectEditResult::Done;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("  FAILED  usage: host_level_save_test <Invasion .rif> <folder>\n");
		return 1;
	}

	std::string const stock = argv[1], folder = argv[2];
	std::string const target = folder + "\\Saved-over.rif", backup = target + ".bak", temporary = target + ".saving";

	CreateDirectoryA(folder.c_str(), nullptr);
	SetFileAttributesA(target.c_str(), FILE_ATTRIBUTE_NORMAL);
	DeleteFileA(target.c_str());
	DeleteFileA(backup.c_str());
	Check(CopyFileA(stock.c_str(), target.c_str(), FALSE) != 0, "the 'opened' file is a copy of the stock level: " + target);

	// a temporary file left by an earlier, broken save is not in the way
	{
		FILE *junk = fopen(temporary.c_str(), "wb");

		if (junk)
		{
			fputs("junk", junk);
			fclose(junk);
		}
	}

	OpenLevelFile open;

	Check(open.Open(target.c_str()) == LevelOpenResult::Opened && open.Level(), "it opens");

	int32_t garage = 54;   // gARAGDOR's object number on Invasion (host_autolink_file_test)

	Check(LinksIn(target, garage) == 7, "gARAGDOR has its 7 stock links in it");

	// ---- (1) a change, saved over the file
	std::string backupPath, why;

	Check(CutLinks(&open, garage, 6), "gARAGDOR's links cut to 6 in the editor's bytes");
	host::LevelSaveOver const first = host::LevelSave_Over(&open, target.c_str(), &backupPath, &why);

	Check(first == host::LevelSaveOver::Saved && backupPath == backup, "saved over the file, the one before kept as .bak: " + backupPath);
	Check(LinksIn(target, garage) == 6, "the file loads with the change: " + std::to_string(LinksIn(target, garage)) + " links");
	Check(Bytes(backup) == Bytes(stock), "the .bak is the file as it was, byte for byte");
	Check(GetFileAttributesA(temporary.c_str()) == INVALID_FILE_ATTRIBUTES, "no temporary file is left");

	// ---- (2) another change: the .bak holds the one before
	bool const cut5 = CutLinks(&open, garage, 5);
	host::LevelSaveOver const second = host::LevelSave_Over(&open, target.c_str(), &backupPath, &why);

	Check(cut5 && second == host::LevelSaveOver::Saved, "saved again");
	Check(LinksIn(target, garage) == 5 && LinksIn(backup, garage) == 6, "the file has 5, its .bak the 6 saved before: " + std::to_string(LinksIn(target, garage)) + " / " +
		std::to_string(LinksIn(backup, garage)));

	// ---- (3) read-only: refused, untouched
	std::vector<uint8_t> const before = Bytes(target);

	SetFileAttributesA(target.c_str(), FILE_ATTRIBUTE_READONLY);
	bool const cut4 = CutLinks(&open, garage, 4);
	host::LevelSaveOver const third = host::LevelSave_Over(&open, target.c_str(), &backupPath, &why);

	Check(cut4 && third == host::LevelSaveOver::ReadOnly, "a read-only file is refused: " + why);
	SetFileAttributesA(target.c_str(), FILE_ATTRIBUTE_NORMAL);
	Check(Bytes(target) == before && GetFileAttributesA(temporary.c_str()) == INVALID_FILE_ATTRIBUTES, "and is as it was, with no temporary file");

	// ---- (4) a file not there yet is made, with no .bak
	std::string const fresh = folder + "\\Saved-new.rif";

	DeleteFileA(fresh.c_str());
	DeleteFileA((fresh + ".bak").c_str());
	host::LevelSaveOver const fourth = host::LevelSave_Over(&open, fresh.c_str(), &backupPath, &why);

	Check(fourth == host::LevelSaveOver::Saved && backupPath.empty() && LinksIn(fresh, garage) == 4 && GetFileAttributesA((fresh + ".bak").c_str()) == INVALID_FILE_ATTRIBUTES,
		"a file not there yet is made, with no .bak");

	// ---- (5) not a .rif
	Check(host::LevelSave_Over(&open, (folder + "\\level.txt").c_str(), &backupPath, &why) == host::LevelSaveOver::BadTarget, "a path that is not a .rif is refused");

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
