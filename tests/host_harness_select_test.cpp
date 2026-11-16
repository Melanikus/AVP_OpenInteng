// The test for src/host's harness select step (harness_select.h): a normal run ignores it, a row from outside is checked against the list,
// the row's object is selected as a click does, and the printed lines are the panel's, through the safe text. Written 2026-11-14 by
// Melanikus (Dietrich) for AVP_OpenInteng (checkpoint 1). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "field_panel_host.h"
#include "harness_select.h"
#include "selection.h"

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

// Every letter six pixels wide
static int Width(int)
{
	return 6;
}

// A row of the list: a name and an ID whose first byte is first, the rest 0 (first 0 makes the game's "no name")
static LevelObject Row(const char *name, uint8_t first)
{
	LevelObject row;

	row.name = name;
	memset(row.id, 0, sizeof(row.id));
	row.id[0] = first;
	return row;
}

static void TestIgnoredWithSomeoneWatching()
{
	puts("With someone at the screen the question is ignored:");

	LevelObject const rows[] = { Row("door", 1), Row("lift", 2) };
	ObjectList list;

	list.Load(rows, 2);
	Selection_Clear();

	uint32_t const changes = Selection_Changes();

	Check(HarnessSelect_Row(false, &list, 1) == HarnessSelectAnswer::Ignored && Selection_Changes() == changes &&
		Selection_Get().kind == SelectedKind::Nothing && list.Chosen() == 0,
		"outside a -picture run a select row changes nothing: no selection, and the list's chosen row as it was");
}

static void TestARowIsSelected()
{
	puts("A row the list has is selected as a click selects it:");

	LevelObject const rows[] = { Row("door", 1), Row("lift", 2), Row("fan", 3) };
	ObjectList list;

	list.Load(rows, 3);
	Selection_Clear();

	Check(HarnessSelect_Row(true, &list, 2) == HarnessSelectAnswer::Selected, "row 2 is selected");

	Selected const now = Selection_Get();

	Check(now.kind == SelectedKind::Object && now.objectId[0] == 3 && list.Chosen() == 2,
		"the selection is row 2's object, known by its ID, and the list's chosen row is 2");

	uint32_t const changes = Selection_Changes();

	Check(HarnessSelect_Row(true, &list, 0) == HarnessSelectAnswer::Selected && Selection_Get().objectId[0] == 1 && list.Chosen() == 0 &&
		Selection_Changes() == changes + 1, "another row moves the selection once, and the list with it");
}

static void TestSeveralRows()
{
	puts("Several rows (add):");

	LevelObject const rows[] = { Row("door", 1), Row("lift", 2), Row("vent", 3) };
	ObjectList list;

	list.Load(rows, 3);
	Selection_Clear();
	HarnessSelect_Row(true, &list, 0);
	Check(Selection_SetCount() == 1, "one row selected");
	Check(HarnessSelect_Row(true, &list, 2, true) == HarnessSelectAnswer::Selected && Selection_SetCount() == 2, "row 2 added: two objects are selected");
	Check(HarnessSelect_Row(true, &list, 2, true) == HarnessSelectAnswer::Selected && Selection_SetCount() == 1, "row 2 again takes it out, as a Shift+click does");
	Check(HarnessSelect_Row(false, &list, 1, true) == HarnessSelectAnswer::Ignored && Selection_SetCount() == 1, "not a -picture run: ignored");
}

static void TestRowsFromOutsideAreChecked()
{
	puts("A row from outside is checked against the list:");

	LevelObject const rows[] = { Row("door", 1), Row("lift", 2), Row("fan", 3) };
	ObjectList list;

	list.Load(rows, 3);
	HarnessSelect_Row(true, &list, 1);

	uint32_t const changes = Selection_Changes();

	Check(HarnessSelect_Row(true, &list, 3) == HarnessSelectAnswer::Refused && Selection_Changes() == changes && list.Chosen() == 1 &&
		Selection_Get().objectId[0] == 2, "row 3 of a list of 3 is refused, and nothing changes");
	Check(HarnessSelect_Row(true, &list, 0xFFFFFFFFull) == HarnessSelectAnswer::Refused &&
		HarnessSelect_Row(true, &list, 0xFFFFFFFFFFFFFFFFull) == HarnessSelectAnswer::Refused && Selection_Changes() == changes &&
		list.Chosen() == 1, "the largest numbers a message can carry are refused, with nothing wrapping round to a row");
	// The editor is built for x86, where a row is cut to 32 bits once it is a place in the list: a row past 2^32 whose low bits name row 1
	// must be refused by the check in 64 bits, never taken as row 1
	Check(HarnessSelect_Row(true, &list, 0x100000001ull) == HarnessSelectAnswer::Refused && Selection_Changes() == changes && list.Chosen() == 1,
		"a row past 2^32 is refused, though its low 32 bits name a row the list has");
	Check(HarnessSelect_Row(true, 0, 0) == HarnessSelectAnswer::Refused && Selection_Changes() == changes, "no list is refused");

	ObjectList empty;

	empty.Load(0, 0);
	Check(HarnessSelect_Row(true, &empty, 0) == HarnessSelectAnswer::Refused && Selection_Changes() == changes, "an empty list has no row 0");

	LevelObject const nameless[] = { Row("first", 5), Row("noname", 0) };
	ObjectList zero;

	zero.Load(nameless, 2);
	Check(HarnessSelect_Row(true, &zero, 1) == HarnessSelectAnswer::Refused && Selection_Changes() == changes &&
		Selection_Get().objectId[0] == 2 && zero.Chosen() == 0,
		"an object whose ID is all 0 is refused, as the selection refuses it: the selection stays, and so does the list's chosen row");
}

static void TestThePanelLines()
{
	puts("The panel's lines, as the output says them:");

	ui::FieldDescriptor fields[1] = { ui::EmptyFieldDescriptor() };
	ui::FieldValue values[1] = { ui::EmptyFieldValue() };

	fields[0].name = "loops";
	fields[0].kind = ui::FieldKind::Bit;
	fields[0].bit = 0x1u;
	values[0].word = 0x1u;

	FieldPanelHost shown;

	Check(shown.Show("door\x01" "main", fields, 1, values, 0), "(a record of one field is shown)");
	shown.LayOut(ui::Box{ 0, 0, 300, 4 * 15 + 2 * kPaintMarginDown }, 15);

	PaintText lines[8];
	size_t count = 0;

	shown.Plan(Width, lines, 8, &count);

	std::vector<std::string> const out = HarnessSelect_PanelLines(lines, count);

	Check(count == 2 && out.size() == 3, "the title and one field are two lines drawn, and three output lines with the count");
	Check(out.size() == 3 && out[0] == "AVP_OpenInteng: panel 0: door?main" && out[1] == "AVP_OpenInteng: panel 1: loops: on" &&
		out[2] == "AVP_OpenInteng: panel lines 2", "each line says its place and the text the panel drew, title first, then the count");

	PaintText odd;

	odd.text = std::string("a\x01" "b\xC8" "c");
	odd.x = 0;
	odd.y = 0;

	std::vector<std::string> const oddOut = HarnessSelect_PanelLines(&odd, 1);

	Check(oddOut.size() == 2 && oddOut[0] == "AVP_OpenInteng: panel 0: a?b?c", "a line's bytes outside 32 to 126 are printed as ?");

	std::vector<std::string> const none = HarnessSelect_PanelLines(0, 5);

	Check(none.size() == 1 && none[0] == "AVP_OpenInteng: panel lines 0", "no lines to read gives the count line alone, with 0");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestIgnoredWithSomeoneWatching();
	TestARowIsSelected();
	TestSeveralRows();
	TestRowsFromOutsideAreChecked();
	TestThePanelLines();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
