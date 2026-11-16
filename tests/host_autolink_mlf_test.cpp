// The test for AutoLink's "Load from file" (host::MlfParse / MlfReadFile / AutoLink_FromMlf, src/host/autolink_mlf.cpp, autolink_edit.cpp): a HOSTILE-INPUT test. Written 2026-11-15 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_autolink_mlf_test.exe [<a level .rif> <a folder it may write its files into>]. With no arguments only the parser is checked. The level is only read.
// THE PARSER (no level needed): a good file of two records parses to them; every way a file can be bad is REFUSED, with the record's number, and no record is given back: empty, a name with no
// end, a name over 63 bytes, an empty name, a name with a control letter or a letter above 0x7e, a record cut short in its name, in its numbers, a matrix number beyond any turn, more than
// kMlfMostRecords records, a file over kMlfMostBytes; random bytes never crash it (a fixed seed). THE FILE: a missing file, an empty file and a too big file are refused by MlfReadFile.
// THE LEVEL: records made from real modules (name, place, the view at the module's place, an identity turn) are run: the views are drawn and the links written, the level is saved as a copy
// the game's loader takes; a record naming no module is skipped and counted; a refused file changes nothing (the level's image is the same before and after).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "module_links.h"
#include "autolink_edit.h"
#include "autolink_mlf.h"

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

static std::string CopyPathIn(std::string const &folder, char const *name)
{
	return folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + name;
}

static bool WriteWhole(std::string const &path, std::vector<uint8_t> const &bytes)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD put = 0;
	bool const good = bytes.empty() || (WriteFile(file, bytes.data(), (DWORD)bytes.size(), &put, 0) && put == bytes.size());

	CloseHandle(file);
	return good;
}

// One record's bytes: the name (a NUL and the padding to 4), the place, the matrix, the view
static void Put(std::vector<uint8_t> *out, std::string const &name, int32_t const place[3], int32_t const turn[9], int32_t const view[3])
{
	size_t const padded = (name.size() + 1 + 3) & ~(size_t)3;
	size_t const at = out->size();

	out->resize(at + padded, 0);
	memcpy(out->data() + at, name.data(), name.size());

	for (int i = 0; i < 3; i++)
	{
		out->insert(out->end(), (uint8_t const *)&place[i], (uint8_t const *)&place[i] + 4);
	}

	for (int i = 0; i < 9; i++)
	{
		out->insert(out->end(), (uint8_t const *)&turn[i], (uint8_t const *)&turn[i] + 4);
	}

	for (int i = 0; i < 3; i++)
	{
		out->insert(out->end(), (uint8_t const *)&view[i], (uint8_t const *)&view[i] + 4);
	}
}

static int32_t const kIdentity[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };

static bool Refused(std::vector<uint8_t> const &bytes, std::string *why = nullptr)
{
	std::vector<host::MlfRecord> records;
	std::string reason;
	bool const ok = host::MlfParse(bytes.data(), bytes.size(), &records, &reason);

	if (why)
	{
		*why = reason;
	}

	return !ok && records.empty() && !reason.empty();
}

static void ParserChecks()
{
	printf("THE PARSER\n");

	int32_t const place[3] = { 10, -20, 30 };
	int32_t const view[3] = { 11, 22, -33 };
	std::vector<uint8_t> good;

	Put(&good, "Room1", place, kIdentity, view);
	Put(&good, "A", place, kIdentity, view);

	std::vector<host::MlfRecord> records;
	std::string why;

	Check(host::MlfParse(good.data(), good.size(), &records, &why) && records.size() == 2 && records[0].name == "Room1" && records[1].name == "A" && records[0].place[1] == -20 && records[0].view[2] == -33 &&
		records[0].turn[4] == 0x10000, "a good file of two records parses to them (names, place, matrix, view)");

	// every bad file is refused whole, with a reason, giving no record
	std::vector<uint8_t> bytes;
	std::string reason;

	Check(Refused(std::vector<uint8_t>()), "an empty file is refused");

	bytes = std::vector<uint8_t>(good.begin(), good.begin() + 3);
	Check(Refused(bytes), "a file of 3 bytes (a name with no end) is refused");

	bytes.assign(70, 'x');
	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "a name of 70 letters with no end is refused: " + reason); }

	std::string const sixtyFour(64, 'n');
	bytes.clear();
	Put(&bytes, sixtyFour, place, kIdentity, view);
	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "a name of 64 bytes is refused: " + reason); }

	std::string const sixtyThree(63, 'n');
	bytes.clear();
	Put(&bytes, sixtyThree, place, kIdentity, view);
	Check(host::MlfParse(bytes.data(), bytes.size(), &records, &why) && records.size() == 1, "a name of 63 bytes is taken");

	bytes.clear();
	Put(&bytes, "", place, kIdentity, view);
	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "an empty name is refused: " + reason); }

	bytes.clear();
	Put(&bytes, std::string("Ro\x01m"), place, kIdentity, view);
	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "a name with a control letter is refused: " + reason); }

	bytes.clear();
	Put(&bytes, std::string("Ro\xe9m"), place, kIdentity, view);
	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "a name with a letter above 0x7e is refused: " + reason); }

	bytes = good;
	bytes.resize(good.size() - 1);
	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "a file cut one byte short is refused: " + reason); }

	bytes = good;
	bytes.resize(8 + 10);
	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "a record cut short in its numbers is refused: " + reason); }

	bytes = good;
	bytes.push_back('Z');
	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "a byte left over after the last record is refused: " + reason); }

	int32_t bigTurn[9];

	memcpy(bigTurn, kIdentity, sizeof(bigTurn));
	bigTurn[3] = 0x7fffffff;
	bytes.clear();
	Put(&bytes, "Room1", place, bigTurn, view);
	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "a matrix number of 0x7fffffff is refused: " + reason); }

	bigTurn[3] = (int32_t)0x80000000;
	bytes.clear();
	Put(&bytes, "Room1", place, bigTurn, view);
	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "a matrix number of 0x80000000 is refused: " + reason); }

	bytes.clear();

	for (size_t at = 0; at < host::kMlfMostRecords + 1; at++)
	{
		Put(&bytes, "M", place, kIdentity, view);
	}

	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "more than " + std::to_string(host::kMlfMostRecords) + " records are refused: " + reason); }

	bytes.clear();

	for (size_t at = 0; at < host::kMlfMostRecords; at++)
	{
		Put(&bytes, "M", place, kIdentity, view);
	}

	Check(host::MlfParse(bytes.data(), bytes.size(), &records, &why) && records.size() == host::kMlfMostRecords, "exactly " + std::to_string(host::kMlfMostRecords) + " records are taken");

	bytes.assign(host::kMlfMostBytes + 1, 'a');
	{ bool const refusedNow = Refused(bytes, &reason); Check(refusedNow, "a file over " + std::to_string(host::kMlfMostBytes) + " bytes is refused: " + reason); }

	// random bytes: never a crash, and whatever comes back is whole or nothing (a fixed seed, 3000 files)
	unsigned int seed = 12345u;
	size_t taken = 0, refused = 0;

	for (int round = 0; round < 3000; round++)
	{
		bytes.assign(1 + (round * 7) % 400, 0);

		for (uint8_t &b : bytes)
		{
			seed = seed * 1664525u + 1013904223u;
			b = (uint8_t)(seed >> 24);
		}

		// a third of them start with a plain name, so the parser gets past the first test
		if (round % 3 == 0 && bytes.size() > 8)
		{
			memcpy(bytes.data(), "Room", 4);
			bytes[4] = 0;
		}

		std::vector<host::MlfRecord> got;
		std::string reasonRandom;

		if (host::MlfParse(bytes.data(), bytes.size(), &got, &reasonRandom))
		{
			taken++;
		}
		else
		{
			refused++;

			if (!got.empty())
			{
				Check(false, "a refused file gave records back (round " + std::to_string(round) + ")");
				return;
			}
		}
	}

	// a good file with a few bytes changed or cut: 3000 times, each is taken whole or refused with nothing (this reaches the cases the pure noise does not)
	size_t mutatedTaken = 0, mutatedRefused = 0;

	for (int round = 0; round < 3000; round++)
	{
		bytes = good;

		for (int change = 0; change < 1 + round % 4; change++)
		{
			seed = seed * 1664525u + 1013904223u;
			bytes[(seed >> 8) % bytes.size()] = (uint8_t)(seed >> 24);
		}

		if (round % 5 == 0)
		{
			bytes.resize(1 + (seed >> 8) % bytes.size());
		}

		std::vector<host::MlfRecord> got;
		std::string reasonMutated;

		if (host::MlfParse(bytes.data(), bytes.size(), &got, &reasonMutated))
		{
			mutatedTaken++;

			if (got.empty() || got.size() > 2)
			{
				Check(false, "a mutated file gave a wrong number of records (round " + std::to_string(round) + ")");
				return;
			}
		}
		else
		{
			mutatedRefused++;

			if (!got.empty() || reasonMutated.empty())
			{
				Check(false, "a refused mutated file gave records or no reason (round " + std::to_string(round) + ")");
				return;
			}
		}
	}

	Check(mutatedTaken > 0 && mutatedRefused > 0, "3000 mutated files: " + std::to_string(mutatedTaken) + " taken whole, " + std::to_string(mutatedRefused) + " refused with a reason and no record");
	Check(true, "3000 random files: " + std::to_string(taken) + " taken, " + std::to_string(refused) + " refused, none crashed, none refused with records");

	// MlfPathFor
	Check(host::MlfPathFor("C:\\a b\\Level.RIF") == "C:\\a b\\Level.mlf" && host::MlfPathFor("C:\\a.b\\Level") == "C:\\a.b\\Level.mlf" && host::MlfPathFor("").empty(), "the .mlf's path: the extension replaced, or added when the file has none");
}

static void FileChecks(std::string const &folder)
{
	printf("THE FILE\n");

	std::vector<uint8_t> bytes;
	std::string why;

	{ bool const refusedNow = !host::MlfReadFile(CopyPathIn(folder, "no-such-file.mlf").c_str(), &bytes, &why) && !why.empty(); Check(refusedNow, "a missing file is refused: " + why); }

	std::string const emptyPath = CopyPathIn(folder, "mlf-test-empty.mlf");

	Check(WriteWhole(emptyPath, std::vector<uint8_t>()), "an empty file is written");
	{ bool const refusedNow = !host::MlfReadFile(emptyPath.c_str(), &bytes, &why); Check(refusedNow, "an empty file is refused: " + why); }
	DeleteFileA(emptyPath.c_str());

	std::string const bigPath = CopyPathIn(folder, "mlf-test-big.mlf");

	Check(WriteWhole(bigPath, std::vector<uint8_t>(host::kMlfMostBytes + 1, 'a')), "a file one byte over the cap is written");
	{ bool const refusedNow = !host::MlfReadFile(bigPath.c_str(), &bytes, &why); Check(refusedNow, "a file one byte over the cap is refused: " + why); }
	DeleteFileA(bigPath.c_str());

	int32_t const place[3] = { 1, 2, 3 };
	std::vector<uint8_t> good;

	Put(&good, "Room1", place, kIdentity, place);

	std::string const goodPath = CopyPathIn(folder, "mlf-test-good.mlf");

	Check(WriteWhole(goodPath, good), "a good file is written");

	std::vector<host::MlfRecord> records;

	Check(host::MlfReadFile(goodPath.c_str(), &bytes, &why) && bytes == good && host::MlfParse(bytes.data(), bytes.size(), &records, &why) && records.size() == 1, "a good file is read whole and parsed");
	DeleteFileA(goodPath.c_str());
}

static void LevelChecks(char const *levelPath, std::string const &folder)
{
	printf("THE LEVEL\n");

	OpenLevelFile level;

	if (level.Open(levelPath) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	// the level's scale: the ENVSDSCL double inside REBENVDT, 1.0 when there is none
	double scale = 1.0;
	{
		Chunk *const data = level.Level()->lookup_single_child("REBENVDT");
		Chunk_With_Children *const holder = data && !data->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(data) : nullptr;
		Chunk *const scaleChunk = holder ? holder->lookup_single_child("ENVSDSCL") : nullptr;
		std::vector<unsigned char> raw;

		if (scaleChunk && level.PayloadOf(scaleChunk, &raw) && raw.size() >= 8)
		{
			memcpy(&scale, raw.data(), 8);
		}

		if (!(scale > 0.0))
		{
			scale = 1.0;
		}
	}

	std::set<int32_t> modules;

	host::ModuleIndexes(level.Level(), &modules);

	// records from the first modules that have names: the view at the module's own place (in file numbers: the view's place is times 100 in memory), an identity turn
	std::vector<uint8_t> file;
	std::vector<host::MlfRecord> records;
	List<Object_Chunk *> all;
	size_t made = 0;

	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done() && made < 6; each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !modules.count((int32_t)object->object_data.index_num))
		{
			continue;
		}

		std::string name(object->object_data.o_name, strnlen(object->object_data.o_name, sizeof(object->object_data.o_name)));
		int32_t const place[3] = { (int32_t)object->object_data.location.x, (int32_t)object->object_data.location.y, (int32_t)object->object_data.location.z };
		// the view in file numbers: the memory place is the file number x 100 x the level's scale, the .mlf's is "times 100" only, so the scale is divided out here
		int32_t const view[3] = { (int32_t)(place[0] * scale), (int32_t)(place[1] * scale), (int32_t)(place[2] * scale) };

		Put(&file, name, place, kIdentity, view);
		made++;
	}

	int32_t const nowhere[3] = { 0, 0, 0 };

	Put(&file, "NoSuchModuleAnywhere", nowhere, kIdentity, nowhere);

	std::string why;

	Check(host::MlfParse(file.data(), file.size(), &records, &why) && records.size() == made + 1, "the records made from " + std::to_string(made) + " real modules and one unknown name parse");

	std::vector<char> before, after;

	Check(level.BuildLevel(before), "the level as the next save would write it is made (before)");

	// a refused file changes nothing: the parse stops before the level is looked at
	std::vector<uint8_t> broken = file;

	broken.pop_back();

	std::vector<host::MlfRecord> none;

	{ bool const refusedNow = !host::MlfParse(broken.data(), broken.size(), &none, &why) && none.empty(); Check(refusedNow, "a cut file is refused: " + why); }
	Check(level.BuildLevel(after) && before == after, "a refused file left the level's image as it was");

	host::MlfCounts counts;
	host::AutoLinkResult const result = host::AutoLink_FromMlf(&level, records, scale, 0, &counts, &why);

	Check(result == host::AutoLinkResult::Done, std::string("the records are run") + (result == host::AutoLinkResult::Done ? "" : ": " + why));
	printf("  --  %zu records: %zu used, %zu unknown, %zu links added, %zu cut, %zu modules written, %zu skipped\n", counts.records, counts.used, counts.unknown, counts.linksAdded, counts.cut, counts.written,
		counts.skipped);
	Check(counts.records == made + 1 && counts.unknown >= 1 && counts.used + counts.unknown == counts.records, "the record naming no module is skipped and counted");
	Check(counts.used > 0, "the views of the real modules are drawn: " + std::to_string(counts.used));

	// the copy loads
	std::string const copyPath = CopyPathIn(folder, "mlf-test-copy.rif");

	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile reopened;

	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");
	DeleteFileA(copyPath.c_str());
}

int main(int argc, char **argv)
{
	ParserChecks();

	if (argc >= 3)
	{
		CreateDirectoryA(argv[2], NULL);
		FileChecks(argv[2]);
		LevelChecks(argv[1], argv[2]);
	}

	if (checksFailed)
	{
		printf("FAILED host_autolink_mlf_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_autolink_mlf_test\n");
	return 0;
}
