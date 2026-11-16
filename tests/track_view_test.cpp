// The test for the track's lines in the three views (track_view.h). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (P10). AVP_OpenInteng's
// own code: see License.txt. No window, none of the game.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "track_view.h"

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

static void Put(std::vector<uint8_t> &bytes, size_t at, int32_t value)
{
	memcpy(&bytes[at], &value, 4);
}

// A track of `count` sections: the count, the sections (76 bytes each), then the flags and the timer
static std::vector<uint8_t> Track(int32_t count)
{
	std::vector<uint8_t> bytes(4 + (size_t)count * 76 + 8, 0);

	Put(bytes, 0, count);
	return bytes;
}

int main()
{
	puts("The track's lines in the three views:");

	std::vector<uint8_t> two = Track(2);

	Put(two, 4 + 32, 100);  Put(two, 4 + 36, -200); Put(two, 4 + 40, 300);     // section 0: pivot start
	Put(two, 4 + 44, 400);  Put(two, 4 + 48, 500);  Put(two, 4 + 52, -600);    // pivot end
	Put(two, 4 + 56, 10);   Put(two, 4 + 60, 20);   Put(two, 4 + 64, 30);      // object offset
	Put(two, 4 + 76 + 32, -700);                                               // section 1: pivot start x

	std::vector<TrackLine> lines;
	int32_t maxAbs[3];

	Check(TrackView_Lines(two.data(), two.size(), 0, &lines, maxAbs) && lines.size() == 2, "two sections: a line each, no crosses with a tick of 0");
	Check(lines[0].from[0] == 100 && lines[0].from[1] == -200 && lines[0].from[2] == 300 && lines[0].to[0] == 400 && lines[0].to[1] == 500 && lines[0].to[2] == -600
		&& lines[0].colour == kTrackPivotColour, "a section's line is from its pivot start to its pivot end");
	Check(maxAbs[0] == 700 && maxAbs[1] == 500 && maxAbs[2] == 600, "the largest distance from the centre on each axis is over every place");

	Check(TrackView_Lines(two.data(), two.size(), 5, &lines, maxAbs) && lines.size() == 20, "with a tick, each section adds three crosses (both pivots and the object offset) of three lines each");
	Check(lines[1].from[0] == 95 && lines[1].to[0] == 105 && lines[1].colour == kTrackPivotColour && lines[7].from[0] == 5 && lines[7].to[0] == 15 && lines[7].from[1] == 20 && lines[7].colour == kTrackOffsetColour,
		"the crosses are centred on the places: x from 95 to 105 at the pivot start (100), from 5 to 15 at the offset (10), for a half of 5");

	// the current section only (FUN_004f0da0): its start and end markers and the line between, no cross at the object offset
	size_t sections = 0;
	Check(TrackView_Section(two.data(), two.size(), 5, 1, &lines, maxAbs, &sections) && sections == 2 && lines.size() == 7 && lines[0].from[0] == -700,
		"one section shown: its line and the crosses at its start and end only, and the count is of the whole track");
	Check(TrackView_Section(two.data(), two.size(), 5, 0, &lines, maxAbs, nullptr) && lines.size() == 7 && lines[0].from[0] == 100 && maxAbs[0] == 700,
		"the other section the same way, and the largest distances are still about the whole track");
	Check(TrackView_Section(two.data(), two.size(), 0, 5, &lines, maxAbs, nullptr) && lines.empty(), "a section the track doesn't have shows nothing");

	std::vector<uint8_t> hostile = Track(1);
	Put(hostile, 0, 0x7fffffff);
	Check(!TrackView_Lines(hostile.data(), hostile.size(), 0, &lines, maxAbs) && lines.empty(), "a count the bytes can't hold is refused, with no lines");
	Put(hostile, 0, -1);
	Check(!TrackView_Lines(hostile.data(), hostile.size(), 0, &lines, maxAbs), "a negative count is refused");
	Check(!TrackView_Lines(two.data(), 11, 0, &lines, maxAbs) && !TrackView_Lines(nullptr, 100, 0, &lines, maxAbs), "too few bytes, or none, is refused");

	std::vector<uint8_t> none = Track(0);
	Check(TrackView_Lines(none.data(), none.size(), 5, &lines, maxAbs) && lines.empty() && maxAbs[0] == 0, "a track with no sections is right, and has no lines");

	std::vector<uint8_t> many = Track(1500);
	Check(TrackView_Lines(many.data(), many.size(), 0, &lines, maxAbs) && lines.size() == kTrackViewMostSections, "a track is cut at the most sections");

	std::vector<uint8_t> edge = Track(1);
	Put(edge, 4 + 56, INT32_MAX);
	Check(TrackView_Lines(edge.data(), edge.size(), 100, &lines, maxAbs) && lines[7].to[0] == INT32_MAX && maxAbs[0] == INT32_MAX, "a cross at the end of the range is held, never wrapped");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks ok");
	return checksFailed ? 1 : 0;
}
