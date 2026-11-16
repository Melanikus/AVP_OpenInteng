// The test for src/ui's field panel. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. The field spec's checks are stand-ins written here, each doing one thing the test can see. Each check's
// words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "field_panel.h"

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

// ---- Stand-ins for the field spec's own checks ----

static const char kPathTooLong[] = "the path to a sound is at most 20 letters, with its folder and a slash in front";
static const char kThirteen[] = "13 isn't allowed here";
static const char kNever[] = "the spec never allows this";

// As F6's NameLengthIsAllowed does it for a name the game opens with another in front: the folder, a "/", then the name, at most 20
static bool PathCheck(const void *, const unsigned char *, size_t length, const unsigned char *, size_t prefixLength, const char **why)
{
	if (prefixLength + 1 + length > 20)
	{
		*why = kPathTooLong;
		return false;
	}

	return true;
}

static bool VolumeCheck(const void *, double value, const char **why)
{
	if (value == 13.0)
	{
		*why = kThirteen;
		return false;
	}

	return true;
}

static bool NeverANumber(const void *, double, const char **why)
{
	*why = kNever;
	return false;
}

static bool NeverAText(const void *, const unsigned char *, size_t, const unsigned char *, size_t, const char **why)
{
	*why = kNever;
	return false;
}

// A text box's own check, as the panel gives one: this one always refuses
static bool NeverKeepText(const void *, const unsigned char *, size_t, const char **why)
{
	*why = kNever;
	return false;
}

// Refuses, and gives no reason at all
static bool RefusesSaysNothing(const void *, double, const char **)
{
	return false;
}

// Says yes, and sets a reason anyway: only the answer counts
static bool AllowsWithWords(const void *, double, const char **why)
{
	*why = kNever;
	return true;
}

// A spec reason with a control letter and a byte the font doesn't have, to see the status line make them safe
static const char kHostileWhy[] = "bad\x01words\xC8";

static bool HostileWords(const void *, double, const char **why)
{
	*why = kHostileWhy;
	return false;
}

// A spec reason far longer than the status line reads, with no zero in its first kFieldHelpMost + 1 bytes
static char longWhy[400];

static bool LongWords(const void *, double, const char **why)
{
	*why = longWhy;
	return false;
}

// Remembers what it was given: the fixed light set name's check
static size_t heardLength = 0;
static unsigned char heard[32];
static size_t heardPrefixLength = 99;
static const unsigned char *heardPrefix = (const unsigned char *)"x";

static bool Listens(const void *, const unsigned char *bytes, size_t length, const unsigned char *prefix, size_t prefixLength, const char **)
{
	heardLength = length;
	memcpy(heard, bytes, length < sizeof(heard) ? length : sizeof(heard));
	heardPrefix = prefix;
	heardPrefixLength = prefixLength;
	return true;
}

// Hands its context back, to see it's the descriptor's own
static const void *heardContext = 0;

static bool ContextCheck(const void *context, double, const char **)
{
	heardContext = context;
	return true;
}

static const Choice kKinds[] =
{
	{ 0, "standard" },
	{ 1, "flicker" },
	{ 2, "pulse" },
};

// ---- A sound's record, for most of the tests ----

enum
{
	Folder,
	Wav,
	ObjectName,
	Volume,
	Loops,
	Kind,
	LightSet,
	SoundFields
};

static const char kFolderWhy[] = "the game opens every sound inside this folder";
static const char kWavWhy[] = "the game can't play a sound with no file";
static const char kObjectWhy[] = "the game keeps 63 letters of an object's name";
static const char kVolumeHelp[] = "the game's loudest is 127";

static FieldDescriptor sound[SoundFields];

static void MakeTheSound()
{
	for (size_t i = 0; i < SoundFields; i++)
	{
		sound[i] = EmptyFieldDescriptor();
	}

	sound[Folder].name = "sound folder";
	sound[Folder].kind = FieldKind::Text;
	sound[Folder].readOnly = true;
	sound[Folder].capacity = 255;
	sound[Folder].required = true;
	sound[Folder].requiredWhy = kFolderWhy;

	sound[Wav].name = "wav";
	sound[Wav].help = "the sound's file, inside the sound folder";
	sound[Wav].kind = FieldKind::Text;
	sound[Wav].capacity = 255;
	sound[Wav].required = true;
	sound[Wav].requiredWhy = kWavWhy;
	sound[Wav].textCheck = PathCheck;
	sound[Wav].prefixField = Folder;

	sound[ObjectName].name = "name";
	sound[ObjectName].kind = FieldKind::Text;
	sound[ObjectName].capacity = 63;
	sound[ObjectName].capacityWhy = kObjectWhy;

	sound[Volume].name = "volume";
	sound[Volume].help = kVolumeHelp;
	sound[Volume].kind = FieldKind::Number;
	sound[Volume].lowest = 0.0;
	sound[Volume].highest = 127.0;
	sound[Volume].unit = "steps";
	sound[Volume].numberCheck = VolumeCheck;

	sound[Loops].name = "loops";
	sound[Loops].kind = FieldKind::Bit;
	sound[Loops].bit = 0x1u;

	sound[Kind].name = "kind";
	sound[Kind].kind = FieldKind::Choice;
	sound[Kind].choices = kKinds;
	sound[Kind].choiceCount = 3;

	sound[LightSet].name = "light set";
	sound[LightSet].kind = FieldKind::Text;
	sound[LightSet].capacity = 8;
	sound[LightSet].fixed = true;
	sound[LightSet].textCheck = Listens;
}

// A full fixed name: exactly 8 bytes, with no zero at all
static const unsigned char kFullLightSet[8] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H' };

static FieldValue TextValue(const char *text)
{
	FieldValue value = EmptyFieldValue();

	value.bytes = (const unsigned char *)text;
	value.length = text ? strlen(text) + 1 : 0;
	return value;
}

static FieldValue values[SoundFields];

static void MakeTheValues()
{
	for (size_t i = 0; i < SoundFields; i++)
	{
		values[i] = EmptyFieldValue();
	}

	values[Folder] = TextValue("sounds");
	values[Wav] = TextValue("boom.wav");
	values[ObjectName] = TextValue("door");
	values[Volume].number = 64.0;
	values[Loops].word = 0x80000100u;
	values[Kind].choice = 1;
	values[LightSet].bytes = kFullLightSet;
	values[LightSet].length = sizeof(kFullLightSet);
}

// ---- Driving it ----

static PanelAnswer Type(FieldPanel &panel, const char *letters)
{
	PanelAnswer answer = PanelAnswer::Nothing;

	for (const char *at = letters; *at; at++)
	{
		answer = panel.Handle(LetterEvent((unsigned char)*at));
	}

	return answer;
}

// Selects everything, types over it, and presses Enter: the answer is Enter's
static PanelAnswer TypeOver(FieldPanel &panel, const char *letters)
{
	panel.Handle(LetterEvent('a', true));

	if (!*letters)
	{
		panel.Handle(KeyEvent(Key::Backspace));
	}

	Type(panel, letters);
	return panel.Handle(KeyEvent(Key::Enter));
}

// Each line is read into a room filled with x first, so a line that's never written shows as x's, not as whatever was there
static std::string ShownIn(const FieldPanel &panel, size_t field)
{
	char out[300];

	memset(out, 'x', sizeof(out) - 1);
	out[sizeof(out) - 1] = 0;
	panel.Shown(field, out, sizeof(out));
	return out;
}

static std::string Status(const FieldPanel &panel)
{
	char out[300];

	memset(out, 'x', sizeof(out) - 1);
	out[sizeof(out) - 1] = 0;
	panel.StatusLine(out, sizeof(out));
	return out;
}

static std::string Help(const FieldPanel &panel)
{
	char out[300];

	memset(out, 'x', sizeof(out) - 1);
	out[sizeof(out) - 1] = 0;
	panel.HelpRow(out, sizeof(out));
	return out;
}

static bool TextIs(const FieldPanel &panel, size_t field, const char *text)
{
	size_t length = 0;
	const unsigned char *const value = panel.TextValue(field, &length);

	return value && length == strlen(text) && memcmp(value, text, length) == 0;
}

static FieldPanel panel;
static FieldPanel big;
static FieldDescriptor many[kFieldPanelMost + 1];
static FieldValue manyValues[kFieldPanelMost + 1];

static void TestLoading()
{
	puts("Loading:");

	FieldListProblem problem = { 99, FieldListRefusal::None };

	Check(!panel.Loaded() && panel.Count() == 0 && panel.Focused() == -1 && panel.Handle(LetterEvent('a')) == PanelAnswer::Refused,
		"a panel with nothing loaded refuses every event");

	Check(panel.Load(0, 0, 0, &problem) && panel.Loaded() && panel.Count() == 0 && panel.Focused() == -1, "an empty list loads: a record with nothing to edit");
	Check(panel.Handle(LetterEvent('a')) == PanelAnswer::Nothing && panel.Handle(KeyEvent(Key::Tab)) == PanelAnswer::Nothing && panel.ClickField(0) == PanelAnswer::Nothing
		&& panel.Paste((const unsigned char *)"x", 1) == PanelAnswer::Nothing, "and an empty panel does nothing with any event");
	Check(Help(panel) == "" && Status(panel) == "" && ShownIn(panel, 0) == "" && panel.Flag(0) == FieldFlag::None && !panel.Changed(0) && panel.TextValue(0, 0) == 0,
		"and shows no help, no status, and no field");

	for (size_t i = 0; i < kFieldPanelMost + 1; i++)
	{
		many[i] = EmptyFieldDescriptor();
		many[i].name = "a number";
		many[i].kind = FieldKind::Number;
		many[i].highest = 1000.0;
		manyValues[i] = EmptyFieldValue();
		manyValues[i].number = (double)i;
	}

	Check(big.Load(many, kFieldPanelMost, manyValues, &problem) && big.Count() == kFieldPanelMost && big.Focused() == 0, "512 fields load, and the first has focus");

	bool round = true;

	for (size_t i = 1; i < kFieldPanelMost; i++)
	{
		round = round && big.Handle(KeyEvent(Key::Tab)) == PanelAnswer::FocusMoved && big.Focused() == (int)i;
	}

	Check(round && big.Handle(KeyEvent(Key::Tab)) == PanelAnswer::FocusMoved && big.Focused() == 0, "Tab visits all 512 in order, and goes round to the first");
	Check(big.Handle(KeyEvent(Key::Tab, true)) == PanelAnswer::FocusMoved && big.Focused() == (int)kFieldPanelMost - 1, "and Shift+Tab from the first goes to the last");
	Check(big.NumberValue(511) == 511.0, "each field holds its own value");

	Check(!big.Load(many, kFieldPanelMost + 1, manyValues, &problem) && problem.why == FieldListRefusal::TooMany && !big.Loaded() && big.Count() == 0,
		"513 fields are refused as a whole, and the panel is left with nothing");
	Check(big.Handle(LetterEvent('5')) == PanelAnswer::Refused && big.NumberValue(0) == 0.0, "so nothing of the list before can be edited or read");
	Check(!big.Load(0, 10000, 0, &problem) && problem.why == FieldListRefusal::TooMany, "and 10,000 are refused by their count");
	Check(!big.Load(many, 3, 0, &problem) && !big.Loaded(), "fields counted with no values are refused");

	MakeTheSound();
	MakeTheValues();
	sound[Wav].textCheck = 0;
	Check(!panel.Load(sound, SoundFields, values, &problem) && problem.why == FieldListRefusal::BadPrefix && problem.field == Wav,
		"a field the game opens with a name in front, with no spec's check to use it, refuses the whole list: a missing check never lets an edit through");
	Check(!panel.Loaded() && panel.Handle(LetterEvent('a')) == PanelAnswer::Refused && panel.Paste((const unsigned char *)"x", 1) == PanelAnswer::Refused,
		"and the panel refuses every event and every paste");
	sound[Wav].textCheck = PathCheck;

	Check(panel.Load(sound, SoundFields, values, &problem) && panel.Loaded() && panel.Count() == SoundFields && panel.Focused() == Folder, "the sound's record loads");

	bool noneChanged = true;
	bool noneFlagged = true;

	for (size_t i = 0; i < SoundFields; i++)
	{
		noneChanged = noneChanged && !panel.Changed(i);
		noneFlagged = noneFlagged && panel.Flag(i) == FieldFlag::None;
	}

	Check(noneChanged && noneFlagged, "with nothing changed and nothing flagged");
	Check(TextIs(panel, Folder, "sounds") && TextIs(panel, Wav, "boom.wav") && panel.NumberValue(Volume) == 64.0 && panel.BitWord(Loops) == 0x80000100u
		&& panel.ChoiceValue(Kind) == 1, "and every value is the level's own");
	size_t length = 99;
	Check(panel.NumberValue(Wav) == 0.0 && panel.BitWord(Volume) == 0 && !panel.BitChecked(Volume) && panel.ChoiceValue(Folder) == 0 && panel.TextValue(Volume, &length) == 0
		&& length == 0, "asking a field for another kind's value gives 0");
	Check(panel.NumberValue(SoundFields) == 0.0 && panel.BitWord(SIZE_MAX) == 0 && !panel.BitChecked(SoundFields) && panel.ChoiceValue(SoundFields) == 0
		&& panel.TextValue(SoundFields, 0) == 0 && panel.Flag(SoundFields) == FieldFlag::None && panel.FlagWhy(SoundFields) == 0 && !panel.Changed(SoundFields)
		&& ShownIn(panel, SoundFields) == "", "and so does a place past the end, which is never read");
	Check(ShownIn(panel, Loops) == "", "a bit shows no text: the view draws its box");

	panel.Handle(LetterEvent('x'));
	Check(panel.StatusWords() == kPanelReadOnly, "(the status line has words now)");
	panel.Load(sound, SoundFields, values, 0);
	Check(panel.StatusWords() == 0 && Status(panel) == "", "and a new load clears them");
}

static void TestEveryKind()
{
	puts("Every kind, edited:");

	MakeTheSound();
	MakeTheValues();
	panel.Load(sound, SoundFields, values, 0);

	// The read-only folder
	Check(panel.Handle(LetterEvent('x')) == PanelAnswer::Refused && panel.StatusWords() == kPanelReadOnly && !panel.StatusIsTheSpecs() && TextIs(panel, Folder, "sounds"),
		"a read-only field has focus, so it can be read, but refuses a letter, saying it can't be changed here");
	Check(panel.Paste((const unsigned char *)"x", 1) == PanelAnswer::Refused && panel.StatusWords() == kPanelReadOnly && !panel.Changed(Folder), "and refuses a paste");

	char help[300];

	Check(panel.Handle(KeyEvent(Key::Tab)) == PanelAnswer::FocusMoved && panel.Focused() == Wav, "Tab goes to the wav");
	panel.HelpRow(help, sizeof(help));
	Check(std::string(help) == "the sound's file, inside the sound folder", "and the help row shows the wav's help");

	// The wav: 6 letters of folder, a slash, so at most 13
	Check(TypeOver(panel, "thirteen.wav!") == PanelAnswer::Kept && TextIs(panel, Wav, "thirteen.wav!") && panel.Changed(Wav), "a wav that fits with its folder in front is kept");
	Check(TypeOver(panel, "fourteen.wav!!") == PanelAnswer::Refused && panel.StatusWords() == kPathTooLong && panel.StatusIsTheSpecs(),
		"one a letter too long is refused, in the spec's own words");
	Check(TextIs(panel, Wav, "thirteen.wav!"), "and the value is the last one kept: nothing was kept and taken back");
	Check(panel.Handle(KeyEvent(Key::Escape)) == PanelAnswer::GaveUp && ShownIn(panel, Wav) == "thirteen.wav!", "Esc gives the edit up");
	Check(TypeOver(panel, "") == PanelAnswer::Refused && panel.StatusWords() == kWavWhy && panel.StatusIsTheSpecs() && TextIs(panel, Wav, "thirteen.wav!"),
		"emptying a required name is refused, with the spec's reason it's required");
	panel.Handle(KeyEvent(Key::Escape));

	// The object's name: 63 letters at most, and optional
	panel.Handle(KeyEvent(Key::Tab));
	std::string sixtyThree(63, 'n');
	Check(TypeOver(panel, sixtyThree.c_str()) == PanelAnswer::Kept && panel.Changed(ObjectName), "a name of 63 letters is kept");
	Check(Type(panel, "x") == PanelAnswer::Refused && panel.StatusWords() == kObjectWhy && panel.StatusIsTheSpecs(), "a 64th is refused, with the spec's reason for 63");
	panel.Handle(KeyEvent(Key::Escape));
	Check(TypeOver(panel, "") == PanelAnswer::Kept && TextIs(panel, ObjectName, ""), "an optional name can be emptied");
	Check(panel.Flag(ObjectName) == FieldFlag::None, "and empty isn't flagged when it's optional");
	panel.Handle(LetterEvent('a', true));
	Check(Type(panel, "\t") == PanelAnswer::Refused && panel.StatusWords() == kPanelNotAFontLetter && !panel.StatusIsTheSpecs(),
		"a letter the font doesn't have is refused, in the panel's own words: that's about typing");
	panel.Handle(KeyEvent(Key::Escape));

	// The volume
	panel.Handle(KeyEvent(Key::Tab));
	char help2[300];
	panel.HelpRow(help2, sizeof(help2));
	Check(std::string(help2) == kVolumeHelp, "the volume's help is in the help row");
	Check(TypeOver(panel, "100") == PanelAnswer::Kept && panel.NumberValue(Volume) == 100.0 && panel.Changed(Volume), "a volume the spec allows is kept");
	Check(TypeOver(panel, "13") == PanelAnswer::Refused && panel.StatusWords() == kThirteen && panel.StatusIsTheSpecs() && panel.NumberValue(Volume) == 100.0,
		"one the spec refuses is refused in its words, and the value stays");
	Check(panel.Handle(KeyEvent(Key::Escape)) == PanelAnswer::GaveUp && panel.StatusWords() == 0 && !panel.StatusIsTheSpecs() && Status(panel) == "",
		"the next event clears the status line");
	Check(TypeOver(panel, "200") == PanelAnswer::Refused && panel.StatusWords() == kVolumeHelp && panel.StatusIsTheSpecs() && panel.NumberValue(Volume) == 100.0,
		"one the spec allows but outside the field's range is still refused, with the field's help as the reason");
	panel.Handle(KeyEvent(Key::Escape));
	Check(Type(panel, ",") == PanelAnswer::Refused && panel.StatusWords() == kPanelUseADot && !panel.StatusIsTheSpecs(), "a comma is told to use a dot");
	panel.Handle(KeyEvent(Key::Escape));

	// Focus leaving keeps an edit that can be kept, and gives up one that can't, saying why
	panel.Handle(LetterEvent('a', true));
	Type(panel, "50");
	Check(panel.Handle(KeyEvent(Key::Tab)) == PanelAnswer::FocusMoved && panel.NumberValue(Volume) == 50.0 && panel.Focused() == Loops, "Tab away keeps a typed volume the spec allows");
	panel.Handle(KeyEvent(Key::Tab, true));
	panel.Handle(LetterEvent('a', true));
	Type(panel, "13");
	Check(panel.Handle(KeyEvent(Key::Tab)) == PanelAnswer::FocusMoved && panel.NumberValue(Volume) == 50.0 && panel.StatusWords() == kThirteen,
		"and gives up one the spec refuses, with its reason on the status line, and focus still moves");

	// The bit, the choice
	Check(panel.Handle(KeyEvent(Key::Space)) == PanelAnswer::Kept && panel.BitChecked(Loops) && panel.BitWord(Loops) == 0x80000101u && panel.Changed(Loops),
		"Space turns the bit on, and every other bit of the level's word is kept");
	panel.Handle(KeyEvent(Key::Tab));
	Check(panel.Handle(KeyEvent(Key::Enter)) == PanelAnswer::Moved && panel.Handle(KeyEvent(Key::Down)) == PanelAnswer::Moved
		&& panel.Handle(KeyEvent(Key::Enter)) == PanelAnswer::Kept && panel.ChoiceValue(Kind) == 2 && ShownIn(panel, Kind) == "pulse" && panel.Changed(Kind),
		"Enter opens the list, Down lights the next, Enter picks it");
	panel.Handle(KeyEvent(Key::Enter));
	panel.Handle(KeyEvent(Key::Tab));
	panel.Handle(KeyEvent(Key::Tab, true));
	Check(panel.Focused() == Kind && panel.Handle(KeyEvent(Key::Down)) == PanelAnswer::Nothing && panel.ChoiceValue(Kind) == 2,
		"focus leaving an open list closes it, so Down back on it changes nothing");

	// The fixed name, full, with no zero
	panel.Handle(KeyEvent(Key::Tab));
	heardLength = 0;
	Check(panel.Flag(LightSet) == FieldFlag::None && heardLength == 8 && memcmp(heard, "ABCDEFGH", 8) == 0 && heardPrefix == 0 && heardPrefixLength == 0,
		"a full fixed name with no zero reaches the spec's check as its 8 bytes and their length, with no name in front");
	Check(TextIs(panel, LightSet, "ABCDEFGH") && ShownIn(panel, LightSet) == "ABCDEFGH", "and it's kept and shown whole");
	panel.Handle(LetterEvent('a', true));
	Type(panel, "ABCDEFGZ");
	Check(panel.Handle(KeyEvent(Key::Enter)) == PanelAnswer::Kept && heardLength == 8 && memcmp(heard, "ABCDEFGZ", 8) == 0 && TextIs(panel, LightSet, "ABCDEFGZ"),
		"an edit that fills it again reaches the check as 8 bytes, and is kept");
	Check(panel.Changed(LightSet), "and a name of the same length with a letter changed is changed");
	Check(Type(panel, "Q") == PanelAnswer::Refused && panel.StatusWords() == kPanelFull && !panel.StatusIsTheSpecs(), "and a ninth letter is refused as full");
	panel.Handle(KeyEvent(Key::Escape));

	Check(panel.ClickField(Volume) == PanelAnswer::FocusMoved && panel.Focused() == Volume, "a click gives a field focus");
	Check(panel.ClickField(Volume) == PanelAnswer::Nothing && panel.ClickField(SoundFields) == PanelAnswer::Nothing && panel.ClickField(SIZE_MAX) == PanelAnswer::Nothing,
		"a click on the field that has it, or past the end, does nothing");
	panel.Handle(LetterEvent('a', true));
	Type(panel, "77");
	Check(panel.ClickField(ObjectName) == PanelAnswer::FocusMoved && panel.NumberValue(Volume) == 77.0, "a click away keeps the edit, as Tab does");
}

// Names longer than their fields hold, kept here for the whole test, as a level's bytes would be while it's loaded
static std::string const seventy(70, 'o');
static std::string const threeHundred(300, 'f');

static void TestHostileValues()
{
	puts("Hostile values from a level:");

	MakeTheSound();
	MakeTheValues();
	values[Volume].number = 500.0;
	values[Kind].choice = 99;
	values[Folder] = TextValue("");
	values[ObjectName] = TextValue(seventy.c_str());
	panel.Load(sound, SoundFields, values, 0);

	Check(panel.Flag(Volume) == FieldFlag::OutOfRange && panel.FlagWhy(Volume) == kVolumeHelp && panel.NumberValue(Volume) == 500.0 && ShownIn(panel, Volume) == "500"
		&& !panel.Changed(Volume), "a volume outside its range is kept as it is, shown and flagged with the field's help");
	Check(panel.Flag(Kind) == FieldFlag::UnknownChoice && panel.ChoiceValue(Kind) == 99 && ShownIn(panel, Kind) == "unknown (99)" && !panel.Changed(Kind),
		"a choice the list doesn't have is kept as it is, shown as unknown, and flagged");
	Check(panel.Flag(Folder) == FieldFlag::EmptyButRequired && panel.FlagWhy(Folder) == kFolderWhy && TextIs(panel, Folder, "") && !panel.Changed(Folder),
		"an empty required name is kept empty and flagged with the spec's reason, never filled in");
	Check(panel.Flag(Wav) == FieldFlag::None, "and the wav with an empty folder in front still fits");
	Check(panel.Flag(ObjectName) == FieldFlag::NotLoaded && panel.FlagWhy(ObjectName) == kPanelNotLoaded && panel.TextValue(ObjectName, 0) == 0 && ShownIn(panel, ObjectName) == ""
		&& !panel.Changed(ObjectName), "a name longer than its capacity can't be loaded: it's flagged, shows nothing, has no value to save, and is never changed");

	panel.ClickField(ObjectName);
	Check(panel.Handle(LetterEvent('x')) == PanelAnswer::Refused && panel.StatusWords() == kPanelNotLoaded && panel.Paste((const unsigned char *)"x", 1) == PanelAnswer::Refused
		&& !panel.Changed(ObjectName), "and it refuses every letter and paste, saying it couldn't be read");

	// A volume that isn't a number at all, made from its bits
	uint64_t const nanBits = 0x7FF8000000000001ull;
	double nan = 0.0;
	memcpy(&nan, &nanBits, sizeof(nan));
	values[Volume].number = nan;
	values[ObjectName] = TextValue("caf\xC0");
	values[Folder] = TextValue(threeHundred.c_str());
	values[Wav] = TextValue("x");
	panel.Load(sound, SoundFields, values, 0);

	Check(panel.Flag(Volume) == FieldFlag::OutOfRange && ShownIn(panel, Volume) == "not a number" && !panel.Changed(Volume), "a NaN volume is kept, shown as not a number, flagged, and unchanged");
	Check(ShownIn(panel, ObjectName) == "caf?" && !panel.Changed(ObjectName), "a name with a byte the font doesn't have shows a question mark, and its bytes are kept");
	panel.ClickField(ObjectName);
	Check(Type(panel, "e") == PanelAnswer::Changed && panel.StatusWords() == kPanelCantTypeSome, "starting an edit of it says the edit replaces the letters the editor can't type");
	Check(panel.Handle(KeyEvent(Key::Escape)) == PanelAnswer::GaveUp && TextIs(panel, ObjectName, "caf\xC0") && !panel.Changed(ObjectName), "and Esc gives the level's own bytes back");

	Check(panel.Flag(Folder) == FieldFlag::NotLoaded && panel.Flag(Wav) == FieldFlag::PrefixNotLoaded && panel.FlagWhy(Wav) == kPanelPrefixNotLoaded,
		"a folder too long to load leaves its wav flagged as unable to be checked");
	panel.ClickField(Wav);
	Check(TypeOver(panel, "y") == PanelAnswer::Refused && panel.StatusWords() == kPanelPrefixNotLoaded && !panel.StatusIsTheSpecs() && TextIs(panel, Wav, "x"),
		"and an edit of the wav is refused, in the panel's words, as it can't be checked");
	panel.Handle(KeyEvent(Key::Escape));

	values[Folder] = TextValue("sounds");
	values[Wav] = TextValue("a-very-long-name.wav");
	values[LightSet].bytes = 0;
	values[LightSet].length = 5;
	panel.Load(sound, SoundFields, values, 0);
	Check(panel.Flag(Wav) == FieldFlag::SpecRefusesIt && panel.FlagWhy(Wav) == kPathTooLong && TextIs(panel, Wav, "a-very-long-name.wav") && !panel.Changed(Wav),
		"a wav the spec refuses as it is, with its folder in front, is kept and flagged in the spec's words");
	Check(panel.Flag(LightSet) == FieldFlag::NotLoaded, "a text counted with no bytes can't be loaded");
}

static void TestFailingSafe()
{
	puts("A check that refuses, or says odd things:");

	FieldDescriptor list[2];
	FieldValue two[2] = { EmptyFieldValue(), EmptyFieldValue() };

	list[0] = EmptyFieldDescriptor();
	list[0].name = "count";
	list[0].kind = FieldKind::Number;
	list[0].highest = 100.0;
	list[0].numberCheck = NeverANumber;
	list[1] = EmptyFieldDescriptor();
	list[1].name = "label";
	list[1].kind = FieldKind::Text;
	list[1].capacity = 63;
	list[1].textCheck = NeverAText;
	two[0].number = 5.0;
	two[1] = TextValue("old");

	panel.Load(list, 2, two, 0);
	Check(TypeOver(panel, "7") == PanelAnswer::Refused && panel.NumberValue(0) == 5.0 && panel.StatusWords() == kNever, "a number check that always refuses keeps nothing typed");
	Check(panel.Handle(KeyEvent(Key::Tab)) == PanelAnswer::FocusMoved && panel.NumberValue(0) == 5.0 && !panel.Changed(0), "not even when focus leaves");
	Check(panel.Flag(0) == FieldFlag::SpecRefusesIt && panel.FlagWhy(0) == kNever, "and the level's own value is flagged as one the spec refuses, but kept");
	Check(TypeOver(panel, "new") == PanelAnswer::Refused && TextIs(panel, 1, "old") && panel.StatusWords() == kNever, "a text check that always refuses keeps nothing typed");
	Check(panel.Handle(KeyEvent(Key::Tab)) == PanelAnswer::FocusMoved && TextIs(panel, 1, "old") && !panel.Changed(1) && panel.StatusWords() == kNever,
		"not even when focus leaves, which says why");

	list[0].numberCheck = RefusesSaysNothing;
	panel.Load(list, 1, two, 0);
	Check(TypeOver(panel, "7") == PanelAnswer::Refused && panel.NumberValue(0) == 5.0 && panel.StatusWords() == kPanelSpecGaveNoReason && !panel.StatusIsTheSpecs(),
		"a check that refuses with no reason still refuses, and the panel says the rules don't allow it");
	Check(panel.Flag(0) == FieldFlag::SpecRefusesIt && panel.FlagWhy(0) == 0, "and the level's value is flagged with no words");

	list[0].numberCheck = AllowsWithWords;
	panel.Load(list, 1, two, 0);
	Check(TypeOver(panel, "7") == PanelAnswer::Kept && panel.NumberValue(0) == 7.0 && panel.StatusWords() == 0, "a check that says yes is kept, whatever reason it also sets: only the answer counts");

	list[0].numberCheck = ContextCheck;
	list[0].checkContext = &list[1];
	heardContext = 0;
	panel.Load(list, 1, two, 0);
	Check(TypeOver(panel, "8") == PanelAnswer::Kept && heardContext == &list[1], "a check is given its descriptor's own context");
	Check(panel.Flag(0) == FieldFlag::None, "and a value it allows isn't flagged");

	list[0].numberCheck = HostileWords;
	list[0].checkContext = 0;
	panel.Load(list, 1, two, 0);
	Check(TypeOver(panel, "9") == PanelAnswer::Refused && Status(panel) == "bad?words?", "a reason with a control letter and a byte the font doesn't have is shown safe");
	panel.Handle(KeyEvent(Key::Escape));

	memset(longWhy, 'w', sizeof(longWhy) - 1);
	longWhy[sizeof(longWhy) - 1] = 0;
	list[0].numberCheck = LongWords;
	panel.Load(list, 1, two, 0);
	Check(TypeOver(panel, "9") == PanelAnswer::Refused && Status(panel).size() == kFieldHelpMost, "a reason longer than the status line reads is cut at 200 bytes when shown");

	char nothing[4] = { 'x', 'x', 'x', 'x' };
	panel.StatusLine(nothing, 0);
	panel.HelpRow(nothing, 0);
	panel.Shown(0, nothing, 0);
	Check(nothing[0] == 'x', "a line of no room is never written");
}

static bool NoQuestionMark(unsigned char letter)
{
	return letter != '?';
}

static void TestTheWords()
{
	puts("The words for each refusal, and pastes:");

	MakeTheSound();
	MakeTheValues();
	sound[ObjectName].letters = NoQuestionMark;
	values[ObjectName] = TextValue("caf\xC0");
	panel.Load(sound, SoundFields, values, 0);

	// The volume: 0 to 127, so at most 3 digits, whole
	panel.ClickField(Volume);
	panel.Handle(LetterEvent('a', true));
	Check(Type(panel, "q") == PanelAnswer::Refused && panel.StatusWords() == kPanelNotANumberLetter && !panel.StatusIsTheSpecs(), "a letter that's never in a number");
	Check(Type(panel, "1234") == PanelAnswer::Refused && panel.StatusWords() == kPanelTooLong, "a fourth digit where the range needs three");
	panel.Handle(KeyEvent(Key::Escape));
	Check(TypeOver(panel, ".") == PanelAnswer::Refused && panel.StatusWords() == kPanelNotANumber, "a dot alone isn't a number");
	panel.Handle(KeyEvent(Key::Escape));
	Check(TypeOver(panel, "1.5") == PanelAnswer::Refused && panel.StatusWords() == kPanelTooManyDecimals && panel.NumberValue(Volume) == 64.0,
		"a fraction in a whole field, which the spec's stand-in allows, is still refused by the box");
	panel.Handle(KeyEvent(Key::Escape));
	Check(panel.Paste((const unsigned char *)"1\n2", 3) == PanelAnswer::Refused && panel.StatusWords() == kPanelBadPaste, "a paste of two lines into a number");
	panel.Handle(LetterEvent('a', true));
	Check(panel.Paste((const unsigned char *)"42", 2) == PanelAnswer::Changed && panel.Handle(KeyEvent(Key::Enter)) == PanelAnswer::Kept && panel.NumberValue(Volume) == 42.0,
		"a paste of a number goes in, and is kept with Enter");

	// A click past the end in the middle of an edit leaves the edit going
	panel.Handle(LetterEvent('a', true));
	Type(panel, "99");
	Check(panel.ClickField(SoundFields) == PanelAnswer::Nothing && ShownIn(panel, Volume) == "99" && panel.NumberValue(Volume) == 42.0,
		"a click past the end changes nothing, and the edit goes on");
	panel.Handle(KeyEvent(Key::Escape));

	// The object's name: 63 letters, no question mark, and a level's name with a byte the editor can't type
	panel.ClickField(ObjectName);
	Check(Type(panel, "e") == PanelAnswer::Changed && panel.Handle(KeyEvent(Key::Enter)) == PanelAnswer::Refused && panel.StatusWords() == kPanelHasABadLetter
		&& TextIs(panel, ObjectName, "caf\xC0"), "an edit still holding the question mark where the level's own letter was, which this field doesn't take, is refused");
	panel.Handle(KeyEvent(Key::Escape));
	panel.Handle(LetterEvent('a', true));
	Check(Type(panel, "?") == PanelAnswer::Refused && panel.StatusWords() == kPanelNotAllowedHere, "a font letter this field doesn't take");
	panel.Handle(KeyEvent(Key::Escape));

	std::string const tooLong(64, 'p');
	panel.Handle(LetterEvent('a', true));
	Check(panel.Paste((const unsigned char *)tooLong.c_str(), tooLong.size()) == PanelAnswer::Refused && panel.StatusWords() == kObjectWhy && panel.StatusIsTheSpecs(),
		"a paste that won't fit is refused with the spec's reason for the field's length");
	Check(panel.Paste((const unsigned char *)"a\nb", 3) == PanelAnswer::Refused && panel.StatusWords() == kPanelPasteOneLine, "a paste of two lines");
	Check(panel.Paste((const unsigned char *)"a\x01", 2) == PanelAnswer::Refused && panel.StatusWords() == kPanelBadPaste, "a paste with a control letter");
	Check(panel.Paste((const unsigned char *)"pasted", 6) == PanelAnswer::Changed && panel.StatusWords() == kPanelCantTypeSome
		&& panel.Handle(KeyEvent(Key::Enter)) == PanelAnswer::Kept && TextIs(panel, ObjectName, "pasted") && panel.Changed(ObjectName),
		"a paste that fits goes in, still with the notice that the edit replaces the level's letters, and is kept with Enter");

	panel.ClickField(Wav);
	Check(panel.Paste((const unsigned char *)"a\nb", 3) == PanelAnswer::Refused && panel.StatusWords() == kPanelPasteOneLine, "(a refused paste puts words on the status line)");
	Check(panel.Paste((const unsigned char *)"x", 1) == PanelAnswer::Changed && panel.StatusWords() == 0, "and the next paste clears them");
	panel.Handle(KeyEvent(Key::Escape));

	panel.ClickField(Folder);
	Check(panel.Paste((const unsigned char *)"x", 1) == PanelAnswer::Refused && panel.StatusWords() == kPanelReadOnly, "a paste into a read-only field");
	Check(panel.ClickField(Loops) == PanelAnswer::FocusMoved && panel.StatusWords() == 0, "a click clears the status line");
	Check(panel.Paste((const unsigned char *)"x", 1) == PanelAnswer::Nothing && panel.BitWord(Loops) == 0x80000100u, "and a paste into a bit means nothing");

	sound[Wav].help = 0;
	panel.ClickField(Wav);
	Check(Help(panel) == "", "a field with no help shows an empty help row");
}

static void TestBoxesForgetOldChecks()
{
	puts("A box loaded again:");

	NumberBox number;
	NumberRule const rule = NumberRuleFor(0.0, 100.0, 0);

	number.Load(5.0, rule, NeverANumber, 0);
	number.Handle(LetterEvent('a', true));
	number.Handle(LetterEvent('7'));
	Check(number.Handle(KeyEvent(Key::Enter)) == NumberAnswer::Refused && number.Refusal() == NumberRefusal::SpecRefused && number.SpecWhy() == kNever
		&& number.Value() == 5.0, "a number box with the spec's check refuses what the check refuses, with its reason");
	Check(number.Handle(LetterEvent(',')) == NumberAnswer::Refused && number.Refusal() == NumberRefusal::UseADot && number.SpecWhy() == 0,
		"and a later refusal of another kind gives no spec's reason");
	number.Load(5.0, rule);
	number.Handle(LetterEvent('a', true));
	number.Handle(LetterEvent('7'));
	Check(number.Handle(KeyEvent(Key::Enter)) == NumberAnswer::Kept && number.Value() == 7.0 && number.SpecWhy() == 0,
		"loaded again with no check, it keeps no old check: the check belongs to the load");

	TextField text;

	text.Load((const unsigned char *)"old", 3, 63, 0, NeverKeepText, 0);
	text.Handle(LetterEvent('x'));
	Check(text.Handle(KeyEvent(Key::Enter)) == TextAnswer::Refused && text.Refusal() == TextRefusal::SpecRefused && text.SpecWhy() == kNever,
		"a text field with the spec's check refuses what the check refuses, with its reason");
	Check(text.Handle(LetterEvent(1)) == TextAnswer::Refused && text.Refusal() == TextRefusal::NotAFontLetter && text.SpecWhy() == 0,
		"and a later refusal of another kind gives no spec's reason");
	text.Load((const unsigned char *)"old", 3, 63, 0);
	text.Handle(LetterEvent('x'));
	Check(text.Handle(KeyEvent(Key::Enter)) == TextAnswer::Kept && text.SpecWhy() == 0, "and loaded again with no check, it keeps no old one");
}

int main()
{
	TestBoxesForgetOldChecks();
	TestLoading();
	TestEveryKind();
	TestHostileValues();
	TestTheWords();
	TestFailingSafe();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
