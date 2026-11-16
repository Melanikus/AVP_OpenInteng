// The test for src/ui's field descriptors and the check of a list of them. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "field_descriptor.h"
#include "number_box.h"

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

// Stand-ins for the field spec's own checks, which the adapter gives in the editor: these take everything
static bool AnyNumber(const void *, double, const char **)
{
	return true;
}

static bool AnyText(const void *, const unsigned char *, size_t, const unsigned char *, size_t, const char **)
{
	return true;
}

static bool NoSpace(unsigned char letter)
{
	return letter != ' ';
}

static const Choice kLightKinds[] =
{
	{ 0, "standard" },
	{ 1, "flicker" },
	{ 2, "pulse" },
};

// A double made from its bits, as a table built from somewhere else could hold one. The bits pass through a volatile, so the compiler
// can't know them and fold a check away, which it may do under the editor's /fp:fast.
static double FromBits(uint64_t bits)
{
	volatile uint64_t fromOutside = bits;
	uint64_t const read = fromOutside;
	double value = 0.0;

	memcpy(&value, &read, sizeof(value));
	return value;
}

static FieldDescriptor NumberField(const char *name)
{
	FieldDescriptor field = EmptyFieldDescriptor();

	field.name = name;
	field.kind = FieldKind::Number;
	field.lowest = 0.0;
	field.highest = 100.0;
	return field;
}

static FieldDescriptor BitField(const char *name)
{
	FieldDescriptor field = EmptyFieldDescriptor();

	field.name = name;
	field.kind = FieldKind::Bit;
	field.bit = 0x4u;
	return field;
}

static FieldDescriptor ChoiceField(const char *name)
{
	FieldDescriptor field = EmptyFieldDescriptor();

	field.name = name;
	field.kind = FieldKind::Choice;
	field.choices = kLightKinds;
	field.choiceCount = sizeof(kLightKinds) / sizeof(kLightKinds[0]);
	return field;
}

static FieldDescriptor TextDescriptor(const char *name)
{
	FieldDescriptor field = EmptyFieldDescriptor();

	field.name = name;
	field.kind = FieldKind::Text;
	field.capacity = 63;
	return field;
}

// Whether a list of one is sound
static bool Sound(const FieldDescriptor &field)
{
	FieldListProblem problem = { 99, FieldListRefusal::BadName };

	return CheckFieldList(&field, 1, &problem) && problem.why == FieldListRefusal::None && problem.field == 0;
}

// Whether a list of one is refused, for that reason
static bool Refused(const FieldDescriptor &field, FieldListRefusal why)
{
	FieldListProblem problem = { 99, FieldListRefusal::None };

	return !CheckFieldList(&field, 1, &problem) && problem.why == why && problem.field == 0;
}

// Whether a list is refused at that field, for that reason
static bool RefusedAt(const FieldDescriptor *list, size_t count, size_t field, FieldListRefusal why)
{
	FieldListProblem problem = { 99, FieldListRefusal::None };

	return !CheckFieldList(list, count, &problem) && problem.why == why && problem.field == field;
}

static FieldDescriptor many[kFieldPanelMost + 1];

static void TestTheList()
{
	puts("A list as a whole:");

	FieldListProblem problem = { 99, FieldListRefusal::BadName };
	FieldDescriptor const one = NumberField("range");

	Check(CheckFieldList(0, 0, &problem) && problem.why == FieldListRefusal::None && problem.field == 0, "an empty list, with no list at all, is sound: a record with nothing to edit shows an empty panel");
	Check(CheckFieldList(&one, 0, &problem) && problem.why == FieldListRefusal::None, "and so is a list counted as empty");
	Check(CheckFieldList(&one, 1, 0), "a list can be checked with nowhere to say the problem");
	Check(!CheckFieldList(0, 3, 0), "and is still refused then");
	Check(RefusedAt(0, 3, 0, FieldListRefusal::NoList), "fields counted with no list are refused, never read");

	for (size_t i = 0; i < kFieldPanelMost + 1; i++)
	{
		many[i] = (i % 4 == 0) ? NumberField("a number") : (i % 4 == 1) ? BitField("a bit") : (i % 4 == 2) ? ChoiceField("a choice") : TextDescriptor("a name");
	}

	Check(CheckFieldList(many, kFieldPanelMost, &problem) && problem.why == FieldListRefusal::None && problem.field == 0,
		"512 fields, as many as the focus ring holds, of every kind, are sound, and a sound list names no field");
	Check(kFieldPanelMost == kFocusMostWidgets && kFieldPanelMost == 512, "the most a panel shows is the focus ring's own most, 512");
	Check(RefusedAt(many, kFieldPanelMost + 1, 0, FieldListRefusal::TooMany), "513 are refused as a whole");
	Check(RefusedAt(0, 10000, 0, FieldListRefusal::TooMany), "and 10,000 are refused by their count before the list is read: with no list at all, the answer is still too many");
	Check(RefusedAt(0, SIZE_MAX, 0, FieldListRefusal::TooMany), "and so is the largest count there is");

	many[5].name = "";
	many[9].name = 0;
	Check(RefusedAt(many, 20, 5, FieldListRefusal::BadName), "a list with two bad fields is refused at the first, by its place");
	many[5].name = "a bit";
	Check(RefusedAt(many, 20, 9, FieldListRefusal::BadName), "and at the next when that's put right");
	many[9].name = "a bit";
	many[511].name = "";
	Check(RefusedAt(many, kFieldPanelMost, 511, FieldListRefusal::BadName), "a bad field at the very end is found too");
	many[511].name = "a name";
	Check(CheckFieldList(many, kFieldPanelMost, &problem) && problem.field == 0, "and the list is sound again, naming no field");
}

static void TestWords()
{
	puts("Names and help:");

	char longest[kFieldNameMost + 2];
	memset(longest, 'a', sizeof(longest));
	longest[kFieldNameMost] = 0;

	FieldDescriptor field = NumberField(0);
	Check(Refused(field, FieldListRefusal::BadName), "a field with no name is refused");
	field.name = "";
	Check(Refused(field, FieldListRefusal::BadName), "and one with an empty name");
	field.name = longest;
	Check(Sound(field), "a name of 48 letters, the most, is sound");
	longest[kFieldNameMost] = 'a';
	longest[kFieldNameMost + 1] = 0;
	Check(Refused(field, FieldListRefusal::BadName), "one of 49 is refused");

	// No zero anywhere in the first 49 bytes: the measure stops one past the most, so it reads none of what comes after
	char noZero[kFieldNameMost + 1];
	memset(noZero, 'a', sizeof(noZero));
	field.name = noZero;
	Check(Refused(field, FieldListRefusal::BadName), "a name with no zero in its first 49 bytes is refused, measured only that far");

	field.name = "a\tname";
	Check(Refused(field, FieldListRefusal::BadName), "a name with a control letter is refused");
	field.name = "caf\xC3\xA9";
	Check(Refused(field, FieldListRefusal::BadName), "and one with bytes from 128 to 255");
	field.name = "del\x7F";
	Check(Refused(field, FieldListRefusal::BadName), "and one with the delete byte, 127");
	field.name = " !~";
	Check(Sound(field), "the space and the tilde, the font's first and last letters, are taken");

	char help[kFieldHelpMost + 2];
	memset(help, 'h', sizeof(help));
	help[kFieldHelpMost] = 0;

	field = NumberField("range");
	field.help = help;
	Check(Sound(field), "help of 200 letters, the most, is sound");
	help[kFieldHelpMost] = 'h';
	help[kFieldHelpMost + 1] = 0;
	Check(Refused(field, FieldListRefusal::BadHelp), "help of 201 is refused");
	field.help = "";
	Check(Refused(field, FieldListRefusal::BadHelp), "and empty help: no help is null");
	field.help = "two\nlines";
	Check(Refused(field, FieldListRefusal::BadHelp), "and help of two lines");
	field.help = 0;
	Check(Sound(field), "no help at all is sound");
}

static void TestKinds()
{
	puts("Kinds:");

	FieldDescriptor field = NumberField("range");
	field.kind = (FieldKind)4;
	Check(Refused(field, FieldListRefusal::NotAKind), "a kind the panel has no box for is refused");
	field.kind = (FieldKind)-1;
	Check(Refused(field, FieldListRefusal::NotAKind), "and so is one below the first");

	field = NumberField("range");
	field.capacity = 63;
	Check(Refused(field, FieldListRefusal::NotItsKind), "a number with a text's capacity is refused");
	field = NumberField("range");
	field.fixed = true;
	Check(Refused(field, FieldListRefusal::NotItsKind), "and one marked fixed");
	field = NumberField("range");
	field.capacityWhy = "why";
	Check(Refused(field, FieldListRefusal::NotItsKind), "and one with a capacity's reason");
	field = NumberField("range");
	field.required = true;
	field.requiredWhy = "why";
	Check(Refused(field, FieldListRefusal::NotItsKind), "and one marked as required, as only a text can be");
	field = NumberField("range");
	field.requiredWhy = "why";
	Check(Refused(field, FieldListRefusal::NotItsKind), "and one with only a required reason");
	field = NumberField("range");
	field.letters = NoSpace;
	Check(Refused(field, FieldListRefusal::NotItsKind), "and one with a letter rule");
	field = NumberField("range");
	field.textCheck = AnyText;
	Check(Refused(field, FieldListRefusal::NotItsKind), "and one with a text's check");
	field = NumberField("range");
	field.prefixField = 0;
	Check(Refused(field, FieldListRefusal::NotItsKind), "and one naming a prefix");
	field = NumberField("range");
	field.bit = 0x1u;
	Check(Refused(field, FieldListRefusal::NotItsKind), "and one with a bit");
	field = NumberField("range");
	field.choices = kLightKinds;
	Check(Refused(field, FieldListRefusal::NotItsKind), "and one with a choice list");
	field = NumberField("range");
	field.choiceCount = 3;
	Check(Refused(field, FieldListRefusal::NotItsKind), "and one with only a count of choices");

	field = TextDescriptor("name");
	field.unit = "mm";
	Check(Refused(field, FieldListRefusal::NotItsKind), "a text with a unit is refused");
	field = TextDescriptor("name");
	field.numberCheck = AnyNumber;
	Check(Refused(field, FieldListRefusal::NotItsKind), "and one with a number's check");

	field = BitField("starts open");
	field.capacity = 8;
	Check(Refused(field, FieldListRefusal::NotItsKind), "a bit with a capacity is refused");
	field = ChoiceField("kind");
	field.bit = 0x1u;
	Check(Refused(field, FieldListRefusal::NotItsKind), "and a choice with a bit");

	Check(Sound(NumberField("range")) && Sound(BitField("starts open")) && Sound(ChoiceField("kind")) && Sound(TextDescriptor("name")),
		"a plain field of each kind is sound");
	field = TextDescriptor("name");
	field.readOnly = true;
	Check(Sound(field), "and a read-only one");
}

static void TestNumbers()
{
	puts("Numbers:");

	FieldDescriptor field = NumberField("range");
	field.lowest = -50.0;
	field.highest = 50.0;
	field.decimals = 2;
	field.unit = "mm";
	field.numberCheck = AnyNumber;
	Check(Sound(field), "a range below 0, with decimals, a unit and the spec's check, is sound");

	field = NumberField("range");
	field.lowest = 10.0;
	field.highest = 5.0;
	Check(Refused(field, FieldListRefusal::BadNumber), "a lowest above the highest is refused");
	field = NumberField("range");
	field.lowest = FromBits(0x7FF8000000000000ull);
	Check(Refused(field, FieldListRefusal::BadNumber), "and a lowest that's NaN, made from its bits");
	field = NumberField("range");
	field.highest = FromBits(0x7FF0000000000000ull);
	Check(Refused(field, FieldListRefusal::BadNumber), "and a highest that's endless");
	field = NumberField("range");
	field.decimals = kNumberMostDecimals + 1;
	Check(Refused(field, FieldListRefusal::BadNumber), "and more decimals than a box can have");
	field = NumberField("range");
	field.highest = kNumberLargest;
	Check(Refused(field, FieldListRefusal::BadNumber), "and a range too large to count every step of");

	char unit[kFieldUnitMost + 2];
	memset(unit, 'u', sizeof(unit));
	unit[kFieldUnitMost] = 0;
	field = NumberField("range");
	field.unit = unit;
	Check(Sound(field), "a unit of 24 letters, the most, is sound");
	unit[kFieldUnitMost] = 'u';
	unit[kFieldUnitMost + 1] = 0;
	Check(Refused(field, FieldListRefusal::BadNumber), "one of 25 is refused");
	field.unit = "";
	Check(Refused(field, FieldListRefusal::BadNumber), "and an empty unit: no unit is null");
	field.unit = "m\xB2";
	Check(Refused(field, FieldListRefusal::BadNumber), "and one with a byte the font doesn't have");
}

static void TestBits()
{
	puts("Bits:");

	FieldDescriptor field = BitField("starts open");
	field.bit = 0;
	Check(Refused(field, FieldListRefusal::BadBit), "no bit is refused");
	field.bit = 0x3u;
	Check(Refused(field, FieldListRefusal::BadBit), "and two bits");
	field.bit = 0xFFFFFFFFu;
	Check(Refused(field, FieldListRefusal::BadBit), "and every bit");

	bool everyOne = true;

	for (unsigned i = 0; i < 32; i++)
	{
		field.bit = 1u << i;
		everyOne = everyOne && Sound(field);
	}

	Check(everyOne, "each of the 32 bits alone is sound, the top one among them");
}

static Choice tooMany[kChoiceMost + 1];

static void TestChoices()
{
	puts("Choices:");

	ChoiceBox box;
	FieldDescriptor field = ChoiceField("kind");

	field.choices = 0;
	field.choiceCount = 0;
	Check(Refused(field, FieldListRefusal::BadChoices) && !box.Load(0, 0, 0), "a choice with no list is refused, as the box refuses it");

	for (size_t i = 0; i < kChoiceMost + 1; i++)
	{
		tooMany[i].value = (int32_t)i;
		tooMany[i].words = "a choice";
	}

	field.choices = tooMany;
	field.choiceCount = kChoiceMost;
	Check(Sound(field) && box.Load(tooMany, kChoiceMost, 0), "256 choices, the most, are sound, and the box takes them");
	field.choiceCount = kChoiceMost + 1;
	Check(Refused(field, FieldListRefusal::BadChoices) && !box.Load(tooMany, kChoiceMost + 1, 0), "257 are refused by both");

	Choice const twice[] = { { 1, "one" }, { 1, "one again" } };
	field.choices = twice;
	field.choiceCount = 2;
	Check(Refused(field, FieldListRefusal::BadChoices) && !box.Load(twice, 2, 1), "two choices with the same value are refused by both");

	Choice const noWords[] = { { 1, "one" }, { 2, 0 } };
	field.choices = noWords;
	Check(Refused(field, FieldListRefusal::BadChoices) && !box.Load(noWords, 2, 1), "and a choice with no words");
}

static void TestTexts()
{
	puts("Texts:");

	FieldDescriptor field = TextDescriptor("name");
	field.capacity = 0;
	Check(Refused(field, FieldListRefusal::BadText), "a capacity of 0 is refused");
	field.capacity = kTextMostBytes + 1;
	Check(Refused(field, FieldListRefusal::BadText), "and one above what a text field holds");
	field.capacity = kTextMostBytes;
	Check(Sound(field), "the most a text field holds is sound");

	size_t const stock[] = { 63, 255, 8, 16, 20, 100 };
	bool allStock = true;

	for (size_t i = 0; i < sizeof(stock) / sizeof(stock[0]); i++)
	{
		field = TextDescriptor("name");
		field.capacity = stock[i];
		field.fixed = stock[i] != 63 && stock[i] != 255;
		allStock = allStock && Sound(field);
	}

	Check(allStock, "every capacity the game has is sound: 63 and 255 ended by a zero, and the fixed 8, 16, 20 and 100");

	field = TextDescriptor("name");
	field.capacityWhy = "the game keeps 63 letters of an object's name";
	field.letters = NoSpace;
	field.textCheck = AnyText;
	Check(Sound(field), "a capacity's reason, a letter rule and the spec's check are sound");
	field.capacityWhy = "";
	Check(Refused(field, FieldListRefusal::BadText), "an empty reason for the capacity is refused");

	char why[kFieldHelpMost + 2];
	memset(why, 'w', sizeof(why));
	why[kFieldHelpMost + 1] = 0;
	field.capacityWhy = why;
	Check(Refused(field, FieldListRefusal::BadText), "and one too long");

	field = TextDescriptor("sound folder");
	field.required = true;
	field.requiredWhy = "the game opens every sound inside this folder";
	Check(Sound(field), "a required text with its reason is sound");
	field.requiredWhy = 0;
	Check(Refused(field, FieldListRefusal::BadRequired), "a required text with no reason is refused");
	field.requiredWhy = "";
	Check(Refused(field, FieldListRefusal::BadRequired), "and one with an empty reason");
	field.requiredWhy = why;
	Check(Refused(field, FieldListRefusal::BadRequired), "and one with a reason too long");
	field.required = false;
	field.requiredWhy = "a reason";
	Check(Refused(field, FieldListRefusal::BadRequired), "a reason on a text that isn't required is refused");
}

static void TestPrefixes()
{
	puts("The name the game puts in front:");

	FieldDescriptor list[3];

	// The third place holds a plain text, so a check that wrongly read one place past a list of two would find a sound field there and
	// let the prefix pass, which this test sees, instead of reading whatever was left on the stack
	list[2] = TextDescriptor("spare");
	list[0] = TextDescriptor("sound folder");
	list[0].readOnly = true;
	list[1] = TextDescriptor("wav");
	list[1].prefixField = 0;
	list[1].textCheck = AnyText;
	Check(CheckFieldList(list, 2, 0), "a wav with the level's sound folder in front of it, shown read-only, is sound");

	list[1].textCheck = 0;
	Check(RefusedAt(list, 2, 1, FieldListRefusal::BadPrefix), "a prefix with no spec's check to work out the two together is refused");
	list[1].textCheck = AnyText;

	list[1].prefixField = 1;
	Check(RefusedAt(list, 2, 1, FieldListRefusal::BadPrefix), "a field that's its own prefix is refused");
	list[1].prefixField = 2;
	Check(RefusedAt(list, 2, 1, FieldListRefusal::BadPrefix), "and one past the end of the list");
	list[1].prefixField = -2;
	Check(RefusedAt(list, 2, 1, FieldListRefusal::BadPrefix), "and one below 0, other than -1 for none");
	list[1].prefixField = INT32_MIN;
	Check(RefusedAt(list, 2, 1, FieldListRefusal::BadPrefix), "and the lowest place there is");

	list[0] = NumberField("volume");
	list[1].prefixField = 0;
	Check(RefusedAt(list, 2, 1, FieldListRefusal::BadPrefix), "a prefix that's a number, not a text, is refused");

	list[0] = TextDescriptor("rif");
	list[1] = TextDescriptor("folder");
	list[1].prefixField = 0;
	list[1].textCheck = AnyText;
	list[2] = TextDescriptor("picture");
	list[2].prefixField = 1;
	list[2].textCheck = AnyText;
	Check(RefusedAt(list, 3, 2, FieldListRefusal::BadPrefix), "a chain of two prefixes is refused: the game puts one name in front");

	list[0] = TextDescriptor("one");
	list[0].prefixField = 1;
	list[0].textCheck = AnyText;
	list[1] = TextDescriptor("other");
	list[1].prefixField = 0;
	list[1].textCheck = AnyText;
	Check(RefusedAt(list, 2, 0, FieldListRefusal::BadPrefix), "and two fields naming each other");

	list[0] = TextDescriptor("wav");
	list[0].prefixField = 1;
	list[0].textCheck = AnyText;
	list[1] = TextDescriptor("sound folder");
	Check(CheckFieldList(list, 2, 0), "a prefix later in the list than its field is sound: the order is the panel's, not the game's");
}

static void TestEmptyDescriptor()
{
	puts("An empty descriptor:");

	FieldDescriptor const field = EmptyFieldDescriptor();

	Check(!field.name && !field.help && !field.unit && !field.numberCheck && !field.textCheck && !field.letters && !field.choices && !field.capacityWhy
		&& !field.requiredWhy && !field.checkContext, "has no words, no list, no rule and no check");
	Check(field.prefixField == -1 && field.capacity == 0 && field.bit == 0 && field.choiceCount == 0 && field.decimals == 0 && !field.fixed && !field.required
		&& !field.readOnly, "names no prefix, and has every limit at 0");
	Check(Refused(field, FieldListRefusal::BadName), "and is refused until it has a name");
}

int main()
{
	TestTheList();
	TestWords();
	TestKinds();
	TestNumbers();
	TestBits();
	TestChoices();
	TestTexts();
	TestPrefixes();
	TestEmptyDescriptor();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
