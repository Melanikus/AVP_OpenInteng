// The test for src/host's field panel host: the selected thing's record, shown read-only in the fields place. Written 2026-11-14 by
// Melanikus (Dietrich) for AVP_OpenInteng (the two-dev sprint). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "field_panel_host.h"

using namespace ui;
using host::PaintText;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// Every letter six pixels wide, so the lines' places come from plain sums
static int Width(int)
{
	return 6;
}

static const Choice kKinds[] =
{
	{ 0, "standard" },
	{ 1, "flicker" },
	{ 2, "pulse" },
};

enum
{
	Name,
	Volume,
	Loops,
	Kind,
	FieldCount
};

static FieldDescriptor record[FieldCount];
static FieldValue values[FieldCount];
static unsigned char nameBytes[64];

// A record of four fields, one of each kind but a number twice over in use: a name, a volume, a flag and a kind. Every descriptor says
// it can be changed (readOnly false): the host must hold them read-only all the same.
static void MakeTheRecord()
{
	for (size_t i = 0; i < FieldCount; i++)
	{
		record[i] = EmptyFieldDescriptor();
		values[i] = EmptyFieldValue();
	}

	record[Name].name = "name";
	record[Name].kind = FieldKind::Text;
	record[Name].capacity = 63;

	record[Volume].name = "volume";
	record[Volume].help = "the game's loudest is 127";
	record[Volume].kind = FieldKind::Number;
	record[Volume].lowest = 0.0;
	record[Volume].highest = 127.0;
	record[Volume].unit = "steps";

	record[Loops].name = "loops";
	record[Loops].kind = FieldKind::Bit;
	record[Loops].bit = 0x1u;

	record[Kind].name = "kind";
	record[Kind].kind = FieldKind::Choice;
	record[Kind].choices = kKinds;
	record[Kind].choiceCount = 3;

	memcpy(nameBytes, "door 1", 6);
	values[Name].bytes = nameBytes;
	values[Name].length = 6;
	values[Volume].number = 96.0;
	values[Loops].word = 3u;
	values[Kind].choice = 1;
}

// One line is 15 pixels tall. A box holds as many lines as fit between the margins every panel's text keeps above and below it
// (panel_paint.h), so a box of n lines is n lines and the two margins tall.
static int32_t const kLine = 15;
static int32_t const kMargins = 2 * host::kPaintMarginDown;
static Box const kBox = { 100, 40, 200, 10 * kLine + kMargins };

// What a record shows: every line it plans, each one ended by a |
static std::string Lines(host::FieldPanelHost const &shown, size_t *count)
{
	PaintText lines[64];
	std::string all;

	shown.Plan(Width, lines, 64, count);

	for (size_t i = 0; i < *count; i++)
	{
		all += lines[i].text + "|";
	}

	return all;
}

// A check of what a record plans: every line, in order, each ended by a |. A failed check says what it wanted and what it got.
static void CheckLines(host::FieldPanelHost const &shown, std::string const &wanted, std::string const &what)
{
	size_t count = 0;
	std::string const got = Lines(shown, &count);

	Check(got == wanted, what);

	if (got != wanted)
	{
		printf("      wanted: %s\n      got:    %s\n", wanted.c_str(), got.c_str());
	}
}

// The same for a piece of what a record plans
static void CheckLinesHold(host::FieldPanelHost const &shown, std::string const &piece, std::string const &what)
{
	size_t count = 0;
	std::string const got = Lines(shown, &count);
	bool const holds = got.find(piece) != std::string::npos;

	Check(holds, what);

	if (!holds)
	{
		printf("      wanted a piece: %s\n      got:            %s\n", piece.c_str(), got.c_str());
	}
}

static void TestShowing()
{
	puts("A record shown:");

	MakeTheRecord();

	host::FieldPanelHost shown;
	FieldListProblem problem;
	size_t count = 0;

	Check(!shown.Showing() && shown.Title().empty(), "a host starts showing nothing");
	shown.LayOut(kBox, kLine);
	CheckLines(shown, "", "and plans no line");

	Check(shown.Show("Door", record, FieldCount, values, &problem), "a record of four fields is shown");
	Check(shown.Showing() && shown.Title() == "Door" && shown.Panel().Count() == FieldCount, "it holds its title and its four fields");
	CheckLines(shown, "Door|name: door 1|volume: 96 steps|loops: on|kind: flicker|",
		"it plans the title, then one line a field: the label, the value as it is shown, a number's unit, a bit as on or off");

	PaintText lines[64];
	shown.Plan(Width, lines, 64, &count);
	bool evenlySpaced = count == 5;

	for (size_t i = 1; i < count; i++)
	{
		evenlySpaced = evenlySpaced && lines[i].y - lines[i - 1].y == kLine && lines[i].x == lines[0].x;
	}

	Check(evenlySpaced, "the lines go down the box one line height apart, all from the same left");
	Check(count > 0 && lines[0].x == kBox.x + host::kPaintMarginAcross && lines[0].y == kBox.y + host::kPaintMarginDown,
		"and the first starts inside the box's margins, as every panel's text does");

	// A value that's odd is marked, never shown as if it were fine
	values[Volume].number = 200.0;
	values[Loops].word = 0u;
	Check(shown.Show("Door", record, FieldCount, values, &problem), "(a record with a number out of its range is shown)");
	CheckLines(shown, "Door|name: door 1|! volume: 200 steps|loops: off|kind: flicker|", "a number outside its range is shown as it is, marked with a ! in front");
	shown.LayOut(Box{ 0, 0, 100, 10 * kLine + kMargins }, kLine);
	CheckLinesHold(shown, "|! ..: 200 steps|", "a marked line cut to a narrow box keeps its mark, and its value: the label gives way");
	shown.LayOut(kBox, kLine);
	values[Volume].number = 96.0;
	values[Kind].choice = 77;
	Check(shown.Show("Door", record, FieldCount, values, &problem), "(a record with a choice the list doesn't have is shown)");
	CheckLinesHold(shown, "kind: ", "a choice the list doesn't have is shown on its line");
	CheckLinesHold(shown, "|! kind: ", "and marked too");
	values[Kind].choice = 1;

	shown.Clear();
	Check(!shown.Showing() && shown.Title().empty() && shown.Panel().Count() == 0, "Clear shows nothing again");
	CheckLines(shown, "", "and plans no line");
}

static void TestTheHostKeepsItsOwnCopy()
{
	puts("The host keeps its own copy:");

	MakeTheRecord();

	host::FieldPanelHost shown;

	shown.LayOut(kBox, kLine);
	shown.Show("Door", record, FieldCount, values, 0);

	// The adapter's arrays change, as a stack array would after the call returns
	record[Name].name = "changed";
	record[Volume].highest = 1.0;
	values[Volume].number = 5.0;
	nameBytes[0] = 'X';
	CheckLines(shown, "Door|name: door 1|volume: 96 steps|loops: on|kind: flicker|", "the words and values shown are the ones given, whatever the arrays become");

	// And the adapter's list is wholly gone, as a stack array is once its function has returned: the panel may read nothing in it
	memset(record, 0xA5, sizeof(record));
	CheckLines(shown, "Door|name: door 1|volume: 96 steps|loops: on|kind: flicker|", "and when the adapter's whole list is overwritten, they still are");
	Check(shown.ClickField(Volume) == PanelAnswer::FocusMoved, "(a click on a field gives it focus)");

	char help[256];
	shown.Panel().HelpRow(help, sizeof(help));
	Check(strcmp(help, "the game's loudest is 127") == 0, "and the help line of the field with focus is the one given, read from the host's own copy");

	// A second record replaces the first
	MakeTheRecord();
	record[Volume].name = "level";
	Check(shown.Show("Window", record, 2, values, 0) && shown.Panel().Count() == 2, "a second Show replaces the record and its fields");
	CheckLines(shown, "Window|name: door 1|level: 96 steps|", "and its title");
}

static void TestReadOnly()
{
	puts("Inspect is read-only:");

	MakeTheRecord();

	host::FieldPanelHost shown;

	Check(shown.Handle(KeyEvent(Key::Tab)) == PanelAnswer::Nothing && shown.ClickField(0) == PanelAnswer::Nothing, "with nothing shown, events mean nothing");

	shown.Show("Door", record, FieldCount, values, 0);
	Check(shown.ClickField(Volume) == PanelAnswer::FocusMoved, "a click on a field gives it focus, so its help line can be read");

	char help[256];
	shown.Panel().HelpRow(help, sizeof(help));
	Check(strcmp(help, "the game's loudest is 127") == 0, "and the help line is that field's");

	Check(shown.Handle(LetterEvent('5')) == PanelAnswer::Refused && strcmp(shown.Panel().StatusWords(), kPanelReadOnly) == 0
		&& shown.Panel().NumberValue(Volume) == 96.0, "typing in it is refused with the panel's own words, though the adapter said it could change, and the value stays");

	Check(shown.ClickField(Name) == PanelAnswer::FocusMoved && shown.Handle(LetterEvent('x')) == PanelAnswer::Refused
		&& shown.Panel().Changed(Name) == false, "a text refuses typing too");

	Check(shown.ClickField(Loops) == PanelAnswer::FocusMoved && shown.Handle(KeyEvent(Key::Space)) == PanelAnswer::Refused && shown.Panel().BitChecked(Loops),
		"a flag refuses to be turned over");

	Check(shown.ClickField(Kind) == PanelAnswer::FocusMoved && shown.Handle(KeyEvent(Key::Down)) != PanelAnswer::Kept && shown.Panel().ChoiceValue(Kind) == 1,
		"a choice keeps the value it has");

	Check(shown.Handle(KeyEvent(Key::Tab)) == PanelAnswer::FocusMoved, "Tab moves focus on");
}

static void TestRefusals()
{
	puts("A record refused:");

	MakeTheRecord();

	host::FieldPanelHost shown;
	FieldListProblem problem;

	Check(shown.Show("Door", record, FieldCount, values, &problem) && shown.Showing(), "(a record is shown first)");

	record[Volume].name = "";
	Check(!shown.Show("Door", record, FieldCount, values, &problem) && !shown.Showing() && shown.Title().empty() && shown.Panel().Count() == 0,
		"a list the panel's check refuses, here a field with no name, shows nothing, not even what was shown before");
	Check(problem.why == FieldListRefusal::BadName && problem.field == Volume, "and says which field and why");
	Check(!shown.Show("Door", record, FieldCount, values, 0), "with no place for the problem, it's refused all the same");
	MakeTheRecord();

	Check(!shown.Show("Door", 0, FieldCount, values, &problem) && !shown.Showing(), "no list where fields are counted is refused");
	Check(!shown.Show("Door", record, FieldCount, 0, &problem) && !shown.Showing(), "no values where fields are counted is refused");

	FieldDescriptor many[host::kFieldPanelHostMostFields + 1];
	FieldValue manyValues[host::kFieldPanelHostMostFields + 1];

	for (size_t i = 0; i < host::kFieldPanelHostMostFields + 1; i++)
	{
		many[i] = record[Volume];
		manyValues[i] = values[Volume];
	}

	Check(shown.Show("Many", many, host::kFieldPanelHostMostFields, manyValues, &problem) && shown.Panel().Count() == host::kFieldPanelHostMostFields,
		"the most fields, 64, are shown");
	Check(!shown.Show("Many", many, host::kFieldPanelHostMostFields + 1, manyValues, &problem) && !shown.Showing(), "one more is refused as a whole");

	Check(shown.Show("Nothing to edit", 0, 0, 0, &problem) && shown.Showing() && shown.Panel().Count() == 0, "a record with no fields shows its title alone");
	shown.LayOut(kBox, kLine);
	CheckLines(shown, "Nothing to edit|", "and plans that one line");
}

static void TestTheTitle()
{
	puts("The title is read as hostile text:");

	MakeTheRecord();

	host::FieldPanelHost shown;

	shown.Show("bad\x01name\xC8", record, FieldCount, values, 0);
	Check(shown.Title() == "bad?name?", "a control letter and a byte the font has no letter for are each shown as a ?");

	char longTitle[300];
	memset(longTitle, 'a', sizeof(longTitle) - 1);
	longTitle[sizeof(longTitle) - 1] = '\0';
	shown.Show(longTitle, record, FieldCount, values, 0);
	Check(shown.Title().size() == host::kFieldPanelHostTitleMost, "a title longer than 63 letters is cut to 63");

	shown.Show(0, record, FieldCount, values, 0);
	Check(shown.Title().empty() && shown.Showing(), "no title is an empty one");

	// A name from the level with no zero in it: it ends at its length, never past it
	unsigned char noEnd[64];
	memset(noEnd, 'z', sizeof(noEnd));
	values[Name].bytes = noEnd;
	values[Name].length = 10;
	shown.Show("Door", record, FieldCount, values, 0);
	shown.LayOut(kBox, kLine);
	CheckLinesHold(shown, "|name: zzzzzzzzzz|", "a text value with no zero in it is read to its length and no further");
	values[Name].length = sizeof(noEnd);
	shown.Show("Door", record, FieldCount, values, 0);
	CheckLinesHold(shown, "|! name: |", "one longer than its field's capacity is shown as unreadable and marked, with none of it shown");

	values[Name].bytes = (const unsigned char *)"a\x01" "b";
	values[Name].length = 3;
	shown.Show("Door", record, FieldCount, values, 0);
	CheckLinesHold(shown, "name: a?b|", "a control letter inside a value is shown as a ?");
}

// A spec's check that refuses every text, so the value it's given is flagged as one the spec refuses
static bool RefuseAll(const void *, const unsigned char *, size_t, const unsigned char *, size_t, const char **)
{
	return false;
}

static void TestTheMarkIsNeverCut()
{
	puts("A flagged value's mark is never cut away:");

	MakeTheRecord();

	host::FieldPanelHost shown;
	unsigned char longName[250];

	memset(longName, 'a', sizeof(longName));
	record[Name].capacity = 255;
	record[Name].textCheck = RefuseAll;
	values[Name].bytes = longName;
	values[Name].length = sizeof(longName);
	Check(shown.Show("Door", record, FieldCount, values, 0), "(a record with a long name its spec refuses is shown)");
	shown.LayOut(kBox, kLine);
	CheckLinesHold(shown, "|! ..: aaa", "a long value the spec refuses keeps its mark, though its line is cut to the box (its label first, to \"..\")");
	shown.Clear();
}

// A door's record as the strategy adapter gives it: the five flags by their own words, then the two times in tenths of a second (the
// labels are src/model/strategy_fields.cpp's door fields, in the order tests/host_strategy_record_test.cpp proves). Only the switch is
// on, and the times are 5 and 30, the values of the door checkpoint 1 found cut at 150 pixels (Invasion, row 103).
enum
{
	DoorLocked,
	DoorProximity,
	DoorStartsOpen,
	DoorSwitch,
	DoorHorizontal,
	DoorOpenTime,
	DoorCloseTime,
	DoorFieldCount
};

static FieldDescriptor door[DoorFieldCount];
static FieldValue doorValues[DoorFieldCount];

static void MakeTheDoor()
{
	static const char *const kFlags[] = { "Door is locked", "Proximity door", "Starts open", "Switch activated door ('lift door')",
		"Door is horizontal (ie. in floor/ceiling)" };

	for (size_t i = 0; i < DoorFieldCount; i++)
	{
		door[i] = EmptyFieldDescriptor();
		doorValues[i] = EmptyFieldValue();
	}

	for (size_t i = DoorLocked; i <= DoorHorizontal; i++)
	{
		door[i].name = kFlags[i];
		door[i].kind = FieldKind::Bit;
		door[i].bit = 1u << i;
		doorValues[i].word = 1u << DoorSwitch;
	}

	door[DoorOpenTime].name = "Time take to open (0.1 - 25 sec)";
	door[DoorCloseTime].name = "Time take to close (0.1 - 25 sec)";

	for (size_t i = DoorOpenTime; i <= DoorCloseTime; i++)
	{
		door[i].kind = FieldKind::Number;
		door[i].lowest = 0.0;
		door[i].highest = 255.0;
		door[i].unit = "tenths s";
	}

	doorValues[DoorOpenTime].number = 5.0;
	doorValues[DoorCloseTime].number = 30.0;
}

// The lines a record plans, one a place: the title first, then each field's
static std::vector<std::string> LinesOf(host::FieldPanelHost const &shown)
{
	PaintText lines[64];
	size_t count = 0;
	std::vector<std::string> all;

	shown.Plan(Width, lines, 64, &count);

	for (size_t i = 0; i < count; i++)
	{
		all.push_back(lines[i].text);
	}

	return all;
}

static bool EndsWith(std::string const &text, std::string const &end)
{
	return text.size() >= end.size() && text.compare(text.size() - end.size(), end.size(), end) == 0;
}

static void TestTheValueIsNeverCut()
{
	puts("A field's value is never cut while it can be shown whole:");

	MakeTheDoor();

	host::FieldPanelHost shown;

	Check(shown.Show("Door (50)", door, DoorFieldCount, doorValues, 0), "(a door's record is shown)");

	// 150 pixels across leaves 142 inside the margins: 23 letters six pixels wide
	shown.LayOut(Box{ 0, 0, 150, 10 * kLine + kMargins }, kLine);
	CheckLines(shown, "Door (50)|Door is locked: off|Proximity door: off|Starts open: off|Switch activated ..: on|Door is horizont..: off|"
		"Time take..: 5 tenths s|Time tak..: 30 tenths s|",
		"at 150 pixels, every line of a door's record shows its value whole: a label too long is cut with the game's \"..\" instead");

	shown.LayOut(Box{ 0, 0, 400, 10 * kLine + kMargins }, kLine);
	CheckLines(shown, "Door (50)|Door is locked: off|Proximity door: off|Starts open: off|Switch activated door ('lift door'): on|"
		"Door is horizontal (ie. in floor/ceiling): off|Time take to open (0.1 - 25 sec): 5 tenths s|Time take to close (0.1 - 25 sec): 30 tenths s|",
		"and a line that fits is shown whole: a label is cut only when its line is too wide");

	// The widest value, ": 30 tenths s", is 78 pixels: from 78 pixels inside the margins, 86 across, every value fits whole
	static const char *const kValues[] = { ": off", ": off", ": off", ": on", ": off", ": 5 tenths s", ": 30 tenths s" };
	bool everyWidth = true;
	int32_t firstBad = 0;

	for (int32_t across = 86; across <= 400; across++)
	{
		shown.LayOut(Box{ 0, 0, across, 10 * kLine + kMargins }, kLine);

		std::vector<std::string> const lines = LinesOf(shown);
		bool good = lines.size() == 1 + DoorFieldCount;

		for (size_t i = 0; good && i < DoorFieldCount; i++)
		{
			good = EndsWith(lines[1 + i], kValues[i]) && (int64_t)lines[1 + i].size() * 6 <= across - 2 * host::kPaintMarginAcross;
		}

		everyWidth = everyWidth && good;
		firstBad = !good && firstBad == 0 ? across : firstBad;
	}

	Check(everyWidth, "at every width from 86 pixels to 400, every line ends with its whole value, and fits inside the margins"
		+ (firstBad ? " (not at " + std::to_string(firstBad) + ")" : std::string()));

	// 90 across leaves 82 inside: room for ": 30 tenths s" (78), and not for ".." (12) as well
	shown.LayOut(Box{ 0, 0, 90, 10 * kLine + kMargins }, kLine);
	CheckLinesHold(shown, "|: 30 tenths s|", "a value that fits alone is shown whole, with its label left out when not even \"..\" fits beside it");

	doorValues[DoorOpenTime].number = 300.0;
	Check(shown.Show("Door (50)", door, DoorFieldCount, doorValues, 0), "(a door whose time is past its field's highest is shown)");
	shown.LayOut(Box{ 0, 0, 150, 10 * kLine + kMargins }, kLine);
	CheckLinesHold(shown, "|! Time ..: 300 tenths s|", "a value the panel flags keeps its ! mark in front, and its value whole, when its label is cut");
	shown.Clear();

	// A value too wide for the line even with the label down to ".."
	MakeTheRecord();

	unsigned char wide[40];

	memset(wide, 'b', sizeof(wide));
	values[Name].bytes = wide;
	values[Name].length = sizeof(wide);
	Check(shown.Show("Door", record, FieldCount, values, 0), "(a record with a name 40 letters long is shown)");
	shown.LayOut(Box{ 0, 0, 80, 10 * kLine + kMargins }, kLine);
	CheckLinesHold(shown, "|..: bbbbbb..|", "a value too wide even alone is cut with \"..\", so it's seen to be cut, and its label is \"..\" alone");
	shown.Clear();

	// Lines of more letters than a line holds (kPaintMostLetters, 255), in a box wide enough for all of them
	MakeTheRecord();

	unsigned char many[254];

	memset(many, 'c', sizeof(many));
	record[Name].capacity = 255;
	values[Name].bytes = many;
	values[Name].length = 250;
	Check(shown.Show("Door", record, FieldCount, values, 0), "(a record with a name 250 letters long is shown)");
	shown.LayOut(Box{ 0, 0, 2000, 10 * kLine + kMargins }, kLine);
	CheckLinesHold(shown, "|n..: " + std::string(250, 'c') + "|",
		"a line of more letters than a line holds cuts its label, and its value of 250 letters stays whole");

	values[Name].length = 254;
	Check(shown.Show("Door", record, FieldCount, values, 0), "(a record with a name 254 letters long is shown)");
	shown.LayOut(Box{ 0, 0, 2000, 10 * kLine + kMargins }, kLine);
	CheckLinesHold(shown, "|..: " + std::string(249, 'c') + "..|",
		"a value of more letters than a line holds is cut with \"..\", so it's seen to be cut: no letter is lost unseen");
	shown.Clear();
}

static void TestTheLines()
{
	puts("The lines in a box:");

	MakeTheRecord();

	host::FieldPanelHost shown;
	size_t count = 0;

	shown.Show("Door", record, FieldCount, values, 0);

	shown.LayOut(Box{ 0, 0, 200, 5 * kLine + kMargins }, kLine);
	CheckLines(shown, "Door|name: door 1|volume: 96 steps|loops: on|kind: flicker|", "a box five lines tall (and its margins) shows all of it");

	shown.LayOut(Box{ 0, 0, 200, 4 * kLine + kMargins }, kLine);
	CheckLines(shown, "Door|name: door 1|volume: 96 steps|... and 2 more|",
		"one line short, the last line says how many more there are, and the fields that fit are shown from the first");

	shown.LayOut(Box{ 0, 0, 200, 2 * kLine + kMargins }, kLine);
	CheckLines(shown, "Door|... and 4 more|", "two lines hold the title and the count");

	shown.LayOut(Box{ 0, 0, 200, kLine + kMargins }, kLine);
	CheckLines(shown, "Door|", "one line holds the title alone");

	shown.LayOut(Box{ 0, 0, 200, kLine + kMargins - 1 }, kLine);
	CheckLines(shown, "", "a box a pixel too short for a line and its margins plans none");
	shown.LayOut(Box{ 0, 0, 2 * host::kPaintMarginAcross, 150 }, kLine);
	CheckLines(shown, "", "a box with nothing across but its margins plans none");
	shown.LayOut(Box{ 0, 0, 0, 150 }, kLine);
	CheckLines(shown, "", "a box of no width plans none");
	shown.LayOut(Box{ 0, 0, 200, 150 }, 0);
	CheckLines(shown, "", "a line of no height plans none, and divides by nothing");
	shown.LayOut(Box{ 0, 0, 200, 150 }, -kLine);
	CheckLines(shown, "", "nor a line below 0");
	shown.LayOut(Box{ 0, 0, -5, -5 }, kLine);
	CheckLines(shown, "", "nor a box below 0");

	shown.LayOut(Box{ 0, 0, 200, 150 }, kLine);
	PaintText lines[3];
	shown.Plan(Width, lines, 3, &count);
	Check(count == 3, "never more lines than the room given");
	shown.Plan(Width, lines, 0, &count);
	Check(count == 0, "and none for no room");
	shown.Plan(Width, 0, 3, &count);
	Check(count == 0, "and none when there's nowhere to put them");
	shown.Plan(0, lines, 3, &count);
	Check(count == 0, "or no font measure to cut them by");

	// A box at the far end of the numbers: the lines that would go past the end are left out, never wrapped round to the other end
	shown.LayOut(Box{ 0, INT32_MAX - 20, 200, 150 }, kLine);
	PaintText atTheEnd[10];
	shown.Plan(Width, atTheEnd, 10, &count);
	Check(count == 2 && atTheEnd[0].y == INT32_MAX - 20 + host::kPaintMarginDown && atTheEnd[1].y == INT32_MAX - 20 + kLine + host::kPaintMarginDown,
		"a box at the far end of the numbers plans the lines that fit before the end, and wraps nothing");
	shown.LayOut(Box{ INT32_MAX - 2, 0, 200, 150 }, kLine);
	shown.Plan(Width, atTheEnd, 10, &count);
	Check(count == 0, "a box at the far right plans none");
}

static void TestFieldAt()
{
	puts("The field under a click:");

	MakeTheRecord();

	host::FieldPanelHost shown;

	Check(shown.FieldAt(150, 50) == -1, "with nothing shown, none");
	shown.Show("Door", record, FieldCount, values, 0);
	shown.LayOut(kBox, kLine);

	// The title's line is the first, and it starts below the top margin
	int32_t const top = kBox.y + host::kPaintMarginDown;

	Check(shown.FieldAt(150, kBox.y) == -1 && shown.FieldAt(150, top - 1) == -1 && shown.FieldAt(150, top + 5) == -1,
		"the margin above the first line, and the title's line, are no field");
	Check(shown.FieldAt(150, top + kLine - 1) == -1 && shown.FieldAt(150, top + kLine) == Name, "the title's line ends where the first field's begins");
	Check(shown.FieldAt(150, top + kLine + 1) == Name && shown.FieldAt(150, top + 2 * kLine) == Volume
		&& shown.FieldAt(150, top + 4 * kLine + 14) == Kind, "each line below the title is its field, from the top to the bottom of its line");
	Check(shown.FieldAt(150, top + 5 * kLine) == -1, "the line past the last field is none");
	Check(shown.FieldAt(kBox.x - 1, top + kLine + 1) == -1 && shown.FieldAt(kBox.x + kBox.width, top + kLine + 1) == -1
		&& shown.FieldAt(150, kBox.y - 1) == -1 && shown.FieldAt(150, kBox.y + kBox.height) == -1, "outside the box, none");

	// A box with nothing across but its margins has no room for a line's text, so no click in it is on a field
	shown.LayOut(Box{ kBox.x, kBox.y, 2 * host::kPaintMarginAcross, kBox.height }, kLine);
	Check(shown.FieldAt(kBox.x + host::kPaintMarginAcross, top + kLine + 1) == -1, "a box with nothing across but its margins has no field under a click");

	// A box of four lines holds the title, two fields and the "more" line
	shown.LayOut(Box{ kBox.x, kBox.y, kBox.width, 4 * kLine + kMargins }, kLine);
	Check(shown.FieldAt(150, top + 3 * kLine) == -1 && shown.FieldAt(150, top + 2 * kLine) == Volume, "the \"more\" line is no field, and the fields shown still are");

	shown.LayOut(Box{ 100, 40, 200, 150 }, 0);
	Check(shown.FieldAt(150, 50) == -1, "a line of no height divides by nothing");
	shown.LayOut(Box{ 100, 40, 200, 150 }, kLine);
	Check(shown.FieldAt(INT32_MAX, INT32_MAX) == -1 && shown.FieldAt(INT32_MIN, INT32_MIN) == -1, "a place at the ends of the numbers is none");
}

static void TestHeightFor()
{
	puts("The height the host asks for:");

	MakeTheRecord();

	host::FieldPanelHost shown;

	Check(shown.HeightFor(kLine) == 0, "with nothing shown, it asks for no height");
	shown.Show("Door", record, FieldCount, values, 0);
	Check(shown.HeightFor(kLine) == (1 + FieldCount) * kLine + kMargins, "a record asks for a line for its title and one for each field, and the margins above and below");
	Check(shown.HeightFor(0) == 0 && shown.HeightFor(-kLine) == 0, "a line of no height, or below 0, asks for none");
	Check(shown.HeightFor(INT32_MAX) == INT32_MAX, "a line as tall as an int holds asks for no more than an int holds, and nothing wraps round");
	Check(shown.HeightFor(1000000000) == INT32_MAX, "and so does a line a billion pixels tall: five of them are past what an int holds, and that doesn't wrap round to some other number");

	// What it asks for is what holds every line, and one pixel less doesn't
	shown.LayOut(Box{ 0, 0, 200, shown.HeightFor(kLine) }, kLine);
	CheckLines(shown, "Door|name: door 1|volume: 96 steps|loops: on|kind: flicker|", "a box that tall shows every line of the record");
	shown.LayOut(Box{ 0, 0, 200, shown.HeightFor(kLine) - 1 }, kLine);
	CheckLinesHold(shown, "... and 2 more|", "and one pixel less has a line fewer, and says how many fields have no room (the words take a line)");

	shown.Show("Nothing to edit", 0, 0, 0, 0);
	Check(shown.HeightFor(kLine) == kLine + kMargins, "a record of no fields asks for its title's line alone");
	shown.Clear();
	Check(shown.HeightFor(kLine) == 0, "and a host that was cleared asks for none again");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestShowing();
	TestTheHostKeepsItsOwnCopy();
	TestReadOnly();
	TestRefusals();
	TestTheTitle();
	TestTheMarkIsNeverCut();
	TestTheValueIsNeverCut();
	TestTheLines();
	TestFieldAt();
	TestHeightFor();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
