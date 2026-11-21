// The test for the three Options switches (src/host/editor_options.h): the defaults, a round trip through a real settings file, a value that is read only when it is exactly 0 or 1, and hostile input (long, signed,
// spaced, empty, binary-looking values, a missing file, no file name, another key). Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include <string>

#include <windows.h>

#include "editor_options.h"

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

static std::string TempFile(char const *name)
{
	char folder[MAX_PATH];

	GetTempPathA(MAX_PATH, folder);
	return std::string(folder) + name;
}

static void Put(std::string const &file, char const *key, char const *value)
{
	WritePrivateProfileStringA("Editor", key, value, file.c_str());
}

static std::string ReadWhole(char const *path)
{
	std::string text;
	FILE *file = nullptr;

	if (fopen_s(&file, path, "rb") != 0 || !file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0 && text.size() < (1u << 20))
	{
		text.append(block, got);
	}

	fclose(file);
	return text;
}

// THE START SIZE AND WINDOWED (the owner's ruling 11-18): [Editor] StartWidth=, StartHeight=, Windowed= of our own settings file, read as hostile input, and the default file the zip ships
static void TestTheStart()
{
	int32_t number = 7;

	Check(EditorOptions_StartNumber("1280", &number) && number == 1280 && EditorOptions_StartNumber("720", &number) && number == 720 && EditorOptions_StartNumber("16384", &number) &&
		number == 16384, "a size is read: 1280, 720, and 16384 the most");
	number = 7;
	Check(!EditorOptions_StartNumber("0", &number) && !EditorOptions_StartNumber("16385", &number) && !EditorOptions_StartNumber("-640", &number) && !EditorOptions_StartNumber("+640", &number) &&
		!EditorOptions_StartNumber("640px", &number) && !EditorOptions_StartNumber("6 40", &number) && !EditorOptions_StartNumber("", &number) && !EditorOptions_StartNumber("123456", &number) &&
		!EditorOptions_StartNumber("99999999999999999999", &number) && !EditorOptions_StartNumber(nullptr, &number) && number == 7,
		"and nothing else is a size: 0, past 16384, a sign, letters after it, a space in it, empty, six digits or more, none; the value is untouched");
	Check(EditorOptions_StartFlag("true") == 1 && EditorOptions_StartFlag("True") == 1 && EditorOptions_StartFlag("TRUE") == 1 && EditorOptions_StartFlag("1") == 1 &&
		EditorOptions_StartFlag("false") == 0 && EditorOptions_StartFlag("FALSE") == 0 && EditorOptions_StartFlag("0") == 0, "Windowed is true or 1, false or 0, in any case");
	Check(EditorOptions_StartFlag("yes") == -1 && EditorOptions_StartFlag("truex") == -1 && EditorOptions_StartFlag(" true") == -1 && EditorOptions_StartFlag("") == -1 &&
		EditorOptions_StartFlag("2") == -1 && EditorOptions_StartFlag(nullptr) == -1, "and nothing else: yes, truex, a space before it, empty, 2, none");

	std::string const file = TempFile("avp_open_inteng_start_test.ini");
	char const *why = "x";

	DeleteFileA(file.c_str());
	EditorStart start = EditorOptions_Start(file.c_str(), &why);
	Check(!start.sizeGiven && !start.windowed && why == nullptr, "no file: no size of its own and not windowed, nothing refused");

	Put(file, "StartWidth", "1280");
	Put(file, "StartHeight", "720");
	Put(file, "Windowed", "true");
	start = EditorOptions_Start(file.c_str(), &why);
	Check(start.sizeGiven && start.width == 1280 && start.height == 720 && start.windowed && why == nullptr, "StartWidth=1280, StartHeight=720, Windowed=true: 1280 x 720, windowed");

	Put(file, "StartHeight", nullptr);
	start = EditorOptions_Start(file.c_str(), &why);
	Check(!start.sizeGiven && start.windowed && why == nullptr, "a width with no height is no size (both or neither)");

	Put(file, "StartHeight", "72O");
	Put(file, "Windowed", "maybe");
	start = EditorOptions_Start(file.c_str(), &why);
	Check(!start.sizeGiven && !start.windowed && why != nullptr && strcmp(why, "StartHeight") == 0, "a height that isn't a number and a Windowed that isn't a word: neither taken, and the first named");

	Put(file, "StartHeight", std::string(300, '7').c_str());
	Put(file, "Windowed", "1");
	start = EditorOptions_Start(file.c_str(), &why);
	Check(!start.sizeGiven && start.windowed && why != nullptr, "a value of 300 digits is cut to the reader's room, so it is not a number");
	DeleteFileA(file.c_str());

	// the default settings file the release zip ships beside the exe (release.yml), and the acceptance's copy of it (acceptance-start-size-release)
	std::string const shipped = ReadWhole("dist/AVP_OpenInteng.ini");
	std::string const copy = ReadWhole("tests/scenarios/release-ini/AVP_OpenInteng.ini");
	Check(!shipped.empty() && shipped == copy, "dist/AVP_OpenInteng.ini is there, and the acceptance's copy is byte for byte the same");

	char full[MAX_PATH] = "";
	start = EditorOptions_Start(GetFullPathNameA("dist/AVP_OpenInteng.ini", MAX_PATH, full, nullptr) ? full : "", &why);
	Check(start.sizeGiven && start.width == 1280 && start.height == 720 && start.windowed && why == nullptr, "and it says 1280 x 720, windowed (the owner, 11-18)");

	char data[16];
	GetPrivateProfileStringA("Editor", "DataFolder", "none", data, sizeof(data), full);
	Check(strcmp(data, "none") == 0, "it names no data folder: that is each mapper's own (the editor says so at its start)");

	// THE AUTO-PLOT SHIPS ON (the owner 11-21, "Ship AutoPlot=1 in the ini"): Waypoint's Alien auto waypoint menu shows only with AutoPlot=1 under [Editor], read at the
	// editor's start exactly so (editor_main.cpp: GetPrivateProfileIntA("Editor", "AutoPlot", 0, ...) == 1; autoplot_switch.h). The gap closers stay off, opt-in:
	// AutoPlotClosers is on only with the text "1" (autoplot_closers.h), and the shipped file does not say it
	Check(GetPrivateProfileIntA("Editor", "AutoPlot", 0, full) == 1, "it shows the Alien auto waypoint menu: AutoPlot=1 under [Editor] (the owner 11-21)");

	char closers[16];
	GetPrivateProfileStringA("Editor", "AutoPlotClosers", "", closers, sizeof(closers), full);
	Check(strcmp(closers, "1") != 0, "and it leaves the gap closers off: no AutoPlotClosers=1");
}

int main()
{
	TestTheStart();

	EditorOptions const start = EditorOptions_Defaults();

	Check(start.selectByPolygons && !start.preferBmp && !start.polygonViewDistance, "the defaults: Select by polygons on (the editor's click has always been the hit test), the other two off");

	std::string const file = TempFile("avp_open_inteng_options_test.ini");

	DeleteFileA(file.c_str());

	char const *why = "x";
	EditorOptions loaded = EditorOptions_Load(file.c_str(), &why);

	Check(loaded.selectByPolygons == start.selectByPolygons && loaded.preferBmp == start.preferBmp && loaded.polygonViewDistance == start.polygonViewDistance && why == nullptr, "no file: the defaults, no complaint");

	// the round trip
	Check(EditorOptions_Save(file.c_str(), "SelectByPolygons", false) && EditorOptions_Save(file.c_str(), "PreferBmp", true) && EditorOptions_Save(file.c_str(), "PolygonViewDistance", true), "the three keys are written");
	loaded = EditorOptions_Load(file.c_str(), &why);
	Check(!loaded.selectByPolygons && loaded.preferBmp && loaded.polygonViewDistance && why == nullptr, "and read back: off, on, on");
	Check(EditorOptions_Save(file.c_str(), "SelectByPolygons", true) && EditorOptions_Load(file.c_str(), nullptr).selectByPolygons, "a switch changed is written and read back");

	// the file holds what was written, as plain 0 and 1
	{
		char text[16];

		GetPrivateProfileStringA("Editor", "PreferBmp", "?", text, sizeof(text), file.c_str());
		Check(strcmp(text, "1") == 0, "the file holds the text 1");
	}

	// hostile values: read only when exactly 0 or 1
	char const *const bad[] = { "2", "-1", "01", "1 x", "yes", "true", "0x1", "", "999999999999999999999", "+1" };
	bool allRefused = true;

	for (char const *value : bad)
	{
		DeleteFileA(file.c_str());
		Put(file, "SelectByPolygons", value);
		Put(file, "PreferBmp", value);

		EditorOptions const got = EditorOptions_Load(file.c_str(), &why);
		bool const kept = got.selectByPolygons == start.selectByPolygons && got.preferBmp == start.preferBmp;

		// an empty value is the same as no value for the system: the key is missing then, which is no complaint
		if (value[0] != '\0')
		{
			bool const refused = kept && why != nullptr && strcmp(why, "SelectByPolygons") == 0;

			if (!refused)
			{
				printf("  (the value \"%s\" was not refused as it should be)\n", value);
			}

			allRefused = allRefused && refused;
		}
		else
		{
			allRefused = allRefused && kept;
		}
	}

	Check(allRefused, "a value that is not exactly 0 or 1 (2, -1, 01, text, 0x1, a long number, +1) is refused: the default stays and the first key is named");

	// Windows' own settings reader takes the spaces and tabs off both ends of a value, so " 1" is the text 1 by the time it is read: that is the system's rule, and it is still exactly a 1
	{
		DeleteFileA(file.c_str());
		Put(file, "PreferBmp", " 1 ");
		Check(EditorOptions_Load(file.c_str(), &why).preferBmp && why == nullptr, "a 1 with spaces around it is the text 1 for the system's reader: read as 1");
	}

	// a long line
	{
		std::string const lots(4000, '1');

		DeleteFileA(file.c_str());
		Put(file, "PolygonViewDistance", lots.c_str());
		loaded = EditorOptions_Load(file.c_str(), &why);
		Check(!loaded.polygonViewDistance && why != nullptr && strcmp(why, "PolygonViewDistance") == 0, "a value of 4000 ones is refused, never overrunning the buffer");
	}

	// the other calls
	Check(!EditorOptions_Save(file.c_str(), "Textured", true) && !EditorOptions_Save(file.c_str(), "", true) && !EditorOptions_Save(file.c_str(), nullptr, true) && !EditorOptions_Save(nullptr, "PreferBmp", true) && !EditorOptions_Save("", "PreferBmp", true),
		"another key, an empty key, no key, no file, an empty file name: all refused");

	loaded = EditorOptions_Load(nullptr, &why);
	Check(loaded.selectByPolygons && !loaded.preferBmp && !loaded.polygonViewDistance && why == nullptr, "no file name: the defaults");

	// the other keys of the file are left alone
	DeleteFileA(file.c_str());
	WritePrivateProfileStringA("Editor", "Textured", "0", file.c_str());
	EditorOptions_Save(file.c_str(), "PreferBmp", true);

	{
		char text[16];

		GetPrivateProfileStringA("Editor", "Textured", "?", text, sizeof(text), file.c_str());
		Check(strcmp(text, "0") == 0, "another key of the file is as it was");
	}

	DeleteFileA(file.c_str());
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
