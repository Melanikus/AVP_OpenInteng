// The test for src/host/video_drop (a video file handed to the editor by a drop or by Browse..., checked before anything opens it). Written 2026-11-22 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: a dropped path is hostile (Tao 3; Bishop 2's brief for the owner's Video mode). Opening \\server\share\... makes Windows contact that computer and can hand it the
// user's login hash, so every name that could reach another computer must be refused FROM ITS LETTERS, before any call: the test never names a computer that exists (its
// names are *.invalid, which no name server answers), and a wrong answer here would still only be a refusal missed, never a file opened (nothing here opens a path).
// It checks: the UNC and \\?\ and \\.\ and \??\ forms, GLOBALROOT, forward slashes, a name with no drive, a drive-relative name, a stream, control characters, the
// endings of the one list (video_import.h) and others, one file only, a mapped drive's and a SUBST's target words, the drop handle's own list (a DROPFILES block made here, as Explorer's), the
// harness's UTF-8 list, and a real file's full name on this computer's own drive.
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>
#include <shlobj.h>

#include "video_drop.h"
#include "video_import.h"

static int checks = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;

	if (!good)
	{
		checksFailed++;
		printf("FAILED: %s\n", what.c_str());
	}
}

// One path handed over: refused, with these words in the reason
static void Refused(std::wstring const &given, char const *words, char const *what)
{
	std::wstring path;
	std::string why;
	bool const taken = host::VideoDrop_Check({ given }, 1, &path, &why);

	Check(!taken && path.empty() && why.find(words) != std::string::npos, std::string(what) + " (it said \"" + why + "\")");
}

// Writes `bytes` to a new file; false when it can't
static bool WriteNew(std::wstring const &path, char const *bytes)
{
	HANDLE const file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD wrote = 0;
	BOOL const done = WriteFile(file, bytes, (DWORD)strlen(bytes), &wrote, nullptr);

	CloseHandle(file);
	return done && wrote == strlen(bytes);
}

int main()
{
	// ---- another computer, a device, Windows' own prefixes: refused from the letters
	Refused(L"\\\\server.invalid\\share\\clip.mp4", "on another computer", "a UNC name is refused");
	Refused(L"//server.invalid/share/clip.mp4", "on another computer", "and so is one written with forward slashes");
	Refused(L"\\/server.invalid/share/clip.mp4", "on another computer", "or with one of each");
	Refused(L"\\\\?\\UNC\\server.invalid\\share\\clip.mp4", "on another computer", "\\\\?\\UNC\\ is refused");
	Refused(L"\\\\?\\C:\\clips\\clip.mp4", "on another computer, or is a device", "every \\\\?\\ name is refused, a local one too (a drop never gives one)");
	Refused(L"\\\\.\\C:\\clips\\clip.mp4", "is a device", "\\\\.\\ is refused");
	Refused(L"\\\\.\\pipe\\clip.mp4", "is a device", "and so is a pipe");
	Refused(L"\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1\\clip.mp4", "is a device", "GLOBALROOT is refused");
	Refused(L"\\??\\C:\\clips\\clip.mp4", "is a device", "the NT \\??\\ form is refused");
	Refused(L"\\??\\UNC\\server.invalid\\share\\clip.mp4", "on another computer", "and \\??\\UNC\\");

	// ---- not a full name on a drive
	Refused(L"clip.mp4", "not a full name on a drive", "a name with no drive is refused");
	Refused(L"\\clips\\clip.mp4", "not a full name on a drive", "and one from the current drive's root");
	Refused(L"C:clip.mp4", "not a full name on a drive", "and a drive-relative one");
	Refused(L"1:\\clip.mp4", "not a full name on a drive", "and a drive that isn't a letter");
	Refused(L"", "could not be read", "an empty name is refused");
	Refused(L"C:\\clips\\clip.mp4:hidden", "a stream", "a file's stream is refused");
	Refused(L"C:\\clips\\clip.mp4:hidden:$DATA", "a stream", "and one named with its type");
	Refused(std::wstring(L"C:\\clips\\clip") + (wchar_t)0x0a + L".mp4", "control character", "a name with a new line in it is refused");
	Refused(std::wstring(L"C:\\clips\\clip") + (wchar_t)0x07 + L".mp4", "control character", "and one with a bell");
	Refused(std::wstring(32768, L'a'), "could not be read", "a name longer than any Windows has is refused");

	// ---- one file at a time
	{
		std::wstring path;
		std::string why;

		Check(!host::VideoDrop_Check({ L"C:\\a.mp4", L"C:\\b.mp4" }, 2, &path, &why) && why == "Not taken: one video at a time (2 files were dropped)",
			"two files at once are refused, in words (it said \"" + why + "\")");
		Check(!host::VideoDrop_Check({ L"C:\\a.mp4" }, 40, &path, &why) && why == "Not taken: one video at a time (40 files were dropped)",
			"and the words count every file dropped, not only those listed (it said \"" + why + "\")");
		Check(!host::VideoDrop_Check({}, 0, &path, &why) && why == "Not taken: no file was dropped", "nothing dropped is said so (it said \"" + why + "\")");
	}

	// ---- what a drive letter stands for (QueryDosDevice's words): another computer's share, or this one's
	Check(host::VideoDrop_TargetIsNetwork(L"\\Device\\LanmanRedirector\\;Z:000000000001abcd\\server.invalid\\share"), "a mapped drive (the SMB redirector) is a share");
	Check(host::VideoDrop_TargetIsNetwork(L"\\Device\\Mup\\;Z:000000000001abcd\\server.invalid\\share"), "and so is one through the multiple-UNC provider");
	Check(host::VideoDrop_TargetIsNetwork(L"\\??\\UNC\\server.invalid\\share"), "and a SUBST of a UNC name");
	Check(host::VideoDrop_TargetIsNetwork(L"\\device\\lanmanredirector\\x"), "in any capitals");
	Check(host::VideoDrop_TargetIsNetwork(L"\\Device\\WebDavRedirector\\server.invalid"), "and a WebDAV drive");
	Check(!host::VideoDrop_TargetIsNetwork(L"\\Device\\HarddiskVolume3"), "a disk of this computer is not");
	Check(!host::VideoDrop_TargetIsNetwork(L"\\??\\C:\\clips"), "nor a SUBST of a local folder");

	// ---- the endings, on a real file of this computer (a temporary folder of the test's own)
	wchar_t temp[MAX_PATH + 1] = {};
	DWORD const tempLength = GetTempPathW(MAX_PATH, temp);
	std::wstring const folder = std::wstring(temp, tempLength) + L"video-drop-test-" + std::to_wstring(GetCurrentProcessId());
	bool const made = tempLength > 0 && CreateDirectoryW(folder.c_str(), nullptr);

	Check(made, "the test's own folder is made");

	if (made)
	{
		std::wstring const mp4 = folder + L"\\clip.mp4";
		std::wstring const upper = folder + L"\\CLIP.WMV";
		std::wstring const level = folder + L"\\Genshd1.rif";

		WriteNew(mp4, "not really a video: only the name is checked here");
		WriteNew(upper, "x");
		WriteNew(level, "x");

		std::wstring path;
		std::string why;

		Check(host::VideoDrop_Check({ mp4 }, 1, &path, &why) && _wcsicmp(path.c_str(), mp4.c_str()) == 0, "a .mp4 on this computer's own drive is taken, by its full name (it said \"" + why + "\")");
		Check(host::VideoDrop_Check({ upper }, 1, &path, &why), "an ending in capitals is taken (.WMV) (it said \"" + why + "\")");

		for (wchar_t const *ending : { L".m4v", L".avi", L".mkv", L".mov", L".webm", L".ts" })
		{
			Check(host::VideoDrop_Check({ folder + L"\\other" + ending }, 1, &path, &why), "a " + host::VideoDrop_Words(ending) + " is taken by its name, as the one list says");
		}

		Check(!host::VideoDrop_Check({ level }, 1, &path, &why) && why == std::string("Not taken: Genshd1.rif is not a video the editor takes (") + host::VideoImport_EndingsWords() + ")",
			"a level file is refused, by its own name (it said \"" + why + "\")");

		for (wchar_t const *ending : { L".exe", L".mkv.exe", L".smk", L".mp4.exe", L".mp4.exe.", L"" })
		{
			Check(!host::VideoDrop_Check({ folder + L"\\clip" + ending }, 1, &path, &why) && why.find("is not a video the editor takes") != std::string::npos,
				"an ending the editor doesn't take is refused: \"" + host::VideoDrop_Words(ending) + "\"");
		}

		// Windows drops a name's last spaces and dots (clip.mp4 and "clip.mp4 " are one file), so the check judges the name Windows would open: taken as clip.mp4
		Check(host::VideoDrop_Check({ mp4 + L" " }, 1, &path, &why) && _wcsicmp(path.c_str(), mp4.c_str()) == 0, "a name with a space after its ending is judged as the file Windows opens, clip.mp4");

		// forward slashes and .. are folded into the full name, on the same drive
		std::wstring slashed = mp4;

		for (wchar_t &c : slashed)
		{
			c = c == L'\\' ? L'/' : c;
		}

		Check(host::VideoDrop_Check({ slashed }, 1, &path, &why) && _wcsicmp(path.c_str(), mp4.c_str()) == 0, "a name with forward slashes is taken as the same file");
		Check(host::VideoDrop_Check({ folder + L"\\sub\\..\\clip.mp4" }, 1, &path, &why) && _wcsicmp(path.c_str(), mp4.c_str()) == 0, "and one with .. in it");

		// THE DROP HANDLE: a DROPFILES block, wide names each ended by a nought and the list by another, as Explorer hands one over
		std::wstring list = mp4 + L'\0' + level + L'\0' + L'\0';
		size_t const listBytes = list.size() * sizeof(wchar_t);
		HGLOBAL const block = GlobalAlloc(GHND, sizeof(DROPFILES) + listBytes);
		DROPFILES *const files = block ? (DROPFILES *)GlobalLock(block) : nullptr;

		if (files)
		{
			files->pFiles = sizeof(DROPFILES);
			files->fWide = TRUE;
			memcpy((char *)files + sizeof(DROPFILES), list.data(), listBytes);
			GlobalUnlock(block);

			size_t more = 0;
			std::vector<std::wstring> const both = host::VideoDrop_FromHdrop((HDROP)block, 16, &more);

			Check(both.size() == 2 && both[0] == mp4 && both[1] == level && more == 0, "a drop's handle gives both its files, as Windows lists them");

			std::vector<std::wstring> const first = host::VideoDrop_FromHdrop((HDROP)block, 1, &more);

			Check(first.size() == 1 && more == 1, "at most the files asked for, the rest counted");
			Check(!host::VideoDrop_Check(first, first.size() + more, &path, &why) && why == "Not taken: one video at a time (2 files were dropped)", "and both are counted in the refusal");
		}
		else
		{
			Check(false, "a DROPFILES block is made for the drop handle's check");
		}

		if (block)
		{
			GlobalFree(block);
		}

		// THE HARNESS'S LIST: UTF-8, one path a line
		std::string const utf8 = "C:/one/clip.mp4\nD:\\two\\\xc3\xa9t\xc3\xa9.avi";
		std::vector<std::wstring> const harness = host::VideoDrop_FromHarness(utf8.data(), utf8.size());

		Check(harness.size() == 2 && harness[0] == L"C:/one/clip.mp4" && harness[1] == std::wstring(L"D:\\two\\") + (wchar_t)0xe9 + L"t" + (wchar_t)0xe9 + L".avi",
			"the harness's list gives each line as a path, its UTF-8 read");

		std::string const broken = "C:\\a\xff.mp4";
		std::vector<std::wstring> const bad = host::VideoDrop_FromHarness(broken.data(), broken.size());

		Check(bad.size() == 1 && bad[0].empty(), "a line that isn't UTF-8 gives no path (refused by the check)");

		DeleteFileW(mp4.c_str());
		DeleteFileW(upper.c_str());
		DeleteFileW(level.c_str());
		RemoveDirectoryW(folder.c_str());
	}

	// ---- a drive letter no drive has: refused by this computer's own table, asking no other computer
	{
		wchar_t unused = 0;
		DWORD const drives = GetLogicalDrives();

		for (wchar_t letter = L'Z'; letter >= L'D' && unused == 0; letter--)
		{
			unused = (drives & (1u << (letter - L'A'))) ? 0 : letter;
		}

		if (unused)
		{
			std::wstring const nowhere = std::wstring(1, unused) + L":\\clips\\clip.mp4";
			std::wstring path;
			std::string why;

			Check(!host::VideoDrop_Check({ nowhere }, 1, &path, &why) && why.find("is not one this computer knows") != std::string::npos,
				"a drive letter no drive has is refused (it said \"" + why + "\")");
		}
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
