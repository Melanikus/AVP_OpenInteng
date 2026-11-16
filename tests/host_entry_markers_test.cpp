// The test for Waypoint mode's entry point markers and the write-back of a moved one (src/host/entry_markers.*, inteng's FUN_00588260 / FUN_0058a6b0), on a REAL level (the first argument), saved as a COPY
// (the second is the folder) and loaded again with the game's own loader. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_entry_markers_test.exe <a level .rif> <a folder for the copy>. With no arguments it says so and passes. The level is only read.
// CHECKS: the markers of a module with adjacent modules lead outside its AI module and have neither bit 2 nor 4; a marker moved to its own place changes nothing; moved far away, every entry of the pair is given
// the same point, held INSIDE a box of the other AI module (checked from the shapes here), with the hand-set bit; no module outside the two AI modules changes; the copy goes through the loader; the opened file
// is byte for byte what it was.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "object_edit.h"
#include "entry_markers.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

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

static std::vector<host::ModuleSnapshot> Snapshots(OpenLevelFile *open)
{
	std::vector<host::ModuleSnapshot> all;

	if (host::ReadModuleSnapshots(open, &all) != host::ObjectEditResult::Done)
	{
		all.clear();
	}

	return all;
}

static host::ModuleSnapshot const *Find(std::vector<host::ModuleSnapshot> const &all, int32_t index)
{
	for (host::ModuleSnapshot const &one : all)
	{
		if (one.index == index)
		{
			return &one;
		}
	}

	return nullptr;
}

// A module's box, from its shape and place
static bool BoxOf(OpenLevelFile *open, int32_t index, int64_t lo[3], int64_t hi[3])
{
	::List<Object_Chunk *> all;

	open->Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (object && (int32_t)object->object_data.index_num == index && object->get_assoc_shape())
		{
			ChunkShape const &data = object->get_assoc_shape()->shape_data;
			int64_t const at[3] = { object->object_data.location.x, object->object_data.location.y, object->object_data.location.z };

			lo[0] = at[0] + data.min.x; hi[0] = at[0] + data.max.x;
			lo[1] = at[1] + data.min.y; hi[1] = at[1] + data.max.y;
			lo[2] = at[2] + data.min.z; hi[2] = at[2] + data.max.z;
			return true;
		}
	}

	return false;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_entry_markers_test <a level .rif> <a folder for the copy>: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	std::vector<char> original;

	Check(ReadWholeFile(argv[1], &original), "the level is read (it is only read, never written)");

	OpenLevelFile open;

	if (original.empty() || open.Open(argv[1]) != LevelOpenResult::Opened || !open.Level())
	{
		Check(false, "the game's loader takes the level");
		return 1;
	}

	std::vector<host::ModuleSnapshot> const start = Snapshots(&open);
	std::vector<host::EntryMarker> markers;
	int32_t module = -1;

	// a module whose AI module has markers: the first with adjacent modules
	for (host::ModuleSnapshot const &one : start)
	{
		if (one.module && one.hasAdjacency && one.adjacency.size() >= 2 && host::EntryMarkers_Of(&open, one.index, &markers) && !markers.empty())
		{
			module = one.index;
			break;
		}
	}

	Check(module >= 0 && !markers.empty(), "a module of the level has entry point markers: module " + std::to_string(module) + ", " + std::to_string(markers.size()));

	if (module < 0)
	{
		return 1;
	}

	bool allOut = true;

	for (host::EntryMarker const &marker : markers)
	{
		host::ModuleSnapshot const *const a = Find(start, marker.main);
		host::ModuleSnapshot const *const b = Find(start, marker.adjacent);

		allOut = allOut && a && b && (marker.flags & 6) == 0 && marker.adjacent != marker.main;
	}

	Check(allOut, "every marker has two modules, and neither bit 2 nor bit 4 of its flags");

	host::EntryMarker const first = markers[0];
	size_t written = 99;

	Check(host::EntryMarkers_Move(&open, first, first.point, &written) == host::EntryMoveResult::NothingMoved && written == 0, "a marker put back where it is changes nothing");

	int32_t farOff[3] = { first.point[0] + 5000000, first.point[1] - 3000000, first.point[2] + 4000000 };

	Check(host::EntryMarkers_Move(&open, first, farOff, &written) == host::EntryMoveResult::Done && written >= 1, "a marker moved far away is written: " + std::to_string(written) + " entries");

	std::vector<host::ModuleSnapshot> const after = Snapshots(&open);
	host::ModuleSnapshot const *const mainAfter = Find(after, first.main);
	bool pointed = false;
	bool flagged = true;
	int32_t common[3] = { 0, 0, 0 };

	for (host::ModuleAdjacencyEntry const &entry : mainAfter->adjacency)
	{
		if (entry.objectIndex == first.adjacent)
		{
			pointed = true;
			common[0] = entry.point[0]; common[1] = entry.point[1]; common[2] = entry.point[2];
			flagged = flagged && (entry.flags & 1) != 0;
		}
	}

	Check(pointed && flagged, "the entry of the pair has the hand-set bit");

	// the point is inside a box of the other AI module (the pair's own module or one of its slaves)
	int64_t lo[3], hi[3];
	bool anyBox = false;

	for (host::ModuleSnapshot const &one : after)
	{
		if (one.module && (one.index == first.adjacent || one.masterIndex == first.adjacent || one.masterIndex == Find(after, first.adjacent)->masterIndex) && BoxOf(&open, one.index, lo, hi))
		{
			bool in = true;

			for (int axis = 0; axis < 3; axis++)
			{
				in = in && common[axis] >= lo[axis] && common[axis] <= hi[axis];
			}

			anyBox = anyBox || in;
		}
	}

	Check(anyBox, "the point was held inside a box of the other AI module: " + std::to_string(common[0]) + " " + std::to_string(common[1]) + " " + std::to_string(common[2]));
	Check(common[0] != farOff[0], "and it is not where the marker was dragged");

	// no module outside the two AI modules changed
	bool others = after.size() == start.size();

	for (size_t i = 0; others && i < start.size(); i++)
	{
		host::ModuleSnapshot const *const theOther = Find(start, first.adjacent);
		bool const involved = start[i].index == first.main || start[i].index == first.adjacent || start[i].masterIndex == first.adjacent || start[i].masterIndex == first.main || start[i].masterIndex == (theOther ? theOther->masterIndex : -2) ||
			start[i].index == (theOther ? theOther->masterIndex : -2);
		bool same = start[i].adjacency.size() == after[i].adjacency.size();

		for (size_t e = 0; same && e < start[i].adjacency.size(); e++)
		{
			same = start[i].adjacency[e].objectIndex == after[i].adjacency[e].objectIndex && start[i].adjacency[e].point[0] == after[i].adjacency[e].point[0] && start[i].adjacency[e].flags == after[i].adjacency[e].flags;
		}

		others = others && (same || involved);
	}

	Check(others, "no module outside the two AI modules had its adjacency changed");

	std::string const copyPath = std::string(argv[2]) + "\\entry-markers-copy.rif";

	CreateDirectoryA(argv[2], NULL);
	DeleteFileA(copyPath.c_str());
	Check(open.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile copy;

	Check(copy.Open(copyPath.c_str()) == LevelOpenResult::Opened && copy.Level(), "the game's loader takes the copy");

	std::vector<host::ModuleSnapshot> const reread = Snapshots(&copy);
	host::ModuleSnapshot const *const mainCopy = Find(reread, first.main);
	bool same = mainCopy != nullptr;

	for (size_t e = 0; same && e < mainCopy->adjacency.size() && e < mainAfter->adjacency.size(); e++)
	{
		same = mainCopy->adjacency[e].point[0] == mainAfter->adjacency[e].point[0] && mainCopy->adjacency[e].flags == mainAfter->adjacency[e].flags;
	}

	Check(same, "the copy holds the entries as the editor does");

	std::vector<char> originalAfter;

	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");
	DeleteFileA(copyPath.c_str());

	std::vector<host::EntryMarker> none;

	Check(!host::EntryMarkers_Of(nullptr, 0, &none) && none.empty(), "no level: no markers");
	Check(host::EntryMarkers_Of(&open, 1 << 30, &none) && none.empty(), "a module number that is none: no markers");
	Check(host::EntryMarkers_Move(nullptr, first, farOff, &written) == host::EntryMoveResult::NoLevel, "no level: refused");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
