// The test for src/model/undo_ring.cpp: inteng's undo ring (FUN_0057e5c0 / e630 / e810 / e940 / e960 / e820 / e6d0 / e770 / eb00). Written 2026-11-18 by
// Melanikus (Frost) for AVP_OpenInteng (TM-A ruling 5, texture-map-closeout-frost.md "RULING 5 READ"). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// Every expected order and slot below is traced by hand through inteng's code:
//  1. the sizes: 0x38 bytes are 14 whole numbers a record, 0xc are 3 (FUN_0057e5c0 rounds up to 4-byte words); a step's records come back newest first on undo,
//     oldest first on redo; a new record after an undo drops the steps waiting to be redone;
//  2. full with three steps: the oldest is dropped (FUN_0057e820 0x57e88a);
//  3. full with fewer than three: the ring doubles (0x57e8a8), and when its tail isn't at the start the slots are turned round so it is (FUN_0057eb00), every
//     record kept in order;
//  4. while an undo runs no record is made;
//  5. the NAMED limit: past it the oldest step is dropped, or with one step the record isn't kept;
//  6. Clear, and a ring with no slots.

#include <stdio.h>

#include <string>
#include <vector>

#include "undo_ring.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::string List(std::vector<int32_t> const &values)
{
	std::string words;

	for (size_t i = 0; i < values.size(); i++)
	{
		words += (i ? " " : "") + std::to_string(values[i]);
	}

	return words.empty() ? "nothing" : words;
}

// A record whose first word is `value` (and the second value x 10 when there is room)
static bool Put(model::UndoRing &ring, int32_t value)
{
	int32_t *const record = ring.NewRecord();

	if (!record)
	{
		return false;
	}

	record[0] = value;

	if (ring.RecordInts() > 1)
	{
		record[1] = value * 10;
	}

	return true;
}

static std::vector<int32_t> UndoOnce(model::UndoRing &ring, bool *done = nullptr)
{
	std::vector<int32_t> seen;
	bool const ran = ring.Undo([&](int32_t *record) { seen.push_back(record[0]); }, [] {});

	if (done)
	{
		*done = ran;
	}

	return seen;
}

static std::vector<int32_t> RedoOnce(model::UndoRing &ring)
{
	std::vector<int32_t> seen;
	ring.Redo([&](int32_t *record) { seen.push_back(record[0]); }, [] {});
	return seen;
}

static void TheBasics()
{
	puts("1. sizes, undo, redo");

	Check(model::UndoRing(25000, 0x38).RecordInts() == 14 && model::UndoRing(100000, 0xc).RecordInts() == 3, "0x38 bytes are 14 whole numbers a record, 0xc are 3");
	Check(model::UndoRing(25000, 5).RecordInts() == 2 && model::UndoRing(1, 0).Count() == 2 && model::UndoRing(1, 0).RecordInts() == 1, "5 bytes are 2; a count below 2 is 2, a size below 4 is 4");

	model::UndoRing ring(4, 8);

	ring.Reset();
	ring.BeginStep();
	Put(ring, 10);
	Put(ring, 11);
	Check(ring.Steps() == 1 && ring.RedoSteps() == 0, "two records make one step");

	int32_t secondWord = 0;
	bool after = false;
	ring.Undo([&](int32_t *record) { if (record[0] == 10) secondWord = record[1]; }, [&] { after = true; });
	Check(secondWord == 100 && after, "the record comes back whole (its second word 100), and the after call runs");

	std::vector<int32_t> const redone = RedoOnce(ring);
	Check(redone == std::vector<int32_t>({ 10, 11 }) && ring.Steps() == 1 && ring.RedoSteps() == 0, "redo gives them oldest first: 10 11 (it gives " + List(redone) + ")");

	std::vector<int32_t> const undone = UndoOnce(ring);
	Check(undone == std::vector<int32_t>({ 11, 10 }) && ring.Steps() == 0 && ring.RedoSteps() == 1, "undo gives them newest first: 11 10 (it gives " + List(undone) + ")");

	ring.BeginStep();
	Put(ring, 12);
	Check(ring.RedoSteps() == 0 && ring.Steps() == 1, "a new record after the undo drops the step waiting to be redone");

	std::vector<int32_t> const last = UndoOnce(ring);
	Check(last == std::vector<int32_t>({ 12 }), "and its own step is just 12 (it is " + List(last) + ")");

	bool ran = true;
	UndoOnce(ring, &ran);
	Check(!ran, "with no step left, undo does nothing");
}

static void TheOldestDropped()
{
	puts("2. a full ring with three steps drops the oldest");

	model::UndoRing ring(6, 4);

	ring.Reset();

	for (int32_t value = 1; value <= 3; value++)
	{
		ring.BeginStep();
		Put(ring, value);
	}

	Check(ring.Steps() == 2 && ring.Count() == 6, "after three steps of one record in six slots: two steps, still six slots");

	std::vector<int32_t> const first = UndoOnce(ring);
	std::vector<int32_t> const second = UndoOnce(ring);
	bool ran = true;
	UndoOnce(ring, &ran);
	Check(first == std::vector<int32_t>({ 3 }) && second == std::vector<int32_t>({ 2 }) && !ran, "they undo as 3, then 2, then nothing: step 1 was dropped (" + List(first) + " / " + List(second) + ")");
}

static void TheDoubling()
{
	puts("3. a full ring with fewer than three steps doubles");

	{
		model::UndoRing ring(4, 4);

		ring.Reset();
		ring.BeginStep();
		Put(ring, 1);
		Put(ring, 2);
		Check(Put(ring, 3) && ring.Count() == 8, "a third record of one step in four slots: the ring is eight slots");

		std::vector<int32_t> const undone = UndoOnce(ring);
		Check(undone == std::vector<int32_t>({ 3, 2, 1 }), "and the step undoes as 3 2 1 (it is " + List(undone) + ")");
	}

	{
		// The tail at slot 2 after a drop (as in 2), then two more records of step 3: the second meets step 2's start, so the ring doubles, turned round from the tail
		model::UndoRing ring(6, 4);

		ring.Reset();

		for (int32_t value = 1; value <= 3; value++)
		{
			ring.BeginStep();
			Put(ring, value);
		}

		Put(ring, 4);
		Check(Put(ring, 5) && ring.Count() == 12 && ring.Steps() == 2, "a record that meets the oldest step with two held: the ring is twelve slots, two steps");

		std::vector<int32_t> const third = UndoOnce(ring);
		std::vector<int32_t> const second = UndoOnce(ring);
		Check(third == std::vector<int32_t>({ 5, 4, 3 }) && second == std::vector<int32_t>({ 2 }), "every record kept in order: 5 4 3, then 2 (" + List(third) + " / " + List(second) + ")");

		std::vector<int32_t> const again = RedoOnce(ring);
		std::vector<int32_t> const again2 = RedoOnce(ring);
		Check(again == std::vector<int32_t>({ 2 }) && again2 == std::vector<int32_t>({ 3, 4, 5 }), "and redo the same way: 2, then 3 4 5 (" + List(again) + " / " + List(again2) + ")");
	}
}

static void WhileUndoing()
{
	puts("4. no record while an undo runs");

	model::UndoRing ring(8, 4);

	ring.Reset();
	ring.BeginStep();
	Put(ring, 1);

	bool madeOne = true;
	ring.Undo([&](int32_t *) { ring.BeginStep(); madeOne = ring.NewRecord() != nullptr; }, [] {});
	Check(!madeOne && ring.Steps() == 0 && ring.RedoSteps() == 1, "NewRecord gives nothing inside the undo, and nothing is added");
}

static void TheLimit()
{
	puts("5. the NAMED limit");

	{
		model::UndoRing ring(4, 4, 4);

		ring.Reset();
		ring.BeginStep();
		Put(ring, 1);
		Put(ring, 2);
		Check(!Put(ring, 3) && ring.Count() == 4, "one step, full, at the limit: the record isn't kept, the ring stays four slots");

		std::vector<int32_t> const undone = UndoOnce(ring);
		Check(undone == std::vector<int32_t>({ 2, 1 }), "and the step undoes what it holds: 2 1 (it is " + List(undone) + ")");
	}

	{
		model::UndoRing ring(4, 4, 4);

		ring.Reset();
		ring.BeginStep();
		Put(ring, 1);
		ring.BeginStep();
		Check(Put(ring, 9) && ring.Steps() == 1 && ring.Count() == 4, "two steps, full, at the limit: the oldest is dropped instead of doubling");

		std::vector<int32_t> const undone = UndoOnce(ring);
		bool ran = true;
		UndoOnce(ring, &ran);
		Check(undone == std::vector<int32_t>({ 9 }) && !ran, "and only the new step undoes (" + List(undone) + ")");
	}

	{
		model::UndoRing ring(4, 4, 4);

		ring.Reset();
		ring.BeginStep();
		Put(ring, 1);
		Put(ring, 2);
		ring.BeginStep();
		Check(Put(ring, 7) && ring.Steps() == 1, "a new step's start that fills the ring at the limit: the oldest step is dropped");

		std::vector<int32_t> const undone = UndoOnce(ring);
		Check(undone == std::vector<int32_t>({ 7 }), "and the new step undoes as 7 (it is " + List(undone) + ")");
	}
}

static void ClearAndNoSlots()
{
	puts("6. Clear, and a ring with no slots");

	model::UndoRing none(8, 4);
	bool ran = true;

	Check(!none.HasSlots() && none.NewRecord() == nullptr, "before Reset there are no slots and no record");
	UndoOnce(none, &ran);
	Check(!ran, "and nothing to undo");

	model::UndoRing ring(8, 4);

	ring.Reset();
	ring.BeginStep();
	Put(ring, 1);
	ring.Clear();
	UndoOnce(ring, &ran);
	Check(ring.Steps() == 0 && ring.RedoSteps() == 0 && !ran, "Clear drops every step");
	Check(Put(ring, 2) && ring.Steps() == 1, "and the ring takes records again");
}

int main()
{
	TheBasics();
	TheOldestDropped();
	TheDoubling();
	WhileUndoing();
	TheLimit();
	ClearAndNoSlots();

	printf("undo_ring_test: %s, %d failed\n", checksFailed ? "FAILED" : "passed", checksFailed);
	return checksFailed ? 1 : 0;
}
