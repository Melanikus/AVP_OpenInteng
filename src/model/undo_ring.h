// undo_ring.h: inteng's UNDO RING (the class of FUN_0057e5c0, vtable 0x5afeb4), the store every undo of the Texture Map keeps its steps in: the polygon edits
// (DAT_00914ca8, 25000 records of 0x38 bytes), the selection (DAT_00914c10, 100000 records of 0xc bytes), and the others built on it (TM-A, ruling 5: undo as
// deep as inteng's; texture-map-closeout-frost.md "RULING 5 READ"). Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// HOW IT WORKS (inteng's, every number its own). The ring is `count` SLOTS of RecordInts() whole numbers each (the record's size in bytes rounded up to whole
// 4-byte words). A slot whose first word is -1 is empty; -2 starts an undo step; anything else is a record, which the caller writes (its first word must be
// neither -1 nor -2: inteng's records start with a shape number). A step is its -2 slot and the records after it. NewRecord gives the place for the next record
// of the step being made, first starting a new step when one was asked for (BeginStep), when undone steps are waiting to be redone (they are dropped), or when the
// ring holds no step. The slot after the newest record is always empty. When the ring is full, the OLDEST step is dropped, except while it holds fewer than three
// steps: then it DOUBLES (FUN_0057eb00). Undo walks back from the newest record to its step's -2, handing each record to the caller (which swaps it with what the
// level holds, so the same call redoes it), then the caller's "after"; Redo walks forward the same way. While either runs, NewRecord gives nothing.
//
// NAMED DIFFERENCE: inteng doubles without a limit (the byte count, count x 2 x RecordInts() x 4, unchecked). Here the ring never grows past `mostInts` whole
// numbers (kMostUndoRingInts unless given): past it, the oldest step is dropped when there are at least two, else the record isn't kept (NewRecord gives null:
// the undo of that one step then puts back only what was recorded before). A count below 2 is taken as 2, a record size below 4 bytes as 4.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <vector>

namespace model
{
	int64_t const kMostUndoRingInts = (int64_t)1 << 26;   // 256 MB of whole numbers: past the edits ring's 350,000 doubled seven times

	class UndoRing
	{
	public:
		// FUN_0057e5c0 (count, record size in bytes): no slots yet; Reset makes them
		UndoRing(int32_t count, int32_t recordBytes, int64_t mostInts = kMostUndoRingInts);

		// FUN_0057e630: every step dropped, the slots made again (count of them, each empty)
		void Reset();

		// FUN_0057e810: Reset, when the ring has its slots
		void Clear();

		// FUN_0057e940: the next record starts a new step (nothing while an undo or redo runs, or with no slots)
		void BeginStep();

		// FUN_0057e820: the place of the next record, RecordInts() whole numbers; null with no slots, while an undo or redo runs, or past the limit
		int32_t *NewRecord();

		// FUN_0057e6d0: the newest step undone: each record from the newest back (each), then after. False when there is no step, no slots, or one is running
		bool Undo(std::function<void(int32_t *record)> const &each, std::function<void()> const &after);

		// FUN_0057e770: the newest undone step done again: each record in order, then after. False when there is none
		bool Redo(std::function<void(int32_t *record)> const &each, std::function<void()> const &after);

		int32_t Steps() const { return steps; }           // +4
		int32_t RedoSteps() const { return redoSteps; }   // +8
		int32_t Count() const { return count; }           // +0x18
		int32_t RecordInts() const { return recordInts; } // +0x1c
		bool HasSlots() const { return !slots.empty(); }  // +0x20 not null

	private:
		int64_t Last() const;              // the place of the last slot's first word: (count - 1) x RecordInts()
		int64_t Next(int64_t at) const;
		int64_t Prev(int64_t at) const;
		void StartStepIfAsked();           // FUN_0057e960
		void DropFrom(int64_t at);         // the step starting at `at` made empty; the tail moves to the step after it
		bool Grow(int64_t newCount);       // FUN_0057eb00

		int32_t count;
		int32_t recordInts;
		int64_t mostInts;
		std::vector<int32_t> slots;        // +0x20
		int64_t head = 0;                  // +0xc
		int64_t tail = 0;                  // +0x10
		int32_t steps = 0;                 // +4
		int32_t redoSteps = 0;             // +8
		bool replaying = false;            // +0x14
		bool emptied = false;              // +0x15
		bool stepAsked = false;            // +0x16
	};
}
