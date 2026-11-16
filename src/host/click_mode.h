// click_mode.h: "command, then click" (P4 of plans/sub-modes.md). Many of inteng's items do nothing by themselves: they say what the NEXT
// click in the view means (pick an object, a module, a polygon, a corner of one). This remembers the command and what it waits for, says so on
// the status line, and hands the click's answer back to whoever started it. Esc gives it up. Plain data, no window, so a test can drive it.
// Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (P4). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// How it is used:
//   1. An item's action calls ClickMode_Start(command, wants, words): the editor now waits.
//   2. The editor's next click in the view is picked (pick_game.h) and given to ClickMode_Click. It does NOT change the selection.
//   3. If the click found what was waited for, the answer says so, with the command and the report, and the editor stops waiting. If not,
//      it keeps waiting and the status line says what was wrong. Esc, or ClickMode_Cancel, stops it with nothing done.
// One command waits at a time: starting another replaces the one before.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "selection.h"

namespace host
{
	// What the click has to find
	enum class ClickWants
	{
		Nothing,   // not waiting
		Object,    // an object the level file names (a door, a switch, a placed object)
		Module,    // a module (a room, a part of the level's shape)
		Polygon,   // a polygon of a module or of an object's shape
		Corner,    // a polygon's corner: the one nearest where the click hit (P1's nearestCorner)
	};

	// The longest the words of a command can be, with their 0
	size_t const kClickModeMostCommandWords = 48;

	// The longest the status line's words for this can be, with their 0
	size_t const kClickModeMostWords = 160;

	// Starts waiting for a click that finds wants, for command (the number the caller gave to its action; it is only handed back) whose name
	// in the status line is commandWords. A wants of Nothing, or a command name that is null or empty, starts nothing and gives back false.
	// The words are copied, cut at kClickModeMostCommandWords - 1 letters, and every byte outside space to ~ is shown as "?".
	bool ClickMode_Start(int32_t command, ClickWants wants, const char *commandWords);

	// Stops waiting, with nothing done. Gives back true if it was waiting.
	bool ClickMode_Cancel();

	bool ClickMode_Waiting();

	// What was waited for now, or Nothing
	ClickWants ClickMode_Wants();

	// What the status line says while waiting ("Command - click a polygon (Esc cancels)"), or, right after a click that was the wrong kind,
	// what was wrong. Empty when not waiting. The words stay as they are until the next call that changes them.
	const char *ClickMode_Words();

	struct ClickAnswer
	{
		bool done;           // true: the click found what was waited for, and the editor has stopped waiting
		int32_t command;     // the number ClickMode_Start was given (when done)
		ClickWants wants;    // what was waited for (when done)
		PickReport report;   // what the click found (when done)
	};

	// A click's pick. When nothing is waiting it gives back done = false and changes nothing, so the caller then does what a click always does.
	// A click that found a different kind than the one waited for, or nothing at all, gives back done = false and keeps waiting, with
	// ClickMode_Words saying what was wrong.
	ClickAnswer ClickMode_Click(const PickReport &report);

	// Whether report holds what wants asks for, for a test and for ClickMode_Click: an object needs PickFound::Object, a module
	// PickFound::Module, a polygon a hit with a polygon number from 0 up on a module or an object (not on something the game made), a
	// corner a polygon that also has its nearest corner read.
	bool ClickMode_Satisfies(ClickWants wants, const PickReport &report);
}
