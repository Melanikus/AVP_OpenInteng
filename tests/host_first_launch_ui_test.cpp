// The test for the first-launch setup form (src/host/first_launch_ui.h): its three things said whether the game is found or not, the question and its buttons,
// every line inside the window, hostile names, and the one ini line it writes (DataFolder=) with every other byte of the file kept. Written 2026-11-23 by
// Melanikus (Drake) for AVP_OpenInteng (the owner: the community can't set up maps without avptools' shape_rifs, and doesn't know the folders). AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code".
//
// Pure: every letter 8 pixels wide, a line 16 high. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "first_launch_ui.h"

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

static int32_t Eight(unsigned char)
{
	return 8;
}

static bool Has(std::string const &text, char const *words)
{
	return text.find(words) != std::string::npos;
}

static std::vector<std::string> Lines(std::string const &text)
{
	std::vector<std::string> lines;
	size_t from = 0;

	for (;;)
	{
		size_t const end = text.find('\n', from);
		lines.push_back(text.substr(from, end == std::string::npos ? std::string::npos : end - from));

		if (end == std::string::npos)
		{
			return lines;
		}

		from = end + 1;
	}
}

static bool Inside(DialogRect const &part, int32_t width, int32_t height)
{
	return part.x >= 0 && part.y >= 0 && part.x + part.width <= width && part.y + part.height <= height;
}

static FirstLaunchFacts Found()
{
	FirstLaunchFacts facts;

	facts.programFolder = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Aliens versus Predator Classic\\Tools";
	facts.found = true;
	facts.gameFolder = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Aliens versus Predator Classic";
	facts.levels = 63;
	facts.huds = 23;
	facts.shapesToWrite = 240;
	return facts;
}

static FirstLaunchFacts NotFound()
{
	FirstLaunchFacts facts;

	facts.programFolder = "D:\\Downloads\\AVP_OpenInteng-1.0.0-alpha.7";
	facts.looked = { "D:\\Downloads" };
	return facts;
}

static void TheForm(FirstLaunchFacts const &facts, int32_t screenWidth, std::vector<std::string> const &buttons, std::vector<int32_t> const &answers,
	std::string const &what)
{
	DialogFormSpec const spec = FirstLaunch_Form(facts, Eight, 16, screenWidth);

	Check(spec.title == "Open Inteng", what + ": titled Open Inteng");
	Check(spec.width >= 400 && spec.width <= 760 && (screenWidth - 64 < 400 || spec.width <= screenWidth - 64), what + ": the window " +
		std::to_string(spec.width) + " wide, inside the screen with room");
	Check(spec.widgets.size() == 1 + buttons.size() && !spec.widgets.empty() && spec.widgets[0].type == DialogWidgetType::Label, what + ": the words, then " +
		std::to_string(buttons.size()) + " buttons");

	if (spec.widgets.size() != 1 + buttons.size())
	{
		return;
	}

	DialogWidget const &words = spec.widgets[0];
	bool fits = true;

	for (std::string const &line : Lines(words.text))
	{
		fits = fits && 1 + (int32_t)line.size() * 8 <= words.box.width;
	}

	Check(fits && Inside(words.box, spec.width, spec.height), what + ": every line of the words inside their box, inside the window (" +
		std::to_string(Lines(words.text).size()) + " lines)");

	bool named = true;
	bool placed = true;

	for (size_t k = 0; k < buttons.size(); k++)
	{
		DialogWidget const &button = spec.widgets[k + 1];

		named = named && button.type == DialogWidgetType::Button && button.text == buttons[k] && button.answer == answers[k];
		placed = placed && Inside(button.box, spec.width, spec.height) && button.box.y >= words.box.y + words.box.height &&
			(int32_t)button.text.size() * 8 + 2 <= button.box.width && (k == 0 || button.box.x >= spec.widgets[k].box.x + spec.widgets[k].box.width);
	}

	Check(named, what + ": its buttons and what each answers");
	Check(placed, what + ": each button under the words, inside the window, its word inside it, none over another");
}

static void TheWords()
{
	std::string const found = FirstLaunch_Words(Found());

	Check(found.find("Open Inteng is in: C:\\Program Files (x86)\\Steam\\steamapps\\common\\Aliens versus Predator Classic\\Tools\n\n") == 0,
		"1. where Open Inteng is, its whole path (the owner: \"Full paths here\")");
	Check(Has(found, "It belongs in your game's Tools folder: Steam's ...\\steamapps\\common\\Aliens versus Predator Classic\\Tools") &&
		Has(found, "C:\\Program Files (x86)\\Fox\\Aliens versus Predator\\Tools (C:\\Program Files\\... on 32-bit Windows)"),
		"2. where it belongs: Steam's Tools, or the original Fox install's");
	Check(Has(found, "Around it, in C:\\Program Files (x86)\\Steam\\steamapps\\common\\Aliens versus Predator Classic, it found 63 levels and 23 HUD files."),
		"3. what it found around it: \"found 63 levels and 23 HUD files\"");
	Check(Has(found, "Open Inteng can make the object shapes from your game's 63 levels, into C:\\Program Files (x86)\\Steam\\steamapps\\common\\Aliens versus "
		"Predator Classic\\shape_rifs, once. Nothing there is replaced. Make them now?"), "the shapes question, with the folder they go into");

	FirstLaunchFacts ini = Found();
	ini.fromIni = true;
	ini.gameFolder = "E:\\Games\\AvP";
	Check(Has(FirstLaunch_Words(ini), "In the game's folder that DataFolder= names, E:\\Games\\AvP, it found 63 levels"), "found where DataFolder= says: said so");

	FirstLaunchFacts some = Found();
	some.notOwn = { "derelict.RIF" };
	some.missing = { "hangar.RIF", "statue.RIF" };
	std::string const someWords = FirstLaunch_Words(some);
	Check(Has(someWords, " 1 isn't Steam's own: derelict.RIF.") && Has(someWords, " Missing: hangar.RIF, statue.RIF."), "a file that isn't Steam's own, and the "
		"missing ones, named");

	FirstLaunchFacts many = Found();
	for (int k = 0; k < 12; k++)
	{
		many.notOwn.push_back("level" + std::to_string(k) + ".RIF");
	}
	Check(Has(FirstLaunch_Words(many), " 12 aren't Steam's own: level0.RIF, level1.RIF, level2.RIF, level3.RIF, level4.RIF, level5.RIF, level6.RIF, level7.RIF "
		"and 4 more."), "a long list: 8 names, the rest counted");

	FirstLaunchFacts none = Found();
	none.shapesToWrite = 0;
	Check(!Has(FirstLaunch_Words(none), "Make them now?"), "no shapes to make: no question");

	std::string const notFound = FirstLaunch_Words(NotFound());
	Check(notFound.find("Open Inteng is in: D:\\Downloads\\AVP_OpenInteng-1.0.0-alpha.7\n\n") == 0 && Has(notFound, "It belongs in your game's Tools folder"),
		"not found: where it is, and where it belongs");
	Check(Has(notFound, "It found no levels around it. It looked in: D:\\Downloads.") && Has(notFound, "Move the Open Inteng folder into your game's Tools folder, "
		"or tell it where your game is (the folder that holds avp_rifs). Set the game's folder now?"), "not found: where it looked, how to fix it, and the offer");

	// HOSTILE NAMES: a folder or file name with control bytes, a new line, a high byte and a format
	FirstLaunchFacts hostile = NotFound();
	hostile.programFolder = "D:\\x\x1b[2J\ny\xff%s";
	hostile.looked = { "D:\\%n\r" };
	std::string const hostileWords = FirstLaunch_Words(hostile);
	bool printable = true;

	for (char const letter : hostileWords)
	{
		printable = printable && (letter == '\n' || ((unsigned char)letter >= 32 && (unsigned char)letter <= 126));
	}

	Check(printable && Has(hostileWords, "Open Inteng is in: D:\\x?[2J?y?%s\n") && Has(hostileWords, "It looked in: D:\\%n?."), "hostile names: every byte outside "
		"the space to the tilde a '?', %s and %n the letters they are");
}

static void TheIniLine()
{
	std::string out;
	// the zip's own ini, as shipped: comments, CRLF, [Editor] with AutoPlot=1
	std::string const shipped = "; AVP_OpenInteng.ini\r\n; DataFolder: the folder that holds avp_rifs\r\n[Editor]\r\nPlotPreview=1\r\nAutoPlot=1\r\n";

	Check(FirstLaunch_IniWithLine(shipped, "Editor", "DataFolder", "E:\\Games\\AvP", &out) &&
		out == "; AVP_OpenInteng.ini\r\n; DataFolder: the folder that holds avp_rifs\r\n[Editor]\r\nPlotPreview=1\r\nAutoPlot=1\r\nDataFolder=E:\\Games\\AvP\r\n",
		"the zip's ini: DataFolder= added at the end of [Editor], every other byte kept, CRLF");

	std::string const two = "[Editor]\r\nAutoPlot=1\r\n\r\n[Other]\r\nX=1\r\n";
	Check(FirstLaunch_IniWithLine(two, "Editor", "DataFolder", "E:\\AvP", &out) && out == "[Editor]\r\nAutoPlot=1\r\nDataFolder=E:\\AvP\r\n\r\n[Other]\r\nX=1\r\n",
		"a section followed by an empty line and another: added after the section's last line, before the empty one");

	std::string const has = "[editor]\r\n  datafolder = C:\\Old\r\nAutoPlot=1\r\n";
	Check(FirstLaunch_IniWithLine(has, "Editor", "DataFolder", "E:\\New", &out) && out == "[editor]\r\nDataFolder=E:\\New\r\nAutoPlot=1\r\n",
		"a DataFolder= there already (any case, spaces): that line replaced, nothing else");

	std::string const elsewhere = "[Other]\nDataFolder=C:\\NotThis\n[Editor]\nAutoPlot=1\n";
	Check(FirstLaunch_IniWithLine(elsewhere, "Editor", "DataFolder", "E:\\AvP", &out) && out == "[Other]\nDataFolder=C:\\NotThis\n[Editor]\nAutoPlot=1\nDataFolder=E:\\AvP\n",
		"a DataFolder= in another section left alone; an LF file gets LF");

	std::string const commented = "[Editor]\r\n;DataFolder=C:\\Commented\r\n";
	Check(FirstLaunch_IniWithLine(commented, "Editor", "DataFolder", "E:\\AvP", &out) && out == "[Editor]\r\n;DataFolder=C:\\Commented\r\nDataFolder=E:\\AvP\r\n",
		"a commented DataFolder= kept as a comment, the new line under it");

	Check(FirstLaunch_IniWithLine("", "Editor", "DataFolder", "E:\\AvP", &out) && out == "[Editor]\r\nDataFolder=E:\\AvP\r\n", "an empty file: the section and its line");
	Check(FirstLaunch_IniWithLine("[Other]\r\nX=1", "Editor", "DataFolder", "E:\\AvP", &out) && out == "[Other]\r\nX=1\r\n[Editor]\r\nDataFolder=E:\\AvP\r\n",
		"no [Editor], and no new line at the end: one added before the new section");
	Check(FirstLaunch_IniWithLine("[Editor]\r\nAutoPlot=1", "Editor", "DataFolder", "E:\\AvP", &out) && out == "[Editor]\r\nAutoPlot=1\r\nDataFolder=E:\\AvP\r\n",
		"the section's last line without its new line: one added before the new line");
	Check(FirstLaunch_IniWithLine("\xEF\xBB\xBF[Editor]\r\nAutoPlot=1\r\n", "Editor", "DataFolder", "E:\\AvP", &out) &&
		out == "\xEF\xBB\xBF[Editor]\r\nAutoPlot=1\r\nDataFolder=E:\\AvP\r\n", "a UTF-8 mark at the start: [Editor] still found, the mark kept");
	Check(FirstLaunch_IniWithLine("[Editor]\r\nDataFolder=A\r\n[Editor]\r\nDataFolder=B\r\n", "Editor", "DataFolder", "E:\\AvP", &out) &&
		out == "[Editor]\r\nDataFolder=E:\\AvP\r\n[Editor]\r\nDataFolder=B\r\n", "two [Editor] sections: only the first, the one Windows reads, written");

	std::string kept = "untouched";
	Check(!FirstLaunch_IniWithLine(shipped, "Editor", "DataFolder", "E:\\AvP\r\nLevel=evil.rif", &kept) && kept == "untouched", "a value with a new line "
		"(another line hidden in it): refused, nothing written");
	Check(!FirstLaunch_IniWithLine(shipped, "Editor", "Data=Folder", "E:\\AvP", &kept) && !FirstLaunch_IniWithLine(shipped, "Edi]tor", "DataFolder", "x", &kept) &&
		!FirstLaunch_IniWithLine(shipped, "Editor", "DataFolder", std::string("E:\\A\0B", 6), &kept) && !FirstLaunch_IniWithLine(shipped, "", "DataFolder", "x", &kept) &&
		kept == "untouched", "a key with '=', a section with ']', a value with a 0, an empty section: refused");
	Check(FirstLaunch_IniWithLine(shipped, "Editor", "DataFolder", "E:\\%s %n\\AvP", &out) && Has(out, "DataFolder=E:\\%s %n\\AvP\r\n"),
		"a folder named with %s and %n: written as the letters they are");
}

int main()
{
	TheWords();
	TheForm(Found(), 1280, { "Yes", "Not now" }, { kFirstLaunchYes, kFirstLaunchNotNow }, "found, shapes to make");
	TheForm(NotFound(), 1024, { "Browse...", "Not now" }, { kFirstLaunchBrowse, kFirstLaunchNotNow }, "not found");

	FirstLaunchFacts made = Found();
	made.shapesToWrite = 0;
	TheForm(made, 640, { "Ok" }, { kDialogOk }, "found, nothing to make, a 640-pixel screen");
	TheForm(NotFound(), 200, { "Browse...", "Not now" }, { kFirstLaunchBrowse, kFirstLaunchNotNow }, "not found, a 200-pixel screen");

	TheIniLine();

	printf("checks %d; failed %d\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
