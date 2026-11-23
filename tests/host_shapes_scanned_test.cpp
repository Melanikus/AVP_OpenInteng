// The test for the list of maps already searched for object shapes (src/host/shapes_scanned.h) and the ini section it lives in (first_launch_ui.h
// FirstLaunch_IniWithSection): read from the ini's text as Windows would find the section, its bad lines not taken and named, written back with every other
// byte of the file kept, a round trip, the most it holds, and every refusal. Written 2026-11-23 by Melanikus (Drake) for AVP_OpenInteng (the owner's rule:
// "save a list of map shas that it already searched ... whenever any new map not in its ini is found it'll scan it too"). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "first_launch_ui.h"
#include "shapes_scanned.h"

using namespace host;

static int checksFailed = 0;
static int checksRun = 0;

static void Check(bool good, std::string const &what)
{
	checksRun++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::string Sha(int k)   // a made-up SHA-256: 64 hex letters from a number
{
	char text[65];

	for (int i = 0; i < 64; i++)
	{
		text[i] = "0123456789abcdef"[(k * 7 + i * 13 + (k >> (i % 16))) & 15];
	}

	text[64] = 0;
	return text;
}

static void TheReading()
{
	std::string const shipped = "; AVP_OpenInteng.ini\r\n[Editor]\r\nAutoPlot=1\r\n";
	ShapesScannedList none = ShapesScanned_Read(shipped);
	Check(!none.present && none.entries.empty() && none.refused.empty(), "the zip's ini: no list (the player hasn't said Yes)");

	ShapesScannedList empty = ShapesScanned_Read(shipped + "[shapesscanned]\r\n");
	Check(empty.present && empty.entries.empty(), "an empty [ShapesScanned] (any case): there, with no map on it");

	std::string const ini = "[Editor]\r\nAutoPlot=1\r\n\r\n[ShapesScanned]\r\n; a comment\r\n" + Sha(1) + "=Genshd1.RIF\r\n\r\n  " + Sha(2) +
		" = Derelict.RIF  \r\n" + std::string(64, 'A') + "=Upper.rif\r\n[Other]\r\n" + Sha(3) + "=NotThis.rif\r\n";
	ShapesScannedList list = ShapesScanned_Read(ini);
	Check(list.present && list.entries.size() == 3 && list.entries[0].sha256 == Sha(1) && list.entries[0].name == "Genshd1.RIF" && list.entries[1].sha256 == Sha(2) &&
		list.entries[1].name == "Derelict.RIF" && list.entries[2].sha256 == std::string(64, 'a'), "its lines in order, spaces around them dropped, the key in small letters, "
		"comments and empty lines skipped, another section's line not taken (" + std::to_string(list.entries.size()) + ")");
	Check(ShapesScanned_Has(list, Sha(2)) && ShapesScanned_Has(list, std::string(64, 'A')) && !ShapesScanned_Has(list, Sha(3)), "Has: on the list (any case), "
		"or not");

	std::string const bad = "[ShapesScanned]\r\n" + Sha(1).substr(0, 63) + "=short\r\n" + Sha(1) + "0=long\r\n" + std::string(63, 'a') + "g=nothex\r\n" + Sha(4) +
		"=one\r\n" + Sha(4) + "=again\r\n=noKey\r\n";
	ShapesScannedList refused = ShapesScanned_Read(bad);
	Check(refused.entries.size() == 1 && refused.entries[0].name == "one" && refused.refused.size() == 5, "63 or 65 letters, a letter not hex, a repeat and no key: "
		"not taken, each named (" + std::to_string(refused.refused.size()) + ")");

	std::string const hostile = "[ShapesScanned]\r\n" + Sha(5) + "=evil\x1b[2J\xff%s.rif\r\n";
	ShapesScannedList h = ShapesScanned_Read(hostile);
	Check(h.entries.size() == 1 && h.entries[0].name == "evil?[2J?%s.rif", "a hostile name: by the safe text rule (\"" + (h.entries.empty() ? std::string() :
		h.entries[0].name) + "\")");

	std::string many = "[ShapesScanned]\n";
	for (int k = 0; k < 4100; k++)
	{
		many += Sha(1000 + k) + "=m" + std::to_string(k) + ".rif\n";
	}
	ShapesScannedList lots = ShapesScanned_Read(many);
	Check(lots.entries.size() == kShapesScannedMost && lots.refused.size() == 4100 - kShapesScannedMost, "4,100 lines: the first 4,096 taken, the rest named (" +
		std::to_string(lots.entries.size()) + ", " + std::to_string(lots.refused.size()) + ")");

	ShapesScannedList twice = ShapesScanned_Read("[ShapesScanned]\n" + Sha(6) + "=a\n[ShapesScanned]\n" + Sha(7) + "=b\n");
	Check(twice.entries.size() == 1 && twice.entries[0].sha256 == Sha(6), "two [ShapesScanned] sections: only the first, as Windows reads the first");
}

static void TheWriting()
{
	std::vector<ShapesScannedEntry> const entries = { { Sha(1), "Genshd1.RIF" }, { std::string(64, 'B'), "Derelict.RIF" } };
	std::string const shipped = "; AVP_OpenInteng.ini\r\n[Editor]\r\nAutoPlot=1\r\n";
	std::string out;

	Check(ShapesScanned_Write(shipped, entries, &out) && out.compare(0, shipped.size(), shipped) == 0 &&
		out.substr(shipped.size()) == "[ShapesScanned]\r\n; the maps Open Inteng has searched for object shapes (it writes this list; a map not on it is searched at the "
		"next start)\r\n" + Sha(1) + "=Genshd1.RIF\r\n" + std::string(64, 'b') + "=Derelict.RIF\r\n", "the zip's ini: the section added at its end, every byte before "
		"it kept, CRLF, the key in small letters");

	ShapesScannedList back = ShapesScanned_Read(out);
	Check(back.present && back.entries.size() == 2 && back.entries[1].sha256 == std::string(64, 'b') && back.refused.empty(), "read back: the same list");

	std::string const between = "[Editor]\nAutoPlot=1\n\n[ShapesScanned]\n; mine\n" + Sha(9) + "=old.rif\n\n\n[Other]\nX=1\n";
	Check(ShapesScanned_Write(between, entries, &out) && out == "[Editor]\nAutoPlot=1\n\n[ShapesScanned]\n; the maps Open Inteng has searched for object shapes (it writes "
		"this list; a map not on it is searched at the next start)\n" + Sha(1) + "=Genshd1.RIF\n" + std::string(64, 'b') + "=Derelict.RIF\n\n\n[Other]\nX=1\n",
		"a section between others: its lines replaced, the empty lines after it and the next section kept, LF kept");

	Check(FirstLaunch_IniWithSection("[ShapesScanned]", "ShapesScanned", { "a=1" }, &out) && out == "[ShapesScanned]\r\na=1\r\n",
		"a header at the very end, with no new line: one put after it");
	Check(FirstLaunch_IniWithSection("[Editor]\r\nX=1", "Other", { "a=1" }, &out) && out == "[Editor]\r\nX=1\r\n[Other]\r\na=1\r\n",
		"no such section, and no new line at the end: one added before the new section");
	Check(FirstLaunch_IniWithSection("[shapesscanned]\r\nold=1\r\n[ShapesScanned]\r\nkeep=2\r\n", "ShapesScanned", { "new=3" }, &out) &&
		out == "[shapesscanned]\r\nnew=3\r\n[ShapesScanned]\r\nkeep=2\r\n", "two sections of the name: only the first written");

	std::string kept = "untouched";
	std::vector<ShapesScannedEntry> tooMany(kShapesScannedMost + 1, { Sha(1), "x" });
	Check(!ShapesScanned_Write(shipped, tooMany, &kept) && kept == "untouched", "more than 4,096 entries: refused, nothing written");
	Check(!ShapesScanned_Write(shipped, { { "notahash", "x" } }, &kept) && kept == "untouched", "an entry that isn't 64 hex letters: refused");
	Check(!FirstLaunch_IniWithSection(shipped, "Shapes]Scanned", { "a" }, &kept) && !FirstLaunch_IniWithSection(shipped, "Shapes", { "a\r\nb=1" }, &kept) &&
		kept == "untouched", "a section with ']' or a line with a new line: refused");

	std::vector<ShapesScannedEntry> full;
	for (int k = 0; k < (int)kShapesScannedMost; k++)
	{
		full.push_back({ Sha(5000 + k), "level" + std::to_string(k) + ".rif" });
	}
	bool const wrote = ShapesScanned_Write(shipped, full, &out);
	ShapesScannedList fullBack = ShapesScanned_Read(out);
	Check(wrote && fullBack.entries.size() == kShapesScannedMost && fullBack.refused.empty() && out.compare(0, shipped.size(), shipped) == 0,
		"4,096 entries written and read back whole (" + std::to_string(out.size()) + " bytes)");

	std::vector<ShapesScannedEntry> const hostile = { { Sha(1), "evil\r\nDataFolder=C:\\x" } };
	Check(ShapesScanned_Write(shipped, hostile, &out) && out.find("\r\nDataFolder=") == std::string::npos && out.find("evil??DataFolder=C:\\x") != std::string::npos,
		"a name with a new line in it can't make a line of its own: written as '?'");
}

// THE BOUNDS (Vasquez's review of Drake's list, taken by Frost in shapes part 3, 11-23): a name kept to 64 bytes, read or written; the lines not taken named
// to a most and the rest counted; a line that would start a section refused by the section writer
static void TheBounds()
{
	std::string const longName(200, 'n');
	ShapesScannedList const read = ShapesScanned_Read("[ShapesScanned]\r\n" + Sha(1) + "=" + longName + ".rif\r\n");
	Check(read.entries.size() == 1 && read.entries[0].name == std::string(kShapesScannedMostName, 'n'), "a name of 204 letters in the ini: kept as its first 64 (" +
		std::to_string(read.entries.empty() ? 0 : read.entries[0].name.size()) + ")");
	Check(ShapesScanned_Name(longName).size() == kShapesScannedMostName && ShapesScanned_Name("Genshd1.RIF") == "Genshd1.RIF" &&
		ShapesScanned_Name("a\tb") == "a?b", "ShapesScanned_Name: the safe text rule, then 64 bytes at most");

	std::string out;
	Check(ShapesScanned_Write("", { { Sha(2), longName } }, &out) && out.find(std::string(kShapesScannedMostName, 'n') + "\r\n") != std::string::npos &&
		out.find(std::string(kShapesScannedMostName + 1, 'n')) == std::string::npos, "a name of 200 letters written: 64 of them");

	std::string many = "[ShapesScanned]\r\n";
	for (int k = 0; k < 40; k++)
	{
		many += "not a key " + std::to_string(k) + "\r\n";
	}
	ShapesScannedList const refused = ShapesScanned_Read(many);
	Check(refused.refused.size() == kShapesScannedMostRefused && refused.refusedMore == 40 - kShapesScannedMostRefused && refused.entries.empty(),
		"40 lines not taken: 16 named, 24 counted (" + std::to_string(refused.refused.size()) + ", " + std::to_string(refused.refusedMore) + ")");

	Check(!FirstLaunch_IniWithSection("", "ShapesScanned", { "[Editor]" }, &out) && !FirstLaunch_IniWithSection("", "ShapesScanned", { "  [x" }, &out) &&
		FirstLaunch_IniWithSection("", "ShapesScanned", { "a=[b]" }, &out), "the section writer refuses a line that starts (after spaces) with '[': it would be another "
		"section's header; a '[' later in a line is fine");
}

int main()
{
	TheReading();
	TheWriting();
	TheBounds();

	printf("checks %d; failed %d\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
