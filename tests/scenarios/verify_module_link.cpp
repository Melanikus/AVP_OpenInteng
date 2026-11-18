#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "object_edit.h"

#include "arg_number.h"

// LINK WITH AVP's files, for its acceptance (tests/scenarios/acceptance-module-linking-link-avp.txt): module.bbb planted as the game writes it, module.aaa read as the GAME'S reader reads
// it (avpx projload.cpp:3204-3353 LoadModuleData) and compared with the level copy's own links, read with the library and not through the editor's code.
// Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE:
//   verify_module_link plant <...\module.bbb> <x> <y> <z> <m11> <m12> <m13> <m21> <m22> <m23> <m31> <m32> <m33>   writes the 48 bytes the game writes (VDB_World, VDB_Mat)
//   verify_module_link plant-size <...\module.bbb> <bytes, 0 to 4096>                                         writes that many zero bytes (a file of the wrong size)
//   verify_module_link present <file> | absent <file>                                                        0 when the file is there / is not
//   verify_module_link remove <...\module.aaa or module.bbb>                                                 the file deleted when it is there
//   verify_module_link aaa <...\module.aaa> <the level opened .rif> <the level copy .rif> <the name's last part>
//       module.aaa's name, after its last '/' or ':', is that last part (the game compares no more, ignoring case); its records are exactly the copy's: every object whose MODULEDT
//       has a VMDARRAY, in the file's order, its index, its item count, then each item's object_index and branch_no; and they differ from the level as opened in exactly one module.
// Only a file named module.aaa or module.bbb is written or deleted. Gives back 0 when every check passed, 1 when one failed, 2 for a command line it does not take.

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls: files are read by Windows' own

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

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

// The file's own name (after its last '\' or '/') is one of the two, and the path holds no ".."
static bool LinkFileNamed(char const *path, bool aaaToo)
{
	if (!path || strstr(path, "..") != nullptr)
	{
		return false;
	}

	char const *name = path;

	for (char const *at = path; *at; at++)
	{
		if (*at == '\\' || *at == '/')
		{
			name = at + 1;
		}
	}

	return _stricmp(name, "module.bbb") == 0 || (aaaToo && _stricmp(name, "module.aaa") == 0);
}

// (Named for this file alone: Snyk Code took a call of the old name, WriteBytes, for level_file_test.cpp's function of that name.)
static bool PlantLinkFileBytes(char const *path, std::vector<uint8_t> const &bytes)
{
	HANDLE const file = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD wrote = 0;
	bool const good = bytes.empty() || (WriteFile(file, bytes.data(), (DWORD)bytes.size(), &wrote, 0) && wrote == bytes.size());

	CloseHandle(file);
	return good;
}

static void PutInt(std::vector<uint8_t> *out, int32_t value)
{
	for (int i = 0; i < 4; i++)
	{
		out->push_back((uint8_t)(((uint32_t)value >> (8 * i)) & 0xff));
	}
}

static int32_t IntAt(std::vector<uint8_t> const &bytes, size_t at)
{
	return (int32_t)((uint32_t)bytes[at] | ((uint32_t)bytes[at + 1] << 8) | ((uint32_t)bytes[at + 2] << 16) | ((uint32_t)bytes[at + 3] << 24));
}

static bool FileThere(char const *path)
{
	DWORD const kind = GetFileAttributesA(path);

	return kind != INVALID_FILE_ATTRIBUTES && !(kind & FILE_ATTRIBUTE_DIRECTORY);
}

struct Record
{
	int32_t index;
	std::vector<int32_t> pairs;   // object_index, branch_no, ...
};

// module.aaa as the game's reader takes it, checked: the name field with its zero inside, then whole records to the file's end (a count that runs past the end is refused)
static bool ReadAaa(char const *path, std::string *name, std::vector<Record> *records)
{
	HANDLE const file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		Check(false, std::string("module.aaa is there: ") + path);
		return false;
	}

	LARGE_INTEGER size = {};
	bool good = GetFileSizeEx(file, &size) && size.QuadPart >= 60 && size.QuadPart <= (16 << 20);
	std::vector<uint8_t> bytes;

	if (good)
	{
		bytes.assign((size_t)size.QuadPart, 0);

		DWORD got = 0;

		good = ReadFile(file, bytes.data(), (DWORD)bytes.size(), &got, 0) && got == bytes.size();
	}

	CloseHandle(file);
	Check(good, "module.aaa is read whole, 60 bytes to 16 MiB: " + std::to_string((long long)size.QuadPart) + " bytes");

	if (!good)
	{
		return false;
	}

	uint8_t const *const zero = (uint8_t const *)memchr(bytes.data(), 0, 60);

	Check(zero != nullptr, "the name field holds its end (a zero) within its 60 bytes");

	if (!zero)
	{
		return false;
	}

	name->assign((char const *)bytes.data(), (size_t)(zero - bytes.data()));

	size_t at = 60;

	while (at < bytes.size())
	{
		if (bytes.size() - at < 8)
		{
			Check(false, "a record's index and count are whole at byte " + std::to_string((unsigned long long)at));
			return false;
		}

		Record one;

		one.index = IntAt(bytes, at);

		int32_t const count = IntAt(bytes, at + 4);

		at += 8;

		if (count < 0 || (size_t)count > (bytes.size() - at) / 8)
		{
			Check(false, "record of module " + std::to_string(one.index) + ": its count " + std::to_string(count) + " fits the file");
			return false;
		}

		for (int32_t k = 0; k < count; k++)
		{
			one.pairs.push_back(IntAt(bytes, at));
			one.pairs.push_back(IntAt(bytes, at + 4));
			at += 8;
		}

		records->push_back(one);
	}

	return true;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	std::string const mode = argc >= 2 ? argv[1] : "";

	if (mode == "plant" && argc == 15 && LinkFileNamed(argv[2], false))
	{
		std::vector<uint8_t> bytes;

		for (int i = 3; i < 15; i++)
		{
			long number = 0;

			if (!ArgNumber("a number of module.bbb", argv[i], -2147483647L - 1, 2147483647L, &number))
			{
				return 2;
			}

			PutInt(&bytes, (int32_t)number);
		}

		Check(PlantLinkFileBytes(argv[2], bytes), std::string("module.bbb planted, 48 bytes: ") + argv[2]);
		return checksFailed ? 1 : 0;
	}

	if (mode == "plant-size" && argc == 4 && LinkFileNamed(argv[2], false))
	{
		long count = 0;

		if (!ArgNumber("how many bytes", argv[3], 0, 4096, &count))
		{
			return 2;
		}

		Check(PlantLinkFileBytes(argv[2], std::vector<uint8_t>((size_t)count, 0)), "module.bbb planted, " + std::to_string(count) + " zero bytes");
		return checksFailed ? 1 : 0;
	}

	if ((mode == "present" || mode == "absent") && argc == 3)
	{
		bool const there = FileThere(argv[2]);

		Check(mode == "present" ? there : !there, std::string(argv[2]) + (there ? " is there" : " is not there"));
		return checksFailed ? 1 : 0;
	}

	if (mode == "remove" && argc == 3 && LinkFileNamed(argv[2], true))
	{
		Check(!FileThere(argv[2]) || DeleteFileA(argv[2]), std::string(argv[2]) + " is gone");
		return checksFailed ? 1 : 0;
	}

	if (mode == "aaa" && argc == 6 && LinkFileNamed(argv[2], true))
	{
		std::string name;
		std::vector<Record> records;

		if (!ReadAaa(argv[2], &name, &records))
		{
			return 1;
		}

		size_t const cut = name.find_last_of("/:");
		std::string const last = cut == std::string::npos ? name : name.substr(cut + 1);

		Check(_stricmp(last.c_str(), argv[5]) == 0, "the name \"" + name + "\" ends in the level's file name, as the game compares it: \"" + argv[5] + "\"");

		OpenLevelFile before, copy;

		Check(before.Open(argv[3]) == LevelOpenResult::Opened && before.Level(), "the game's loader takes the level the editor opened");
		Check(copy.Open(argv[4]) == LevelOpenResult::Opened && copy.Level(), "the game's loader takes the level copy");

		if (checksFailed)
		{
			return 1;
		}

		std::vector<host::ModuleSnapshot> was, snapshots;

		Check(host::ReadModuleSnapshots(&before, &was) == host::ObjectEditResult::Done && host::ReadModuleSnapshots(&copy, &snapshots) == host::ObjectEditResult::Done, "both levels' objects are read");

		// the records the game's reader would take from a level: every object whose MODULEDT has a VMDARRAY, in the file's order
		auto const recordsOf = [](std::vector<host::ModuleSnapshot> const &all)
		{
			std::vector<Record> out;

			for (host::ModuleSnapshot const &snapshot : all)
			{
				if (snapshot.module && snapshot.hasLinks)
				{
					Record one;

					one.index = snapshot.index;

					for (host::ModuleLinkItem const &item : snapshot.links)
					{
						one.pairs.push_back(item.objectIndex);
						one.pairs.push_back(item.branchNo);
					}

					out.push_back(one);
				}
			}

			return out;
		};

		std::vector<Record> const wanted = recordsOf(snapshots), old = recordsOf(was);

		Check(records.size() == wanted.size() && !wanted.empty(), "module.aaa has a record for each of the copy's " + std::to_string((unsigned long long)wanted.size()) + " modules with links (it has "
			+ std::to_string((unsigned long long)records.size()) + ")");

		size_t same = 0;

		for (size_t i = 0; i < records.size() && i < wanted.size(); i++)
		{
			same += records[i].index == wanted[i].index && records[i].pairs == wanted[i].pairs ? 1 : 0;
		}

		Check(same == wanted.size(), "every record is the copy's, in its order: index, count, then object_index and branch_no per item (" + std::to_string((unsigned long long)same) + " of "
			+ std::to_string((unsigned long long)wanted.size()) + " the same)");

		// the scenario's link: module.aaa's records differ from the level as it was opened in exactly one module, which gained links (the tree was written into the level before
		// module.aaa was made: a module.aaa from the level as opened would be the same as old)
		std::vector<std::string> changed;

		for (Record const &one : records)
		{
			Record const *was = nullptr;

			for (Record const &other : old)
			{
				was = other.index == one.index ? &other : was;
			}

			if (!was || was->pairs != one.pairs)
			{
				changed.push_back("module " + std::to_string(one.index) + ": " + std::to_string((unsigned long long)(was ? was->pairs.size() / 2 : 0)) + " links before, "
					+ std::to_string((unsigned long long)(one.pairs.size() / 2)) + " in module.aaa");
			}
		}

		for (std::string const &one : changed)
		{
			printf("  (%s)\n", one.c_str());
		}

		Check(changed.size() == 1 && records.size() >= old.size(), "exactly one module's record differs from the level as it was opened: the module the scenario linked (the tree went into the level before module.aaa)");
		printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
		return checksFailed ? 1 : 0;
	}

	printf("  verify_module_link: a command line it does not take (see the file's USE; only a file named module.aaa or module.bbb is written or deleted)\n");
	return 2;
}
