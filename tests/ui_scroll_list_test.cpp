// The test for src/ui's scroll list. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>

#include <string>

#include "scroll_list.h"

using namespace ui;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// The chosen row is a real one and shows, and the first row shown leaves no empty rows at the end
static bool InPlace(const ScrollList &list)
{
	if (list.Count() == 0)
	{
		return list.Chosen() == -1 && list.First() == 0;
	}

	int64_t const latestFirst = list.Count() > list.RowsShown() ? (int64_t)list.Count() - list.RowsShown() : 0;

	return list.Chosen() >= 0 && list.Chosen() < list.Count() && list.First() <= list.Chosen() && (int64_t)list.Chosen() < (int64_t)list.First() + list.RowsShown() &&
		list.First() >= 0 && list.First() <= latestFirst;
}

static void TestEmpty()
{
	puts("No rows:");

	ScrollList list;

	Check(list.Chosen() == -1 && list.Handle(KeyEvent(Key::Down)) == ListAnswer::Nothing && !list.Click(0), "with no rows, none is chosen, and nothing moves");

	list.SetRows(-5, 0);
	Check(list.Count() == 0 && list.RowsShown() == 1 && InPlace(list), "a count below 0 is none, and fewer than one row shown is one");
}

static void TestMoving()
{
	puts("Moving:");

	ScrollList list;

	list.SetRows(100, 10);
	Check(list.Chosen() == 0 && list.First() == 0, "the first row is chosen when rows come");
	Check(list.Handle(KeyEvent(Key::Up)) == ListAnswer::Nothing && list.Chosen() == 0, "Up at the first row stays");
	Check(list.Handle(KeyEvent(Key::PageDown)) == ListAnswer::Moved && list.Chosen() == 10 && list.First() == 1 && InPlace(list), "Page Down moves a screenful, and the list follows");
	Check(list.Handle(KeyEvent(Key::End)) == ListAnswer::Moved && list.Chosen() == 99 && list.First() == 90 && InPlace(list), "End goes to the last row, with no empty rows after it");
	Check(list.Handle(KeyEvent(Key::Down)) == ListAnswer::Nothing && list.Chosen() == 99, "Down at the last row stays");
	Check(list.Handle(KeyEvent(Key::Home)) == ListAnswer::Moved && list.Chosen() == 0 && list.First() == 0, "Home goes to the first");
	Check(list.Click(42) && list.Chosen() == 42 && InPlace(list), "a click chooses its row, and it shows");
	Check(!list.Click(-1) && !list.Click(100) && !list.Click(2147483647) && list.Chosen() == 42, "a click past either end chooses nothing");
	Check(list.Handle(KeyEvent(Key::Letter)) == ListAnswer::Nothing, "and other keys mean nothing to it");
}

static void TestChangingRows()
{
	puts("Rows coming and going:");

	ScrollList list;

	list.SetRows(50, 10);
	list.Click(40);
	list.SetRows(20, 10);
	Check(list.Chosen() == 19 && list.First() == 10 && InPlace(list), "when the chosen row has gone, the last row is chosen");
	list.SetRows(0, 10);
	Check(list.Chosen() == -1 && InPlace(list), "with every row gone, none is chosen");
	list.SetRows(3, 10);
	Check(list.Chosen() == 0 && list.First() == 0 && InPlace(list), "rows that come back start chosen at the first");

	list.SetRows(2147483647, 2147483647);
	list.Handle(KeyEvent(Key::End));
	list.Handle(KeyEvent(Key::PageDown));
	Check(list.Chosen() == 2147483646 && InPlace(list), "the largest count and screenful can't wrap a sum round");
	list.Handle(KeyEvent(Key::PageUp));
	Check(list.Chosen() == 0 && InPlace(list), "and a screenful back from the end stops at the first row");
}

static void TestAlwaysInPlace()
{
	puts("Every change keeps the list in place:");

	ScrollList list;
	unsigned int seed = 4242;
	bool inPlace = true;
	Key const keys[] = { Key::Up, Key::Down, Key::PageUp, Key::PageDown, Key::Home, Key::End };

	for (int i = 0; i < 50000; i++)
	{
		seed = seed * 1103515245u + 12345u;

		switch ((seed >> 16) % 4)
		{
			case 0: list.SetRows((int32_t)((seed >> 4) % 300) - 20, (int32_t)((seed >> 12) % 40) - 5); break;
			case 1: list.Click((int32_t)((seed >> 6) % 400) - 50); break;
			default: list.Handle(KeyEvent(keys[(seed >> 8) % 6])); break;
		}

		inPlace = inPlace && InPlace(list);
	}

	Check(inPlace, "after 50,000 changes of count, screenful, clicks and keys, the chosen row was always real and in sight");
}

int main()
{
	TestEmpty();
	TestMoving();
	TestChangingRows();
	TestAlwaysInPlace();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
