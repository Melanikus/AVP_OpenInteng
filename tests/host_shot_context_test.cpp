// The test of a screenshot's context (shot_context.h, GitHub issue #78): the JSON's fields, the privacy scrub (no path, no user folder), hostile names (quotes, backslashes, control
// bytes, bytes above 127, overlong names, overlong lists), and the file's name. Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code". Each refusal or cut is checked with the reason it is there.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "shot_context.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Has(std::string const &text, std::string const &part)
{
	return text.find(part) != std::string::npos;
}

// plain ASCII from 0x20 to 0x7e only: what a JSON reader, a log or a mail can carry without a surprise
static bool PlainAscii(std::string const &text)
{
	for (char const c : text)
	{
		if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7e)
		{
			return false;
		}
	}

	return true;
}

// the JSON's quotes balance: every string closes (an escaped quote does not count), so no text broke out of its string
static bool StringsClose(std::string const &json)
{
	bool inside = false;

	for (size_t i = 0; i < json.size(); i++)
	{
		if (inside && json[i] == '\\')
		{
			i++;
			continue;
		}

		inside = json[i] == '"' ? !inside : inside;
	}

	return !inside;
}

static void PathsCase()
{
	// THE PRIVACY RULE: a log line holding a user folder comes out without it (Bishop / Melanikus 11-17: "strip any C:\Users\... from the log lines")
	std::string const line = "level: couldn't open C:\\Users\\Melanikus\\Documents\\AVP_OpenInteng\\edited\\Invasion.rif (in use)";
	std::string const scrubbed = host::Shot_ScrubPaths(line);

	Check(!Has(scrubbed, "Users") && !Has(scrubbed, "Melanikus") && !Has(scrubbed, "Documents") && !Has(scrubbed, "C:"), "privacy: a user folder in a log line is taken out (" + scrubbed + ")");
	Check(scrubbed == "level: couldn't open <path>\\Invasion.rif (in use)", "privacy: the file's name and the words around it stay");

	// forward slashes, a folder name with spaces, a network path, a path at the very start and end
	Check(host::Shot_ScrubPaths("D:/Games/Aliens versus Predator/avp_rifs/Stat101.RIF") == "<path>\\Stat101.RIF", "privacy: a forward-slash path with spaces in a folder's name keeps only the file");
	Check(host::Shot_ScrubPaths("from \\\\NAS-HOME\\share\\maps\\Genshd1.rif, read") == "from <path>\\Genshd1.rif, read", "privacy: a network path loses its server and share (the computer's name)");
	Check(host::Shot_ScrubPaths("C:\\") == "<path>\\", "privacy: a bare drive becomes <path>");
	Check(host::Shot_ScrubPaths("two: C:\\a\\x.rif and E:\\b\\y.rif") == "two: <path>\\x.rif and <path>\\y.rif", "privacy: two paths on one line are both cut");
	Check(host::Shot_ScrubPaths("ratio 3:2, not a path; ABC:\\x") == "ratio 3:2, not a path; ABC:\\x", "a colon that starts no drive path (a ratio, a word before it) is left alone");
	Check(host::Shot_ScrubPaths("no path here") == "no path here", "words with no path are untouched");
}

static void JsonStringCase()
{
	Check(host::Shot_JsonString("a\"b\\c", 64) == "\"a\\\"b\\\\c\"", "hostile name: a quote and a backslash are escaped (the name can't end its string)");
	Check(host::Shot_JsonString(std::string("x\ny\x01", 4), 64) == "\"x\\u000ay\\u0001\"", "hostile name: control bytes are written \\u00XX");
	Check(host::Shot_JsonString("\xe9t\xe9", 64) == "\"\\u00e9t\\u00e9\"", "hostile name: bytes above 127 are written \\u00XX (Latin-1), so the JSON is plain ASCII");
	Check(host::Shot_JsonString(std::string(1000, 'm'), 128) == "\"" + std::string(128, 'm') + "\"", "hostile name: an overlong name is cut to the bound");
	Check(host::Shot_JsonString(std::string("ab\0cd", 5), 64) == "\"ab\\u0000cd\"", "hostile name: a zero byte inside a name is written, not taken as its end");
}

static void JsonCase()
{
	host::ShotContext c;

	c.build = "0123456789abcdef";
	c.map = "C:\\Users\\Melanikus\\AppData\\Local\\Temp\\Invasion.RIF";
	c.levelBytes = 1988464;
	c.levelSha256 = std::string(64, 'a');
	c.screen = "Module Linking";
	c.pos[0] = 1500;
	c.pos[1] = -250;
	c.pos[2] = 3000;
	c.turn = 1024;
	c.base = "gARAGDOR";
	c.current = "Bend";
	c.picks = { "Door" };
	c.selected = { "Door", "evil\"},\"screen\":\"hacked" };
	c.status = "Screenshot saved: C:\\Users\\Melanikus\\Documents\\AVP_OpenInteng\\shots\\Invasion-20261121-090507.png";

	for (int i = 0; i < 30; i++)
	{
		c.log.push_back("line " + std::to_string(i) + " C:\\Users\\Someone Else\\x.txt");
	}

	for (int i = 0; i < 1100; i++)
	{
		c.visible.push_back("m" + std::to_string(i));
	}

	std::string const json = host::Shot_Json(c);

	Check(json.compare(0, 11, "{\"format\":1") == 0 && json.back() == '}', "the JSON is one object, format 1 first");
	Check(Has(json, "\"privacy\":\"paths and user names removed\""), "the JSON says what was taken out");
	Check(Has(json, "\"map\":\"Invasion.RIF\"") && !Has(json, "Temp"), "map: the level's file name only, never its folder");
	Check(Has(json, "\"screen\":\"Module Linking\"") && Has(json, "\"base\":\"gARAGDOR\"") && Has(json, "\"picks\":[\"Door\"]"), "screen, base and picks are named");
	Check(Has(json, "\"current\":\"Bend\""), "current: the module the camera is in");
	Check(Has(json, "\"pos\":{\"x\":1500,\"y\":-250,\"z\":3000}") && Has(json, "\"pos_metres\":{\"x\":1.500,\"y\":-0.250,\"z\":3.000}") && Has(json, "\"turn\":1024"), "pos in game units and in metres, and the turn");
	Check(!Has(json, "Users") && !Has(json, "Melanikus") && !Has(json, "Someone"), "privacy: no user folder or user name anywhere (map, status, log)");
	Check(Has(json, "\"status\":\"Screenshot saved: <path>\\\\Invasion-20261121-090507.png\""), "the status words stay, their path cut to the file");
	Check(Has(json, "\"line 10 <path>\\\\x.txt\"") && !Has(json, "\"line 9 ") && Has(json, "\"line 29 "), "log: only the last 20 lines (10 to 29)");
	Check(Has(json, "\"visible_more\":76") && Has(json, "\"m1023\"") && !Has(json, "\"m1024\""), "visible: at most 1024 names, the rest counted in visible_more");
	Check(Has(json, "evil\\\"},\\\"screen\\\":\\\"hacked") && StringsClose(json), "hostile name: a name made to break out of its string stays inside it");
	Check(PlainAscii(json), "the whole JSON is plain ASCII");
	Check(json.size() < 64 * 1024, "the JSON is bounded (" + std::to_string(json.size()) + " bytes with 1100 visible names and 30 log lines)");
}

static void FileNameCase()
{
	host::ShotTime when;

	when.year = 2026, when.month = 11, when.day = 21, when.hour = 9, when.minute = 5, when.second = 7;

	host::ShotTime badMonth = when, badYear = when, badSecond = when;

	badMonth.month = 13, badYear.year = 10000, badSecond.second = 60;

	Check(host::Shot_FileName("C:\\x\\Invasion.RIF", when, 0) == "Invasion-20261121-090507.png", "the file's name: <map>-<date>-<time>.png, no folder, no ending");
	Check(host::Shot_FileName("C:\\x\\Invasion.RIF", when, 7) == "Invasion-20261121-090507-7.png", "a second shot in the same second: -<number> before .png");
	Check(host::Shot_FileName("..\\..\\evil\x01 x.rif", when, 0) == "evil__x-20261121-090507.png", "hostile map name: no folder, no control byte or space in the file's name");
	Check(host::Shot_FileName("....rif", when, 0) == "level-20261121-090507.png", "a name with nothing usable left is \"level\"");
	Check(host::Shot_FileName("a.rif", when, 100).empty() && host::Shot_FileName("a.rif", when, -1).empty(), "a number outside 0 to 99 gives no name");
	Check(host::Shot_FileName("a.rif", badMonth, 0).empty() && host::Shot_FileName("a.rif", badYear, 0).empty() && host::Shot_FileName("a.rif", badSecond, 0).empty(),
		"a time outside the calendar gives no name (month 13, year 10000, second 60)");
	Check(host::Shot_FileName(std::string(200, 'L') + ".rif", when, 99).size() == 48 + 1 + 15 + 3 + 4, "an overlong map name is cut to 48");
}

static void LastLinesCase()
{
	std::vector<std::string> const lines = host::Shot_LastLines("half of a line\r\none\r\n\r\ntwo\nthree", true, 2);

	Check(lines.size() == 2 && lines[0] == "two" && lines[1] == "three", "log tail: the last 2 lines, line ends and empty lines taken off");
	Check(host::Shot_LastLines("half of a line\none", true, 20).size() == 1, "log tail cut from a file: its first part (maybe half a line) is left out");
	Check(host::Shot_LastLines("first\nsecond", false, 20).size() == 2, "log tail that is the whole file: its first line is kept");
	Check(host::Shot_LastLines("no line end at all", true, 20).empty(), "log tail cut inside one line: nothing (no whole line was read)");
}

int main()
{
	PathsCase();
	LastLinesCase();
	JsonStringCase();
	JsonCase();
	FileNameCase();

	printf("%s host_shot_context_test (%d failed)\n", checksFailed ? "FAILED" : "passed", checksFailed);
	return checksFailed ? 1 : 0;
}
