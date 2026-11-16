// The test for the key layout's setting in AVP_OpenInteng.ini (key_layout.h). Written 2026-11-14 by Melanikus (Dietrich) for
// AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: the setting is input from outside the code (Tao 3): anyone can write anything in the .ini. Only the two exact words count; anything
// else means Classic, with the reason given to log. And writing it must change that one line and nothing else of the file. The words are
// tried here as text; then a settings file the test makes beside its own program, in its own folder, never anyone else's, is written and
// read back through Windows' own .ini calls, and deleted.
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "key_layout.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// The setting's text as Windows' reading would give it: the letters, and how many it gave
static const char *Read(const char *text, size_t length, size_t room, KeyLayout *layout)
{
	return KeyLayout_FromSetting(text, length, room, layout);
}

static void TestTheWords()
{
	puts("The setting's words:");

	KeyLayout layout = KeyLayout::ModernWasd;
	size_t const room = kKeyLayoutSettingRoom;

	Check(Read("Classic", 7, room, &layout) == 0 && layout == KeyLayout::Classic, "Classic is Classic, with no reason");
	Check(Read("ModernWASD", 10, room, &layout) == 0 && layout == KeyLayout::ModernWasd, "ModernWASD is Modern, with no reason");

	layout = KeyLayout::ModernWasd;
	const char *why = Read("", 0, room, &layout);
	Check(why && strstr(why, "no KeyLayout= line") && layout == KeyLayout::Classic, "no line means Classic, and says so");

	layout = KeyLayout::ModernWasd;
	why = Read(0, 7, room, &layout);
	Check(why && layout == KeyLayout::Classic, "no text at all means Classic");

	char const full[] = "ModernWASDModernWASDModernWASDM";
	layout = KeyLayout::ModernWasd;
	why = Read(full, room - 1, room, &layout);
	Check(why && strstr(why, "too long") && layout == KeyLayout::Classic, "a line that filled its room may have been cut, so it's too long: Classic");

	layout = KeyLayout::ModernWasd;
	why = Read("classic", 7, room, &layout);
	Check(why && strstr(why, "neither") && layout == KeyLayout::Classic, "classic, small c, isn't the word: Classic, and says so");

	layout = KeyLayout::ModernWasd;
	why = Read("modernwasd", 10, room, &layout);
	Check(why && layout == KeyLayout::Classic, "modernwasd, small letters, isn't the word either");

	layout = KeyLayout::ModernWasd;
	why = Read("ModernWASD ", 11, room, &layout);
	Check(why && layout == KeyLayout::Classic, "the word with something after it isn't the word");

	char const withZero[] = { 'M', 'o', 'd', 'e', 'r', 'n', 'W', 'A', 'S', 'D', '\0', 'x' };
	layout = KeyLayout::ModernWasd;
	why = Read(withZero, sizeof(withZero), room, &layout);
	Check(why && layout == KeyLayout::Classic, "the word, a 0 and more after it isn't the word: the length read counts, not the first 0");

	char const odd[] = { 'C', 'l', 'a', 's', 's', 'i', (char)0xEC };
	layout = KeyLayout::ModernWasd;
	why = Read(odd, sizeof(odd), room, &layout);
	Check(why && layout == KeyLayout::Classic, "a byte outside plain letters makes it no word");

	layout = KeyLayout::ModernWasd;
	why = Read("102", 3, room, &layout);
	Check(why && strstr(why, "neither") && layout == KeyLayout::Classic,
		"a retired action's number (102, 111, 112: the view state table) is no word of the setting: refused as unknown, never mapped");

	layout = KeyLayout::ModernWasd;
	why = Read("Classic", 7, 0, &layout);
	Check(why && layout == KeyLayout::Classic, "a room of nothing reads nothing");
}

// The whole of a small file, as bytes, or empty
static std::string FileBytes(const char *path)
{
	std::string bytes;
	FILE *file = fopen(path, "rb");

	if (file)
	{
		char buffer[4096];
		size_t got = 0;

		while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0)
		{
			bytes.append(buffer, got);
		}

		fclose(file);
	}

	return bytes;
}

static void TestTheFile()
{
	puts("The settings file, written and read back:");

	// A file beside this test's own program: its own folder, nobody else's
	char path[MAX_PATH];
	DWORD const length = GetModuleFileNameA(NULL, path, sizeof(path));
	char *const lastSlash = length > 0 && length < sizeof(path) ? strrchr(path, '\\') : 0;

	if (!lastSlash || (size_t)(lastSlash - path) + 40 >= sizeof(path))
	{
		Check(false, "(the test's own folder is found)");
		return;
	}

	strcpy_s(lastSlash + 1, sizeof(path) - (size_t)(lastSlash + 1 - path), "key_layout_test.ini");

	std::string const original =
		"; the mapper's own notes\r\n"
		"[Editor]\r\n"
		"DataFolder=C:\\AvP\\data\r\n"
		"Level=Invasion\r\n"
		"\r\n"
		"[Other]\r\n"
		"Something=kept\r\n";
	FILE *file = fopen(path, "wb");

	if (!file)
	{
		Check(false, "(the test's settings file is made)");
		return;
	}

	fwrite(original.data(), 1, original.size(), file);
	fclose(file);

	KeyLayout layout = KeyLayout::ModernWasd;
	const char *why = KeyLayout_Load(path, &layout);
	Check(why && strstr(why, "no KeyLayout= line") && layout == KeyLayout::Classic, "a file with no KeyLayout= line means Classic");

	Check(KeyLayout_Save(path, KeyLayout::ModernWasd), "Modern is saved");
	why = KeyLayout_Load(path, &layout);
	Check(why == 0 && layout == KeyLayout::ModernWasd, "and read back as Modern");

	std::string const written = FileBytes(path);
	std::string withoutTheLine = written;
	size_t const at = withoutTheLine.find("KeyLayout=ModernWASD\r\n");

	if (at != std::string::npos)
	{
		withoutTheLine.erase(at, strlen("KeyLayout=ModernWASD\r\n"));
	}

	Check(at != std::string::npos && withoutTheLine == original, "every other line of the file is exactly as it was, comments and other sections too");
	Check(at != std::string::npos && at < written.find("[Other]"), "and the line went into [Editor]");

	Check(KeyLayout_Save(path, KeyLayout::Classic), "Classic is saved over it");
	why = KeyLayout_Load(path, &layout);
	Check(why == 0 && layout == KeyLayout::Classic && FileBytes(path).find("KeyLayout=Classic\r\n") != std::string::npos
		&& FileBytes(path).find("ModernWASD") == std::string::npos, "and read back as Classic, the one line changed in place");

	WritePrivateProfileStringA("Editor", "KeyLayout", "classic", path);
	why = KeyLayout_Load(path, &layout);
	Check(why && layout == KeyLayout::Classic, "a line someone wrote by hand with another word means Classic, with a reason");

	WritePrivateProfileStringA("Editor", "KeyLayout", "ModernWASDModernWASDModernWASDModernWASD", path);
	why = KeyLayout_Load(path, &layout);
	Check(why && strstr(why, "too long") && layout == KeyLayout::Classic, "a line longer than its room means Classic, and says it's too long");

	Check(!KeyLayout_Save(0, KeyLayout::ModernWasd) && KeyLayout_Load(0, &layout) != 0 && layout == KeyLayout::Classic,
		"no file name writes nothing and reads Classic: Windows would have used its own win.ini");

	DeleteFileA(path);
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheWords();
	TestTheFile();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
