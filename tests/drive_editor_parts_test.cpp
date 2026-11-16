// The test for the simulated-input harness's own parts (tests/drive_editor/drive_editor.cpp): the state line read into its parts, the
// camera's forward and right made from its turn, the menu titles found in a picture, and pictures compared. Written 2026-11-14 by
// Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: a harness that reads a line wrongly, or finds titles where there are none, would pass an editor that's broken: the checker needs
// checking too (Tao 4). Every input here is made up, so the answer each part must give is known. It starts no editor.

#define DRIVE_EDITOR_PARTS_ONLY
#include "drive_editor/drive_editor.cpp"

static bool Near(double a, double b)
{
	return fabs(a - b) < 1e-6;
}

// A made-up picture: background everywhere, and lit blocks where asked
static Picture MadeUpPicture(UINT width, UINT height)
{
	Picture picture;
	picture.width = width;
	picture.height = height;
	picture.bgra.assign((size_t)width * height * 4, 20);
	return picture;
}

static void Light(Picture *picture, UINT left, UINT right, UINT top, UINT bottom)
{
	for (UINT y = top; y <= bottom; y++)
	{
		for (UINT x = left; x <= right; x++)
		{
			BYTE *const p = &picture->bgra[(y * picture->width + x) * 4];
			p[0] = p[1] = p[2] = 230;
		}
	}
}

static void TestTheStateLine()
{
	State const s = ParseState("AVP_OpenInteng: state: menu none item none, camera 100 -200 300 1024 -64, view mode polyline, view 800x600 at 10,20, "
		"window 1024x768, panels 1024x740");
	Check(s.read && s.menu == -1, "a state line with no menu open is read, and says none");
	Check(Near(s.camera[0], 100) && Near(s.camera[1], -200) && Near(s.camera[2], 300) && Near(s.camera[3], 1024) && Near(s.camera[4], -64),
		"the camera's place, turn and look are read as written");
	Check(strcmp(s.mode, "polyline") == 0 && s.viewWidth == 800 && s.viewHeight == 600 && s.viewX == 10 && s.viewY == 20 &&
		s.windowWidth == 1024 && s.windowHeight == 768 && s.panelsWidth == 1024 && s.panelsHeight == 740, "the view mode and every size are read as written");

	State const open = ParseState("AVP_OpenInteng: state: menu 2 item 3, camera 0 0 0 0 0, view mode normal, view 1x1 at 0,0, window 1x1, panels 1x1");
	Check(open.read && open.menu == 2 && open.item == 3, "an open menu's number and its lit item are read");
	Check(s.item == -1, "and no lit item reads as none");

	State const cut = ParseState("AVP_OpenInteng: state: menu none item none, camera 1 2 3 4 5, view mode normal, view 800x600 at 0,0, window 1024x768");
	Check(!cut.read, "a line with a part missing isn't taken as read");
	Check(!ParseState("something else entirely").read, "and neither is a line that isn't a state line");

	// The camera's own axes, from its turn, as CreateEulerMatrix makes them (maths.cpp:355-397): forward (-sin, 0, cos), right (cos, 0, sin)
	State const ahead = ParseState("AVP_OpenInteng: state: menu none item none, camera 0 0 0 0 0, view mode normal, view 1x1 at 0,0, window 1x1, panels 1x1");
	Check(Near(ahead.forward[0], 0) && Near(ahead.forward[2], 1) && Near(ahead.right[0], 1) && Near(ahead.right[2], 0),
		"turned 0, the camera faces along +z, with +x to its right");
	State const quarter = ParseState("AVP_OpenInteng: state: menu none item none, camera 0 0 0 1024 0, view mode normal, view 1x1 at 0,0, window 1x1, panels 1x1");
	Check(Near(quarter.forward[0], -1) && fabs(quarter.forward[2]) < 1e-9 && fabs(quarter.right[0]) < 1e-9 && Near(quarter.right[2], 1),
		"turned a quarter (1024), it faces along -x, with +z to its right");
	double const dot = quarter.forward[0] * quarter.right[0] + quarter.forward[2] * quarter.right[2];
	Check(fabs(dot) < 1e-9, "forward and right are at right angles");
}

static void TestFindingTitles()
{
	// Four titles on a toolbar line, rows 5 to 15, each with gaps of up to 4 pixels between its own letters, and 20 or more between titles
	Picture bar = MadeUpPicture(400, 100);
	Light(&bar, 10, 18, 5, 15);
	Light(&bar, 22, 40, 5, 15);
	Light(&bar, 60, 90, 5, 15);
	Light(&bar, 130, 150, 6, 14);
	Light(&bar, 154, 170, 5, 15);
	Light(&bar, 200, 230, 5, 15);
	Light(&bar, 50, 350, 40, 60);   // something lit further down: not on the toolbar's line
	std::vector<Span> const titles = FindTitles(bar);
	Check(titles.size() == 4, "four titles are found on the toolbar's line: " + std::to_string(titles.size()));
	bool const bounds = titles.size() == 4 && titles[0].left == 10 && titles[0].right == 40 && titles[1].left == 60 && titles[1].right == 90 &&
		titles[2].left == 130 && titles[2].right == 170 && titles[3].left == 200 && titles[3].right == 230 && titles[0].top == 5 && titles[0].bottom == 15;
	Check(bounds, "each from its first lit column to its last, the letters' own small gaps inside it, and the line's rows");

	Check(FindTitles(MadeUpPicture(400, 100)).empty(), "a picture with nothing lit has no titles");
	Check(FindTitles(Picture()).empty(), "and a picture that couldn't be read has none either");
}

static void TestComparingPictures()
{
	Picture const a = MadeUpPicture(100, 50);
	Picture b = MadeUpPicture(100, 50);
	Check(Near(ShareDifferent(a, b), 0.0), "two pictures the same differ by nothing");
	Light(&b, 0, 49, 0, 49);
	Check(Near(ShareDifferent(a, b), 0.5), "half of one lit differs by half");
	Check(ShareDifferent(a, MadeUpPicture(100, 51)) < 0, "pictures of different sizes can't be compared, and say so");

	double const x[3] = { 1, 0, 0 };
	double const z[3] = { 0, 0, 1 };
	double const none[3] = { 0, 0, 0 };
	Check(Near(Angle(x, z), 90.0) && Near(Angle(x, x), 0.0), "the angle between two directions is in degrees");
	Check(Near(Angle(none, x), 180.0), "and a move of nothing is as far off as can be, so no check passes on it");
}

int main()
{
	TestTheStateLine();
	TestFindingTitles();
	TestComparingPictures();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
