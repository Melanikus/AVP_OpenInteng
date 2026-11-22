// The test for THE FOLDERS ON THE WAY to a video (src/host/video_import.h; Vasquez's Video mode review, the MUST; Drake 11-22): which link kinds a folder may be (a junction or
// a mounted volume, OneDrive's placeholders; never a symbolic link nor a kind not known), a real junction to a local folder still taken, the read through it done, and the
// paths the walk refuses (a folder that isn't there, a "folder" that is a file, a path that isn't a whole one on a drive). The symbolic link itself, which Windows makes only
// with Developer Mode or its privilege, is host_video_symlink_folder_test's. Folders are made in the temp folder and taken away again.
// Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>
#include <stdlib.h>

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

static bool WriteBytes(std::wstring const &path, size_t count)
{
	HANDLE const file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	std::vector<uint8_t> const bytes(count, 0x5a);
	DWORD wrote = 0;
	bool const good = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &wrote, nullptr) && wrote == bytes.size();

	CloseHandle(file);
	return good;
}

int main()
{
	// THE KINDS (winnt.h's numbers)
	Check(VideoImport_FolderTagTaken(0xA0000003ul), "a junction or a mounted volume (IO_REPARSE_TAG_MOUNT_POINT) is taken: only ever local");
	Check(VideoImport_FolderTagTaken(0x9000001Aul) && VideoImport_FolderTagTaken(0x9000101Aul) && VideoImport_FolderTagTaken(0x9000F01Aul),
		"OneDrive's placeholders (IO_REPARSE_TAG_CLOUD, CLOUD_1, CLOUD_F) are taken");
	Check(!VideoImport_FolderTagTaken(0xA000000Cul), "a symbolic link (IO_REPARSE_TAG_SYMLINK), which can name another computer, is refused");
	Check(!VideoImport_FolderTagTaken(0x8000000Aul), "a DFS link (IO_REPARSE_TAG_DFS), another computer's, is refused");
	Check(!VideoImport_FolderTagTaken(0xA000001Dul) && !VideoImport_FolderTagTaken(0x8000001Bul) && !VideoImport_FolderTagTaken(0x80000014ul),
		"a WSL symbolic link, an app's execution alias and an NFS link are refused");
	Check(!VideoImport_FolderTagTaken(0) && !VideoImport_FolderTagTaken(0xFFFFFFFFul) && !VideoImport_FolderTagTaken(0x9000001Bul),
		"0, all ones and a number next to the cloud's are refused: a kind not known is never taken");

	// THE WALK, on real folders
	wchar_t temp[MAX_PATH];
	DWORD const length = GetTempPathW(MAX_PATH, temp);

	if (length == 0 || length >= MAX_PATH)
	{
		Check(false, "no temp folder: nothing was tested");
		return 1;
	}

	std::wstring const root = std::wstring(temp) + L"video_folder_links_" + std::to_wstring(GetCurrentProcessId());
	std::wstring const real = root + L"\\real";
	std::wstring const junction = root + L"\\junction";
	bool const made = CreateDirectoryW(root.c_str(), nullptr) && CreateDirectoryW(real.c_str(), nullptr) && WriteBytes(real + L"\\a.mp4", 1000);

	Check(made, "the test's folder and a file of 1000 bytes in it are made in the temp folder");

	// a junction to the local folder, made as a mapper makes one (mklink /J needs no privilege)
	std::wstring const command = L"mklink /J \"" + junction + L"\" \"" + real + L"\" >nul";
	bool const joined = made && _wsystem(command.c_str()) == 0 && (GetFileAttributesW(junction.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

	Check(joined, "a junction to it is made (mklink /J)");

	std::vector<uint8_t> bytes;
	std::string why;

	// (each call made on a line of its own, before the words that report it are put together: C++ may build a call's arguments in any order)
	VideoImportResult const byItsOwnPath = VideoImport_ReadFile(real + L"\\a.mp4", &bytes, &why);

	Check(byItsOwnPath == VideoImportResult::Done && bytes.size() == 1000, "the file read by its own path: done, 1000 bytes (" + why + ")");
	bytes.clear();
	why.clear();

	VideoImportResult const throughJunction = joined ? VideoImport_ReadFile(junction + L"\\a.mp4", &bytes, &why) : VideoImportResult::BadCall;

	Check(throughJunction == VideoImportResult::Done && bytes.size() == 1000,
		"read THROUGH THE JUNCTION (a local folder): still done, 1000 bytes (" + why + ")");

	// A FOLDER THAT IS A LINK OF A KIND NOT KNOWN, end to end on this computer: an empty folder given a reparse point of a made-up third-party kind (0x00001234, with its GUID),
	// which Windows lets any user set (only a symbolic link needs the privilege). The walk reads its kind on the link itself and refuses it before any open goes through it
	std::wstring const odd = root + L"\\odd";
	bool tagged = made && CreateDirectoryW(odd.c_str(), nullptr);

	if (tagged)
	{
		HANDLE const folder = CreateFileW(odd.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		REPARSE_GUID_DATA_BUFFER point = {};
		DWORD returned = 0;

		point.ReparseTag = 0x00001234ul;
		point.ReparseDataLength = 0;
		point.ReparseGuid = { 0x6d0c1b5e, 0x2f4a, 0x4c7e, { 0x9a, 0x51, 0x3b, 0x8e, 0x22, 0x10, 0x7f, 0x44 } };
		tagged = folder != INVALID_HANDLE_VALUE && DeviceIoControl(folder, FSCTL_SET_REPARSE_POINT, &point, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, nullptr, 0, &returned, nullptr);

		if (folder != INVALID_HANDLE_VALUE)
		{
			CloseHandle(folder);
		}
	}

	Check(tagged && (GetFileAttributesW(odd.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0, "a folder made a link of a made-up kind (0x00001234), which any user may set");
	bytes.clear();
	why.clear();
	VideoImportResult const throughOdd = tagged ? VideoImport_ReadFile(odd + L"\\a.mp4", &bytes, &why) : VideoImportResult::BadCall;

	Check(throughOdd == VideoImportResult::NotAFile && bytes.empty() && why.find("a link of a kind not taken (0x00001234)") != std::string::npos,
		"a video through it: refused by the walk, naming the kind, nothing read (" + why + ")");
	RemoveDirectoryW(odd.c_str());

	// what the walk refuses
	bool const missingTaken = VideoImport_FoldersOnTheWayTaken(root + L"\\no such folder\\a.mp4", &why);

	Check(!missingTaken && why.find("no folder") != std::string::npos, "a folder that isn't there: refused (" + why + ")");

	bool const fileTaken = VideoImport_FoldersOnTheWayTaken(real + L"\\a.mp4\\b.mp4", &why);

	Check(!fileTaken && why.find("not a folder") != std::string::npos, "a file on the way where a folder should be: refused (" + why + ")");
	Check(!VideoImport_FoldersOnTheWayTaken(L"", &why) && !VideoImport_FoldersOnTheWayTaken(L"C:", &why) && !VideoImport_FoldersOnTheWayTaken(L"\\\\server\\share\\a.mp4", &why) &&
		!VideoImport_FoldersOnTheWayTaken(L"a\\b.mp4", &why), "an empty path, a drive alone, another computer's share and a relative path: refused before anything is read");
	why.clear();

	bool const doubleTaken = VideoImport_FoldersOnTheWayTaken(real + L"\\\\a.mp4", &why);

	Check(doubleTaken, "two slashes in a row add no folder: taken (" + why + ")");
	Check(VideoImport_FoldersOnTheWayTaken(L"C:\\a.mp4", &why), "a file right on a drive's root: no folder on the way, taken");

	// taken away again (RemoveDirectory on a junction takes the junction away, never what it points at)
	RemoveDirectoryW(junction.c_str());
	DeleteFileW((real + L"\\a.mp4").c_str());
	RemoveDirectoryW(real.c_str());
	Check(RemoveDirectoryW(root.c_str()) != 0, "and the test's folders are taken away");

	printf("%s: %d failed\n", checksFailed ? "FAILED" : "ok", checksFailed);
	return checksFailed ? 1 : 0;
}
