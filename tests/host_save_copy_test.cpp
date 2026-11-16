// The test for where a saved copy goes (save_copy.h): the folder for copies is <Documents>\AVP_OpenInteng\ wherever the program is (Melanikus 2026-11-16: the
// editor now lives in the game's Tools folder), and nothing is ever placed inside the game's folder. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt. No window, none of the game, no file touched (the places are only worked out).

#include <stdio.h>
#include <string.h>

#include <string>

#include "save_copy.h"

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

int main()
{
	puts("The folder for copies:");
	Check(SaveCopyBase("C:\\Users\\Someone\\Documents") == "C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\", "Documents -> Documents\\AVP_OpenInteng\\");
	Check(SaveCopyBase("C:\\Users\\Someone\\Documents\\") == "C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\", "a Documents folder ending in a slash gives the same");
	Check(SaveCopyBase("C:/Users/Someone/Documents") == "C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\", "forward slashes come out as Windows writes them");
	Check(SaveCopyBase("") == "" && SaveCopyBase(nullptr) == "", "no Documents folder: no folder for copies");
	Check(SaveCopyBase(("C:\\" + std::string(200, 'd')).c_str()) == "", "a Documents path that leaves no room for edited\\<64 letters>-edited.rif: none");

	puts("A copy from a program inside the game's folder (C:\\Games\\AvP\\Tools\\AVP_OpenInteng.exe, as it is now installed):");
	{
		std::string const base = SaveCopyBase("C:\\Users\\Someone\\Documents");
		std::string path;
		std::string folder;
		char const *why = nullptr;

		Check(SaveCopyPlace("C:\\Games\\AvP\\avp_rifs\\Invasion.RIF", base.c_str(), "C:\\Games\\AvP", &path, &folder, &why) &&
			path == "C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\edited\\Invasion-edited.rif" && folder == "C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\edited\\" && why == nullptr,
			"the copy goes to Documents\\AVP_OpenInteng\\edited\\Invasion-edited.rif (" + path + ")");

		Check(SaveCopyPlace("C:\\Games\\AvP\\avp_rifs\\OpenedByEditor\\MelPyth.rif", base.c_str(), "C:\\Games\\AvP", &path, &folder, &why) &&
			path == "C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\edited\\MelPyth-edited.rif", "a level opened by a double click (its copy in avp_rifs\\OpenedByEditor) the same");

		Check(SaveExportPlace("C:\\Games\\AvP\\avp_rifs\\Invasion.RIF", "plan", ".asc", base.c_str(), "C:\\Games\\AvP", &path, &folder, &why) &&
			path == "C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\edited\\plan.asc", "Write Asc File's export goes there too");
	}

	puts("The rules kept:");
	{
		std::string path;
		std::string folder;
		char const *why = nullptr;

		Check(!SaveCopyPlace("C:\\Games\\AvP\\avp_rifs\\Invasion.RIF", "C:\\Games\\AvP\\Documents\\AVP_OpenInteng\\", "C:\\Games\\AvP", &path, &folder, &why) && path.empty() && why &&
			strstr(why, "inside the game's folder") != nullptr, "a folder for copies inside the game's folder is refused (" + std::string(why ? why : "") + ")");
		Check(!SaveCopyPlace("C:\\Games\\AvP\\avp_rifs\\Invasion.RIF", "c:\\games\\avp\\", "C:\\Games\\AvP", &path, &folder, &why) && path.empty(),
			"the game's folder itself, in other capitals, is refused");
		// a copy opened again gets a new name (Invasion-edited-edited.rif); only a name cut at 64 letters can come back to the opened file's own, and that is refused
		std::string const longName(64, 'L');

		Check(SaveCopyPlace("C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\edited\\Invasion-edited.rif", "C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\", "C:\\Games\\AvP", &path, &folder, &why) &&
			path == "C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\edited\\Invasion-edited-edited.rif", "a copy opened again is saved under a new name, never over itself");
		Check(!SaveCopyPlace(("C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\edited\\" + longName + "-edited.rif").c_str(), "C:\\Users\\Someone\\Documents\\AVP_OpenInteng\\", "C:\\Games\\AvP",
			&path, &folder, &why) && path.empty() && why && strstr(why, "opened file itself") != nullptr, "a name cut back to the opened file's own is refused (" + std::string(why ? why : "") + ")");
		Check(!SaveCopyPlace("C:\\Games\\AvP\\avp_rifs\\Invasion.RIF", "", "C:\\Games\\AvP", &path, &folder, &why) && why && strstr(why, "Documents") != nullptr,
			"no folder for copies (Windows gave no Documents folder): nothing is placed, and the reason names Documents\\AVP_OpenInteng");
		Check(SaveCopyName("C:\\Games\\AvP\\avp_rifs\\..\\x\\My Level!.rif") == "My_Level_-edited.rif", "the name is only the file's own letters, made safe");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
