// harness_select.h: the harness's third question, for checkpoint 1's hidden run: select an object list row's object as a click on that row
// does, then say what the field panel draws, line by line. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (the select step
// of Hicks's sim-input harness, AvP-docs\review\sim-input-harness-hicks.md). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's
// own code".
//
// The message comes from outside the editor (any program can post one to a window), so it's answered only in a -picture run, where nobody
// is at the screen, as the harness's other two are (editor_main.cpp, THE HARNESS'S QUESTIONS). Its row is a number from outside: it's
// checked against the list's own count, in 64 bits, before anything is read. The panel's lines are printed through the safe text
// (panel_paint.h), so a name from a hostile level reaches the output only as letters 32 to 126.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "object_list.h"
#include "panel_paint.h"

namespace host
{
	enum class HarnessSelectAnswer
	{
		Ignored,    // not a -picture run: nothing changes, and nothing is to be printed
		Refused,    // no list, a row the list doesn't have, or an object the selection refuses (an ID that's all 0): nothing changes
		Selected    // the object is selected and its row chosen, as a click on the row does
	};

	// The harness's "select row". With nobodyWatching false it's Ignored. Otherwise a row from 0 to one less than the list's count selects
	// that row's object (Selection_Set) and then chooses the row; anything else is Refused, with the list and the selection as they were.
	// With add true the object joins the selection like a Shift+click does (Selection_Toggle: one already in the set is taken out), for a scenario that needs several objects selected.
	HarnessSelectAnswer HarnessSelect_Row(bool nobodyWatching, ObjectList *list, uint64_t row, bool add = false);

	// The output's lines for what the field panel drew: "AVP_OpenInteng: panel <i>: <text>" for each line, i from 0, its text through
	// PanelPaint_SafeText, then "AVP_OpenInteng: panel lines <count>". No lines (or none to read) gives the count line alone, with 0.
	std::vector<std::string> HarnessSelect_PanelLines(const PaintText *lines, size_t count);
}
