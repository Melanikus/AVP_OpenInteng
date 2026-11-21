// host_world_overlay_test.cpp: the 3D view's world lines from more than one owner (src/host/world_overlay.h): an owner's list and the gizmo's extra lines go to the engine
// together, the extra lines first; an owner's empty list leaves the extra lines; the same extra lines are not sent twice. The engine's LevelView_WorldLines is a stand-in that
// keeps what it was given. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>
#include <vector>

#include "level_view.h"     // kLevelViewWorldLinesMax: what the engine takes
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

	// THE WAYPOINT DISPLAY (Vasquez 11-19: every AI module's boxes and links at once, ~235k lines on v2d, sent by its owner every frame): a list the same as the
	// last one is not copied, joined or sent again; the engine keeps it
	host::WorldOverlay_Lines(nullptr, 0);
	Check(sends == 6, "nothing again after nothing: not sent again");

	host::WorldOverlay_Lines(owner.data(), owner.size());
	int const afterOwner = sends;
	host::WorldOverlay_Lines(owner.data(), owner.size());
	Check(sends == afterOwner, "the same owner list again (every frame): not sent again");

	std::vector<host::TrackLine> moved = owner;
	moved[1].to[2] = 7;
	host::WorldOverlay_Lines(moved.data(), moved.size());
	Check(sends == afterOwner + 1 && engine.size() == 3 && engine[1].to[2] == 7, "a list of the same count with one line changed: sent");

	host::WorldOverlay_Extra(gizmo);
	Check(sends == afterOwner + 2 && engine.size() == 5 && engine[0].from[0] == 100 && engine[3].to[2] == 7, "the gizmo still goes in front of an owner's kept list");
	host::WorldOverlay_Lines(moved.data(), moved.size());
	Check(sends == afterOwner + 2, "the owner's list again with the gizmo's on: not sent again");

	// the owner's own change number: the same non-zero number and count = the lines are not even read (a buffer with other lines in it now is NOT sent)
	std::vector<host::TrackLine> big(235000, Line(9, 0xFFFFFFFFu));
	host::WorldOverlay_Lines(big.data(), big.size(), 41);
	int const afterBig = sends;
	Check(engine.size() == 2 + 235000 && engine[2].from[0] == 9 && engine.back().from[0] == 9, "235000 owner lines reach the engine whole, behind the gizmo's (the overlay cuts nothing)");
	big[5].from[0] = 1234;
	host::WorldOverlay_Lines(big.data(), big.size(), 41);
	Check(sends == afterBig && engine[2 + 5].from[0] == 9, "the same change number and count: not read, not sent");
	host::WorldOverlay_Lines(big.data(), big.size(), 42);
	Check(sends == afterBig + 1 && engine[2 + 5].from[0] == 1234, "a new change number: read and sent");
	host::WorldOverlay_Lines(big.data(), big.size() - 1, 42);
	Check(sends == afterBig + 2 && engine.size() == 2 + 234999, "the same number with another count: read and sent");
	host::WorldOverlay_Lines(big.data(), big.size(), 0);
	host::WorldOverlay_Lines(big.data(), big.size(), 0);
	Check(sends == afterBig + 3, "number 0 = no number: compared as a plain list (the same list twice is sent once)");
	host::WorldOverlay_Lines(owner.data(), owner.size(), 43);
	host::WorldOverlay_Lines(owner.data(), owner.size());
	big.assign(owner.size(), Line(5, 0xFFFFFFFFu));
	host::WorldOverlay_Lines(big.data(), big.size(), 43);
	Check(engine.size() == 5 && engine[2].from[0] == 5, "a call without a number forgets the last number: the same number after it is read again");

	host::WorldOverlay_Extra(std::vector<host::TrackLine>());
	host::WorldOverlay_Lines(nullptr, 0);
	Check(engine.empty(), "all gone again: the engine is given nothing");

	// EDIT ACROSS MODULES (Vasquez 11-19): in Waypoint modes 1 to 3 every OTHER network is drawn dimmed as the BACKGROUND, the current one bright as the owner's list.
	// The engine draws in the list's order with no depth writes: the order is gizmo, background, owner, so a bright line is drawn over a dim one where they meet
	std::vector<host::TrackLine> const others = { Line(200, 0xFF595959u), Line(201, 0xFF595959u), Line(202, 0xFF595959u), Line(203, 0xFF595959u) };
	int const beforeBackground = sends;
	host::WorldOverlay_Background(others.data(), others.size(), 7);
	Check(sends == beforeBackground + 1 && engine.size() == 4 && engine[0].from[0] == 200, "the background alone: sent");
	host::WorldOverlay_Lines(owner.data(), owner.size());
	host::WorldOverlay_Extra(gizmo);
	Check(engine.size() == 9 && engine[0].from[0] == 100 && engine[1].from[0] == 101 && engine[2].from[0] == 200 && engine[5].from[0] == 203 && engine[6].from[0] == 1 &&
		engine[8].from[0] == 3, "the order is gizmo, background, owner (the bright current network drawn last)");

	int const afterAll = sends;
	host::WorldOverlay_Background(others.data(), others.size(), 7);
	Check(sends == afterAll, "the same background number and count: not sent again");
	std::vector<host::TrackLine> othersMoved = others;
	othersMoved[0].from[1] = 99;
	host::WorldOverlay_Background(othersMoved.data(), othersMoved.size(), 7);
	Check(sends == afterAll && engine[2].from[1] == 0, "the same number and count: the background is not even read");
	host::WorldOverlay_Background(othersMoved.data(), othersMoved.size(), 8);
	Check(sends == afterAll + 1 && engine[2].from[1] == 99, "a new number: read and sent");
	host::WorldOverlay_Background(othersMoved.data(), othersMoved.size(), 0);
	Check(sends == afterAll + 1, "number 0: compared, the same bytes are not sent");
	host::WorldOverlay_Lines(moved.data(), moved.size());
	Check(sends == afterAll + 2 && engine.size() == 9 && engine[2].from[0] == 200 && engine[7].to[2] == 7, "an owner's change keeps the background where it is");

	// the whole over what the engine takes: the background is cut, never the gizmo or the owner's
	std::vector<host::TrackLine> const huge(kLevelViewWorldLinesMax, Line(300, 0xFF595959u));
	host::WorldOverlay_Background(huge.data(), huge.size(), 9);
	Check(engine.size() == kLevelViewWorldLinesMax && engine[0].from[0] == 100 && engine[2].from[0] == 300 && engine[kLevelViewWorldLinesMax - 4].from[0] == 300 &&
		engine[kLevelViewWorldLinesMax - 3].from[0] == 1 && engine.back().from[0] == 3, "over the engine's most: the background's tail is cut, the gizmo's and the owner's are whole");

	host::WorldOverlay_Background(nullptr, 0, 0);
	Check(engine.size() == 5 && engine[0].from[0] == 100 && engine[2].from[0] == 1, "the background taken away: the gizmo's and the owner's stay");
	host::WorldOverlay_Extra(std::vector<host::TrackLine>());
	host::WorldOverlay_Lines(nullptr, 0);
	Check(engine.empty(), "everything gone: the engine is given nothing");

	// the dim colour: red, green and blue each to 35% (the engine draws these lines with no blending: the alpha is kept as it was)
	Check(host::WorldOverlay_Dim(0xFFFFFFFFu) == 0xFF595959u, "inteng's white box: dim grey 0x595959");
	Check(host::WorldOverlay_Dim(0xFF00FF00u) == 0xFF005900u && host::WorldOverlay_Dim(0xFFFFFF00u) == 0xFF595900u,
		"a plain link's green and an aliens-only lane's yellow: the same hue, dimmed");
	Check(host::WorldOverlay_Dim(0x80FF0000u) == 0x80590000u && host::WorldOverlay_Dim(0xFF000000u) == 0xFF000000u, "the alpha kept; black stays black");
	std::vector<host::TrackLine> toDim = { Line(1, 0xFFFFFFFFu), Line(2, 0xFF00FF00u) };
	host::WorldOverlay_DimLines(&toDim);
	Check(toDim[0].colour == 0xFF595959u && toDim[1].colour == 0xFF005900u && toDim[1].from[0] == 2 && toDim[1].to[1] == 1, "DimLines: every line's colour dimmed, the places untouched");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
