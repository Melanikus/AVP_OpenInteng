// The test for the editor's start (editor_main.cpp): one editor at a time, the camera's limits, no console, and the player's game
// settings never written. Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// It runs none of the game. One editor at a time and the camera's limits are run for real. The rest is read from the files that hold
// it, from the repository's root (the test is run from there): where the editor's start makes a check, and what the project builds. Each
// check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <windows.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include "level_view.h"
#include "one_editor.h"
#include "save_folder.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A whole file as text, or "" if it can't be read (every check on it then fails, saying what it looked for)
static std::string FileText(const char *path)
{
	std::ifstream file(path, std::ios::binary);
	std::ostringstream text;
	text << file.rdbuf();
	return text.str();
}

// The text with every XML comment taken out, so an item that's only mentioned in a comment doesn't count as built
static std::string WithoutXmlComments(std::string text)
{
	for (size_t start = text.find("<!--"); start != std::string::npos; start = text.find("<!--", start))
	{
		size_t const end = text.find("-->", start);
		text.erase(start, end == std::string::npos ? std::string::npos : end + 3 - start);
	}
	return text;
}

// Whether first comes before second in text, both there
static bool ComesBefore(std::string const &text, const char *first, const char *second)
{
	size_t const a = text.find(first);
	size_t const b = text.find(second);
	return a != std::string::npos && b != std::string::npos && a < b;
}

// A folder of the test's own, made in Windows' folder for temporary files: "<temp>\editor_start_test_<pid>_<part>"
static std::wstring TestFolder(const wchar_t *part)
{
	wchar_t temporary[MAX_PATH + 1];
	DWORD const length = GetTempPathW(MAX_PATH + 1, temporary);
	if (length == 0 || length > MAX_PATH - 64)
	{
		return std::wstring();
	}
	wchar_t folder[MAX_PATH + 1];
	_snwprintf_s(folder, MAX_PATH + 1, _TRUNCATE, L"%seditor_start_test_%lu_%s", temporary, (unsigned long)GetCurrentProcessId(), part);
	CreateDirectoryW(folder, NULL);
	return folder;
}

static std::wstring TokenName(std::wstring const &folder)
{
	wchar_t name[128];
	return OneEditor_NameFor(folder.c_str(), name, sizeof(name) / sizeof(name[0])) ? std::wstring(name) : std::wstring();
}

// ONE EDITOR AT A TIME IN EACH FOLDER (one_editor.h). The test plays an editor that's already running by holding a token's name itself.
static void TestOneEditor()
{
	std::wstring const mine = TestFolder(L"mine");
	std::wstring const other = TestFolder(L"other");
	std::wstring const name = TokenName(mine);
	Check(!mine.empty() && !other.empty() && !name.empty() && name.compare(0, wcslen(kOneEditorName), kOneEditorName) == 0,
		"a folder's token is named from the folder (one_editor.cpp)");

	// One folder gives one name however it's spelled: in other capitals, with a last "\", or by its short 8.3 name if it has one
	std::wstring upper = mine;
	CharUpperBuffW(&upper[0], (DWORD)upper.size());
	wchar_t shortName[MAX_PATH + 1] = { 0 };
	DWORD const shortLength = GetShortPathNameW(mine.c_str(), shortName, MAX_PATH + 1);
	bool const shortSame = shortLength == 0 || shortLength > MAX_PATH || TokenName(shortName) == name;
	Check(TokenName(upper) == name && TokenName(mine + L"\\") == name && shortSame,
		"the same folder, spelled in capitals, with a last \\, or by its short name, gives the same token");
	Check(TokenName(other) != name && !TokenName(other).empty(), "and another folder gives another");
	Check(TokenName(mine + L"\\not-there").empty(), "a folder that isn't there gives no name, so no editor goes on from it");
	Check(OneEditor_ClaimFor((mine + L"\\not-there").c_str()) == OneEditorAnswer::FolderNotNamed &&
		strstr(OneEditor_Words(OneEditorAnswer::FolderNotNamed), "already running") == nullptr &&
		strstr(OneEditor_Words(OneEditorAnswer::WindowsWouldNotSay), "already running") == nullptr,
		"and the editor is told so in words of its own, never that another one is already running (Apone's cold read, F2)");

	// Another editor in the same folder: this one is refused
	HANDLE const another = CreateMutexW(NULL, FALSE, name.c_str());
	Check(another != NULL && GetLastError() != ERROR_ALREADY_EXISTS, "the test holds this folder's token, as an editor already running there would");
	Check(OneEditor_ClaimFor(mine.c_str()) == OneEditorAnswer::AnotherIsRunning &&
		strstr(OneEditor_Words(OneEditorAnswer::AnotherIsRunning), "already running") != nullptr, "with another editor running from this folder, this one is refused (one_editor.cpp: ERROR_ALREADY_EXISTS)");
	if (another)
	{
		CloseHandle(another);
	}

	// Another editor in another folder isn't in the way
	HANDLE const elsewhere = CreateMutexW(NULL, FALSE, TokenName(other).c_str());
	Check(OneEditor_ClaimFor(mine.c_str()) == OneEditorAnswer::Taken, "with an editor running only from another folder, this one takes its own folder's token");
	if (elsewhere)
	{
		CloseHandle(elsewhere);
	}
	Check(OneEditor_ClaimFor(mine.c_str()) == OneEditorAnswer::Taken, "and asking again, from the same editor, still says yes: it holds it");

	HANDLE const second = CreateMutexW(NULL, FALSE, name.c_str());
	bool const toldItsTaken = second != NULL && GetLastError() == ERROR_ALREADY_EXISTS;
	if (second)
	{
		CloseHandle(second);
	}
	Check(toldItsTaken, "and a second editor from this folder, asking now, is told it's taken");
	RemoveDirectoryW(mine.c_str());
	RemoveDirectoryW(other.c_str());

	std::string const start = FileText("src/host/editor_main.cpp");
	char const *const claim = "OneEditorAnswer const token = OneEditor_Claim();";
	Check(ComesBefore(start, claim, "ReadSetting(settingsFile, \"DataFolder\"") && ComesBefore(start, claim, "OpenWith_TakeLevel(") &&
		ComesBefore(start, "if (badCommandLine)", claim),
		"the editor's start asks for the token after reading its command line and before it reads its settings or copies a level in "
		"(editor_main.cpp)");
	Check(start.find("if (token != OneEditorAnswer::Taken)\n\t{\n\t\treturn Stop(OneEditor_Words(token));") != std::string::npos,
		"and when it's refused, it stops there, saying which refusal it was (editor_main.cpp)");
}

// THE CAMERA'S LIMITS (level_view.h): no move can carry a number past what an int holds
static void TestCameraLimits()
{
	FlyCamera wayOut = { INT_MAX, INT_MIN, INT_MAX, INT_MIN, INT_MAX };
	FlyCamera_Hold(&wayOut);
	Check(wayOut.x == kFlyCameraMostPlace && wayOut.y == -kFlyCameraMostPlace && wayOut.z == kFlyCameraMostPlace,
		"a camera place past the limit on each axis, either way, is held to 2^24");
	Check(wayOut.turn >= 0 && wayOut.turn < kFlyCameraWholeTurn, "a turn of the most negative int is held within one whole turn, 0 to 4095");
	Check(wayOut.look == kFlyCameraMostLook, "a look past straight up is held to straight up, a quarter turn");

	FlyCamera low = { 0, 0, 0, 0, INT_MIN };
	FlyCamera_Hold(&low);
	Check(low.look == -kFlyCameraMostLook, "and a look past straight down to straight down");

	FlyCamera inside = { 12345, -678, 9, 4095, -1024 };
	FlyCamera_Hold(&inside);
	Check(inside.x == 12345 && inside.y == -678 && inside.z == 9 && inside.turn == 4095 && inside.look == -1024,
		"a camera inside its limits is left exactly as it was");

	FlyCamera turning = { 0, 0, 0, 0, 0 };
	bool stayedInside = true;
	for (int i = 0; i < 100000; i++)
	{
		turning.turn -= 24 + kFlyCameraWholeTurn;   // a key's turn and the most the mouse may add in one frame
		turning.x += 300;
		FlyCamera_Hold(&turning);
		stayedInside = stayedInside && turning.turn >= 0 && turning.turn < kFlyCameraWholeTurn && turning.x <= kFlyCameraMostPlace;
	}
	Check(stayedInside && turning.x == kFlyCameraMostPlace,
		"a camera turned and moved the same way for 100,000 frames stays within its limits, and stops at the edge");

	FlyCamera back = { -1, 0, 0, -1, 0 };
	FlyCamera_Hold(&back);
	Check(back.turn == 4095 && back.x == -1, "a turn of -1 is the same angle as 4095");

	Check(FlyCamera_HeldTo(1LL << 40, kFlyCameraWholeTurn) == kFlyCameraWholeTurn && FlyCamera_HeldTo(-(1LL << 40), 5) == -5,
		"FlyCamera_HeldTo holds a number far past an int's reach to its limit, either way");

	std::string const start = FileText("src/host/editor_main.cpp");
	std::string const view = FileText("src/host/level_view.cpp");
	Check(start.find("\tFlyCamera_Hold(camera);\n}") != std::string::npos, "every frame's move ends with the camera held to its limits (editor_main.cpp)");
	Check(start.find("mouseTurn = FlyCamera_HeldTo(") != std::string::npos && start.find("mouseLook = FlyCamera_HeldTo(") != std::string::npos,
		"and what the mouse adds between two frames is held too (editor_main.cpp)");
	Check(view.find("FlyCamera_Hold(startHere);") != std::string::npos, "and so is the place a level starts the camera at (level_view.cpp)");
}

// THE CAMERA'S AXES (level_view.cpp, LevelView_CameraAxes): W, A, S and D move along them. After a frame the game's VDB_Mat is the camera's
// turn transposed, so the camera's forward and right are its columns. Read from the code, with the game's own transpose checked too, so a
// pin where the game stops transposing fails here instead of moving the camera the mirror way.
static void TestCameraAxes()
{
	std::string const view = FileText("src/host/level_view.cpp");
	Check(view.find("forward[0] = view.mat13;\n\tforward[1] = view.mat23;\n\tforward[2] = view.mat33;") != std::string::npos,
		"the camera's forward is read from the view matrix's third column (level_view.cpp)");
	Check(view.find("right[0] = view.mat11;\n\tright[1] = view.mat21;\n\tright[2] = view.mat31;") != std::string::npos,
		"and its right from the first column");

	// PrepareVDBForShowView transposes the matrix in its own body: the transpose comes before the function's closing brace
	std::string const game = FileText("avpx/3dc/avp/view.cpp");
	size_t const prepare = game.find("void PrepareVDBForShowView(VIEWDESCRIPTORBLOCK *VDB_Ptr)");
	size_t const transpose = prepare == std::string::npos ? std::string::npos : game.find("TransposeMatrixCH(&VDB_Ptr->VDB_Mat);", prepare);
	bool const transposedInIt = transpose != std::string::npos && game.substr(prepare, transpose - prepare).find("\n}") == std::string::npos;
	Check(ComesBefore(game, "Global_VDB_Ptr->VDB_Mat = dptr_s->ObMat;", "void PrepareVDBForShowView(VIEWDESCRIPTORBLOCK *VDB_Ptr)") && transposedInIt &&
		ComesBefore(game, "\tUpdateCamera();", "\tPrepareVDBForShowView(Global_VDB_Ptr);"),
		"because the game's frame sets the view matrix to the camera's turn and then transposes it (avpx view.cpp:605, 723-726, 806)");
}

// NO CONSOLE, AND THE PLAYER'S GAME SETTINGS NEVER WRITTEN: what the project builds, read from it
static void TestTheProject()
{
	std::string const project = WithoutXmlComments(FileText("win32/VS2026/AVP_OpenInteng.vcxproj"));
	Check(!project.empty(), "the project file is read (win32/VS2026/AVP_OpenInteng.vcxproj)");
	Check(project.find("avpx\\3dc\\ConfigFile.cpp") == std::string::npos,
		"the game's ConfigFile.cpp, which writes the player's AliensVsPredator.cfg, is not built into the editor");
	Check(project.find("<SubSystem Condition=\"'$(StockProof)' != 'true'\">Windows</SubSystem>") != std::string::npos &&
		project.find("<EntryPointSymbol Condition=\"'$(StockProof)' != 'true'\">mainCRTStartup</EntryPointSymbol>") != std::string::npos,
		"the editor is a Windows program that starts at main, so it opens no console window");
	Check(project.find("<SubSystem Condition=\"'$(StockProof)' == 'true'\">Console</SubSystem>") != std::string::npos,
		"and the F4 stock proof stays a console program, run from a command line");
	Check(project.find("<SubSystem>Console</SubSystem>") == std::string::npos, "and no line makes every build a console one");

	// The editor's own answers for the game's settings (edge_own.cpp, from "THE PLAYER'S GAME SETTINGS" on) touch no file at all
	std::string const own = FileText("src/host/edge_own.cpp");
	size_t const settingsStart = own.find("// THE PLAYER'S GAME SETTINGS.");
	Check(settingsStart != std::string::npos, "the editor's own answers for the game's settings are found (edge_own.cpp)");
	std::string const settings = settingsStart == std::string::npos ? std::string() : own.substr(settingsStart);
	const char *const fileCalls[] = { "fopen", "ofstream", "ifstream", "fstream", "CreateFile", "WritePrivateProfile", "_open(",
		"MoveFile", "DeleteFile", "CopyFile", "rename(", "remove(" };
	std::string named;
	for (const char *call : fileCalls)
	{
		// remove( is the standard algorithm's when it's std::remove: that one is allowed, a file's remove isn't
		size_t at = settings.find(call);
		while (at != std::string::npos && strcmp(call, "remove(") == 0 && at >= 5 && settings.compare(at - 5, 5, "std::") == 0)
		{
			at = settings.find(call, at + 1);
		}
		if (at != std::string::npos)
		{
			named += std::string(named.empty() ? "" : ", ") + call;
		}
	}
	Check(!settings.empty() && named.empty(),
		"and they name no call that opens, writes, moves or deletes a file" + (named.empty() ? std::string() : ": " + named));
}

// "Direct3D couldn't start" says why: the last error the game noted
static void TestDirect3DReason()
{
	std::string const start = FileText("src/host/editor_main.cpp");
	Check(ComesBefore(start, "if (!InitialiseDirect3D())", "GetLastErrorMessage()") &&
		ComesBefore(start, "GetLastErrorMessage()", "LevelView_Start(why"),
		"when Direct3D can't start, the box gives the last error the game noted (editor_main.cpp)");
}

// THE EDITOR'S OWN FOLDER FOR THE GAME'S FILES (SAVE-FOLDER-1): the game's save folder is set to <the program's folder>\settings\
// before any of the game's code can ask for it or write its log, and a refusal stops the editor. Read from the editor's start: every
// call that could reach the game's log or its save folder comes after the setter.
static void TestOwnSaveFolder()
{
	std::string const start = FileText("src/host/editor_main.cpp");
	size_t const mainAt = start.find("int main(");
	std::string const body = mainAt == std::string::npos ? std::string() : start.substr(mainAt);
	// (the level's open, LevelView_Open, is in OpenTheLevel since 2c6d3a1b, All stock maps' reopen: main's own call to it is what comes after the setter)
	const char *const after[] = { "ReadSetting(settingsFile, \"DataFolder\"", "OpenWith_TakeLevel(", "InitialiseDirect3D()",
		"LevelView_Start(", "OpenTheLevel(levelName, &camera" };
	bool allAfter = !body.empty();
	for (const char *call : after)
	{
		allAfter = allAfter && ComesBefore(body, "if (!SaveFolder_UseThisFolder(settingsFolder))", call);
	}
	Check(allAfter && ComesBefore(body, "OneEditorAnswer const token = OneEditor_Claim();", "if (!SaveFolder_UseThisFolder(settingsFolder))"),
		"the editor gives the game its own settings folder after taking its token and before its settings, a level, Direct3D or the "
		"game's start (editor_main.cpp)");
	// and the level is opened nowhere else before it: LevelView_Open is called once, inside OpenTheLevel, and OpenTheLevel only by main (after the
	// setter) and by LevelReopen_Open (All stock maps' reopen of a level, which runs only once the editor is started: level_reopen.h)
	size_t const openAt = start.find("LevelView_Open("), openerAt = start.find("static bool OpenTheLevel("), reopenAt = start.find("bool LevelReopen_Open(");
	size_t openerCalls = 0;

	for (size_t at = start.find("OpenTheLevel("); at != std::string::npos; at = start.find("OpenTheLevel(", at + 1))
	{
		openerCalls += at == openerAt + strlen("static bool ") ? 0 : 1;
	}

	Check(openAt != std::string::npos && start.find("LevelView_Open(", openAt + 1) == std::string::npos && openerAt < openAt && openAt < reopenAt &&
		reopenAt < mainAt && openerCalls == 2, "and the level is opened only through OpenTheLevel: LevelView_Open once, inside it, and OpenTheLevel called twice, by main "
		"and by All stock maps' reopen (" + std::to_string((unsigned long long)openerCalls) + " calls)");
	Check(body.find("LogString") == std::string::npos || ComesBefore(body, "if (!SaveFolder_UseThisFolder(settingsFolder))", "LogString"),
		"and no line of the game's log is written before it");
	Check(body.find("if (!SaveFolder_UseThisFolder(settingsFolder))\n\t{\n\t\treturn Stop(") != std::string::npos,
		"and if the game won't take it, the editor stops there, never falling back to the player's own folder");
	Check(body.find("\"%ssettings\\\\\", ownFolder)") != std::string::npos,
		"the folder is the program's own, \"settings\" beside it (editor_main.cpp)");

	// The data folder has to be a full path before the editor moves into it (Vasquez's find)
	char const *const dataCheck = "if (char const *const notUsable = SaveFolder_WhyNotUsable(dataFolder, checkedDataFolder, sizeof(checkedDataFolder)))";
	Check(ComesBefore(body, dataCheck, "SetCurrentDirectoryA(dataFolder)") && ComesBefore(body, dataCheck, "OpenWith_TakeLevel(") &&
		body.find(std::string(dataCheck) + "\n\t{\n\t\treturn Stop(") != std::string::npos,
		"DataFolder is held to the settings folder's own rule before the editor moves into it or copies a level into it (editor_main.cpp)");
	char checked[MAX_PATH];
	const char *const refused[] = { "data", "C:data", "\\\\server\\x", "//server/x", "\\\\.\\pipe\\x", "\\\\?\\C:\\x", "C:\\x\\..\\y", "C:\\x:y" };
	bool allRefused = true;
	for (const char *dataFolder : refused)
	{
		allRefused = allRefused && SaveFolder_WhyNotUsable(dataFolder, checked, sizeof(checked)) != nullptr;
	}
	Check(allRefused, "and that rule refuses, as a DataFolder, data, C:data, \\\\server\\x, //server/x, \\\\.\\pipe\\x, \\\\?\\C:\\x, C:\\x\\..\\y and C:\\x:y (Apone's cold read, F1)");
}

int main()
{
	TestOneEditor();
	TestCameraLimits();
	TestCameraAxes();
	TestTheProject();
	TestDirect3DReason();
	TestOwnSaveFolder();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
