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

int main()
{
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
