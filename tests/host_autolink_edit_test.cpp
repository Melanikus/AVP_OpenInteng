// The test for AutoLink's run over a level (host::AutoLink_Run, src/host/autolink_edit.cpp: inteng's FUN_004c8930 over host::AutoLinkPass). Written 2026-11-15 by Melanikus
// (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_autolink_edit_test.exe <a level .rif> <a folder it may write its copy into> [every-nth-module, default 6]. With no arguments it says so and passes. The level is only
// read; the copy is the only file written.
// CHECKS (on a real level, the sources being every n-th module): the runs are done and the game's loader takes the writes; a source that is not a module is skipped; a short visibility
// distance cuts links and keeps fewer than no limit; the modules run hold links the level reads back, all to other modules; a SECOND run keeping the old links adds nothing (Normal
// Link refuses a module already in the tree); the level is saved as a COPY, loaded again and holds exactly the links the run made; the opened file is byte for byte what it was.
// REPORTED (not a pass mark): how many of the stored links the run finds (the oracle of host_autolink_pass_test).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "module_links.h"
#include "autolink_edit.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls: files are read by Windows' own

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

static bool ReadWholeFile(std::string const &path, std::vector<char> *out)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER size;
	bool good = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 31);

	if (good)
	{
		out->assign((size_t)size.QuadPart, 0);
		DWORD got = 0;
		good = ReadFile(file, out->data(), (DWORD)out->size(), &got, 0) && got == out->size();
	}

	CloseHandle(file);
	return good;
}

static std::string CopyPathIn(std::string const &folder, char const *name)
{
	return folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + name;
}

// The level's scale: the ENVSDSCL double inside REBENVDT, 1.0 when there is none
static double LevelScale(OpenLevelFile const &open)
{
	Chunk *const data = open.Level()->lookup_single_child("REBENVDT");
	Chunk_With_Children *const holder = data && !data->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(data) : nullptr;
	Chunk *const scale = holder ? holder->lookup_single_child("ENVSDSCL") : nullptr;
	std::vector<unsigned char> bytes;
	double value = 1.0;

	if (scale && open.PayloadOf(scale, &bytes) && bytes.size() >= 8)
	{
		memcpy(&value, bytes.data(), 8);
	}

	return value > 0.0 ? value : 1.0;
}

static std::map<int32_t, std::set<int32_t> > LinksOf(OpenLevelFile &level, std::set<int32_t> const &modules)
{
	std::map<int32_t, std::set<int32_t> > result;

	for (int32_t const module : modules)
	{
		std::vector<host::LinkItem> items;

		if (host::ReadModuleLinks(level.Level(), module, &items))
		{
			for (host::LinkItem const &item : items)
			{
				result[module].insert(item.objectIndex);
			}
		}
	}

	return result;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_autolink_edit_test: give a level .rif and a folder for the copy; with none there is nothing to run.\n");
		printf("passed host_autolink_edit_test (0 ok)\n");
		return 0;
	}

	std::vector<char> original;

	if (!ReadWholeFile(argv[1], &original))
	{
		printf("  FAILED  the level can be read: %s\n", argv[1]);
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		return 1;
	}

	size_t const step = argc >= 4 ? (size_t)atoi(argv[3]) : 6;
	double const scale = LevelScale(level);
	std::set<int32_t> modules;

	host::ModuleIndexes(level.Level(), &modules);

	std::vector<int32_t> sources;
	size_t counter = 0;

	for (int32_t const module : modules)
	{
		if ((counter++ % (step ? step : 1)) == 0)
		{
			sources.push_back(module);
		}
	}

	std::map<int32_t, std::set<int32_t> > const stored = LinksOf(level, modules);
	host::AutoLinkCounts tiny, full, again;
	std::string why;
	std::vector<int32_t> withBogus = sources;

	withBogus.push_back(-12345);   // not a module: skipped

	// 1. a short visibility distance (1 metre = 100000): links are cut
	host::AutoLinkResult const tinyResult = host::AutoLink_Run(&level, &withBogus, true, scale, 100000, &tiny, &why);

	Check(tinyResult == host::AutoLinkResult::Done, std::string("AutoLink (delete old) with a visibility distance of 1 metre is done") + (tinyResult == host::AutoLinkResult::Done ? "" : ": " + why));
	Check(tiny.sources == sources.size(), "the source that is not a module is skipped: " + std::to_string(tiny.sources) + " modules run of " + std::to_string(sources.size()));
	printf("  --  1 m: %zu views, %zu portals, %zu links added, %zu cut, %zu modules written\n", tiny.views, tiny.portals, tiny.linksAdded, tiny.cut, tiny.written);

	// 2. no limit, delete old: more links than the short run
	DWORD const started = GetTickCount();
	host::AutoLinkResult const fullResult = host::AutoLink_Run(&level, &sources, true, scale, 0, &full, &why);
	DWORD const took = GetTickCount() - started;

	Check(fullResult == host::AutoLinkResult::Done, std::string("AutoLink (delete old) with no limit is done") + (fullResult == host::AutoLinkResult::Done ? "" : ": " + why));
	printf("  --  no limit: %zu views, %zu portals, %zu links added, %zu cut, %zu modules written, %zu skipped, %lu ms\n", full.views, full.portals, full.linksAdded, full.cut, full.written, full.skipped,
		(unsigned long)took);
	Check(full.linksAdded > tiny.linksAdded && tiny.cut > 0 && full.cut == 0, "a short visibility distance cuts links; no limit cuts none and keeps more");

	std::map<int32_t, std::set<int32_t> > const first = LinksOf(level, modules);
	size_t readable = 0, wrongModule = 0;

	for (int32_t const source : sources)
	{
		std::map<int32_t, std::set<int32_t> >::const_iterator f = first.find(source);

		if (f != first.end())
		{
			readable++;

			for (int32_t const link : f->second)
			{
				wrongModule += modules.count(link) && link != source ? 0 : 1;
			}
		}
	}

	Check(readable > 0 && wrongModule == 0, "the modules run hold links the level reads back, all to other modules: " + std::to_string(readable) + " modules with links");

	// 3. a second run keeping the old links adds nothing
	Check(host::AutoLink_Run(&level, &sources, false, scale, 0, &again, &why) == host::AutoLinkResult::Done, "AutoLink with the old links kept is done");
	Check(again.linksAdded == 0, "a second run over the same modules adds no link: " + std::to_string(again.linksAdded));

	// the match with the links the level stored (the oracle)
	size_t storedLinks = 0, foundLinks = 0, extraLinks = 0, newLinks = 0;

	for (int32_t const source : sources)
	{
		std::map<int32_t, std::set<int32_t> >::const_iterator s = stored.find(source), f = first.find(source);

		if (s != stored.end())
		{
			for (int32_t const link : s->second)
			{
				storedLinks++;
				foundLinks += (f != first.end() && f->second.count(link)) ? 1 : 0;
			}
		}

		if (f != first.end())
		{
			for (int32_t const link : f->second)
			{
				newLinks++;
				extraLinks += (s != stored.end() && s->second.count(link)) ? 0 : 1;
			}
		}
	}

	printf("  --  THE MATCH with the stored links: %zu stored in the modules run, the run's links hold %zu of them (%.1f%%); %zu links now, %zu not stored before\n", storedLinks, foundLinks,
		storedLinks ? 100.0 * (double)foundLinks / (double)storedLinks : 0.0, newLinks, extraLinks);

	// 4. the copy
	std::string const copyFolder = argv[2];
	std::string const copyPath = CopyPathIn(copyFolder, "autolink-test-copy.rif");

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile after;

	Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the copy");

	if (after.Level())
	{
		Check(LinksOf(after, modules) == first, "the copy, read again, holds exactly the links the run made");
	}

	std::vector<char> originalAfter;

	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");
	DeleteFileA(copyPath.c_str());

	if (checksFailed)
	{
		printf("FAILED host_autolink_edit_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_autolink_edit_test\n");
	return 0;
}
