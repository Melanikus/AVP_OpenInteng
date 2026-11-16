// The test for src/ui's number box. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It drives the box with the same events the window will send, and reads no file. The ranges are real fields' ranges from the field
// spec: a coordinate (the loader's limit, -16777216 to 16777216), a light's range (up to 268435456) and a time in 16.16 seconds (up
// to 32767.99998). Each check's words say what's refused and why. The program gives back a non-zero result if any check failed.

#include <locale.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>
#include <vector>

#include "number_box.h"
#include "real_numbers.h"

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

static std::string Written(double value, unsigned decimals)
{
	char out[64];

	WriteNumber(value, decimals, out, sizeof(out));
	return out;
}

static std::string ShownOf(const NumberBox &box)
{
	char out[64];

	box.Shown(out, sizeof(out));
	return out;
}

// Replaces whatever is typed with letters, as Ctrl+A and typing does
static void TypeOver(NumberBox &box, const char *letters)
{
	box.Handle(LetterEvent('a', true));
	box.Handle(KeyEvent(Key::Delete));

	for (size_t i = 0; letters[i]; i++)
	{
		box.Handle(LetterEvent((unsigned char)letters[i]));
	}
}

static NumberRule const kCoordinate = NumberRuleFor(-16777216.0, 16777216.0, 0);
static NumberRule const kLightRange = NumberRuleFor(65.0, 268435456.0, 0);
static NumberRule const kSeconds = NumberRuleFor(0.0, 32767.99998, 5);

static void TestRules()
{
	puts("Working out a field's rule:");

	Check(kCoordinate.good && kCoordinate.mayBeNegative && kCoordinate.longest == 8 && kCoordinate.decimals == 0,
		"a coordinate takes a minus, and 8 digits after it (\"-16777216\")");
	Check(kLightRange.good && !kLightRange.mayBeNegative && kLightRange.longest == 9, "a light's range takes no minus, and 9 digits (\"268435456\")");
	Check(kSeconds.good && kSeconds.longest == 11 && kSeconds.decimals == 5, "16.16 seconds take 11 letters (\"32767.99998\")");
	Check(NumberRuleFor(0.0, 1.0, 0).longest == 1, "a field from 0 to 1 takes one digit");

	double const nan = std::numeric_limits<double>::quiet_NaN();
	double const inf = std::numeric_limits<double>::infinity();

	Check(!NumberRuleFor(nan, 1.0, 0).good && !NumberRuleFor(0.0, inf, 0).good, "a range with an end that isn't a number can't be a field's");
	Check(!NumberRuleFor(5.0, 1.0, 0).good, "and nor can one whose lowest is above its highest");
	Check(!NumberRuleFor(0.0, 1.0, kNumberMostDecimals + 1).good, "or one with more decimals than kNumberMostDecimals");
	Check(!NumberRuleFor(0.0, 1e15, 0).good && !NumberRuleFor(0.0, 1e10, 5).good && NumberRuleFor(0.0, 1e14, 0).good,
		"or one so large, with its decimals, that a double couldn't count every step of it exactly");
}

static void TestWriting()
{
	puts("Writing a number out:");

	Check(Written(0.0, 0) == "0" && Written(-5.0, 0) == "-5" && Written(16777216.0, 0) == "16777216" && Written(-16777216.0, 0) == "-16777216",
		"whole numbers are written as they are");
	Check(Written(0.5, 1) == "0.5" && Written(32767.99998, 5) == "32767.99998" && Written(1.0, 2) == "1.00", "decimals are written with exactly the field's number of them");
	Check(Written(-0.0, 0) == "0" && Written(-0.0001, 2) == "0.00", "a negative zero is 0, with no minus");
	Check(Written(std::numeric_limits<double>::quiet_NaN(), 0) == "not a number", "a float that isn't a number says so");
	Check(Written(1e300, 0) == "too large" && Written(std::numeric_limits<double>::infinity(), 0) == "too large" && Written(1e14, 2) == "too large",
		"and one too large to write out exactly says that, rather than a wrong number");

	char small[4];

	memset(small, 'X', sizeof(small));
	WriteNumber(123456.0, 0, small, sizeof(small));
	Check(small[3] == 0 && strcmp(small, "123") == 0, "what's written is cut to its room, and always ends with a zero");

	char untouched = 'X';

	WriteNumber(1.0, 0, &untouched, 0);
	Check(untouched == 'X', "and with no room, nothing is written");

	// Brazil's language writes 0,5. With the C library set to it, printf would write a comma, and this still writes a dot.
	const char *const set = setlocale(LC_ALL, "Portuguese_Brazil.1252");
	char viaPrintf[16];

	snprintf(viaPrintf, sizeof(viaPrintf), "%.1f", 0.5);

	bool const languageTookEffect = set && strcmp(viaPrintf, "0,5") == 0;

	Check(Written(0.5, 1) == "0.5" && Written(32767.99998, 5) == "32767.99998",
		std::string("with the C library set to Brazilian Portuguese, a number is still written with a dot") +
		(languageTookEffect ? " (printf wrote \"0,5\" there)" : " (this machine had no such language to set, so printf couldn't be compared)"));

	NumberBox box;

	box.Load(0.0, kSeconds);
	TypeOver(box, "0.5");
	Check(box.Handle(KeyEvent(Key::Enter)) == NumberAnswer::Kept && box.Value() == 0.5, "and \"0.5\" is still read as a half, by the game's reader, which knows no language");
	setlocale(LC_ALL, "C");
}

static void TestTyping()
{
	puts("Typing:");

	NumberBox box;

	Check(!box.Load(1.0, NumberRuleFor(5.0, 1.0, 0)) && !box.Loaded() && box.Handle(LetterEvent('1')) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::NotLoaded,
		"a box given a rule that isn't good has no value, and refuses every event");

	box.Load(100.0, kLightRange);
	Check(ShownOf(box) == "100" && !box.Editing(), "a value shows as it is");
	Check(box.Handle(LetterEvent(',')) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::UseADot && ShownOf(box) == "100",
		"a comma is refused with \"use a dot: 0.5\": the dot is the decimal mark, whatever the computer's language");
	Check(box.Handle(LetterEvent('x')) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::NotANumberLetter, "a letter is refused: it's never part of a number");
	Check(box.Handle(LetterEvent('-')) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::NotANumberLetter, "and a minus, where the range doesn't go below 0");
	Check(box.Handle(LetterEvent(7)) == NumberAnswer::Refused && box.Handle(LetterEvent(200)) == NumberAnswer::Refused, "and control characters and bytes 128-255, as in every text field");

	TypeOver(box, "268435456");
	Check(ShownOf(box) == "268435456" && box.Handle(LetterEvent('0')) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::TooLong,
		"the text can be as long as the field's numbers, and no longer");

	box.Load(0.0, kCoordinate);
	TypeOver(box, "-16777216");
	Check(ShownOf(box) == "-16777216" && box.Handle(LetterEvent('1')) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::TooLong,
		"a minus has its own place, so a coordinate takes \"-16777216\" and nothing more");
}

static void TestKeeping()
{
	puts("Keeping a number:");

	NumberBox box;

	box.Load(0.0, kCoordinate);
	TypeOver(box, "-16777216");
	Check(box.Handle(KeyEvent(Key::Enter)) == NumberAnswer::Kept && box.Value() == -16777216.0 && ShownOf(box) == "-16777216" && !box.Editing(), "a number inside the range is kept");

	TypeOver(box, "-1677721");
	box.Handle(KeyEvent(Key::End));
	box.Handle(LetterEvent('7'));
	Check(box.Handle(KeyEvent(Key::Enter)) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::OutOfRange && box.Value() == -16777216.0 && box.Editing(),
		"one past the range is refused, the value stays, and the edit goes on so it can be put right");

	TypeOver(box, "1.5");
	Check(box.Handle(KeyEvent(Key::Enter)) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::TooManyDecimals, "a dot in a whole number is refused");

	const char *const notNumbers[][2] =
	{
		{ "", "nothing at all" },
		{ "-", "a minus alone" },
		{ "1-2", "a minus that isn't in front" },
		{ "1..2", "two dots" },
		{ "--1", "two minuses" },
	};

	for (size_t i = 0; i < sizeof(notNumbers) / sizeof(notNumbers[0]); i++)
	{
		TypeOver(box, notNumbers[i][0]);
		Check(box.Handle(KeyEvent(Key::Enter)) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::NotANumber && box.Value() == -16777216.0,
			std::string("[") + notNumbers[i][0] + "] is refused by the game's reader: " + notNumbers[i][1]);
	}

	TypeOver(box, "-0");
	Check(box.Handle(KeyEvent(Key::Enter)) == NumberAnswer::Kept && box.Value() == 0.0 && !signbit(box.Value()) && ShownOf(box) == "0", "\"-0\" is kept as 0");

	box.Load(1.0, kSeconds);
	TypeOver(box, "32767.99998");
	Check(box.Handle(KeyEvent(Key::Enter)) == NumberAnswer::Kept && fabs(box.Value() - 32767.99998) < 0.0000001 && ShownOf(box) == "32767.99998",
		"16.16 seconds take 11 letters, the longest the field needs");
	TypeOver(box, "1.123456");
	Check(box.Handle(KeyEvent(Key::Enter)) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::TooManyDecimals, "more decimals than the field has are refused");
	TypeOver(box, "32768");
	Check(box.Handle(KeyEvent(Key::Enter)) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::OutOfRange, "and so is a time past its highest");

	Check(box.Handle(KeyEvent(Key::Escape)) == NumberAnswer::GaveUp && fabs(box.Value() - 32767.99998) < 0.0000001 && ShownOf(box) == "32767.99998",
		"Esc gives the edit up, and shows the value kept");

	TypeOver(box, "99999");
	Check(box.Leave() == NumberAnswer::GaveUp && box.Refusal() == NumberRefusal::OutOfRange && !box.Editing() && fabs(box.Value() - 32767.99998) < 0.0000001,
		"leaving with a number that can't be kept gives it up, says why, and keeps the value");
	TypeOver(box, "12.5");
	Check(box.Leave() == NumberAnswer::Kept && box.Value() == 12.5 && ShownOf(box) == "12.50000", "and leaving with one that can be kept keeps it");
	Check(box.Leave() == NumberAnswer::Nothing, "leaving with no edit does nothing");
}

static void TestLevelValues()
{
	puts("Values from a level that the field wouldn't take:");

	NumberBox box;

	box.Load(5000.0, NumberRuleFor(0.0, 1000.0, 0));
	Check(box.OutOfRangeNow() && box.Value() == 5000.0 && ShownOf(box) == "5000", "a value outside the range is kept and shown as it is, and the panel is told");
	TypeOver(box, "10");
	box.Handle(KeyEvent(Key::Enter));
	Check(box.Value() == 10.0 && !box.OutOfRangeNow(), "until the mapper types one that's kept");

	box.Load(1e12, NumberRuleFor(0.0, 100.0, 0));
	Check(ShownOf(box) == "1000000000000" && box.Value() == 1e12, "a value too long for the field's text shows in full");
	box.Handle(LetterEvent('7'));
	Check(ShownOf(box) == "7", "and an edit of it starts empty, as it can't be typed");
	Check(box.Handle(KeyEvent(Key::Escape)) == NumberAnswer::GaveUp && box.Value() == 1e12, "and Esc keeps the level's own value");

	double const nan = std::numeric_limits<double>::quiet_NaN();

	box.Load(nan, NumberRuleFor(0.0, 100.0, 2));
	Check(ShownOf(box) == "not a number" && box.OutOfRangeNow() && IsNaNOrEndless(box.Value()), "a float that isn't a number is kept, and shown as \"not a number\"");
	box.Handle(KeyEvent(Key::End));
	Check(box.Handle(KeyEvent(Key::Escape)) == NumberAnswer::GaveUp && IsNaNOrEndless(box.Value()), "and Esc keeps it");
}

// A double made from a level's eight bytes. The bits pass through a volatile, so the compiler can't know them and fold a check away,
// just as it can't know a level's.
static double FromLevelBits(uint64_t bits)
{
	volatile uint64_t fromTheLevel = bits;
	uint64_t const read = fromTheLevel;
	double value;

	memcpy(&value, &read, sizeof(value));
	return value;
}

static void TestNotRealFromALevel()
{
	puts("NaN and endless values from a level, with the box built /fp:fast:");

	struct BitsCase
	{
		uint64_t bits;
		const char *shown;
		const char *what;
	};

	BitsCase const cases[] = {
		{ 0x7FF8000000000000ull, "not a number", "the usual NaN" },
		{ 0xFFF8000000000000ull, "not a number", "a NaN with its sign bit set" },
		{ 0x7FF0000000000001ull, "not a number", "a NaN with only its lowest fraction bit set" },
		{ 0xFFFFFFFFFFFFFFFFull, "not a number", "a NaN with every bit set" },
		{ 0x7FF0000000000000ull, "too large", "endless" },
		{ 0xFFF0000000000000ull, "too large", "endless below 0" },
	};

	for (BitsCase const &each : cases)
	{
		double const value = FromLevelBits(each.bits);
		NumberBox box;

		box.Load(value, kCoordinate);
		Check(Written(value, 2) == each.shown && ShownOf(box) == each.shown && box.OutOfRangeNow() && IsNaNOrEndless(box.Value()),
			std::string(each.what) + " is written and shown as \"" + each.shown + "\", kept as it is, and out of the range");
	}

	Check(!NumberRuleFor(FromLevelBits(0x7FF8000000000000ull), 1.0, 0).good && !NumberRuleFor(0.0, FromLevelBits(0x7FF0000000000000ull), 0).good &&
		!NumberRuleFor(FromLevelBits(0xFFF0000000000000ull), 0.0, 0).good, "a range with a NaN or endless end can't be a field's");
	Check(Written(FromLevelBits(0x7FEFFFFFFFFFFFFFull), 0) == "too large", "the largest real double is too large, not NaN");
	Check(Written(FromLevelBits(0x0000000000000001ull), 2) == "0.00" && Written(FromLevelBits(0x8000000000000001ull), 2) == "0.00",
		"and the smallest ones, either side of 0, are 0");
}

static void TestPasting()
{
	puts("Pasting:");

	NumberBox box;

	box.Load(0.0, kSeconds);
	box.Handle(LetterEvent('a', true));

	unsigned char const comma[] = { '0', ',', '5' };
	unsigned char const half[] = { '0', '.', '5' };
	unsigned char const twoLines[] = { '1', '\n', '2' };
	unsigned char const letters[] = { '1', 'e', '5' };

	Check(box.Paste(comma, sizeof(comma)) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::UseADot, "a paste with a comma is refused with \"use a dot: 0.5\"");
	Check(box.Paste(half, sizeof(half)) == NumberAnswer::Changed && ShownOf(box) == "0.5", "a number pasted goes in");
	box.Handle(LetterEvent('a', true));
	Check(box.Paste(twoLines, sizeof(twoLines)) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::BadPaste, "a paste of two lines is refused whole");
	Check(box.Paste(letters, sizeof(letters)) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::BadPaste, "and so is one with a letter");

	std::vector<unsigned char> farTooLong(100000, '1');

	Check(box.Paste(farTooLong.data(), farTooLong.size()) == NumberAnswer::Refused && box.Refusal() == NumberRefusal::BadPaste, "and one far too long, without reading it");
}

// A long run of events, hostile ones among them, chosen by a fixed sum so every run is the same. The value is always the level's own or
// one inside the range, and what's shown always ends inside its room.
static void TestAlwaysInRange()
{
	puts("Every event keeps the value the level's or inside the range:");

	NumberBox box;
	NumberRule const rule = NumberRuleFor(-500.0, 500.0, 2);
	unsigned int seed = 31337;
	bool inRange = true;
	bool shownEnds = true;
	double const fromLevel = 9999.0;
	Key const keys[] = { Key::Letter, Key::Letter, Key::Letter, Key::Left, Key::Right, Key::Home, Key::End, Key::Backspace, Key::Delete, Key::Enter, Key::Escape };
	char const typed[] = "0123456789.-,x";

	box.Load(fromLevel, rule);

	for (int i = 0; i < 30000; i++)
	{
		seed = seed * 1103515245u + 12345u;

		unsigned int const choice = (seed >> 16) % 13;

		if (choice < 11)
		{
			Event event = KeyEvent(keys[choice], ((seed >> 8) & 1) != 0);

			event.letter = (unsigned char)typed[(seed >> 4) % (sizeof(typed) - 1)];
			box.Handle(event);
		}
		else if (choice == 11)
		{
			unsigned char paste[8];
			size_t const length = (seed >> 3) % sizeof(paste);

			for (size_t j = 0; j < length; j++)
			{
				paste[j] = (unsigned char)typed[((seed >> (j % 20)) + j) % (sizeof(typed) - 1)];
			}

			box.Paste(paste, length);
		}
		else
		{
			box.Leave();
		}

		char shown[12];

		memset(shown, 'X', sizeof(shown));
		box.Shown(shown, sizeof(shown));
		inRange = inRange && (box.Value() == fromLevel || (box.Value() >= -500.0 && box.Value() <= 500.0));
		shownEnds = shownEnds && memchr(shown, 0, sizeof(shown)) != 0;
	}

	Check(inRange, "after 30,000 events, commas, letters and pastes among them, the value was always the level's own or inside the range");
	Check(shownEnds, "and what's shown always ended with a zero inside its room");
}

int main()
{
	TestRules();
	TestWriting();
	TestTyping();
	TestKeeping();
	TestLevelValues();
	TestNotRealFromALevel();
	TestPasting();
	TestAlwaysInRange();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
