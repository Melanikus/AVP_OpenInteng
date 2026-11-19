// host_movable_track_end_test.cpp: movable.h's TRACK SECTION END adapter (src/host/movable_track_end.cpp, with track_editor.h TrackEdit_SectionEnd / TrackEdit_SetSectionEnd) on
// the stock levels' tracks: in Track Editing, a section end's place and turn read as the end marker shows them, a new place and turn put as Finish End Pos Edit puts them (the
// next section starts there, turned so), the level's bytes untouched until Stop Track Edit writes the track, a saved copy taken by the game's own loader with it, and no
// editor, another object or a section past the track refused. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// No place function is given to the track editor here (as in host_track_editor_test), so a marker is its pivot.
//
// HOW IT RUNS: tests/host_movable_track_end_test.args builds it; host_movable_track_end_test <level file> [...]. A non-zero result means a check failed.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"

#include "level_file.h"
#include "movable.h"
#include "movable_track_end.h"
#include "track_edit.h"
#include "track_editor.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

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

static bool SameTurn(float const a[4], float const b[4])
{
	// a turn and its negative are the same turn
	double same = 0.0, opposite = 0.0;

	for (int i = 0; i < 4; i++)
	{
		same += fabs((double)a[i] - (double)b[i]);
		opposite += fabs((double)a[i] + (double)b[i]);
	}

	return same < 1e-4 || opposite < 1e-4;
}

static bool SamePlace(int32_t const a[3], int32_t const b[3])
{
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static std::string Words(int32_t const p[3])
{
	return std::to_string(p[0]) + " " + std::to_string(p[1]) + " " + std::to_string(p[2]);
}

static host::MovableRef RefOf(uint8_t const *id, int32_t section)
{
	host::MovableRef ref;

	ref.kind = host::MovableKind::TrackSectionEnd;
	memcpy(ref.id, id, sizeof(ref.id));
	ref.number = section;
	return ref;
}

static bool SameSections(host::TrackData const &a, host::TrackData const &b)
{
	return a.sections.size() == b.sections.size() && (a.sections.empty() || memcmp(a.sections.data(), b.sections.data(), a.sections.size() * sizeof(a.sections[0])) == 0);
}

// One level: the first object whose track has two sections or more. False when it has none.
static bool TestFile(std::string const &path, std::string const &folder)
{
	OpenLevelFile f;

	if (f.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		return false;
	}

	for (host::TrackObjectId const &id : host::TrackObjectsWithTracks(&f))
	{
		host::TrackLoaded fromFile;

		if (host::TrackLoad(&f, id.bytes, &fromFile) != host::TrackEditResult::Done || fromFile.track.sections.size() < 2)
		{
			continue;
		}

		printf("%s\n", path.c_str());

		host::MovablePose got;

		host::MovableTrackEnd_UseEditor(nullptr);
		Check(host::MovableTrackEnd_Get(&f, RefOf(id.bytes, 0), &got) == host::MovableResult::NoSuchThing, "no editor given: NoSuchThing");

		host::TrackEditor ed;
		host::TrackAction act;

		Check(host::TrackEdit_Enter(&f, id.bytes, &ed, &act, false) && ed.working, "Edit Track holds the track (" + std::to_string(fromFile.track.sections.size()) + " sections)");

		if (!ed.working)
		{
			return true;
		}

		host::MovableTrackEnd_UseEditor(&ed);

		int32_t endMarker[3];

		host::TrackData_MarkerAt(ed.working->track, 0, true, nullptr, endMarker);
		Check(host::MovableTrackEnd_Get(&f, RefOf(id.bytes, 0), &got) == host::MovableResult::Done && SamePlace(got.place, endMarker) &&
			SameTurn(got.turn, ed.working->track.sections[0].quatEnd), "section 0's end: its marker's place (" + Words(endMarker) + ") and its end turn");

		// a new place and a quarter turn about the vertical axis
		host::MovablePose moved;

		moved.place[0] = endMarker[0] + 1000;
		moved.place[1] = endMarker[1] - 500;
		moved.place[2] = endMarker[2] + 250;
		moved.turn[0] = 0.0f;
		moved.turn[1] = 0.70710678f;
		moved.turn[2] = 0.0f;
		moved.turn[3] = 0.70710678f;

		Check(host::MovableTrackEnd_Set(&f, RefOf(id.bytes, 0), moved) == host::MovableResult::Done, "the new end is put");

		int32_t nowEnd[3], nextStart[3];

		host::TrackData_MarkerAt(ed.working->track, 0, true, nullptr, nowEnd);
		host::TrackData_MarkerAt(ed.working->track, 1, false, nullptr, nextStart);
		Check(SamePlace(nowEnd, moved.place) && SameTurn(ed.working->track.sections[0].quatEnd, moved.turn), "section 0 ends at the new place, turned so (" + Words(nowEnd) + ")");
		Check(SamePlace(nextStart, moved.place) && SameTurn(ed.working->track.sections[1].quatStart, moved.turn), "section 1 starts there, turned so, as after Finish End Pos Edit");

		host::TrackLoaded stillInFile;

		Check(host::TrackLoad(&f, id.bytes, &stillInFile) == host::TrackEditResult::Done && SameSections(stillInFile.track, fromFile.track),
			"the level's bytes are not touched: the track is edited in memory");

		host::MovablePose back;

		Check(host::MovableTrackEnd_Get(&f, RefOf(id.bytes, 0), &back) == host::MovableResult::Done && SamePlace(back.place, moved.place) && SameTurn(back.turn, moved.turn),
			"read back: the new place and turn");

		host::TrackData const held = ed.working->track;

		Check(host::MovableTrackEnd_Set(&f, RefOf(id.bytes, 0), back) == host::MovableResult::Done && SameSections(ed.working->track, held), "the pose it has: Done, the track as it was");

		// refusals
		uint8_t other[8];

		memcpy(other, id.bytes, sizeof(other));
		other[0] ^= 0x5a;
		other[5] ^= 0xa5;

		host::MovablePose untouched;

		untouched.place[0] = 12345;
		Check(host::MovableTrackEnd_Get(&f, RefOf(id.bytes, (int32_t)held.sections.size()), &untouched) == host::MovableResult::NoSuchThing && untouched.place[0] == 12345 &&
			host::MovableTrackEnd_Set(&f, RefOf(id.bytes, (int32_t)held.sections.size()), moved) == host::MovableResult::NoSuchThing, "a section past the track: NoSuchThing");
		Check(host::MovableTrackEnd_Get(&f, RefOf(id.bytes, -1), &untouched) == host::MovableResult::NoSuchThing, "section -1: NoSuchThing");
		Check(host::MovableTrackEnd_Get(&f, RefOf(other, 0), &untouched) == host::MovableResult::NoSuchThing &&
			host::MovableTrackEnd_Set(&f, RefOf(other, 0), moved) == host::MovableResult::NoSuchThing && SameSections(ed.working->track, held), "another object's ID: NoSuchThing, nothing changed");
		Check(host::MovableTrackEnd_Get(nullptr, RefOf(id.bytes, 0), &untouched) == host::MovableResult::NothingOpen, "no level open: NothingOpen");

		// Stop Track Edit writes it
		host::TrackAction stop;
		int32_t const cursor[3] = { 0, 0, 0 };

		host::TrackEdit_Command(&f, &ed, 0x51c, cursor, &stop);

		host::TrackLoaded written;

		Check(!ed.working && host::TrackLoad(&f, id.bytes, &written) == host::TrackEditResult::Done && SameSections(written.track, held),
			"Stop Track Edit writes the track as it was put (" + stop.words + ")");
		Check(host::MovableTrackEnd_Get(&f, RefOf(id.bytes, 0), &untouched) == host::MovableResult::NoSuchThing, "no track held after the stop: NoSuchThing");

		std::string const saved = folder + "\\movable_track_end_copy.rif";

		if (f.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
		{
			OpenLevelFile second;
			host::TrackLoaded reread;

			Check(second.Open(saved.c_str()) == LevelOpenResult::Opened && host::TrackLoad(&second, id.bytes, &reread) == host::TrackEditResult::Done &&
				SameSections(reread.track, held), "a saved copy is taken by the game's own loader with the new end");
			DeleteFileA(saved.c_str());
		}
		else
		{
			Check(false, "a copy is saved");
		}

		host::MovableTrackEnd_UseEditor(nullptr);
		return true;
	}

	return false;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("usage: host_movable_track_end_test <level file> [<level file> ...]\n");
		return 2;
	}

	std::string const folder = "movable_track_end_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), 0))
	{
		printf("  FAILED  the test couldn't make its folder\n");
		return 1;
	}

	// every Set puts nothing in the file, but the stop writes the level again: the first three levels with a track of two sections are enough
	int levels = 0;

	for (int a = 1; a < argc && levels < 3; a++)
	{
		levels += TestFile(argv[a], folder) ? 1 : 0;
	}

	RemoveDirectoryA(folder.c_str());
	Check(levels > 0, "at least one level has a track of two sections (" + std::to_string(levels) + ")");
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
