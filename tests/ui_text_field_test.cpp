// The test for src/ui's short text field. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// It drives the field's model with the same events the window will send, and reads no file. Text from a level is hostile (the coding
// standard's rule 1), so every rule the field keeps is tried with hostile text: empty, exactly its capacity and one byte more, far too
// long, with no terminating zero, with control characters and bytes from 128 to 255, and with %n and %s. Each check's words say what's
// refused and why. The program gives back a non-zero result if any check failed.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "text_field.h"
#include "chunk_object_name.hpp"   // the game's own limit for an object's name, so the test uses the field spec's real number

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

// An object's name holds OBJECT_NAME_BYTES bytes with its zero, so 63 letters
static size_t const kObjectNameLetters = OBJECT_NAME_BYTES - 1;

static bool Loads(TextField &field, const char *text, size_t capacity, LetterRule rule = 0)
{
	return field.Load((const unsigned char *)text, strlen(text), capacity, rule);
}

static std::string ShownOf(const TextField &field)
{
	char shown[kTextMostBytes + 1];

	field.Shown(shown, sizeof(shown));
	return shown;
}

static std::string ValueOf(const TextField &field)
{
	return std::string((const char *)field.Value(), field.ValueLength());
}

static void Type(TextField &field, const char *letters)
{
	for (size_t i = 0; letters[i]; i++)
	{
		field.Handle(LetterEvent((unsigned char)letters[i]));
	}
}

// The cursor, the selection and the length are always where they can be
static bool InPlace(const TextField &field)
{
	return field.Length() <= field.Capacity() && field.Cursor() <= field.Length() && field.SelectionStart() <= field.SelectionEnd() &&
		field.SelectionEnd() <= field.Length() && field.ValueLength() <= field.Capacity();
}

// A rule for a field that takes no double quote and no backslash
static bool NoQuoteNoBackslash(unsigned char letter)
{
	return letter != '"' && letter != '\\';
}

// A rule for a field that takes no question mark
static bool NoQuestionMark(unsigned char letter)
{
	return letter != '?';
}

static void TestLoading()
{
	puts("Loading a value from a level:");

	TextField field;

	Check(!field.Loaded() && field.Handle(LetterEvent('a')) == TextAnswer::Refused && field.Refusal() == TextRefusal::NotLoaded,
		"a field with no value refuses every event, so it can never be saved over");

	Check(Loads(field, "Door01", kObjectNameLetters) && ValueOf(field) == "Door01" && ShownOf(field) == "Door01" && !field.Editing(),
		"a plain name loads as it is, and isn't being edited");
	Check(field.Capacity() == 63, "an object name's capacity is the game's own: 63 letters (OBJECT_NAME_BYTES 64 with its zero)");

	Check(!Loads(field, "x", 0), "a capacity of 0 is refused: there would be nothing to edit");
	Check(!Loads(field, "x", kTextMostBytes + 1), "a capacity above kTextMostBytes is refused, as the field's storage is no bigger");
	Check(!field.Loaded() && field.ValueLength() == 0, "and a refused load leaves no value behind");

	std::string const exactly(kObjectNameLetters, 'n');
	std::string const oneMore(kObjectNameLetters + 1, 'n');

	Check(Loads(field, exactly.c_str(), kObjectNameLetters) && field.ValueLength() == kObjectNameLetters, "a name exactly its capacity loads");
	Check(!Loads(field, oneMore.c_str(), kObjectNameLetters) && !field.Loaded(), "a name one byte over its capacity is refused, never cut");

	std::string const farTooLong(100000, 'n');

	Check(!Loads(field, farTooLong.c_str(), kObjectNameLetters) && !field.Loaded(), "a name far too long is refused too");

	// No terminating zero: the field is given 6 bytes with no zero among them, in storage that ends right after them, and reads
	// exactly those 6
	std::vector<unsigned char> noZero(6, 'z');

	Check(field.Load(noZero.data(), noZero.size(), kObjectNameLetters, 0) && ValueOf(field) == "zzzzzz",
		"a name with no terminating zero is read to its length and no further");

	unsigned char const withZero[] = { 'a', 'b', 0, 'c', 'd' };

	Check(field.Load(withZero, sizeof(withZero), kObjectNameLetters, 0) && ValueOf(field) == "ab", "a name ends at its first zero byte");
	Check(!field.Load(0, 5, kObjectNameLetters, 0), "no bytes at all, with a length, is refused");
	Check(field.Load(0, 0, kObjectNameLetters, 0) && field.ValueLength() == 0 && ShownOf(field) == "", "an empty name loads, and shows as nothing");
}

static void TestShowing()
{
	puts("Showing a level's text:");

	TextField field;
	unsigned char const hostile[] = { 'A', 1, 9, 27, 127, 128, 200, 255, 'Z' };

	Check(field.Load(hostile, sizeof(hostile), kObjectNameLetters, 0), "a name with control characters and bytes 128-255 loads");
	Check(ShownOf(field) == "A???????Z", "each byte the font has no letter for shows as a question mark");
	Check(field.ValueLength() == sizeof(hostile) && memcmp(field.Value(), hostile, sizeof(hostile)) == 0, "and the value keeps the level's own bytes");

	Check(Loads(field, "%n%s%x%%", kObjectNameLetters) && ShownOf(field) == "%n%s%x%%", "%n and %s are shown as the letters they are, never used as a format");

	char small[4];

	memset(small, 'X', sizeof(small));
	Loads(field, "Generator", kObjectNameLetters);
	field.Shown(small, sizeof(small));
	Check(small[3] == 0 && strcmp(small, "Gen") == 0, "what's shown is cut to the room it's given, and always ends with a zero");

	char untouched = 'X';

	field.Shown(&untouched, 0);
	Check(untouched == 'X', "and with no room at all, nothing is written");
}

static void TestTyping()
{
	puts("Typing:");

	TextField field;

	Loads(field, "", kObjectNameLetters);
	Type(field, "Lift 2");
	Check(field.Editing() && ShownOf(field) == "Lift 2" && ValueOf(field) == "", "letters go into the edit, and the value is untouched until it's kept");
	Check(field.Handle(KeyEvent(Key::Enter)) == TextAnswer::Kept && ValueOf(field) == "Lift 2" && !field.Editing(), "Enter keeps the edit as the value");

	bool controlRefused = true;

	for (int letter = 0; letter < 32; letter++)
	{
		controlRefused = controlRefused && field.Handle(LetterEvent((unsigned char)letter)) == TextAnswer::Refused && field.Refusal() == TextRefusal::NotAFontLetter;
	}

	controlRefused = controlRefused && field.Handle(LetterEvent(127)) == TextAnswer::Refused;
	Check(controlRefused && ShownOf(field) == "Lift 2", "every control character (0-31 and 127) is refused: the font has no letter for it");

	bool highRefused = true;

	for (int letter = 128; letter < 256; letter++)
	{
		highRefused = highRefused && field.Handle(LetterEvent((unsigned char)letter)) == TextAnswer::Refused && field.Refusal() == TextRefusal::NotAFontLetter;
	}

	Check(highRefused && ShownOf(field) == "Lift 2", "and so is every byte from 128 to 255");

	Loads(field, "", kObjectNameLetters, NoQuoteNoBackslash);
	Check(field.Handle(LetterEvent('"')) == TextAnswer::Refused && field.Refusal() == TextRefusal::NotAllowedHere && field.Length() == 0,
		"a font letter this field's rule doesn't take is refused: the rule comes from the field spec");
	Check(field.Handle(LetterEvent('\\')) == TextAnswer::Refused && field.Handle(LetterEvent('q')) == TextAnswer::Changed, "and the field takes every other letter");

	std::string const full(kObjectNameLetters, 'f');

	Loads(field, full.c_str(), kObjectNameLetters);
	Check(field.Handle(LetterEvent('g')) == TextAnswer::Refused && field.Refusal() == TextRefusal::Full && ShownOf(field) == full,
		"at its capacity one more letter is refused, and nothing already there is cut to make room");
	field.Handle(KeyEvent(Key::Left, true));
	Check(field.Handle(LetterEvent('g')) == TextAnswer::Changed && field.Length() == kObjectNameLetters && ShownOf(field) == full.substr(0, kObjectNameLetters - 1) + "g",
		"with a letter selected, a letter typed takes its place even when the field is full");

	Loads(field, "abc", kObjectNameLetters);
	Check(field.Handle(LetterEvent('x', true)) == TextAnswer::Nothing && ShownOf(field) == "abc", "a letter typed with Ctrl is a shortcut, never a letter in the text");
	Check(field.Handle(LetterEvent('a', true)) == TextAnswer::Moved && field.SelectionStart() == 0 && field.SelectionEnd() == 3, "Ctrl+A selects everything");
	Check(field.Handle(KeyEvent(Key::Space)) == TextAnswer::Changed && ShownOf(field) == " ", "and a space takes the selection's place, as a letter does");
}

static void TestEditing()
{
	puts("Moving, selecting and deleting:");

	TextField field;

	Loads(field, "abcdef", kObjectNameLetters);
	field.Handle(KeyEvent(Key::Home));
	Check(field.Editing() && field.Cursor() == 0, "Home starts an edit, with the cursor at the start");
	field.Handle(KeyEvent(Key::Left));
	Check(field.Cursor() == 0, "Left at the start stays there");
	field.Handle(KeyEvent(Key::End));
	field.Handle(KeyEvent(Key::Right));
	Check(field.Cursor() == 6, "Right at the end stays there");
	field.Handle(KeyEvent(Key::Left, true));
	field.Handle(KeyEvent(Key::Left, true));
	Check(field.SelectionStart() == 4 && field.SelectionEnd() == 6, "Shift with Left selects");
	field.Handle(KeyEvent(Key::Left));
	Check(field.Cursor() == 4 && field.SelectionStart() == field.SelectionEnd(), "Left with a selection goes to its start, and ends the selection");
	field.Handle(KeyEvent(Key::Home, true));
	field.Handle(KeyEvent(Key::Right));
	Check(field.Cursor() == 4, "Right with a selection goes to its end");

	field.Handle(KeyEvent(Key::Backspace));
	Check(ShownOf(field) == "abcef" && field.Cursor() == 3, "Backspace deletes the letter before the cursor");
	field.Handle(KeyEvent(Key::Delete));
	Check(ShownOf(field) == "abcf" && field.Cursor() == 3, "Delete deletes the letter after it");
	field.Handle(KeyEvent(Key::End));
	Check(field.Handle(KeyEvent(Key::Delete)) == TextAnswer::Nothing && ShownOf(field) == "abcf", "Delete at the end deletes nothing");
	field.Handle(KeyEvent(Key::Home));
	Check(field.Handle(KeyEvent(Key::Backspace)) == TextAnswer::Nothing && ShownOf(field) == "abcf", "Backspace at the start deletes nothing");
	field.Handle(KeyEvent(Key::End, true));
	field.Handle(KeyEvent(Key::Backspace));
	Check(ShownOf(field) == "" && field.Cursor() == 0, "Backspace with a selection deletes the selection");

	Check(field.Handle(KeyEvent(Key::Escape)) == TextAnswer::GaveUp && !field.Editing() && ValueOf(field) == "abcdef", "Esc gives the edit up, and the value is as it was");
	Check(field.Handle(KeyEvent(Key::Escape)) == TextAnswer::Nothing && field.Handle(KeyEvent(Key::Enter)) == TextAnswer::Nothing, "Esc and Enter with no edit mean nothing");
	Check(field.Handle(KeyEvent(Key::Tab)) == TextAnswer::Nothing && field.Handle(KeyEvent(Key::Up)) == TextAnswer::Nothing && !field.Editing(),
		"Tab and the list keys belong to the focus ring and the lists, and start no edit");
}

static void TestPasting()
{
	puts("Pasting:");

	TextField field;

	Loads(field, "Ab", 8);
	field.Handle(KeyEvent(Key::End));

	unsigned char const fits[] = { 'c', 'd', 'e' };

	Check(field.Paste(fits, sizeof(fits)) == TextAnswer::Changed && ShownOf(field) == "Abcde" && field.Cursor() == 5, "a paste that fits goes in at the cursor");

	unsigned char const tooLong[] = { '1', '2', '3', '4' };

	Check(field.Paste(tooLong, sizeof(tooLong)) == TextAnswer::Refused && field.Refusal() == TextRefusal::PasteTooLong && ShownOf(field) == "Abcde",
		"a paste that won't fit is refused whole: none of it goes in, and nothing is cut");

	std::vector<unsigned char> farTooLong(1000000, 'a');

	Check(field.Paste(farTooLong.data(), farTooLong.size()) == TextAnswer::Refused && field.Refusal() == TextRefusal::PasteTooLong, "and so is a paste far too long");

	unsigned char const twoLines[] = { 'x', '\n', 'y' };
	unsigned char const carriage[] = { 'x', '\r' };

	Check(field.Paste(twoLines, sizeof(twoLines)) == TextAnswer::Refused && field.Refusal() == TextRefusal::PasteHasALineBreak && ShownOf(field) == "Abcde",
		"a paste of more than one line is refused whole, never cut at the break (\"paste one line\")");
	Check(field.Paste(carriage, sizeof(carriage)) == TextAnswer::Refused && field.Refusal() == TextRefusal::PasteHasALineBreak, "a carriage return is a line break too");

	unsigned char const control[] = { 'x', 7 };
	unsigned char const high[] = { 'x', 233 };

	Check(field.Paste(control, sizeof(control)) == TextAnswer::Refused && field.Refusal() == TextRefusal::PasteHasANonLetter && ShownOf(field) == "Abcde",
		"a paste with a control character is refused whole, by the same filter as typing");
	Check(field.Paste(high, sizeof(high)) == TextAnswer::Refused && field.Refusal() == TextRefusal::PasteHasANonLetter, "and so is one with a byte from 128 to 255");

	Loads(field, "", 8, NoQuoteNoBackslash);

	unsigned char const quote[] = { 'a', '"' };

	Check(field.Paste(quote, sizeof(quote)) == TextAnswer::Refused && field.Refusal() == TextRefusal::PasteHasANonLetter && field.Length() == 0,
		"and one with a letter the field's rule doesn't take");

	Loads(field, "12345678", 8);
	field.Handle(LetterEvent('a', true));

	unsigned char const replace[] = { 'n', 'e', 'w' };

	Check(field.Paste(replace, sizeof(replace)) == TextAnswer::Changed && ShownOf(field) == "new", "a paste takes the selection's place, so a full field takes one over a selection");

	unsigned char copied[8];

	field.Handle(KeyEvent(Key::Home, true));
	Check(field.CopySelection(copied, sizeof(copied)) == 3 && memcmp(copied, "new", 3) == 0, "Copy gives the selected letters");
	Check(field.CopySelection(copied, 2) == 2, "and never more than the room it's given");
}

// Apone's case for rule 6: a level's name holding bytes the editor can't show stays byte for byte until the mapper changes the field
static void TestLettersItCantType()
{
	puts("A level's name with letters the editor can't type:");

	TextField field;
	unsigned char const stock[] = { 'C', 'a', 'f', 233, ' ', 'L', 'i', 'g', 'h', 't' };

	field.Load(stock, sizeof(stock), kObjectNameLetters, 0);

	std::string const shown = ShownOf(field);

	field.Handle(KeyEvent(Key::Tab));
	field.Handle(KeyEvent(Key::Down));
	Check(shown == "Caf? Light" && !field.ShowsCantTypeNotice() && field.ValueLength() == sizeof(stock) && memcmp(field.Value(), stock, sizeof(stock)) == 0,
		"shown, and passed through by focus, its bytes are as the level has them, with no notice");

	field.Handle(KeyEvent(Key::End));
	Check(field.ShowsCantTypeNotice(), "the moment an edit starts, the panel says the name has letters the editor can't type, and that the edit replaces them");
	field.Handle(KeyEvent(Key::Backspace));
	Check(field.Handle(KeyEvent(Key::Escape)) == TextAnswer::GaveUp && !field.ShowsCantTypeNotice() && field.ValueLength() == sizeof(stock) &&
		memcmp(field.Value(), stock, sizeof(stock)) == 0, "Esc gives the level's own bytes back, every one");

	field.Handle(KeyEvent(Key::End));
	field.Handle(KeyEvent(Key::Enter));
	Check(ValueOf(field) == "Caf? Light" && !field.ShowsCantTypeNotice(), "an edit kept replaces them: where a letter couldn't be typed, it's a question mark");

	field.Load(stock, sizeof(stock), kObjectNameLetters, NoQuestionMark);
	field.Handle(KeyEvent(Key::End));
	Check(field.Handle(KeyEvent(Key::Enter)) == TextAnswer::Refused && field.Refusal() == TextRefusal::NotKeptHasABadLetter && field.Editing(),
		"in a field that takes no question mark, an edit still holding one isn't kept, and the edit goes on");
	Check(field.Leave() == TextAnswer::GaveUp && !field.Editing() && field.Refusal() == TextRefusal::NotKeptHasABadLetter && memcmp(field.Value(), stock, sizeof(stock)) == 0,
		"and leaving gives it up, so the field is never left half edited, says why, and the level's own bytes stay");

	field.Load(stock, sizeof(stock), kObjectNameLetters, 0);
	Type(field, "x");
	Check(field.Leave() == TextAnswer::Kept && ValueOf(field) == "Caf? Lightx", "leaving keeps an edit that can be kept");
	Check(field.Leave() == TextAnswer::Nothing, "and leaving with no edit does nothing");
}

// A long run of events, hostile ones among them, chosen by a fixed sum so every run is the same. After each one the cursor, the selection
// and the length are where they can be, and what's shown ends inside its room.
static void TestEveryEventKeepsItInPlace()
{
	puts("Every event keeps the cursor and the selection in place:");

	TextField field;
	unsigned int seed = 12345;
	bool inPlace = true;
	bool shownEnds = true;
	Key const keys[] = { Key::Letter, Key::Left, Key::Right, Key::Home, Key::End, Key::Backspace, Key::Delete, Key::Enter, Key::Escape, Key::Space, Key::Tab };

	field.Load((const unsigned char *)"start", 5, 12, 0);

	for (int i = 0; i < 20000; i++)
	{
		seed = seed * 1103515245u + 12345u;

		unsigned int const choice = (seed >> 16) % 14;

		if (choice < 11)
		{
			Event event = KeyEvent(keys[choice], ((seed >> 8) & 1) != 0, ((seed >> 9) & 15) == 0);

			event.letter = (unsigned char)(seed >> 20);
			field.Handle(event);
		}
		else if (choice < 13)
		{
			unsigned char paste[20];
			size_t const length = (seed >> 4) % sizeof(paste);

			for (size_t j = 0; j < length; j++)
			{
				paste[j] = (unsigned char)((seed >> (j % 24)) * 31 + j);
			}

			field.Paste(paste, length);
		}
		else
		{
			field.Leave();
		}

		char shown[13 + 1];

		memset(shown, 'X', sizeof(shown));
		field.Shown(shown, sizeof(shown));
		inPlace = inPlace && InPlace(field);
		shownEnds = shownEnds && memchr(shown, 0, sizeof(shown)) != 0;
	}

	Check(inPlace, "after 20,000 events, hostile letters and pastes among them, the cursor, the selection and the length were always where they can be");
	Check(shownEnds, "and what's shown always ended with a zero inside its room");
}

int main()
{
	TestLoading();
	TestShowing();
	TestTyping();
	TestEditing();
	TestPasting();
	TestLettersItCantType();
	TestEveryEventKeepsItInPlace();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
