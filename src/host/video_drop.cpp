// video_drop.cpp: a video file handed to the editor, checked before anything opens it (video_drop.h says what and why). Written 2026-11-22 by Melanikus (Dietrich)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "video_drop.h"

#include "video_import.h"

#include <wchar.h>

namespace
{
	size_t const kMostPathLetters = 32767;   // the longest name Windows has at all
	int const kMostDriveHops = 8;            // a SUBST of a SUBST of ...: this many letters followed, then refused

	bool IsSlash(wchar_t c)
	{
		return c == L'\\' || c == L'/';
	}

	bool IsLetter(wchar_t c)
	{
		return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
	}

	bool StartsWithNoCase(std::wstring const &text, wchar_t const *start)
	{
		size_t const length = wcslen(start);

		return text.size() >= length && _wcsnicmp(text.c_str(), start, length) == 0;
	}

	// The file's own name, after the last slash
	std::wstring NameOf(std::wstring const &path)
	{
		size_t const slash = path.find_last_of(L"\\/");

		return slash == std::wstring::npos ? path : path.substr(slash + 1);
	}

	// Whether drive `letter` stands for this computer's own storage: its name in this computer's table (QueryDosDevice, which asks no other computer) is no share,
	// a SUBST of another letter's folder is followed to that letter, and only then is the drive's kind asked (GetDriveType). *why says which part refused
	bool DriveIsLocal(wchar_t letter, std::string *why)
	{
		for (int hop = 0; hop < kMostDriveHops; hop++)
		{
			wchar_t const device[3] = { letter, L':', L'\0' };
			wchar_t target[1024] = {};

			if (QueryDosDeviceW(device, target, (DWORD)(sizeof(target) / sizeof(target[0]))) == 0)
			{
				*why = std::string("drive ") + (char)letter + ": is not one this computer knows";
				return false;
			}

			std::wstring const first(target);

			if (host::VideoDrop_TargetIsNetwork(first))
			{
				*why = std::string("drive ") + (char)letter + ": is another computer's share, and the editor never opens one (Windows would contact that computer)";
				return false;
			}

			// a SUBST of a folder on another letter (\??\Z:\folder): that letter is asked in turn
			if (StartsWithNoCase(first, L"\\??\\") && first.size() >= 6 && IsLetter(first[4]) && first[5] == L':')
			{
				if ((first[4] | 0x20) == (letter | 0x20))
				{
					break;   // a SUBST of a folder on its own letter's drive: that drive is asked below
				}

				letter = first[4];
				continue;
			}

			wchar_t const root[4] = { letter, L':', L'\\', L'\0' };
			UINT const kind = GetDriveTypeW(root);

			if (kind == DRIVE_FIXED || kind == DRIVE_REMOVABLE || kind == DRIVE_CDROM || kind == DRIVE_RAMDISK)
			{
				return true;
			}

			*why = std::string("drive ") + (char)letter + (kind == DRIVE_REMOTE ? ": is another computer's share, and the editor never opens one (Windows would contact that computer)" :
				": is not a drive of this computer");
			return false;
		}

		*why = std::string("drive ") + (char)letter + ": stands for another drive too many times over";
		return false;
	}
}

std::vector<std::wstring> host::VideoDrop_FromHdrop(HDROP drop, size_t most, size_t *more)
{
	std::vector<std::wstring> paths;
	UINT const count = drop ? DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0) : 0;

	for (UINT i = 0; i < count && paths.size() < most; i++)
	{
		UINT const letters = DragQueryFileW(drop, i, nullptr, 0);

		if (letters == 0 || letters > kMostPathLetters)
		{
			paths.push_back(std::wstring());   // (refused by the check: no full path)
			continue;
		}

		std::wstring path(letters + 1, L'\0');
		UINT const got = DragQueryFileW(drop, i, &path[0], letters + 1);

		path.resize(got <= letters ? got : 0);
		paths.push_back(path);
	}

	if (more)
	{
		*more = count > paths.size() ? count - paths.size() : 0;
	}

	return paths;
}

std::vector<std::wstring> host::VideoDrop_FromHarness(char const *text, size_t bytes)
{
	std::vector<std::wstring> paths;
	size_t at = 0;

	while (text && at < bytes)
	{
		size_t end = at;

		while (end < bytes && text[end] != '\n')
		{
			end++;
		}

		int const length = (int)(end - at);
		int const letters = length > 0 ? MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text + at, length, nullptr, 0) : 0;
		std::wstring path(letters > 0 ? (size_t)letters : 0, L'\0');

		if (letters > 0 && (size_t)letters <= kMostPathLetters)
		{
			MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text + at, length, &path[0], letters);
		}
		else
		{
			path.clear();   // not UTF-8, or too long: refused by the check
		}

		paths.push_back(path);
		at = end + 1;
	}

	return paths;
}

bool host::VideoDrop_TargetIsNetwork(std::wstring const &target)
{
	return StartsWithNoCase(target, L"\\Device\\LanmanRedirector") || StartsWithNoCase(target, L"\\Device\\Mup") || StartsWithNoCase(target, L"\\??\\UNC\\") ||
		StartsWithNoCase(target, L"\\Device\\WebDavRedirector") || StartsWithNoCase(target, L"\\Device\\WinDfs") || StartsWithNoCase(target, L"\\\\");
}

std::string host::VideoDrop_Words(std::wstring const &path)
{
	std::string words;

	for (wchar_t const c : path)
	{
		words.push_back(c >= 0x20 && c < 0x7f ? (char)c : '?');
	}

	return words;
}

bool host::VideoDrop_CheckPath(std::vector<std::wstring> const &paths, size_t listed, char const *what, std::wstring *path, std::string *why)
{
	path->clear();

	if (listed == 0 || paths.empty())
	{
		*why = "Not taken: no file was dropped";
		return false;
	}

	if (listed > 1)
	{
		*why = std::string("Not taken: one ") + (what ? what : "file") + " at a time (" + std::to_string((unsigned long long)listed) + " files were dropped)";
		return false;
	}

	std::wstring const &given = paths[0];

	if (given.empty() || given.size() > kMostPathLetters)
	{
		*why = "Not taken: the file's name could not be read";
		return false;
	}

	for (wchar_t const c : given)
	{
		if (c < 0x20)
		{
			*why = "Not taken: the file's name has a control character in it";
			return false;
		}
	}

	std::string const shown = VideoDrop_Words(given);

	// FROM THE LETTERS ALONE, before any call: another computer, a device, Windows' own prefixes
	if ((given.size() >= 2 && IsSlash(given[0]) && IsSlash(given[1])) || StartsWithNoCase(given, L"\\??\\"))
	{
		*why = "Not taken: " + shown + " is on another computer, or is a device, and the editor never opens one (Windows would contact that computer)";
		return false;
	}

	if (!(given.size() >= 3 && IsLetter(given[0]) && given[1] == L':' && IsSlash(given[2])))
	{
		*why = "Not taken: " + shown + " is not a full name on a drive of this computer";
		return false;
	}

	if (given.find(L':', 2) != std::wstring::npos)
	{
		*why = "Not taken: " + shown + " names a part of a file (a stream), not a file";
		return false;
	}

	// the full name, by its letters only (GetFullPathName opens nothing): .. and forward slashes folded, the same drive
	DWORD const need = GetFullPathNameW(given.c_str(), 0, nullptr, nullptr);
	std::wstring full(need > 0 && need <= kMostPathLetters + 1 ? (size_t)need : 0, L'\0');
	DWORD const got = full.empty() ? 0 : GetFullPathNameW(given.c_str(), need, &full[0], nullptr);

	full.resize(got > 0 && got < need ? got : 0);

	if (full.size() < 3 || !IsLetter(full[0]) || (full[0] | 0x20) != (given[0] | 0x20) || full[1] != L':' || full[2] != L'\\')
	{
		*why = "Not taken: " + shown + " is not a full name on a drive of this computer";
		return false;
	}

	std::string drive;

	if (!DriveIsLocal(full[0], &drive))
	{
		*why = "Not taken: " + shown + ": " + drive;
		return false;
	}

	*path = full;
	return true;
}

bool host::VideoDrop_Check(std::vector<std::wstring> const &paths, size_t listed, std::wstring *path, std::string *why)
{
	std::wstring full;

	if (!VideoDrop_CheckPath(paths, listed, "video", &full, why))
	{
		path->clear();
		return false;
	}

	// the ONE list of endings (video_import.h, the owner's ruling 11-22: Media Foundation's common containers)
	if (!VideoImport_ContentType(full))
	{
		*why = "Not taken: " + VideoDrop_Words(NameOf(full)) + " is not a video the editor takes (" + VideoImport_EndingsWords() + ")";
		return false;
	}

	*path = full;
	return true;
}
