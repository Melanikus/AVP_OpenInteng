// The test for src/ui's enum dropdown. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "choice_box.h"

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

static std::string ShownOf(const ChoiceBox &box)
{
	char shown[64];

	box.Shown(shown, sizeof(shown));
	return shown;
}

// A light's type, as the field spec gives it
static Choice const kLightTypes[] = { { 0, "standard" }, { 1, "strobe" }, { 2, "flicker" } };

static void TestLoading()
{
	puts("Loading:");

	ChoiceBox box;

	Check(!box.Loaded() && box.Handle(KeyEvent(Key::Enter)) == ChoiceAnswer::Refused, "a box with no value refuses every event, so it can never change one");
	Check(!box.Load(0, 3, 0) && !box.Load(kLightTypes, 0, 0), "no list, or an empty one, is refused");

	std::vector<Choice> tooMany(kChoiceMost + 1, Choice());

	for (size_t i = 0; i < tooMany.size(); i++)
	{
		tooMany[i].value = (int32_t)i;
		tooMany[i].words = "w";
	}

	Check(!box.Load(tooMany.data(), tooMany.size(), 0) && box.Load(tooMany.data(), kChoiceMost, 0), "a list longer than kChoiceMost is refused, and one exactly that long isn't");

	Choice const noWords[] = { { 0, "a" }, { 1, 0 } };
	Choice const twice[] = { { 0, "a" }, { 5, "b" }, { 5, "c" } };

	Check(!box.Load(noWords, 2, 0) && !box.Loaded(), "a choice with no words is refused");
	Check(!box.Load(twice, 3, 5) && !box.Loaded(), "and two choices with the same value, as a pick between them would be ambiguous");

	Check(box.Load(kLightTypes, 3, 2) && box.Chosen() == 2 && box.Value() == 2 && ShownOf(box) == "flicker" && !box.Open(), "a value in the list shows its words");
}

static void TestUnknownValue()
{
	puts("A value the list doesn't have:");

	ChoiceBox box;

	box.Load(kLightTypes, 3, 7);
	Check(box.Chosen() == -1 && box.Value() == 7 && ShownOf(box) == "unknown (7)", "it shows as unknown, with its number");

	box.Load(kLightTypes, 3, -2147483647 - 1);
	Check(ShownOf(box) == "unknown (-2147483648)" && box.Value() == -2147483647 - 1, "even the most negative number fits, and is kept");

	box.Load(kLightTypes, 3, 7);
	box.Handle(KeyEvent(Key::Enter));
	box.Handle(KeyEvent(Key::Down));
	Check(box.Handle(KeyEvent(Key::Escape)) == ChoiceAnswer::Closed && box.Value() == 7, "opened, moved through and closed with Esc, it's still the level's own value");
	box.Handle(KeyEvent(Key::Enter));
	Check(box.Leave() == ChoiceAnswer::Closed && box.Value() == 7 && !box.Open(), "and leaving closes the list, with the value kept");

	char small[5];

	memset(small, 'X', sizeof(small));
	box.Shown(small, sizeof(small));
	Check(small[4] == 0 && strcmp(small, "unkn") == 0, "what's shown is cut to its room, and always ends with a zero");

	char untouched = 'X';

	box.Shown(&untouched, 0);
	Check(untouched == 'X', "and with no room, nothing is written");
}

static void TestPicking()
{
	puts("Opening, moving and picking:");

	ChoiceBox box;

	box.Load(kLightTypes, 3, 0);
	Check(box.Handle(KeyEvent(Key::Down)) == ChoiceAnswer::Nothing && box.Value() == 0, "while it's closed, Down changes nothing, so moving through the panel can't change a value");
	Check(box.Handle(KeyEvent(Key::Enter)) == ChoiceAnswer::Opened && box.Open() && box.Lit() == 0, "Enter opens it, lit on the value");
	Check(box.Handle(KeyEvent(Key::Up)) == ChoiceAnswer::Nothing && box.Lit() == 0, "Up at the first choice stays there");
	Check(box.Handle(KeyEvent(Key::End)) == ChoiceAnswer::Moved && box.Lit() == 2 && box.Handle(KeyEvent(Key::Down)) == ChoiceAnswer::Nothing && box.Lit() == 2,
		"End lights the last, and Down there stays");
	Check(box.Handle(KeyEvent(Key::PageUp)) == ChoiceAnswer::Moved && box.Lit() == 0, "Page Up stops at the first");
	Check(box.Handle(KeyEvent(Key::PageDown)) == ChoiceAnswer::Moved && box.Lit() == 2, "and Page Down at the last");
	Check(box.Value() == 0, "moving the light changes no value");
	Check(box.Handle(KeyEvent(Key::Space)) == ChoiceAnswer::Picked && box.Value() == 2 && box.Chosen() == 2 && !box.Open(), "Space picks the lit choice, and closes the list");

	box.Load(kLightTypes, 3, 9);
	box.Handle(KeyEvent(Key::Space));
	Check(box.Lit() == 0, "an unknown value opens lit on the first choice");
	Check(box.Handle(KeyEvent(Key::Enter)) == ChoiceAnswer::Picked && box.Value() == 0 && ShownOf(box) == "standard", "and a pick replaces it");

	// A long seeded run: the lit choice is always a real one, and the value is only ever the level's or a pick
	unsigned int seed = 99;
	bool inPlace = true;
	Key const keys[] = { Key::Up, Key::Down, Key::Home, Key::End, Key::PageUp, Key::PageDown, Key::Enter, Key::Escape, Key::Space, Key::Tab };

	box.Load(kLightTypes, 3, 42);

	for (int i = 0; i < 20000; i++)
	{
		seed = seed * 1103515245u + 12345u;
		box.Handle(KeyEvent(keys[(seed >> 16) % 10]));
		inPlace = inPlace && box.Lit() >= 0 && box.Lit() < 3 && (box.Chosen() == -1 ? box.Value() == 42 : box.Value() == kLightTypes[box.Chosen()].value);
	}

	Check(inPlace, "after 20,000 keys, the lit choice was always real, and the value was always the level's own or one picked");
}

int main()
{
	TestLoading();
	TestUnknownValue();
	TestPicking();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
