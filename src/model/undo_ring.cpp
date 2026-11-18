// undo_ring.cpp: inteng's undo ring (see undo_ring.h). Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include "undo_ring.h"

namespace model
{
	namespace
	{
		int32_t const kEmpty = -1;
		int32_t const kStepStart = -2;
	}

	UndoRing::UndoRing(int32_t countIn, int32_t recordBytes, int64_t mostIntsIn)
		: count(countIn < 2 ? 2 : countIn), recordInts(((recordBytes < 4 ? 4 : recordBytes) + 3) / 4), mostInts(mostIntsIn)
	{
		// FUN_0057e5c0: [6] = the count, [7] = (bytes + 3) / 4 (0x57e5df); the slots come with FUN_0057e630
	}

	int64_t UndoRing::Last() const
	{
		return (int64_t)(count - 1) * recordInts;
	}

	int64_t UndoRing::Next(int64_t at) const
	{
		return at < Last() ? at + recordInts : 0;
	}

	int64_t UndoRing::Prev(int64_t at) const
	{
		return at < 1 ? Last() : at - recordInts;
	}

	void UndoRing::Reset()
	{
		// FUN_0057e630: the old slots freed, every count and flag 0, count x [7] whole numbers with each slot's first word -1
		slots.clear();
		head = tail = 0;
		steps = redoSteps = 0;
		replaying = emptied = stepAsked = false;

		if ((int64_t)count * recordInts > mostInts)
		{
			return;   // NAMED: a ring bigger than the limit has no slots
		}

		slots.assign((size_t)((int64_t)count * recordInts), 0);

		for (int64_t at = 0; at < (int64_t)slots.size(); at += recordInts)
		{
			slots[(size_t)at] = kEmpty;
		}
	}

	void UndoRing::Clear()
	{
		// FUN_0057e810
		if (!slots.empty())
		{
			Reset();
		}
	}

	void UndoRing::BeginStep()
	{
		// FUN_0057e940: +0x16 = 1
		if (!slots.empty() && !replaying)
		{
			stepAsked = true;
		}
	}

	void UndoRing::DropFrom(int64_t at)
	{
		// 0x57e8c9-0x57e929 / 0x57ea22-0x57eaa1: the slot and the records after it made empty; the tail at the next step's start (or the empty slot)
		slots[(size_t)at] = kEmpty;

		int64_t next = Next(at);

		while (slots[(size_t)next] != kEmpty && slots[(size_t)next] != kStepStart)
		{
			slots[(size_t)next] = kEmpty;
			next = Next(next);
		}

		tail = next;
	}

	bool UndoRing::Grow(int64_t newCount)
	{
		// FUN_0057eb00: a new block of newCount slots (each first word -1), the old slots copied into it from the tail on, so the tail is at 0
		if (slots.empty() || newCount > INT32_MAX || newCount * recordInts > mostInts)
		{
			return false;   // NAMED: the limit (inteng has none)
		}

		int64_t const oldInts = (int64_t)count * recordInts;
		std::vector<int32_t> made((size_t)(newCount * recordInts), 0);

		for (int64_t at = 0; at < (int64_t)made.size(); at += recordInts)
		{
			made[(size_t)at] = kEmpty;
		}

		int64_t const fromTail = oldInts - tail;

		for (int64_t i = 0; i < fromTail; i++)
		{
			made[(size_t)i] = slots[(size_t)(tail + i)];
		}

		for (int64_t i = 0; i < tail; i++)
		{
			made[(size_t)(fromTail + i)] = slots[(size_t)i];
		}

		head -= tail;

		if (head < 0)
		{
			head += oldInts;
		}

		slots.swap(made);
		count = (int32_t)newCount;
		tail = 0;
		return true;
	}

	void UndoRing::StartStepIfAsked()
	{
		// FUN_0057e960
		if (slots.empty() || replaying || !(redoSteps > 0 || (steps == 0 && !emptied) || stepAsked))
		{
			return;
		}

		// the steps waiting to be redone are dropped, from the head on
		int64_t at = head;

		while (redoSteps != 0)
		{
			redoSteps--;
			slots[(size_t)at] = kEmpty;
			at = Next(at);

			while (slots[(size_t)at] != kEmpty && slots[(size_t)at] != kStepStart)
			{
				slots[(size_t)at] = kEmpty;
				at = Next(at);
			}
		}

		slots[(size_t)head] = kStepStart;
		head = Next(head);

		int32_t const before = steps;

		steps = before + 1;
		emptied = false;
		stepAsked = false;

		if (slots[(size_t)head] == kEmpty)
		{
			return;
		}

		if (steps < 3)
		{
			// 0x57eab2-0x57eaf2: the ring doubles
			head = Prev(head);

			if (Grow((int64_t)count * 2))
			{
				head = Next(head);
				return;
			}

			head = Next(head);   // NAMED: past the limit, as when it holds three: the oldest step dropped

			if (before == 0)
			{
				// only the new step's start, and the ring isn't empty: nothing to keep, so the ring starts again with it
				Reset();
				slots[0] = kStepStart;
				head = Next(0);
				steps = 1;
				return;
			}
		}

		// 0x57ea48-0x57eaab: the oldest step dropped
		steps = before;
		DropFrom(head);

		if (steps == 0)
		{
			emptied = true;
		}
	}

	int32_t *UndoRing::NewRecord()
	{
		// FUN_0057e820
		if (slots.empty())
		{
			return nullptr;
		}

		while (!replaying)
		{
			StartStepIfAsked();

			if (emptied)
			{
				return nullptr;
			}

			int64_t const at = head;

			head = Next(head);

			if (slots[(size_t)head] == kEmpty)
			{
				return &slots[(size_t)at];
			}

			if (steps > 2)
			{
				// 0x57e88a-0x57e939: the oldest step dropped
				steps--;
				DropFrom(head);

				if (steps == 0)
				{
					emptied = true;
					return nullptr;
				}

				return &slots[(size_t)at];
			}

			// 0x57e893-0x57e8c2: the ring doubles, and the record is tried again
			head = Prev(head);

			if (!Grow((int64_t)count * 2))
			{
				// NAMED: past the limit
				if (steps < 2)
				{
					return nullptr;
				}

				head = Next(head);
				steps--;
				DropFrom(head);
				return &slots[(size_t)at];
			}
		}

		return nullptr;
	}

	bool UndoRing::Undo(std::function<void(int32_t *record)> const &each, std::function<void()> const &after)
	{
		// FUN_0057e6d0
		if (slots.empty() || replaying || steps == 0)
		{
			return false;
		}

		replaying = true;
		head = Prev(head);

		while (slots[(size_t)head] != kEmpty && slots[(size_t)head] != kStepStart)
		{
			if (each)
			{
				each(&slots[(size_t)head]);
			}

			head = Prev(head);
		}

		redoSteps++;
		steps--;

		if (after)
		{
			after();
		}

		replaying = false;
		return true;
	}

	bool UndoRing::Redo(std::function<void(int32_t *record)> const &each, std::function<void()> const &after)
	{
		// FUN_0057e770
		if (slots.empty() || replaying || redoSteps == 0)
		{
			return false;
		}

		replaying = true;
		head = Next(head);

		while (slots[(size_t)head] != kEmpty && slots[(size_t)head] != kStepStart)
		{
			if (each)
			{
				each(&slots[(size_t)head]);
			}

			head = Next(head);
		}

		steps++;
		redoSteps--;

		if (after)
		{
			after();
		}

		replaying = false;
		return true;
	}
}
