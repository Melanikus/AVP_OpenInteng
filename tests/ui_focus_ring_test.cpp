// The test for src/ui's focus ring: which widget has keyboard focus, and the order Tab moves it in. Written 2026-11-14 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdio.h>

#include <string>

#include "focus_ring.h"

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

// Focus is always on a widget that can take it, or on none only when none can
static bool Settled(const FocusRing &ring)
{
	bool anyCan = false;

	for (int i = 0; i < ring.Count(); i++)
	{
		anyCan = anyCan || ring.CanTakeFocus(i);
	}

	return anyCan ? ring.CanTakeFocus(ring.Focused()) : ring.Focused() == -1;
}

static void TestOrder()
{
	puts("The order:");

	FocusRing ring;

	Check(ring.Focused() == -1 && !ring.Next() && !ring.Previous(), "an empty ring has no focus, and Tab moves nothing");

	int const a = ring.Add(true);
	int const b = ring.Add(true);
	int const c = ring.Add(true);

	Check(a == 0 && b == 1 && c == 2 && ring.Focused() == a, "widgets take places in the order they're added, and the first that can takes focus");
	Check(ring.Next() && ring.Focused() == b && ring.Next() && ring.Focused() == c, "Tab goes forward");
	Check(ring.Next() && ring.Focused() == a, "and from the last back to the first");
	Check(ring.Previous() && ring.Focused() == c && ring.Previous() && ring.Focused() == b, "Shift+Tab goes back, and round from the first to the last");

	FocusRing one;

	one.Add(true);
	Check(!one.Next() && !one.Previous() && one.Focused() == 0, "with one widget that can take focus, Tab keeps it there and says it didn't move");
}

static void TestPassingOver()
{
	puts("Hidden and disabled widgets:");

	FocusRing ring;

	ring.Add(true);
	int const hidden = ring.Add(false);
	int const third = ring.Add(true);

	Check(ring.Next() && ring.Focused() == third, "Tab passes over a widget that can't take focus");
	Check(ring.Previous() && ring.Focused() == 0, "and so does Shift+Tab");
	Check(!ring.FocusOn(hidden) && ring.Focused() == 0, "a click on a widget that can't take focus gives it none");
	Check(ring.FocusOn(third) && ring.Focused() == third, "a click on one that can gives it focus");

	ring.SetCanTakeFocus(third, false);
	Check(ring.Focused() == 0 && Settled(ring), "when the one with focus is hidden, focus moves on to the next that can take it");

	ring.SetCanTakeFocus(0, false);
	Check(ring.Focused() == -1 && Settled(ring), "when none can, none has it");
	Check(!ring.Next() && !ring.Previous() && ring.Focused() == -1, "and Tab moves nothing");

	ring.SetCanTakeFocus(third, true);
	Check(ring.Focused() == third, "when one can again, it takes focus");

	ring.SetCanTakeFocus(-1, true);
	ring.SetCanTakeFocus(3, true);
	ring.SetCanTakeFocus(1000000, true);
	Check(ring.Count() == 3 && ring.Focused() == third && !ring.CanTakeFocus(3), "a place that isn't in the ring is left alone");
	Check(!ring.FocusOn(-1) && !ring.FocusOn(3) && !ring.FocusOn(2147483647) && ring.Focused() == third, "and a click on one gives focus to nothing");
}

static void TestFull()
{
	puts("A full ring:");

	FocusRing ring;
	bool allAdded = true;

	for (size_t i = 0; i < kFocusMostWidgets; i++)
	{
		allAdded = allAdded && ring.Add(i % 3 == 0) == (int)i;
	}

	Check(allAdded && ring.Count() == (int)kFocusMostWidgets, "it takes kFocusMostWidgets widgets");
	Check(ring.Add(true) == -1 && ring.Count() == (int)kFocusMostWidgets, "and refuses one more, so nothing is written past its end");

	bool visitedAll = true;
	int visits = 0;

	for (int i = 0; i < (int)kFocusMostWidgets; i++)
	{
		ring.Next();
		visitedAll = visitedAll && ring.Focused() % 3 == 0;
		visits += ring.Focused() == 0 ? 1 : 0;
	}

	Check(visitedAll && visits > 0, "Tab round a full ring visits only the widgets that can take focus, and comes back to the start");
}

// A long run of changes chosen by a fixed sum, so every run is the same. After each one, focus is settled.
static void TestAlwaysSettled()
{
	puts("Focus is always settled:");

	FocusRing ring;
	unsigned int seed = 777;
	bool settled = true;

	for (int i = 0; i < 20; i++)
	{
		ring.Add((i & 1) != 0);
	}

	for (int i = 0; i < 50000; i++)
	{
		seed = seed * 1103515245u + 12345u;

		int const place = (int)((seed >> 8) % 24) - 2;

		switch ((seed >> 16) % 4)
		{
			case 0: ring.SetCanTakeFocus(place, ((seed >> 4) & 1) != 0); break;
			case 1: ring.Next(); break;
			case 2: ring.Previous(); break;
			default: ring.FocusOn(place); break;
		}

		settled = settled && Settled(ring);
	}

	Check(settled, "after 50,000 changes, places outside the ring among them, focus was always on a widget that can take it, or on none when none could");
}

int main()
{
	TestOrder();
	TestPassingOver();
	TestFull();
	TestAlwaysSettled();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
