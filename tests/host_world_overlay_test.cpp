// host_world_overlay_test.cpp: the 3D view's world lines from more than one owner (src/host/world_overlay.h): an owner's list and the gizmo's extra lines go to the engine
// together, the extra lines first; an owner's empty list leaves the extra lines; the same extra lines are not sent twice. The engine's LevelView_WorldLines is a stand-in that
// keeps what it was given. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>
#include <vector>

#include "world_overlay.h"

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

static int sends = 0;
static std::vector<host::TrackLine> engine;

void LevelView_WorldLines(const host::TrackLine *lines, size_t count)
{
	sends++;
	engine.assign(lines, lines ? lines + count : lines);
}

static host::TrackLine Line(int32_t tag, uint32_t colour)
{
	host::TrackLine line = { { tag, 0, 0 }, { tag, 1, 0 }, colour };

	return line;
}

int main()
{
	std::vector<host::TrackLine> const owner = { Line(1, 0xFF0000FFu), Line(2, 0xFF0000FFu), Line(3, 0xFF0000FFu) };
	std::vector<host::TrackLine> const gizmo = { Line(100, 0xFFFF0000u), Line(101, 0xFFFF0000u) };

	host::WorldOverlay_Lines(owner.data(), owner.size());
	Check(sends == 1 && engine.size() == 3 && engine[0].from[0] == 1, "an owner's lines go to the engine as they are");

	host::WorldOverlay_Extra(gizmo);
	Check(sends == 2 && engine.size() == 5 && engine[0].from[0] == 100 && engine[1].from[0] == 101 && engine[2].from[0] == 1 && engine[4].from[0] == 3,
		"the gizmo's lines go in front of the owner's (never the part 4096 cuts off)");

	host::WorldOverlay_Extra(gizmo);
	Check(sends == 2, "the same extra lines again: not sent again");

	host::WorldOverlay_Lines(nullptr, 0);
	Check(sends == 3 && engine.size() == 2 && engine[0].from[0] == 100, "an owner takes its lines away: the gizmo's stay");

	host::WorldOverlay_Lines(owner.data(), 1);
	Check(sends == 4 && engine.size() == 3 && engine[2].from[0] == 1, "a new owner list replaces the last, behind the gizmo's");

	host::WorldOverlay_Extra(std::vector<host::TrackLine>());
	Check(sends == 5 && engine.size() == 1 && engine[0].from[0] == 1, "the gizmo gone: only the owner's");

	host::WorldOverlay_Lines(nullptr, 0);
	Check(sends == 6 && engine.empty(), "both gone: the engine is given nothing (null, 0)");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
