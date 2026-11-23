// The test for ShapeExtract_ReadMapFile (src/host/shape_extract_files.h): a map's file read as HOSTILE input, the read Option C makes of every map
// (Frost's shapes piece for Dietrich, 2026-11-23: "THE HOSTILE MAP READ", All stock maps' own rules as a call of its own). Real files, in a folder this
// test makes in the temp folder and takes away again:
//   a plain file is read whole, and its SHA-256 is the right one (known bytes, a known sum: not only the helper's own word); one of exactly 16 MB is read
//   and each of these is REFUSED, in its own words: no such file; a hard link's second name (and its first, which has two names too); a junction named
//   x.rif; a folder named x.rif; an empty file; a file of 16 MB and 1 byte; and NUL, which is no file on a disk
// A symbolic link is the same rule (a reparse point), but Windows makes one only with Developer Mode on or the symbolic-link privilege, so it has a test of
// its own, host_shape_extract_files_symlink_test, which FAILS where it can't make one (a check that can't run fails, never skips): this one runs anywhere.
// Written 2026-11-23 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The program gives back a non-zero result if any check failed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "make_junction.h"   // a junction made without a shell (Vasquez)
#include "shape_extract.h"
#include "shape_extract_files.h"

// the game's memory code (our_mem.cpp, for the decoder shape_extract.cpp uses) calls these: here they only say what happened
void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

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

static std::wstring Wide(std::string const &narrow)
{
	int const letters = MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), nullptr, 0);
	std::wstring wide(letters > 0 ? (size_t)letters : 0, L'\0');

	if (letters > 0)
	{
		MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), &wide[0], letters);
	}

	return wide;
}

// a file of these bytes, made new (any file there before is replaced: the test's own folder only)
static bool Write(std::string const &path, std::vector<char> const &bytes)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	size_t done = 0;
	bool good = true;

	while (good && done < bytes.size())
	{
		DWORD wrote = 0;
		DWORD const ask = (DWORD)(bytes.size() - done > 1024 * 1024 ? 1024 * 1024 : bytes.size() - done);

		good = WriteFile(file, bytes.data() + done, ask, &wrote, nullptr) && wrote == ask;
		done += wrote;
	}

	CloseHandle(file);
	return good;
}

// the read, with its words, its bytes and its sum
struct Read
{
	std::string why;
	std::vector<char> bytes;
	std::string sha256;
};

static Read ReadOf(std::string const &path)
{
	Read r;

	r.why = ShapeExtract_ReadMapFile(path, &r.bytes, &r.sha256);
	return r;
}

// a refusal: words of its own, and nothing given back
static void Refused(std::string const &path, std::string const &what, std::string const &words)
{
	Read const r = ReadOf(path);

	Check(r.why.find(words) != std::string::npos && r.bytes.empty() && r.sha256.empty(), what + ": REFUSED, saying \"" + words + "\" (it says \"" + r.why +
		"\"), and gives back no bytes and no SHA-256");
}

int main()
{
	puts("the hostile map read");

	char temp[MAX_PATH];
	DWORD const length = GetTempPathA(MAX_PATH, temp);

	if (length == 0 || length >= MAX_PATH)
	{
		printf("  FAILED  no temp folder: nothing was tested\n");
		return 1;
	}

	std::string const root = std::string(temp) + "shape_extract_files_test_" + std::to_string((unsigned long)GetCurrentProcessId());
	std::string const plain = root + "\\Plain.rif", whole = root + "\\Whole16MB.rif", first = root + "\\First.rif", second = root + "\\Second.rif";
	std::string const junction = root + "\\Junction.rif", target = root + "\\target", folder = root + "\\Folder.rif", empty = root + "\\Empty.rif";
	std::string const tooBig = root + "\\TooBig.rif";
	size_t const most = 16 * 1024 * 1024;   // a map's file at most (shape_extract.h kShapeExtractMostMapBytes, open_with.cpp's cap on a level)
	std::vector<char> const abc = { 'a', 'b', 'c' };

	bool const made = CreateDirectoryA(root.c_str(), nullptr) && Write(plain, abc) && Write(whole, std::vector<char>(most, 'x')) &&
		Write(first, std::vector<char>(100, 'y')) && CreateHardLinkA(second.c_str(), first.c_str(), nullptr) && CreateDirectoryA(target.c_str(), nullptr) &&
		CreateDirectoryA(folder.c_str(), nullptr) && Write(empty, std::vector<char>()) && Write(tooBig, std::vector<char>(most + 1, 'z'));

	Check(made, "the test's folder and files are made in the temp folder (a hard link's second name by CreateHardLinkA): " + root);

	// a junction named Junction.rif, to a folder (a junction needs no privilege; made without a shell, make_junction.h)
	bool const joined = made && MakeJunction(Wide(junction), Wide(target)) && (GetFileAttributesA(junction.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

	Check(joined, "Junction.rif is made a junction to a folder (make_junction.h, no shell)");

	// READ: the bytes whole, and the SHA-256 of "abc" (FIPS 180-2's own example), the helper's word on the same bytes too
	Read const good = ReadOf(plain);

	Check(good.why.empty() && good.bytes == abc, "a plain file is read, its 3 bytes whole (it says \"" + good.why + "\")");
	Check(good.sha256 == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "its SHA-256 is the right one: " + good.sha256);
	Check(good.sha256 == ShapeExtract_Sha256(good.bytes), "and the one ShapeExtract_Sha256 gives for those very bytes");

	Read const at = ReadOf(whole);

	Check(at.why.empty() && at.bytes.size() == most && at.sha256 == ShapeExtract_Sha256(at.bytes), "a file of exactly 16 MB is read whole (it says \"" + at.why + "\", " +
		std::to_string((unsigned long long)at.bytes.size()) + " bytes)");

	// REFUSED, each in its own words
	Refused(root + "\\Missing.rif", "no such file", "missing");
	Refused(second, "a hard link's second name", "2 names");
	Refused(first, "and its first name, which has two names too", "2 names");
	Refused(junction, "a junction named Junction.rif", "a link");
	Refused(folder, "a folder named Folder.rif", "a folder");
	Refused(empty, "an empty file", "empty");
	Refused(tooBig, "a file of 16 MB and 1 byte", "larger than 16 MB");
	Refused("NUL", "NUL, Windows' own device, no file on a disk", "not a file on a disk");

	// taken away again (RemoveDirectory on a junction takes the junction away, never what it points at)
	RemoveDirectoryA(junction.c_str());
	RemoveDirectoryA(target.c_str());
	RemoveDirectoryA(folder.c_str());

	for (std::string const &file : { plain, whole, first, second, empty, tooBig })
	{
		DeleteFileA(file.c_str());
	}

	Check(RemoveDirectoryA(root.c_str()) != 0, "and the test's folder is taken away");

	printf(checksFailed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return checksFailed ? 1 : 0;
}
