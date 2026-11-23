// The test for ShapeExtract_ReadMapFile (src/host/shape_extract_files.h) on a SYMBOLIC LINK: a link named x.rif to a plain map file is refused, as every
// reparse point is (host_shape_extract_files_test tries the rest). Windows makes a symbolic link only with Developer Mode on or the symbolic-link
// privilege: where it can't, this test FAILS, saying so, and never skips (a check that can't run fails; Tao 1). GitHub's runners can make one.
// Written 2026-11-23 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The program gives back a non-zero result if any check failed.

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include <windows.h>

#include "shape_extract_files.h"

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

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

int main()
{
	puts("the hostile map read: a symbolic link");

	char temp[MAX_PATH];
	DWORD const length = GetTempPathA(MAX_PATH, temp);

	if (length == 0 || length >= MAX_PATH)
	{
		printf("  FAILED  no temp folder: nothing was tested\n");
		return 1;
	}

	std::string const root = std::string(temp) + "shape_extract_files_symlink_test_" + std::to_string((unsigned long)GetCurrentProcessId());
	std::string const real = root + "\\Real.rif", link = root + "\\Link.rif";
	bool made = CreateDirectoryA(root.c_str(), nullptr) != 0;
	HANDLE const file = made ? CreateFileA(real.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr) : INVALID_HANDLE_VALUE;
	DWORD wrote = 0;

	made = file != INVALID_HANDLE_VALUE && WriteFile(file, "a map", 5, &wrote, nullptr) && wrote == 5;

	if (file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
	}

	Check(made, "the test's folder and a plain map file of 5 bytes in it are made in the temp folder: " + root);

	bool const linked = made && CreateSymbolicLinkA(link.c_str(), real.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
	DWORD const error = linked ? 0 : GetLastError();

	Check(linked, "Link.rif is made a symbolic link to it (mklink)" + (linked ? std::string() : " -- Windows refused (error " + std::to_string((unsigned long)error) +
		"): it needs Developer Mode or the symbolic-link privilege, and a check that can't run FAILS, never skips"));

	if (linked)
	{
		std::vector<char> bytes;
		std::string sha256;
		std::string const why = ShapeExtract_ReadMapFile(link, &bytes, &sha256);

		Check(why.find("a link") != std::string::npos && bytes.empty() && sha256.empty(), "the symbolic link is REFUSED, saying \"a link\" (it says \"" + why +
			"\"), and gives back no bytes and no SHA-256");

		std::string const realWhy = ShapeExtract_ReadMapFile(real, &bytes, &sha256);

		Check(realWhy.empty() && bytes.size() == 5, "and the file it points at, by its own name, is read (it says \"" + realWhy + "\")");
	}

	DeleteFileA(link.c_str());
	DeleteFileA(real.c_str());
	Check(RemoveDirectoryA(root.c_str()) != 0, "and the test's folder is taken away");

	printf(checksFailed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return checksFailed ? 1 : 0;
}
