// given_paths.h: a test's paths from its command line, made WHOLE and checked before any path is made from them, so nothing a test is given is used as
// it came (Snyk Code reads a command line's text as hostile: cpp/PT and cpp/IntegerOverflow on paths built from argv). As verify_shape_identities' and
// verify_level_same's WholeFolder do: no "..", shorter than a path can be, made whole by Windows (GetFullPathNameA), then judged by what is there; what is
// used afterwards is the whole path, never the argument. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng, after the Snyk gate on vasquez-rimimport
// 41e310df (13 notes in host_texture_import_test and host_shape_import_read_test). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <windows.h>
#include <string.h>

#include <string>

// A scratch folder: made when it isn't there (one folder, in a folder that is), then it must be a plain folder on the disk, not a link or a junction.
// *whole: its whole path, without a last '\'. False for anything else.
inline bool GivenScratchFolder(char const *given, std::string *whole)
{
	if (!given || !given[0] || strlen(given) >= MAX_PATH || strstr(given, "..") != nullptr)
	{
		return false;
	}

	char full[MAX_PATH] = "";
	DWORD const length = GetFullPathNameA(given, MAX_PATH, full, nullptr);

	if (length == 0 || length >= MAX_PATH)
	{
		return false;
	}

	if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES && !CreateDirectoryA(full, nullptr))
	{
		return false;
	}

	DWORD const kind = GetFileAttributesA(full);

	if (kind == INVALID_FILE_ATTRIBUTES || (kind & FILE_ATTRIBUTE_DIRECTORY) == 0 || (kind & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
	{
		return false;
	}

	std::string folder(full);

	while (folder.size() > 3 && (folder.back() == '\\' || folder.back() == '/'))
	{
		folder.pop_back();
	}

	*whole = folder;
	return true;
}

// A file the test reads: it must be there, a plain file (not a folder or a link), its whole path ending in `ending` (any capitals: ".rif"). *whole: that
// path; *stem: its name without the folder and the ending ("Derelict"). False for anything else.
inline bool GivenFile(char const *given, char const *ending, std::string *whole, std::string *stem)
{
	if (!given || !given[0] || !ending || strlen(given) >= MAX_PATH || strstr(given, "..") != nullptr)
	{
		return false;
	}

	char full[MAX_PATH] = "";
	DWORD const length = GetFullPathNameA(given, MAX_PATH, full, nullptr);
	size_t const endLength = strlen(ending);

	if (length == 0 || length >= MAX_PATH || (size_t)length <= endLength || _stricmp(full + length - endLength, ending) != 0)
	{
		return false;
	}

	DWORD const kind = GetFileAttributesA(full);

	if (kind == INVALID_FILE_ATTRIBUTES || (kind & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
	{
		return false;
	}

	// the stem: the letters after the last '\' or '/', up to the ending (both found in the whole path, so the place of each is inside it)
	char const *name = full;

	for (char const *at = full; *at; at++)
	{
		name = (*at == '\\' || *at == '/') ? at + 1 : name;
	}

	char const *const endOfStem = full + length - endLength;

	*whole = std::string(full);
	*stem = std::string(name, endOfStem);
	return true;
}
