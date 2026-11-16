// The test for src/host's object header record (object_header.h): the six boxes the old editor's dialog 129 shows for an object's header,
// and only those, read-only, with each difficulty's box the opposite of the bit the file keeps. Written 2026-11-14 by Melanikus (Dietrich)
// for AVP_OpenInteng (the two-dev sprint, D1). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no level: each header is made here, some with hostile names. Each check's words say what it
// proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "field_panel_host.h"
#include "object_header.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// The six boxes, in the order the record holds them: their words, and the bit each shows (chunk_object.hpp:25-27, :34-36)
struct Box
{
	char const *name;
	uint32_t bit;
};

static Box const kBoxes[6] =
{
	{ "Game mode: Marine", 0x00000800 },
	{ "Game mode: Alien", 0x00001000 },
	{ "Game mode: Predator", 0x00002000 },
	{ "Difficulty: Easy", 0x00100000 },
	{ "Difficulty: Medium", 0x00200000 },
	{ "Difficulty: Hard", 0x00400000 },
};

static ObjectHeader AHeader(std::string const &name, uint32_t flags)
{
	ObjectHeader header;

	header.name = name;
	memset(header.id, 0x5A, sizeof(header.id));
	header.flags = flags;
	return header;
}

// Whether box i shows as ticked: the bit it shows is set in its value's word
static bool Ticked(FieldRecord const &record, size_t i)
{
	return (record.values.at(i).word & record.list.at(i).bit) != 0;
}

static void TestTheBoxes()
{
	puts("The boxes dialog 129 shows, and only those:");

	FieldRecord record;

	Check(BuildObjectHeaderRecord(AHeader("door_main", 0), &record), "a header's record is built");

	bool const six = record.list.size() == 6 && record.values.size() == 6;

	Check(six, "it has six boxes, each with its value");

	bool names = six, bits = six, kinds = six, readOnly = six, helps = six;

	if (six)
	{
		for (size_t i = 0; i < 6; i++)
		{
			names = strcmp(record.list[i].name, kBoxes[i].name) == 0 && names;
			bits = record.list[i].bit == kBoxes[i].bit && bits;
			kinds = record.list[i].kind == ui::FieldKind::Bit && kinds;
			readOnly = record.list[i].readOnly && readOnly;
			helps = record.list[i].help != 0 && record.list[i].help[0] != 0 && helps;
		}
	}

	Check(names, "in the old editor's order: Game mode Marine, Alien and Predator, then Difficulty Easy, Medium and Hard");
	Check(bits, "each shows its own bit: 0x800, 0x1000, 0x2000, then 0x100000, 0x200000, 0x400000");
	Check(kinds && readOnly, "each is a bit box, and read-only");
	Check(helps, "each has a help line");
	Check(record.title == "door_main", "the title is the object's name");

	ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };

	Check(ui::CheckFieldList(record.list.data(), record.list.size(), &problem), "the panel's own check takes the list");
}

static void TestTheTicks()
{
	puts("What each box shows:");

	FieldRecord record;

	BuildObjectHeaderRecord(AHeader("a", 0), &record);
	Check(record.list.size() == 6 && !Ticked(record, 0) && !Ticked(record, 1) && !Ticked(record, 2), "with no flags, no game mode is ticked");
	Check(record.list.size() == 6 && Ticked(record, 3) && Ticked(record, 4) && Ticked(record, 5),
		"and every difficulty is: no \"not at this difficulty\" bit is set");

	BuildObjectHeaderRecord(AHeader("a", 0x00000800 | 0x00008000 | 0x00100000), &record);
	Check(record.list.size() == 6 && Ticked(record, 0) && !Ticked(record, 1) && !Ticked(record, 2), "the Marine bit ticks Marine alone");
	Check(record.list.size() == 6 && !Ticked(record, 3) && Ticked(record, 4) && Ticked(record, 5), "the bit \"not at Easy\" unticks Easy alone");

	BuildObjectHeaderRecord(AHeader("a", 0x00001000 | 0x00002000 | 0x00200000 | 0x00400000), &record);
	Check(record.list.size() == 6 && !Ticked(record, 0) && Ticked(record, 1) && Ticked(record, 2), "the Alien and Predator bits tick those two");
	Check(record.list.size() == 6 && Ticked(record, 3) && !Ticked(record, 4) && !Ticked(record, 5),
		"the bits \"not at Medium\" and \"not at Hard\" untick those two");

	BuildObjectHeaderRecord(AHeader("a", 0xFFFFFFFFu), &record);
	Check(record.list.size() == 6 && Ticked(record, 0) && Ticked(record, 1) && Ticked(record, 2) && !Ticked(record, 3) && !Ticked(record, 4) &&
		!Ticked(record, 5), "with every bit set, every game mode is ticked and no difficulty is");
	Check(record.list.size() == 6 && record.values[0].word == 0xFFFFFFFFu && record.values[3].word == (0xFFFFFFFFu ^ 0x00700000u),
		"each box's word is the file's flags: as they are for a game mode, and with the three difficulty bits the opposite for a difficulty");
}

static void TestHostileNames()
{
	puts("A name from a level is hostile:");

	FieldPanelHost host;
	FieldRecord record;
	ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };

	// The longest name the game keeps: 64 bytes, none of them 0
	std::string const longest(64, 'N');

	BuildObjectHeaderRecord(AHeader(longest, 0), &record);
	Check(record.title == longest, "a name of the game's full 64 bytes is the record's title as it is");
	Check(host.Show(record.title.c_str(), record.list.data(), record.list.size(), record.values.data(), &problem) &&
		host.Title().size() <= kFieldPanelHostTitleMost, "the host shows it, cut to its most bytes");

	std::string odd = "door";

	odd += (char)0x01;
	odd += (char)0xC0;
	odd += "x";

	BuildObjectHeaderRecord(AHeader(odd, 0), &record);
	Check(host.Show(record.title.c_str(), record.list.data(), record.list.size(), record.values.data(), &problem) &&
		host.Title().find((char)0x01) == std::string::npos && host.Title().find((char)0xC0) == std::string::npos,
		"a name with a control byte and a byte the font hasn't got is shown with neither");

	BuildObjectHeaderRecord(AHeader("", 0), &record);
	Check(record.title.empty() && record.list.size() == 6, "a header with no name has no title of its own, and its six boxes (the adapter gives it the ID)");
	host.Clear();
}

static void TestTheRecordStartsAgain()
{
	puts("The record starts again:");

	FieldRecord record;

	record.title = "left over";
	record.list.resize(9);
	record.values.resize(9);
	record.text.push_back("words");

	Check(BuildObjectHeaderRecord(AHeader("b", 0), &record) && record.list.size() == 6 && record.values.size() == 6 && record.title == "b",
		"what the record held before is let go of");
	Check(!BuildObjectHeaderRecord(AHeader("b", 0), 0), "no place for the record gives back false");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheBoxes();
	TestTheTicks();
	TestHostileNames();
	TestTheRecordStartsAgain();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
