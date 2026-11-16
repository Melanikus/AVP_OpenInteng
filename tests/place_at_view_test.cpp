// The test for src/host's place function (place_at_view). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (P3). AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code". It needs no window and none of the game. The program gives back a non-zero result if
// any check failed.

#include <stdint.h>
#include <stdio.h>

#include <string>

#include "place_at_view.h"

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

static bool Is(const int32_t place[3], int32_t x, int32_t y, int32_t z)
{
	return place[0] == x && place[1] == y && place[2] == z;
}

int main()
{
	puts("Where a new thing goes:");

	int32_t const camera[3] = { 1000, 2000, 3000 };
	// The camera looks along +z and its right is +x
	ViewAxes const level = { { 0, 0, 65536 }, { 65536, 0, 0 } };
	int32_t place[3] = { 7, 7, 7 };

	Check(PlaceAtView_Make(camera, level, ViewOffset::OnCamera, 999, place) && Is(place, 1000, 2000, 3000), "On Camera is the camera's own place, whatever the distance");
	Check(PlaceAtView_Make(camera, level, ViewOffset::InFront, 2000, place) && Is(place, 1000, 2000, 5000), "In Front goes along the way the camera faces");
	Check(PlaceAtView_Make(camera, level, ViewOffset::Behind, 500, place) && Is(place, 1000, 2000, 2500), "Behind goes the other way");
	Check(PlaceAtView_Make(camera, level, ViewOffset::Right, 100, place) && Is(place, 1100, 2000, 3000), "Right goes along the camera's right");
	Check(PlaceAtView_Make(camera, level, ViewOffset::Left, 100, place) && Is(place, 900, 2000, 3000), "Left goes the other way");
	Check(PlaceAtView_Make(camera, level, ViewOffset::Above, 300, place) && Is(place, 1000, 1700, 3000), "Above is less y, as the game's y points down");
	Check(PlaceAtView_Make(camera, level, ViewOffset::Under, 300, place) && Is(place, 1000, 2300, 3000), "Under is more y");

	// A camera turned 45 degrees to its left looks along (-0.7071, 0, 0.7071): 46341 of 65536
	ViewAxes const turned = { { -46341, 0, 46341 }, { 46341, 0, 46341 } };
	Check(PlaceAtView_Make(camera, turned, ViewOffset::InFront, 1000, place) && place[0] == 1000 - 708 && place[1] == 2000 && place[2] == 3000 + 707,
		"a turned camera: the distance is shared between the axes as the game's own fixed sums do it (the product moved down 16 places, so a negative one rounds down)");

	Check(PlaceAtView_Make(camera, ViewAxes{ { 0, -65536, 0 }, { 65536, 0, 0 } }, ViewOffset::InFront, 100, place) && Is(place, 1000, 1900, 3000),
		"a camera looking up (the game's -y) goes up when it goes in front");

	int32_t kept[3] = { 5, 6, 8 };
	place[0] = 5;
	place[1] = 6;
	place[2] = 8;
	Check(!PlaceAtView_Make(camera, level, ViewOffset::InFront, -1, place) && Is(place, kept[0], kept[1], kept[2]), "a distance below 0 is refused, the place untouched");
	Check(!PlaceAtView_Make(camera, level, ViewOffset::InFront, kPlaceAtViewFarthest + 1, place) && Is(place, 5, 6, 8), "and one past the farthest");
	Check(!PlaceAtView_Make(camera, level, ViewOffset::InFront, kPlaceAtViewFarthest, place) && Is(place, 5, 6, 8),
		"a distance that would carry the place past the farthest from the centre is refused");
	Check(!PlaceAtView_Make(camera, level, (ViewOffset)99, 10, place) && !PlaceAtView_Make(nullptr, level, ViewOffset::OnCamera, 0, place)
		&& !PlaceAtView_Make(camera, level, ViewOffset::OnCamera, 0, nullptr), "a kind that isn't one of the seven, or no camera, or no place, is refused");

	int32_t const nearEdge[3] = { kPlaceAtViewFarthest, 0, 0 };
	Check(PlaceAtView_Make(nearEdge, level, ViewOffset::OnCamera, 0, place) && Is(place, kPlaceAtViewFarthest, 0, 0), "a camera at the farthest is still allowed on itself");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks ok");
	return checksFailed ? 1 : 0;
}
