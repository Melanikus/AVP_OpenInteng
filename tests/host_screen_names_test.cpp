// host_screen_names_test.cpp: what the editor draws never shows a full path (src/host/screen_names.h, Bishop's privacy item for alpha.1: Sound Edit's title and "Saved a copy:"
// showed C:\Users\<name>\... in screenshots). Every full path in the words is cut to its file's or folder's name; the rest of the words, relative paths and colons that are not a
// drive's stay as they were; hostile words (a level's names) never read past their end. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>

#include "screen_names.h"
#include "status_words.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

static void Cut(char const *given, char const *wanted, char const *why)
{
	std::string const got = host::ScreenNames_Cut(given);

	Check(got == wanted, std::string(why) + ": \"" + got + "\"");
}

int main()
{
	printf("FULL PATHS CUT TO THEIR NAMES\n");
	Cut("Saved a copy: C:\\Users\\bob\\Documents\\AVP_OpenInteng\\Invasion.RIF", "Saved a copy: Invasion.RIF", "a save's words keep the file's name only");
	Cut("Saved a copy: C:/Users/bob/AvP/avp_rifs/Invasion.RIF", "Saved a copy: Invasion.RIF", "forward slashes too (the harness's paths)");
	Cut("c:\\users\\bob\\x.rif", "x.rif", "a lower-case drive");
	Cut("Sound Edit - C:\\Users\\bob\\AvP\\sound\\", "Sound Edit - sound", "a folder ending with \\: its own name (Sound Edit's title)");
	Cut("Sound Edit - C:\\Users\\bob\\AvP\\sound", "Sound Edit - sound", "a folder without one: its name is the last part");
	Cut("C:\\Users\\Bob Smith\\My Documents\\x.rif", "x.rif", "folders with spaces in their names");
	Cut("from C:\\a\\b.rif to D:\\c\\d.rif", "from b.rif to d.rif", "two paths in one line");
	Cut("x.rif and E:\\y\\z.rif", "x.rif and z.rif", "a colon ends a part: the second path is a path of its own");
	Cut("\\\\server\\share\\bob\\x.rif", "x.rif", "a network path");
	Cut("the editor couldn't make C:\\x\\run\\AVP_OpenInteng.exe\\AVP_OpenInteng (Windows error 3)", "the editor couldn't make AVP_OpenInteng (Windows error 3)",
		"a path followed by words: the words stay");
	Cut("\"C:\\Users\\bob\\x.rif\"", "\"x.rif\"", "a path in quotes");
	Cut("C:\\Users\\bob\\My Level.rif", "My Level.rif", "a name with a space: its first word is the name's, the rest are kept words");

	printf("WORDS THAT ARE NOT A FULL PATH STAY\n");
	Cut("Screenshot saved: Documents\\AVP_OpenInteng\\shots\\Invasion-20261118-120000.png", "Screenshot saved: Documents\\AVP_OpenInteng\\shots\\Invasion-20261118-120000.png",
		"a relative path (the screenshot key's own place words)");
	Cut("picture graphics/Envrnmts/Invasion/07 128.RIM put on 1 polygon", "picture graphics/Envrnmts/Invasion/07 128.RIM put on 1 polygon", "a picture's game path");
	Cut("Zoom: in, the view is 150% of the game's own", "Zoom: in, the view is 150% of the game's own", "a colon in words");
	Cut("12:30 Mode: Texture Map", "12:30 Mode: Texture Map", "a time and a label");
	Cut("ABC:\\x", "ABC:\\x", "a drive letter inside a word is not a drive");
	Cut("C:\\", "C:\\", "a drive alone names no one");
	Cut("", "", "empty words");
	Cut("C:", "C:", "a drive letter and a colon only");

	printf("HOSTILE WORDS\n");
	{
		std::string many;

		for (int i = 0; i < 20000; i++)
		{
			many += "bob\\";
		}

		std::string const got = host::ScreenNames_Cut("C:\\" + many + "x.rif");

		Check(got == "x.rif", "20000 folders deep: only the name (" + std::to_string(got.size()) + " letters)");
	}
	{
		std::string slashes = "C:";

		slashes.append(5000, '\\');

		std::string const got = host::ScreenNames_Cut(slashes);

		Check(got.find("bob") == std::string::npos && got.size() <= slashes.size(), "a drive and 5000 separators: ends, nothing added (" + std::to_string(got.size()) + ")");
	}
	{
		std::string repeats;

		for (int i = 0; i < 10000; i++)
		{
			repeats += "C:\\Users\\bob\\x.rif ";
		}

		std::string const got = host::ScreenNames_Cut(repeats);

		Check(got.find("bob") == std::string::npos && got.find("Users") == std::string::npos, "10000 paths in a row: none keeps a folder");
	}
	{
		std::string const ends = std::string("C:\\Users\\bob\\x") + '\0' + "C:\\Users\\bob\\y.rif";
		std::string const got = host::ScreenNames_Cut(ends);

		Check(got.find("bob") == std::string::npos, "a zero byte inside the words: both paths cut, nothing past the end read");
	}

	printf("THE STATUS LINE\n");
	{
		StatusWords status;

		status = "Saved a copy: C:\\Users\\bob\\Documents\\AVP_OpenInteng\\Invasion.RIF";

		char const *const now = status;

		Check(now != nullptr && std::string(now) == "Saved a copy: Invasion.RIF", "the status line's copy has the name only");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
