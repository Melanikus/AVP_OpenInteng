// The test for src/host's object list: one row for each object of the open level. Written 2026-11-14 by Melanikus (Dietrich) for
// AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no level: the objects are made up here, as a level could give them, the hostile ones included.
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "object_list.h"

using namespace ui;
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

static int SixWide(int)
{
	return 6;
}

// An object with this name, and an ID made from its number
static LevelObject Object(const char *name, uint32_t number)
{
	LevelObject object;

	object.name = name;
	memset(object.id, 0, sizeof(object.id));
	memcpy(object.id, &number, sizeof(number));
	object.id[7] = 0x5A;
	return object;
}

static Box const kListBox = { 0, 32, 260, 700 };

static void TestEmpty()
{
	puts("No objects:");

	ObjectList list;
	PaintText lines[8];
	PaintBox boxes[8];
	size_t lineCount = 9;
	size_t boxCount = 9;
	bool onRow = true;

	list.Load(0, 5);
	list.LayOut(kListBox, 19);
	Check(list.Count() == 0 && list.Chosen() == -1 && !list.Cut() && list.Shown() == 0, "no list at all, whatever its count, is an empty list with none chosen");
	list.Plan(SixWide, 15, 0xFF000000u, lines, 8, &lineCount, boxes, 8, &boxCount);
	Check(lineCount == 0 && boxCount == 0, "and it shows nothing");
	Check(list.Press(10, 40, &onRow) && !onRow && !list.Handle(KeyEvent(Key::Down)), "a press in it is the list's, and chooses nothing; nor does a key");

	LevelObject const one = Object("door", 1);
	list.Load(&one, 0);
	Check(list.Count() == 0 && list.Chosen() == -1, "a level counting no objects is empty too");
	Check(list.Name(0).empty() && list.Id(0) == 0 && list.FindById(one.id) == -1, "and has no first row to name");
}

static void TestNames()
{
	puts("Names from the level:");

	char noEnd[100];
	memset(noEnd, 'a', sizeof(noEnd));

	LevelObject const objects[] =
	{
		Object("door", 1),
		Object("caf\xC0", 2),
		Object(noEnd, 3),
		Object("%s%n", 4),
		Object(0, 5),
		Object("tab\there", 6),
	};

	ObjectList list;
	list.Load(objects, sizeof(objects) / sizeof(objects[0]));

	Check(list.Count() == 6 && list.Name(0) == "door", "each object is a row, its name as it is");
	Check(list.Name(1) == "caf?", "a byte the font has no letter for shows as a '?'");
	Check(list.Name(2) == std::string(kObjectNameMost, 'a'), "a name with no end is read to 63 bytes, the game's own most, and no further");
	Check(list.Name(3) == "%s%n", "a % is only its letter, never a format");
	Check(list.Name(4).empty(), "an object with no name is an empty row");
	Check(list.Name(5) == "tab?here", "and a control letter shows as a '?'");
	Check(list.Name(6).empty() && list.Name((size_t)-1).empty(), "a row past the end has no name");
}

static std::vector<LevelObject> many;

static void TestBigLevel()
{
	puts("A big level:");

	many.clear();
	for (uint32_t i = 0; i < 5000; i++)
	{
		many.push_back(Object("object", i));
	}

	ObjectList list;
	list.Load(many.data(), many.size());
	list.LayOut(kListBox, 19);

	Check(list.Count() == 5000 && !list.Cut() && list.Chosen() == 0 && list.First() == 0 && list.Shown() == 700 / 19,
		"5000 objects load, the first chosen, and as many rows show as fit: 36 of 19 pixels in 700");
	Check(list.Handle(KeyEvent(Key::PageDown)) && list.Chosen() > 0 && list.Handle(KeyEvent(Key::End)) && list.Chosen() == 4999 && list.First() == 4999 - 36 + 1,
		"Page Down and End move through it, and the last row shows at the bottom");

	PaintText lines[64];
	PaintBox boxes[64];
	size_t lineCount = 0;
	size_t boxCount = 0;

	list.Plan(SixWide, 15, 0xFF4A6A8Au, lines, 64, &lineCount, boxes, 64, &boxCount);
	Check(lineCount == 36 && boxCount == 1 && boxes[0].y == 32 + 35 * 19 && boxes[0].height == 19 && boxes[0].colour == 0xFF4A6A8Au,
		"it shows one line for each shown row, and a band behind the chosen one");
	Check(lines[0].text == "object" && lines[0].x == 4 && lines[0].y == 32 + 2 && lines[35].y == 32 + 35 * 19 + 2, "each name at its row's top left, inside the margin");

	list.Plan(SixWide, 15, 0xFF4A6A8Au, lines, 3, &lineCount, boxes, 0, &boxCount);
	Check(lineCount == 3 && boxCount == 0, "it never writes more lines or boxes than it has room for");

	Check(list.FindById(many[4321].id) == 4321, "the row with an ID is found by its 8 bytes");
	uint8_t unknown[kObjectIdBytes] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	Check(list.FindById(unknown) == -1 && list.FindById(0) == -1, "an ID no row has, or none, finds no row");

	list.Choose(4321);
	Check(list.Chosen() == 4321 && list.First() <= 4321 && 4321 < list.First() + list.Shown(), "a row chosen from outside, such as a pick, is chosen and shown");
	list.Choose(-1);
	list.Choose(5000);
	Check(list.Chosen() == 4321, "a row that isn't there changes nothing");

	bool onRow = false;
	int32_t const first = list.First();
	Check(list.Press(10, 32 + 2 * 19 + 5, &onRow) && onRow && list.Chosen() == first + 2, "a press on the third shown row chooses it");
	Check(list.Press(10, 32 + 2 * 19 + 5, &onRow) && onRow && list.Chosen() == first + 2, "a press on the row chosen already is on a row too, so the editor can select it again");
	Check(!list.Press(300, 100, &onRow) && !onRow && !list.Press(10, 20, &onRow), "a press outside the list's box isn't the list's");

	list.Load(many.data(), 100);
	Check(list.Count() == 100 && list.Chosen() == 0 && list.First() == 0, "loading again starts the list again, the first row chosen and shown");

	list.Load(many.data(), 10);
	list.LayOut(kListBox, 19);
	Check(list.Shown() == 10 && list.Press(10, 32 + 20 * 19, &onRow) && !onRow, "a press below the last row of a short list is the list's, and chooses nothing");
}

static void TestRoom()
{
	puts("The list's room:");

	ObjectList list;
	LevelObject const objects[] = { Object("a", 1), Object("b", 2) };
	PaintText lines[8];
	PaintBox boxes[8];
	size_t lineCount = 0;
	size_t boxCount = 0;
	bool onRow = false;

	list.Load(objects, 2);
	Box const flat = { 0, 32, 260, 0 };
	list.LayOut(flat, 19);
	list.Plan(SixWide, 15, 0xFF000000u, lines, 8, &lineCount, boxes, 8, &boxCount);
	Check(list.Shown() == 0 && lineCount == 0 && boxCount == 0 && !list.Press(10, 32, &onRow), "a box of no height shows no row, and takes no press");

	Box const short_ = { 0, 32, 260, 18 };
	list.LayOut(short_, 19);
	Check(list.Shown() == 0, "and so does one shorter than a row");
	list.LayOut(kListBox, 0);
	Check(list.Shown() == 0, "and rows of no height show none");
	list.LayOut(kListBox, -5);
	Check(list.Shown() == 0 && list.Press(10, 40, &onRow) && !onRow, "nor rows of a height below 0, and a press then chooses nothing");
}

static std::vector<LevelObject> tooMany;

static void TestTooMany()
{
	puts("More objects than the list holds:");

	tooMany.assign(kObjectListMost + 1, Object(0, 0));
	ObjectList list;

	list.Load(tooMany.data(), tooMany.size());
	Check(list.Count() == kObjectListMost && list.Cut(), "a level with more objects than the list holds is cut there, and says so");
	tooMany.clear();
	tooMany.shrink_to_fit();
}

int main()
{
	TestEmpty();
	TestNames();
	TestBigLevel();
	TestRoom();
	TestTooMany();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
