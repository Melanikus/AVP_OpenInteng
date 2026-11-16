// adjacent_list.h: the list the Adjacent Modules screen keeps of the CURRENT module's adjacent modules, and what Link > Add, Link > Delete and Base >
// Set Current do to it, EXACTLY as inteng does (the list is memory until the write-back, adjacent_write.h). Written
// 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (Adjacent Modules, Add / Delete / Set Current). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHERE EACH RULE COMES FROM (re-tools\projects\vasquez-inteng\out):
// - Add, command 0x32 or key N (FUN_00404a20, menu-reads-1-full.txt:7198-7217, 0x404b08-0x404ba7): needs a current module (DAT_005c3868 >= 0) and a
//   picked module (DAT_00606778 != 0). A picked module that is the current module changes nothing. One already in the list changes nothing (the
//   pick is kept). Otherwise it goes on the END of the list (FUN_00405e20, push_back: reads2-apone-4.txt:291-318), the pick is let go, and the
//   "changed" flag (DAT_0060677c) is set. Every case then repaints (FUN_004046b0).
// - Delete, command 0x34 or key D (:7219-7253): finds the picked module in the list, takes its node out, lowers the count, sets "changed", lets go
//   of the pick. NAMED DEVIATION: when the picked module is not in the list (or nothing is picked) inteng shows its "Tried to delete a
//   nonexistent ..." message (FUN_0046a260) and then lowers the count anyway (0x404c4c falls into 0x404c80), which leaves the count wrong; here
//   it is refused and nothing changes.
// - Set Current, command 100 or key S (:7255-7257, FUN_004025d0, menu-reads-2.txt:2-58): the old current module's list is written back first
//   (FUN_00404040), the list is emptied, and for a picked module with a record the list is loaded from that module's ADJMDLEP (FUN_004045b0:
//   reads2-apone-4.txt:3-49: for each entry, the module whose number it names, in the entry's order), and when FUN_004bd890(DAT_0060801c) gives
//   non-zero it becomes the current module; the pick is let go and "changed" is cleared. With no record the current module is none.
//
// THE WRITE-BACK (FUN_00404040) is adjacent_write.h's: the editor calls it when another module is made current, at Find Not Done and at Exit, and only when this list was changed. NOT BUILT, NAMED
// (AvP-docs/director/plans/drake-deviations.md): the condition FUN_004bd890(DAT_0060801c) != 0 at 0x402681 (here taken as true).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace host
{
	enum class AdjacentAdd
	{
		Added,
		NoCurrent,      // no current module: nothing happens (inteng: the whole case is skipped)
		NoPick,         // no module is picked
		IsCurrent,      // the picked module is the current one: nothing is added, the pick stays
		AlreadyThere,   // it is in the list: nothing is added, the pick stays
	};

	enum class AdjacentDelete
	{
		Deleted,
		NotThere,       // not in the list (or nothing is picked): inteng's message and a wrong count; here refused
	};

	class AdjacentList
	{
	public:
		AdjacentList();

		// The current module's object index, or -1 for none (DAT_005c3868)
		int32_t Current() const;

		// The adjacent modules, by object index, in the order they were put on (FUN_00405e20 pushes at the end)
		std::vector<int32_t> const &Members() const;

		// Whether the list was changed since it was loaded (DAT_0060677c)
		bool Changed() const;

		// FUN_004025d0: the list is emptied and, for module (an object index, or -1 for no module record), loaded from entries (the object numbers
		// of its ADJMDLEP entries, in order; a number that names no module is left out by the caller, as FUN_004045b0 matches entries to modules).
		// The module becomes the current one, the changed flag is cleared. With module -1 there is no current module.
		void SetCurrent(int32_t module, std::vector<int32_t> const &entries);

		// The list was put into the level (adjacent_write.h): it is no longer "changed"
		void MarkWritten();

		// The screen is left (FUN_004049d0): no current module, an empty list
		void Clear();

		// Link > Add: see the header. picked is the module's object index, or -1 for none.
		AdjacentAdd Add(int32_t picked);

		// Link > Delete: see the header.
		AdjacentDelete Delete(int32_t picked);

	private:
		int32_t current;
		std::vector<int32_t> members;
		bool changed;
	};
}
