// The test for the set of selected objects (selection.h): Shift+click adds or takes out, a plain pick makes one, "L" clears. Written 2026-11-15
// by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt. No window, none of the game.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

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

static Selected Obj(uint8_t first)
{
	Selected s = { SelectedKind::Object, 0, { first, 1, 2, 3, 4, 5, 6, 7 } };
	return s;
}

static bool LastIs(uint8_t first)
{
	Selected const now = Selection_Get();
	return now.kind == SelectedKind::Object && now.objectId[0] == first;
}

int main()
{
	puts("The set of selected objects:");

	Selection_Clear();
	Check(Selection_SetCount() == 0, "empty at the start");
	Check(Selection_Toggle(Obj(1)) && Selection_Toggle(Obj(2)) && Selection_Toggle(Obj(3)) && Selection_SetCount() == 3 && LastIs(3),
		"Shift+click adds, and the last one added is the one the panel shows");
	uint32_t const before = Selection_Changes();
	Check(Selection_Toggle(Obj(2)) && Selection_SetCount() == 2 && LastIs(3) && Selection_Changes() != before, "Shift+click on one in the set takes it out");
	uint8_t id[kSelectionIdBytes];
	Check(Selection_SetMember(0, id) && id[0] == 1 && Selection_SetMember(1, id) && id[0] == 3 && !Selection_SetMember(2, id), "the others keep their order");
	Check(Selection_Toggle(Obj(3)) && LastIs(1) && Selection_SetCount() == 1, "taking out the last shown makes the last one left the shown one");
	Check(Selection_Toggle(Obj(1)) && Selection_SetCount() == 0 && Selection_Get().kind == SelectedKind::Nothing, "taking out the only one leaves nothing");

	Selection_Toggle(Obj(1));
	Selection_Toggle(Obj(2));
	Check(Selection_Set(Obj(9)) && Selection_SetCount() == 1 && LastIs(9), "a plain pick makes the set that one object");
	Selected module = { SelectedKind::Module, 4, { 0 } };
	Check(Selection_Set(module) && Selection_SetCount() == 0, "a module makes the set empty");
	Check(Selection_Toggle(Obj(5)) && Selection_SetCount() == 1 && LastIs(5), "and Shift+click on an object after a module starts a set of objects");
	Selection_Toggle(Obj(6));
	Selection_Clear();
	Check(Selection_SetCount() == 0 && Selection_Get().kind == SelectedKind::Nothing, "L (Selection_Clear) deselects all");

	Selected none = { SelectedKind::Object, 0, { 0 } };
	Check(!Selection_Toggle(none) && !Selection_Toggle(module) && Selection_SetCount() == 0, "a nameless object or a module can't be toggled in");

	for (int i = 0; i < (int)kSelectionSetMost; i++)
	{
		Selected s = { SelectedKind::Object, 0, { (uint8_t)(i & 255), (uint8_t)(i >> 8), 1, 0, 0, 0, 0, 0 } };
		Selection_Toggle(s);
	}

	Selected extra = { SelectedKind::Object, 0, { 0xAA, 0xBB, 1, 0, 0, 0, 0, 0 } };
	Check(Selection_SetCount() == kSelectionSetMost && !Selection_Toggle(extra) && Selection_SetCount() == kSelectionSetMost, "a full set refuses one more");

	PickReport report;
	memset(&report, 0, sizeof(report));
	report.found = PickFound::Object;
	Selection_Clear();
	memcpy(report.objectId, Obj(7).objectId, kSelectionIdBytes);
	std::string const first = Selection_UsePickToggling(report);
	Check(first == "1 object selected", "the words after a Shift+click say how many: " + first);
	std::string const second = Selection_UsePickToggling(report);
	Check(second == "0 objects selected" && Selection_SetCount() == 0, "and again takes it out: " + second);

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks ok");
	return checksFailed ? 1 : 0;
}
