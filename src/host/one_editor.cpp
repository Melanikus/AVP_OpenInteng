// one_editor.cpp: one editor at a time in each program folder (one_editor.h says why). Written 2026-11-14 by Melanikus (Hicks) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <windows.h>
#include <wchar.h>

#include "one_editor.h"

const wchar_t kOneEditorName[] = L"Local\\AVP_OpenInteng.OneEditor.";

// The token, kept until the program ends: Windows gives it back then by itself
static HANDLE heldToken = NULL;

bool OneEditor_NameFor(const wchar_t *folder, wchar_t *name, size_t nameCount)
{
	if (!folder || !folder[0] || !name || nameCount == 0)
	{
		return false;
	}
	name[0] = L'\0';

	// The folder itself, opened only to ask Windows its final name: no access to what's in it is asked for. A folder needs
	// FILE_FLAG_BACKUP_SEMANTICS to be opened at all. Links and junctions are followed, so their target's name comes back.
	HANDLE const opened = CreateFileW(folder, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (opened == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	wchar_t finalName[32768];
	DWORD const length = GetFinalPathNameByHandleW(opened, finalName, (DWORD)(sizeof(finalName) / sizeof(finalName[0])),
		FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	CloseHandle(opened);
	if (length == 0 || length >= sizeof(finalName) / sizeof(finalName[0]))
	{
		return false;
	}

	// In capitals, as Windows compares paths, and without a last "\" (a drive's own folder ends with one)
	CharUpperBuffW(finalName, length);
	DWORD used = length;
	while (used > 0 && finalName[used - 1] == L'\\')
	{
		used--;
	}

	// The folder's number: 64-bit FNV-1a over the path's characters, written as 16 hex digits
	unsigned long long hash = 14695981039346656037ULL;
	for (DWORD i = 0; i < used; i++)
	{
		hash ^= (unsigned long long)finalName[i];
		hash *= 1099511628211ULL;
	}
	int const written = _snwprintf_s(name, nameCount, _TRUNCATE, L"%s%016llX", kOneEditorName, hash);
	return written > 0;
}

const char *OneEditor_Words(OneEditorAnswer answer)
{
	switch (answer)
	{
		case OneEditorAnswer::Taken:
			return "this editor is the one running from this folder";
		case OneEditorAnswer::AnotherIsRunning:
			return "another AVP_OpenInteng is already running from this folder. Close it first, then open the level again.";
		case OneEditorAnswer::FolderNotNamed:
			return "Windows couldn't give the full name of the program's own folder, so the editor can't tell whether another one is running "
				"from it";
		default:
			return "Windows wouldn't make the token that keeps two editors from running from the same folder";
	}
}

OneEditorAnswer OneEditor_ClaimFor(const wchar_t *folder)
{
	if (heldToken)
	{
		return OneEditorAnswer::Taken;
	}

	wchar_t name[128];
	if (!OneEditor_NameFor(folder, name, sizeof(name) / sizeof(name[0])))
	{
		return OneEditorAnswer::FolderNotNamed;
	}

	HANDLE const token = CreateMutexW(NULL, FALSE, name);
	DWORD const said = GetLastError();

	// No token at all: one by that name is there and this program may not open it (another program holds its name), or Windows wouldn't
	// make one. Either way this editor doesn't go on, and its words say which.
	if (!token)
	{
		return said == ERROR_ACCESS_DENIED ? OneEditorAnswer::AnotherIsRunning : OneEditorAnswer::WindowsWouldNotSay;
	}

	// The token was there already: another editor in this folder is running, and this one only opened it, so it's closed again
	if (said == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(token);
		return OneEditorAnswer::AnotherIsRunning;
	}

	heldToken = token;
	return OneEditorAnswer::Taken;
}

OneEditorAnswer OneEditor_Claim()
{
	// The program's own folder: its full name, up to its last "\"
	wchar_t program[32768];
	DWORD const length = GetModuleFileNameW(NULL, program, (DWORD)(sizeof(program) / sizeof(program[0])));
	if (length == 0 || length >= sizeof(program) / sizeof(program[0]))
	{
		return OneEditorAnswer::FolderNotNamed;
	}
	wchar_t *const lastSlash = wcsrchr(program, L'\\');
	if (!lastSlash)
	{
		return OneEditorAnswer::FolderNotNamed;
	}
	lastSlash[1] = L'\0';
	return OneEditor_ClaimFor(program);
}
