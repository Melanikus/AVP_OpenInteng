#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "arg_number.h"

// THE CAMERA FILE AS INTENG WRITES IT, for the camera file's acceptance (tests/scenarios/acceptance-camera-file-restore.txt): the one .cam in the editor's camera folder is read by its bytes, as inteng's
// format has them (FUN_00470610: the places of slots 0 to 9, 12 bytes each; their matrices, 36 bytes each; then slot 10's place and matrix: 528 bytes), with no code of the editor's.
// Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_camera_file <the camera folder> <slot, 0 to 10> <x> <y> <z> [<nine matrix numbers>]
//   the folder holds exactly one .cam file, of 528 bytes; that slot's place in it is x y z (the file's own numbers: inteng's 1/100 mm), and, when given, its matrix the nine numbers (each within 2,
//   the game's sine table being whole numbers). Gives back 0 when every check passed, 1 when one failed, 2 for a command line it does not take.

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if ((argc != 6 && argc != 15) || strstr(argv[1], "..") != nullptr)
	{
		printf("  verify_camera_file <the camera folder> <slot, 0 to 10> <x> <y> <z> [<nine matrix numbers>]: nothing to check\n");
		return 2;
	}

	long slot = 0;
	long wanted[12] = {};

	if (!ArgNumber("the slot", argv[2], 0, 10, &slot))
	{
		return 2;
	}

	for (int i = 3; i < argc; i++)
	{
		if (!ArgNumber("a number of the slot", argv[i], -2147483647L - 1, 2147483647L, &wanted[i - 3]))
		{
			return 2;
		}
	}

	std::string folder = argv[1];

	if (!folder.empty() && folder.back() != '\\' && folder.back() != '/')
	{
		folder += '\\';
	}

	WIN32_FIND_DATAA found;
	HANDLE const search = FindFirstFileA((folder + "*.cam").c_str(), &found);
	std::vector<std::string> names;

	if (search != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
			{
				names.push_back(found.cFileName);
			}
		} while (FindNextFileA(search, &found));

		FindClose(search);
	}

	Check(names.size() == 1, "the camera folder holds one .cam file: " + std::to_string((unsigned long long)names.size()) + (names.empty() ? std::string() : " (" + names[0] + ")"));

	if (names.size() != 1)
	{
		return 1;
	}

	HANDLE const file = CreateFileA((folder + names[0]).c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	LARGE_INTEGER size = {};
	std::vector<uint8_t> bytes(528);
	DWORD got = 0;
	bool const whole = file != INVALID_HANDLE_VALUE && GetFileSizeEx(file, &size) && size.QuadPart == 528 && ReadFile(file, bytes.data(), 528, &got, 0) && got == 528;

	if (file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
	}

	Check(whole, "it is read whole, 528 bytes (it has " + std::to_string((long long)size.QuadPart) + ")");

	if (!whole)
	{
		return 1;
	}

	auto const at = [&](size_t place) { return (int32_t)((uint32_t)bytes[place] | ((uint32_t)bytes[place + 1] << 8) | ((uint32_t)bytes[place + 2] << 16) | ((uint32_t)bytes[place + 3] << 24)); };
	size_t const placeAt = slot < 10 ? (size_t)slot * 12 : 0x78 + 0x168;
	size_t const matrixAt = slot < 10 ? 0x78 + (size_t)slot * 36 : 0x78 + 0x168 + 0xc;
	std::string had;

	for (int i = 0; i < 3; i++)
	{
		had += (i ? " " : "") + std::to_string(at(placeAt + (size_t)i * 4));
	}

	Check(at(placeAt) == wanted[0] && at(placeAt + 4) == wanted[1] && at(placeAt + 8) == wanted[2], "slot " + std::to_string(slot) + "'s place is " + std::to_string(wanted[0]) + " " + std::to_string(wanted[1]) + " "
		+ std::to_string(wanted[2]) + " (it is " + had + ")");

	if (argc == 15)
	{
		bool same = true;

		had.clear();

		for (int k = 0; k < 9; k++)
		{
			long long const one = at(matrixAt + (size_t)k * 4);

			had += (k ? " " : "") + std::to_string(one);
			same = same && one - wanted[3 + k] <= 2 && wanted[3 + k] - one <= 2;
		}

		Check(same, "slot " + std::to_string(slot) + "'s matrix is the nine numbers given, each within 2 (it is " + had + ")");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
