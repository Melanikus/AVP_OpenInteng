// dll_load.h: loading a DLL the editor ships only when it is the file the editor expects: from the editor's own folder, by its full path, its SHA-256 checked first (Bishop's ruling
// 11-18; one loader for the FMV preview's libsmackerdec.dll and Import video's libsmackerenc.dll). Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng, the call agreed with
// Vasquez. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE STEPS: the name must be a plain one ending in .dll (no folder, no "..", no ':' or slash); the file is <folder>\<name>, never looked for on the search path. It is opened letting
// others only READ it (FILE_SHARE_READ), so while it is hashed and loaded nobody can write, rename or delete it; it must be a plain file on a disk (not a folder, not a link or
// junction: FILE_FLAG_OPEN_REPARSE_POINT) of at most kDllLoadMostBytes. Its SHA-256 (Windows' own BCrypt) is read from that handle and compared with the caller's expected one
// (the build's pin, dll_pins.h, or a hash the user named in the ini). Only then LoadLibraryExW on the full path, the DLL's own needs looked for only beside it and in System32
// (LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32), with the handle still held; it is let go after. Every refusal gives words for the status line, naming the file
// by its name only (never a full path: the screen shows no folder of the user's).
#pragma once

#include <windows.h>

#include <stdint.h>

#include <string>

namespace host
{
	uint64_t const kDllLoadMostBytes = 64ull * 1024 * 1024;

	// A plain DLL name: letters, digits, '_', '-' and '.', not starting with '.', no "..", ending in ".dll" (any capitals), under 64 letters
	bool DllLoad_PlainName(wchar_t const *name);

	// Exactly 64 hex digits (any capitals)
	bool DllLoad_IsSha256Hex(char const *hex);

	// The SHA-256 of an open file's bytes from its start, 64 lowercase hex digits; false with *why when Windows can't hash or read it
	bool DllLoad_HandleSha256(HANDLE file, std::string *hex, std::string *why);

	// The same for a file by its path (opened for reading, shared with readers only); for the tests and the pin tool
	bool DllLoad_FileSha256(wchar_t const *path, std::string *hex, std::string *why);

	// <folder>\<fileName>, loaded only as the top says; nullptr with *why otherwise. An expected hash that is not 64 hex digits (the "" of a build with no pin) loads nothing.
	HMODULE DllLoad_PinnedIn(std::wstring const &folder, wchar_t const *fileName, char const *sha256Hex, std::string *why);

	// The same from the folder the editor itself runs from (GetModuleFileNameW of the editor)
	HMODULE DllLoad_Pinned(wchar_t const *fileName, char const *sha256Hex, std::string *why);

	// That folder, no backslash at its end ("" when Windows won't say)
	std::wstring DllLoad_OwnFolder();
}
