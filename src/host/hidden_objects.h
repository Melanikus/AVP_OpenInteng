// hidden_objects.h: the editor's set of HIDDEN objects (Module Linking's Object > Hide, Un-hide last, Un-hide all), as plain functions. Written 2026-11-15 by Melanikus (Frost)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (FUN_004c1a90, menu-reads-1-full.txt:8362-8396): hidden is bit 4 at +0x5c of the object's record in inteng's own memory, a VIEW flag: nothing is
// written to the level.
//   HIDE (0x96, key H; 0x4c2159-0x4c2208): with some modules picked (the screen's picked list, DAT_00704324 > 0), bit 4 is SET on every picked object, walking the list
//     from its first to its last, and "the last hidden" (DAT_00704370) is the last object of the list. With nothing picked, nothing happens.
//   UN-HIDE ALL (0x98, key U; 0x4c2208-0x4c225a): bit 4 is cleared on EVERY object of the level, and "the last hidden" is 0.
//   UN-HIDE LAST (0x9a, key L; 0x4c225a-0x4c227f): when "the last hidden" is not 0, bit 4 is cleared on that one object. "The last hidden" is NOT reset (a second press clears
//     the same object again, which changes nothing).
//   The screen's start (FUN_004c3a00, 0x4c3b08) sets "the last hidden" to 0 and leaves the bits. Toggle View Distance (0x49), when it turns OFF, clears bit 4 on every
//     object (the same as Un-hide all, except the last hidden is not reset): HiddenObjects_ShowAll(false).
//
// OURS: objects are named by their 8-byte ID (selection.h) as everywhere in the editor, so a hide survives the level being rebuilt by an edit. Two objects with the same ID
// (the ID comes from the name) are hidden together (named deviation: inteng's flag sits on the object itself). The set holds at most kHiddenObjectsMost IDs (a level
// has far fewer objects); a Hide that would pass it hides what fits. The view asks HiddenObjects_Is(id) for each object it draws or picks (the hook is Dietrich's).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace host
{
	size_t const kHiddenIdBytes = 8;
	size_t const kHiddenObjectsMost = 100000;

	// Hides the objects with these IDs (ids = count IDs of kHiddenIdBytes each, in the picked list's order); the last of them becomes "the last hidden". count 0 changes
	// nothing. Gives how many were newly hidden.
	size_t HiddenObjects_Hide(uint8_t const *ids, size_t count);

	// Shows "the last hidden" again (it stays "the last hidden"). False when there is none.
	bool HiddenObjects_UnhideLast();

	// Shows every object; "the last hidden" is forgotten when forgetLast (Un-hide all: yes; Toggle View Distance turning off: no).
	void HiddenObjects_ShowAll(bool forgetLast);

	// The screen's start: "the last hidden" is forgotten, the hidden objects stay hidden
	void HiddenObjects_ForgetLast();

	// Is the object with this ID hidden? (null id: no)
	bool HiddenObjects_Is(uint8_t const *id);

	size_t HiddenObjects_Count();

	// The ID of the hidden object at that place (0 first, in ID order); false for a place past the end
	bool HiddenObjects_At(size_t index, uint8_t *id);

	// The ID of "the last hidden", into id (kHiddenIdBytes); false when there is none
	bool HiddenObjects_Last(uint8_t *id);

	// Counts every change to the set (Hide, Un-hide, Reset): the view asks whether it has to be handed over again
	unsigned HiddenObjects_Version();

	// A new level was opened: nothing is hidden and there is no last
	void HiddenObjects_Reset();
}
