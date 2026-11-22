// The test for a FOLDER ON THE WAY THAT IS A SYMBOLIC LINK (src/host/video_import.h; Vasquez's Video mode review, the MUST; Drake 11-22): a video's path that goes through a folder
// made with mklink /D is refused, and nothing is opened through it, whether the link names a local folder or another computer (\\127.0.0.1\x: this computer's own address,
// so even before the fix no other computer is asked). Before the fix the local one was read (the open went through the folder link) and the remote one asked the network.
// Windows makes a symbolic link only with Developer Mode on or the symbolic-link privilege: where it can't, this test FAILS, saying so, and never skips (a check that can't
// run is a failed check: Tao 1). Folders are made in the temp folder and taken away again.
// Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>

#include <string>
#include <vector>

#include <windows.h>

#include "video_import.h"

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

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

int main()
{
	wchar_t temp[MAX_PATH];
	DWORD const length = GetTempPathW(MAX_PATH, temp);

	if (length == 0 || length >= MAX_PATH)
	{
		Check(false, "no temp folder: nothing was tested");
		return 1;
	}

	std::wstring const root = std::wstring(temp) + L"video_symlink_folder_" + std::to_wstring(GetCurrentProcessId());
	std::wstring const real = root + L"\\real";
	std::wstring const local = root + L"\\local";
	std::wstring const remote = root + L"\\remote";
	bool made = CreateDirectoryW(root.c_str(), nullptr) && CreateDirectoryW(real.c_str(), nullptr);
	HANDLE const file = made ? CreateFileW((real + L"\\a.mp4").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr) : INVALID_HANDLE_VALUE;

	if (file != INVALID_HANDLE_VALUE)
	{
		DWORD wrote = 0;
		char const bytes[100] = {};

		made = WriteFile(file, bytes, sizeof(bytes), &wrote, nullptr) && wrote == sizeof(bytes);
		CloseHandle(file);
	}

	Check(made && file != INVALID_HANDLE_VALUE, "the test's folder and a file of 100 bytes in it are made in the temp folder");

	DWORD const flags = SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
	bool const localMade = made && CreateSymbolicLinkW(local.c_str(), real.c_str(), flags) != 0;
	DWORD const localError = localMade ? 0 : GetLastError();
	bool const remoteMade = made && CreateSymbolicLinkW(remote.c_str(), L"\\\\127.0.0.1\\x", flags) != 0;
	DWORD const remoteError = remoteMade ? 0 : GetLastError();

	Check(localMade && remoteMade, "two folder symbolic links are made (mklink /D: one to the local folder, one to \\\\127.0.0.1\\x)" +
		(localMade && remoteMade ? std::string() : " -- Windows refused (error " + std::to_string((unsigned long)(localError ? localError : remoteError)) +
			"): it needs Developer Mode or the symbolic-link privilege, and a check that can't run FAILS, never skips"));

	if (localMade && remoteMade)
	{
		std::vector<uint8_t> bytes;
		std::string why;
		VideoImportResult const throughLocal = VideoImport_ReadFile(local + L"\\a.mp4", &bytes, &why);

		Check(throughLocal == VideoImportResult::NotAFile && bytes.empty() && why.find("symbolic link") != std::string::npos,
			"a video through a folder that is a symbolic link to a LOCAL folder: refused, nothing read (" + why + ")");

		why.clear();
		VideoImportResult const throughRemote = VideoImport_ReadFile(remote + L"\\a.mp4", &bytes, &why);

		Check(throughRemote == VideoImportResult::NotAFile && bytes.empty() && why.find("symbolic link") != std::string::npos,
			"a video through a folder that is a symbolic link to ANOTHER COMPUTER (\\\\127.0.0.1\\x): refused by the walk, before any open (" + why + ")");

		why.clear();

		VideoImportResult const byItsOwnPath = VideoImport_ReadFile(real + L"\\a.mp4", &bytes, &why);

		Check(byItsOwnPath == VideoImportResult::Done && bytes.size() == 100, "the same file by its own path: still read (" + why + ")");
	}

	// taken away again (RemoveDirectory on a folder link takes the link away, never what it points at)
	RemoveDirectoryW(local.c_str());
	RemoveDirectoryW(remote.c_str());
	DeleteFileW((real + L"\\a.mp4").c_str());
	RemoveDirectoryW(real.c_str());
	Check(RemoveDirectoryW(root.c_str()) != 0, "and the test's folders are taken away");

	printf("%s: %d failed\n", checksFailed ? "FAILED" : "ok", checksFailed);
	return checksFailed ? 1 : 0;
}
