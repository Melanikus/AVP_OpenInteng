// The test of Acoustic Mode's data side (src/host/acoustic_mode.cpp; reassess/object-placement.md section 2, AC-3 .. AC-7 and AC-9). Written 2026-11-17
// by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_acoustic_mode_test.exe <Invasion .rif> <a level with no ENVACOUS: Battle .rif>. The levels are only read. Their edited copies are written into a folder of the test's own,
// acoustic_mode_test_<process id>, made in the folder it runs in (it stops if that folder is already there) and deleted at the end.
// THE CASES:
//  1. the 26 names (FUN_004f69e0) and "Invalid type" around them; a typed reverb kept as inteng keeps it (above 1.0: 2.0, at or below 0.0: 0.0);
//  2. on Invasion: a module A that has a MODACOUS, a module B with a MODULEDT and none, a module C with neither (each found in the level, or
//     the test says it couldn't be), and the frame's lines for B before anything changes;
//  3. Module > Acoustic type (Concert Hall, 7) on A, B and B again: 2 records written; A keeps its reverb, B's record is made: type 7, reverb -1.0,
//     spare 0; B's lines "Type : Concert Hall", "Reverb : Default";
//  4. Module > Reverb 0.5 on B: reverb 0.5, the type kept; the window's start for [C, B] is 0.5 (C has no record), for [C] 2.0;
//  5. Set To Default Acoustic Type on B and C: 1 record written (B: -1), C still has none;
//  5b. Module > Reverb 1.5 on C: its MODULEDT and record made, reverb 2.0;
//  6. Default > Acoustic type (Hangar, 10): the level's record, made when missing (reverb 2.0); "Default Type : Hangar"; Default > Reverb's making
//     changes nothing more;
//  7. the copy saved and read again by the game's loader holds all of it, the object count is the same, and the opened file is byte for byte
//     what it was;
//  8. on the second level (no ENVACOUS): the Default lines "Generic" and 0.00, Default > Reverb's making (0, 2.0), its Ok (0.25), the copy read back.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <array>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "acoustic_mode.h"

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

static bool ReadWholeFile(std::string const &path, std::vector<char> *out)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER size;
	bool good = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 31);

	if (good)
	{
		out->assign((size_t)size.QuadPart, 0);
		DWORD got = 0;
		good = ReadFile(file, out->data(), (DWORD)out->size(), &got, 0) && got == out->size();
	}

	CloseHandle(file);
	return good;
}

typedef std::array<uint8_t, 8> Id;

// The level's modules sorted by what their MODULEDT holds: the first with a MODACOUS, the first with a MODULEDT and none, the first object with
// no MODULEDT at all (a placed object: the functions take any object by its ID, as FUN_004f6670 takes any block with an object)
static void FindThree(File_Chunk *level, Id *a, Id *b, Id *c, bool found[3])
{
	List<Object_Chunk *> objects;

	level->list_objects(&objects);
	found[0] = found[1] = found[2] = false;

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		Id id;

		memcpy(id.data(), &object->object_data.ID, 8);

		Chunk *const data = object->lookup_single_child("MODULEDT");
		Chunk_With_Children *const holder = data ? dynamic_cast<Chunk_With_Children *>(data) : nullptr;
		bool const record = holder && holder->lookup_single_child("MODACOUS");

		// the first object of each kind whose ID is not the ID of an earlier object (two objects of one name share one)
		if (record && !found[0])
		{
			*a = id;
			found[0] = true;
		}
		else if (holder && !record && !found[1])
		{
			*b = id;
			found[1] = true;
		}
		else if (!data && !found[2])
		{
			*c = id;
			found[2] = true;
		}
	}
}

static std::string Lines(std::vector<host::AcousticLine> const &lines)
{
	std::string out;

	for (host::AcousticLine const &line : lines)
	{
		char at[32];

		snprintf(at, sizeof(at), "%s%d@%d:", out.empty() ? "" : "|", line.x, line.y);
		out += at + line.words;
	}

	return out;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	// ---- 1. the names and the reverb's keeping
	Check(strcmp(host::AcousticTypeName(0), "Generic") == 0 && strcmp(host::AcousticTypeName(7), "Concert Hall") == 0 && strcmp(host::AcousticTypeName(10), "Hangar") == 0 &&
		strcmp(host::AcousticTypeName(25), "Psychotic") == 0, "the names: 0 Generic, 7 Concert Hall, 10 Hangar, 25 Psychotic (0x5e4e90, 20 bytes each)");
	Check(strcmp(host::AcousticTypeName(-1), "Invalid type") == 0 && strcmp(host::AcousticTypeName(26), "Invalid type") == 0, "-1 and 26: \"Invalid type\"");
	Check(host::AcousticReverbKept(2.5f) == 2.0f && host::AcousticReverbKept(1.0f) == 1.0f && host::AcousticReverbKept(0.5f) == 0.5f && host::AcousticReverbKept(0.0f) == 0.0f &&
		host::AcousticReverbKept(-3.0f) == 0.0f, "a typed reverb: 2.5 -> 2.0, 1.0 and 0.5 kept, 0 and -3 -> 0.0");
	Check(host::AcousticReverbKept(nanf("")) == 2.0f, "a typed NaN -> 2.0 (it is not at or below 1.0, as the code compares)");

	if (argc < 2)
	{
		printf("  FAILED  usage: host_acoustic_mode_test <Invasion .rif>: no level was named\n");
		return 1;
	}

	std::string const levelPath = argv[1];
	std::vector<char> original;

	Check(ReadWholeFile(levelPath, &original), "the level is read (it is only read, never written)");

	OpenLevelFile level;

	if (original.empty() || level.Open(levelPath.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "the game's loader takes the level");
		return 1;
	}

	// ---- 2. three modules, and B's lines before anything changes
	Id a, b, c;
	bool found[3];

	FindThree(level.Level(), &a, &b, &c, found);
	Check(found[0] && found[1] && found[2], "the level has a module with a MODACOUS, one with a MODULEDT and none, an object with no MODULEDT");

	if (!found[0] || !found[1] || !found[2])
	{
		printf("%d checks failed\n", checksFailed);
		return 1;
	}

	host::AcousticRecord aBefore, levelBefore;
	bool const levelHeld = host::Acoustic_LevelRecord(&level, &levelBefore);

	Check(host::Acoustic_ModuleRecord(&level, a.data(), &aBefore), "A's record is read");
	printf("  (A's record: type %d, reverb %.2f; the level's: %s %d %.2f)\n", aBefore.type, (double)aBefore.reverb, levelHeld ? "held" : "none", levelBefore.type, (double)levelBefore.reverb);

	host::AcousticRecord none;
	Check(!host::Acoustic_ModuleRecord(&level, b.data(), &none) && !host::Acoustic_ModuleRecord(&level, c.data(), &none), "B and C have no record");

	std::vector<host::AcousticLine> lines;
	host::Acoustic_FrameLines(&level, b.data(), &lines);

	char expected[256];
	snprintf(expected, sizeof(expected), "16@32:Default Type : %s|16@48:Default Reverb : %.2f", host::AcousticTypeName(levelHeld ? levelBefore.type : 0), levelHeld ? (double)levelBefore.reverb : 0.0);
	std::string const said = Lines(lines);
	Check(said.find(expected) == 0 && said.find("|16@80:Type : Default|16@96:Reverb : Default") != std::string::npos && lines.size() == 5,
		"B's lines before: the level's type and reverb, B's name, \"Type : Default\", \"Reverb : Default\" (" + said + ")");

	host::Acoustic_FrameLines(&level, nullptr, &lines);
	Check(lines.size() == 2, "with no module under the pointer: the two Default lines only");

	// ---- 3. Module > Acoustic type: Concert Hall on A, B and B again
	size_t changed = 0;
	host::AcousticResult result = host::Acoustic_SetModules(&level, { a, b, b }, host::AcousticField::Type, 7, 0.0f, true, &changed);

	Check(result == host::AcousticResult::Done && changed == 2, std::string("Acoustic type on A, B, B: done, 2 records written (") + host::AcousticWords(result) + ", " + std::to_string(changed) + ")");

	host::AcousticRecord aNow, bNow;
	Check(host::Acoustic_ModuleRecord(&level, a.data(), &aNow) && aNow.type == 7 && memcmp(&aNow.reverb, &aBefore.reverb, 4) == 0 && aNow.spare == aBefore.spare,
		"A: type 7, its reverb and spare word kept");
	Check(host::Acoustic_ModuleRecord(&level, b.data(), &bNow) && bNow.type == 7 && bNow.reverb == -1.0f && bNow.spare == 0, "B's record made: type 7, reverb -1.0, spare 0");

	host::Acoustic_FrameLines(&level, b.data(), &lines);
	Check(Lines(lines).find("|16@80:Type : Concert Hall|16@96:Reverb : Default") != std::string::npos, "B's lines: \"Type : Concert Hall\", \"Reverb : Default\"");

	// ---- 4. Module > Reverb 0.5 on B; the window's start
	result = host::Acoustic_SetModules(&level, { b }, host::AcousticField::Reverb, 0, host::AcousticReverbKept(0.5f), true, &changed);
	Check(result == host::AcousticResult::Done && changed == 1 && host::Acoustic_ModuleRecord(&level, b.data(), &bNow) && bNow.type == 7 && bNow.reverb == 0.5f, "Reverb 0.5 on B: 0.5, its type kept");
	Check(host::Acoustic_ModuleReverbStart(&level, { c, b }) == 0.5f && host::Acoustic_ModuleReverbStart(&level, { c }) == 2.0f,
		"Module > Reverb's window starts at the first record's reverb that isn't -1.0 (0.5 for C, B), else 2.0 (C alone)");

	host::Acoustic_FrameLines(&level, b.data(), &lines);
	Check(Lines(lines).find("|16@96:Reverb : 0.50") != std::string::npos, "B's line \"Reverb : 0.50\"");

	// ---- 5. Set To Default Acoustic Type on B and C: only B has a record
	result = host::Acoustic_SetModules(&level, { b, c }, host::AcousticField::Type, -1, 0.0f, false, &changed);
	Check(result == host::AcousticResult::Done && changed == 1 && host::Acoustic_ModuleRecord(&level, b.data(), &bNow) && bNow.type == -1 && bNow.reverb == 0.5f &&
		!host::Acoustic_ModuleRecord(&level, c.data(), &none), "Set To Default Acoustic Type on B, C: 1 written (B -1, its reverb kept), C still has none");

	// ---- 5b. Module > Reverb 1.5 on C, which has no MODULEDT: its MODULEDT and record made (FUN_004f6670 0x4f66c2), 1.5 kept as 2.0
	result = host::Acoustic_SetModules(&level, { c }, host::AcousticField::Reverb, 0, host::AcousticReverbKept(1.5f), true, &changed);

	host::AcousticRecord cNow;
	Check(result == host::AcousticResult::Done && changed == 1 && host::Acoustic_ModuleRecord(&level, c.data(), &cNow) && cNow.type == -1 && cNow.reverb == 2.0f && cNow.spare == 0,
		"Reverb 1.5 on C (no MODULEDT): its MODULEDT and record made, type -1, reverb 2.0");

	// ---- 6. Default > Acoustic type: Hangar; Default > Reverb's making
	result = host::Acoustic_SetLevel(&level, host::AcousticField::Type, 10, 0.0f);

	host::AcousticRecord levelNow;
	Check(result == host::AcousticResult::Done && host::Acoustic_LevelRecord(&level, &levelNow) && levelNow.type == 10 &&
		(levelHeld ? memcmp(&levelNow.reverb, &levelBefore.reverb, 4) == 0 : levelNow.reverb == 2.0f), "Default > Acoustic type Hangar: the level's type 10 (its reverb kept, or 2.0 when it was made)");
	Check(host::Acoustic_MakeLevelRecord(&level) == host::AcousticResult::Done && host::Acoustic_LevelRecord(&level, &levelNow) && levelNow.type == 10,
		"Default > Reverb's making, the record there: nothing changes");

	host::Acoustic_FrameLines(&level, nullptr, &lines);
	Check(Lines(lines).find("16@32:Default Type : Hangar") == 0, "\"Default Type : Hangar\"");

	// ---- 7. the copy, read again by the game's loader
	std::string const folder = "acoustic_mode_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), nullptr))
	{
		printf("  FAILED  the test couldn't make its folder %s (is it there already?)\n", folder.c_str());
		return 1;
	}

	std::string const copy = folder + "\\acoustic.rif";
	List<Object_Chunk *> objectsBefore;

	level.Level()->list_objects(&objectsBefore);
	Check(level.SaveAs(copy.c_str()) == LevelSaveResult::Saved, "the copy is saved");

	{
		OpenLevelFile again;

		Check(again.Open(copy.c_str()) == LevelOpenResult::Opened && again.Level(), "the game's loader takes the copy");

		if (again.Level())
		{
			host::AcousticRecord aAgain, bAgain, levelAgain;
			List<Object_Chunk *> objectsAfter;

			again.Level()->list_objects(&objectsAfter);
			Check(host::Acoustic_ModuleRecord(&again, a.data(), &aAgain) && aAgain.type == 7 && memcmp(&aAgain.reverb, &aBefore.reverb, 4) == 0, "in the copy A: type 7, its reverb");
			Check(host::Acoustic_ModuleRecord(&again, b.data(), &bAgain) && bAgain.type == -1 && bAgain.reverb == 0.5f && bAgain.spare == 0, "in the copy B: type -1, reverb 0.5");
			host::AcousticRecord cAgain;
			Check(host::Acoustic_ModuleRecord(&again, c.data(), &cAgain) && cAgain.type == -1 && cAgain.reverb == 2.0f, "in the copy C: its record made, type -1, reverb 2.0");
			Check(host::Acoustic_LevelRecord(&again, &levelAgain) && levelAgain.type == 10, "in the copy the level: type 10");
			Check(objectsAfter.size() == objectsBefore.size(), "as many objects as before");
		}
	}

	std::vector<char> after;
	Check(ReadWholeFile(levelPath, &after) && after == original, "the opened file is byte for byte what it was");

	// ---- 8. a level with no ENVACOUS (Battle; read 11-17 through the game's loader: 57 of the 63 stock levels have none, every one has a REBENVDT): the lines, then Default > Reverb
	// makes its record before the window (0, 2.0), and its Ok writes the reverb
	std::string const copy2 = folder + "\\noenv.rif";

	if (argc >= 3)
	{
		OpenLevelFile bare;

		Check(bare.Open(argv[2]) == LevelOpenResult::Opened && bare.Level(), "the second level opens");

		host::AcousticRecord bareRecord;

		if (bare.Level() && !host::Acoustic_LevelRecord(&bare, &bareRecord))
		{
			host::Acoustic_FrameLines(&bare, nullptr, &lines);
			Check(Lines(lines) == "16@32:Default Type : Generic|16@48:Default Reverb : 0.00", "no level record: \"Default Type : Generic\", \"Default Reverb : 0.00\" (0x4f5b7f: ESI = 0)");
			Check(host::Acoustic_MakeLevelRecord(&bare) == host::AcousticResult::Done && host::Acoustic_LevelRecord(&bare, &bareRecord) && bareRecord.type == 0 && bareRecord.reverb == 2.0f &&
				bareRecord.spare == 0, "Default > Reverb's making: the record made, type 0, reverb 2.0");
			Check(host::Acoustic_SetLevel(&bare, host::AcousticField::Reverb, 0, host::AcousticReverbKept(0.25f)) == host::AcousticResult::Done && host::Acoustic_LevelRecord(&bare, &bareRecord) &&
				bareRecord.reverb == 0.25f && bareRecord.type == 0, "its Ok, 0.25: the reverb 0.25, the type kept");
			Check(bare.SaveAs(copy2.c_str()) == LevelSaveResult::Saved, "the second copy is saved");

			OpenLevelFile again;
			host::AcousticRecord againRecord;

			Check(again.Open(copy2.c_str()) == LevelOpenResult::Opened && host::Acoustic_LevelRecord(&again, &againRecord) && againRecord.type == 0 && againRecord.reverb == 0.25f,
				"in the second copy, read by the game's loader: the level's record, type 0, reverb 0.25");
		}
		else
		{
			Check(false, "the second level has no level record (ENVACOUS), as the test needs");
		}
	}
	else
	{
		Check(false, "a second level, one with no ENVACOUS, is named (tests/host_acoustic_mode_test.runargs)");
	}

	DeleteFileA(copy2.c_str());
	DeleteFileA(copy.c_str());
	Check(RemoveDirectoryA(folder.c_str()) != 0, "the test's folder is gone at the end");

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
