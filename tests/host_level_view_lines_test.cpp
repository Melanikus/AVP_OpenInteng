// host_level_view_lines_test.cpp: the 3D view's world lines as the engine is given them (src/host/level_view.h LevelView_ConvertWorldLines, what LevelView_WorldLines
// hands R_SetEditorWorldLines): every field as the editor gave it, as many as the engine takes (kLevelViewWorldLinesMax, the waypoint display's ~235k lines fit), none for
// null or 0, and the same vector kept from call to call (it grows, and shrinks to what was given). The engine's EditorWorldLine is a stand-in with its fields here.
// Written 2026-11-19 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>
#include <vector>

#include "level_view.h"

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

// avpx renderer.h's EditorWorldLine, field for field
struct EngineLine
{
	int x0, y0, z0, x1, y1, z1;
	unsigned int colour;
};

static host::TrackLine Line(int32_t tag)
{
	host::TrackLine line = { { tag, -tag, 3 * tag }, { tag + 1, -tag - 1, 3 * tag + 1 }, 0xFF000000u | (uint32_t)tag };

	return line;
}

int main()
{
	std::vector<EngineLine> out;

	Check(kLevelViewWorldLinesMax >= 524288, "the engine is given up to 524288 lines (was 4096)");

	std::vector<host::TrackLine> const three = { Line(1), Line(2), Line(-70000) };
	size_t used = LevelView_ConvertWorldLines(three.data(), three.size(), kLevelViewWorldLinesMax, &out);
	Check(used == 3 && out.size() == 3, "three lines: three for the engine");
	Check(out[2].x0 == -70000 && out[2].y0 == 70000 && out[2].z0 == -210000 && out[2].x1 == -69999 && out[2].y1 == 69999 && out[2].z1 == -209999 &&
		out[2].colour == (0xFF000000u | (uint32_t)-70000), "every field as the editor gave it (from, to, colour)");

	std::vector<host::TrackLine> big(235000);
	for (size_t i = 0; i < big.size(); i++)
	{
		big[i] = Line((int32_t)i);
	}
	used = LevelView_ConvertWorldLines(big.data(), big.size(), kLevelViewWorldLinesMax, &out);
	Check(used == 235000 && out.size() == 235000 && out[4095].x0 == 4095 && out[4096].x0 == 4096 && out[234999].x0 == 234999,
		"235000 lines (the waypoint display on v2d): all of them, none cut at 4096");

	used = LevelView_ConvertWorldLines(big.data(), big.size(), 1000, &out);
	Check(used == 1000 && out.size() == 1000 && out[999].x0 == 999, "more than the engine takes: the first ones, as many as it takes");

	used = LevelView_ConvertWorldLines(three.data(), 2, kLevelViewWorldLinesMax, &out);
	Check(used == 2 && out.size() == 2 && out[1].x0 == 2, "a shorter list after a long one: the vector holds only it");

	used = LevelView_ConvertWorldLines(nullptr, 5, kLevelViewWorldLinesMax, &out);
	Check(used == 0 && out.empty(), "null: none (count ignored)");

	used = LevelView_ConvertWorldLines(three.data(), 0, kLevelViewWorldLinesMax, &out);
	Check(used == 0 && out.empty(), "count 0: none");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
