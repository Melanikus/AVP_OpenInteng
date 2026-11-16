// The test for src/ui's checkbox and button. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdio.h>

#include <string>

#include "small_widgets.h"

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

static void TestBitBox()
{
	puts("The checkbox:");

	BitBox box;

	Check(!box.Loaded() && box.Handle(KeyEvent(Key::Space)) == SmallAnswer::Refused && box.Click() == SmallAnswer::Refused,
		"a box with no value refuses every press, so it can never change a word");
	Check(!box.Load(0x12345678u, 0) && !box.Loaded(), "a box of no bit is refused");
	Check(!box.Load(0x12345678u, 0x3) && !box.Loaded() && box.Word() == 0, "and a box of two bits, so it can never change more than one");
	Check(!box.Load(0x12345678u, 0xFFFFFFFFu), "and every bit");
	Check(box.Load(0x12345678u, 0x8u) && !box.Load(0x12345678u, 0x6u) && !box.Loaded() && box.Word() == 0, "a refused load after a good one leaves no word behind that could be saved");

	// A word from a level, with bits the editor has no name for, the top one among them
	uint32_t const stock = 0x80000102u;

	Check(box.Load(stock, 0x4u) && !box.Checked() && box.Word() == stock, "a box loads one bit of a level's word, and the word is as the level has it");
	Check(box.Handle(KeyEvent(Key::Space)) == SmallAnswer::Changed && box.Checked() && box.Word() == (stock | 0x4u), "Space turns its bit on, and no other bit changes");
	Check(box.Click() == SmallAnswer::Changed && !box.Checked() && box.Word() == stock, "a click turns it off again, and the word is the level's own once more");
	Check(box.Handle(LetterEvent(' ')) == SmallAnswer::Changed && box.Word() == (stock | 0x4u), "a typed space is Space");
	Check(box.Handle(KeyEvent(Key::Enter)) == SmallAnswer::Nothing && box.Handle(LetterEvent('x')) == SmallAnswer::Nothing && box.Handle(LetterEvent(' ', true)) == SmallAnswer::Nothing &&
		box.Word() == (stock | 0x4u), "every other key means nothing to it");

	Check(box.Load(stock, 0x80000000u) && box.Checked() && box.Click() == SmallAnswer::Changed && box.Word() == 0x00000102u, "the top bit is a bit like any other");

	box.SetEnabled(false);
	Check(box.Click() == SmallAnswer::Refused && box.Handle(KeyEvent(Key::Space)) == SmallAnswer::Refused && box.Word() == 0x00000102u, "a disabled box refuses every press");

	bool onlyItsBit = true;

	for (int bit = 0; bit < 32; bit++)
	{
		BitBox each;
		uint32_t const word = 0xA5A5A5A5u;
		uint32_t const mask = 1u << bit;

		onlyItsBit = onlyItsBit && each.Load(word, mask) && each.Click() == SmallAnswer::Changed && (each.Word() ^ word) == mask;
	}

	Check(onlyItsBit, "for each of the 32 bits, a press changes that bit and leaves the other 31 as they were");
}

static void TestButton()
{
	puts("The button:");

	Button button;

	Check(button.Handle(KeyEvent(Key::Enter)) == SmallAnswer::Pressed && button.Handle(KeyEvent(Key::Space)) == SmallAnswer::Pressed && button.Click() == SmallAnswer::Pressed,
		"Enter, Space and a click press it");
	Check(button.Handle(KeyEvent(Key::Tab)) == SmallAnswer::Nothing && button.Handle(LetterEvent('a')) == SmallAnswer::Nothing, "and nothing else does");

	button.SetEnabled(false);
	Check(button.Click() == SmallAnswer::Refused && button.Handle(KeyEvent(Key::Enter)) == SmallAnswer::Refused, "a disabled button refuses every press");
}

int main()
{
	TestBitBox();
	TestButton();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
