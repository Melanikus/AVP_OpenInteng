// make_junction.h: a junction (a folder that leads to another folder) made for a test, WITHOUT a shell: no command line is ever built or run, so nothing a
// test is given (its scratch folder comes from its own command line) can reach one. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng, after the
// Snyk Code gate found "mklink /J" run through system() with a path from argv (cpp/CommandInjection/test, 3 notes on 3fe28ce8). AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// HOW: the folder `link` is made, then given a mount-point reparse point (IO_REPARSE_TAG_MOUNT_POINT, what mklink /J writes) naming `target`'s whole path, by
// FSCTL_SET_REPARSE_POINT on a handle to the folder itself. A junction needs no privilege (a symbolic link would). Taken away again by RemoveDirectory, which
// removes the junction and never what it leads to.
// THE BUFFER IS OF A FIXED SIZE, the names put in it a letter at a time, each write's place checked against the array's own size: no byte count is ever made
// from the paths and no block copy is made with one, so no path, however long, can write past it (the Snyk Code gate, 548b4b36 and da00e516: a memcpy whose
// size came from the path, cpp/BufferOverflow and cpp/IntegerOverflow; a bound before it wasn't enough to show that, and this way there is nothing to show).
#pragma once

#include <windows.h>
#include <winioctl.h>

#include <stddef.h>

#include <string>

// What FSCTL_SET_REPARSE_POINT takes for a junction (the mount-point form of Windows' REPARSE_DATA_BUFFER), with room for two whole paths and their prefix
struct MakeJunctionPoint
{
	DWORD tag;                     // IO_REPARSE_TAG_MOUNT_POINT
	WORD dataLength;               // the bytes after these first 8
	WORD reserved;
	WORD substituteOffset;         // in the names, in bytes: the name it leads to ("\??\C:\..."), its 0 not counted
	WORD substituteLength;
	WORD printOffset;              // the name to show ("C:\...")
	WORD printLength;
	wchar_t names[2 * (MAX_PATH + 8)];
};

// True when `link` is now a junction to `target` (both whole or relative paths; target must be a folder that is there). False, with nothing left behind,
// when it couldn't be made (link there already, a name too long, the call refused)
inline bool MakeJunction(std::wstring const &link, std::wstring const &target)
{
	wchar_t whole[MAX_PATH] = {};
	DWORD const length = GetFullPathNameW(target.c_str(), MAX_PATH, whole, nullptr);

	if (length == 0 || length >= MAX_PATH)
	{
		return false;
	}

	MakeJunctionPoint point = {};
	size_t const room = sizeof(point.names) / sizeof(point.names[0]);
	size_t at = 0;
	bool fits = true;
	// one letter put at the next place, when there is one
	auto const put = [&point, &at, &fits, room](wchar_t letter)
	{
		fits = fits && at < room;

		if (fits)
		{
			point.names[at++] = letter;
		}
	};

	// the substitute name: "\??\", the whole path; its 0
	for (wchar_t const letter : std::wstring(L"\\??\\"))
	{
		put(letter);
	}

	for (size_t i = 0; i < MAX_PATH && whole[i] != L'\0'; i++)
	{
		put(whole[i]);
	}

	size_t const substituteLetters = at;

	put(L'\0');

	// the print name: the whole path; its 0
	size_t const printAt = at;

	for (size_t i = 0; i < MAX_PATH && whole[i] != L'\0'; i++)
	{
		put(whole[i]);
	}

	size_t const printLetters = at - printAt;

	put(L'\0');

	// the lengths, in bytes, from the letters put (at most `room`, a few hundred: every number fits its 16 bits)
	size_t const used = at;

	point.tag = IO_REPARSE_TAG_MOUNT_POINT;
	point.substituteOffset = 0;
	point.substituteLength = (WORD)(substituteLetters * sizeof(wchar_t));
	point.printOffset = (WORD)(printAt * sizeof(wchar_t));
	point.printLength = (WORD)(printLetters * sizeof(wchar_t));
	point.dataLength = (WORD)(offsetof(MakeJunctionPoint, names) - offsetof(MakeJunctionPoint, substituteOffset) + used * sizeof(wchar_t));

	DWORD const pointBytes = (DWORD)(offsetof(MakeJunctionPoint, names) + used * sizeof(wchar_t));

	if (!fits || !CreateDirectoryW(link.c_str(), nullptr))
	{
		return false;
	}

	HANDLE const folder = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	DWORD returned = 0;
	bool const made = folder != INVALID_HANDLE_VALUE && DeviceIoControl(folder, FSCTL_SET_REPARSE_POINT, &point, pointBytes, nullptr, 0, &returned, nullptr) != 0;

	if (folder != INVALID_HANDLE_VALUE)
	{
		CloseHandle(folder);
	}

	if (!made)
	{
		RemoveDirectoryW(link.c_str());
	}

	return made;
}

// The same with the code page's paths (as the tests' A functions take them), turned into Windows' wide ones as Windows itself does (CP_ACP)
inline bool MakeJunction(std::string const &link, std::string const &target)
{
	auto const wide = [](std::string const &narrow)
	{
		int const letters = MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), nullptr, 0);
		std::wstring text(letters > 0 ? (size_t)letters : 0, L'\0');

		if (letters > 0)
		{
			MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), &text[0], letters);
		}

		return text;
	};
	std::wstring const wideLink = wide(link);
	std::wstring const wideTarget = wide(target);

	return !wideLink.empty() && !wideTarget.empty() && MakeJunction(wideLink, wideTarget);
}
