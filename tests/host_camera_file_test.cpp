// The test for src/host/camera_file: inteng's camera file (<RIFFNAME>.cam, FUN_00470950 reads, FUN_00470610 writes) and the CAMORIGN offset. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: no stock level has a CAMORIGN chunk, so the offset path can't be seen in any real run. This builds small levels in memory the way the game reads them (REBINFF2 > REBENVDT > RIFFNAME, ENVSDSCL, SPECLOBJ > CAMORIGN
// with three known doubles) and checks, with the game's own chunk library: the file's name (the RIFFNAME's last part, cleaned), the offset (the first CAMORIGN of the first SPECLOBJ, times the level's scale times 100.0, cut
// toward zero; 0 with none), the offset ADDED to every place when a camera file is read, and TAKEN OFF again when one is written (FUN_004fd9d0), a file that isn't exactly 528 bytes refused, and a slot's place going
// out and coming back the same. The turn matrices are carried as they are (the editor turns them into its camera: editor_main.cpp).
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <initializer_list>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "environment_chunk.hpp"
#include "level_file.h"
#include "camera_file.h"

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

// ---- Making level files as the game reads them (see host_object_place_edit_test.cpp)

typedef std::vector<char> Bytes;

static void PutNumber(Bytes &out, int value)
{
	char four[4];
	memcpy(four, &value, sizeof(four));
	out.insert(out.end(), four, four + 4);
}

static Bytes MakeChunk(const char *name, Bytes const &inside)
{
	Bytes out(name, name + 8);
	PutNumber(out, (int)(12 + inside.size()));
	out.insert(out.end(), inside.begin(), inside.end());
	return out;
}

static Bytes Join(std::initializer_list<Bytes> parts)
{
	Bytes out;

	for (Bytes const &part : parts)
	{
		out.insert(out.end(), part.begin(), part.end());
	}

	return out;
}

static Bytes NameBytes(const char *name)
{
	Bytes out(name, name + strlen(name) + 1);

	while (out.size() % 4 != 0)
	{
		out.push_back(0);
	}

	return out;
}

static Bytes Doubles(std::initializer_list<double> numbers)
{
	Bytes out;

	for (double number : numbers)
	{
		char eight[8];

		memcpy(eight, &number, sizeof(eight));
		out.insert(out.end(), eight, eight + 8);
	}

	return out;
}

struct LevelSpec
{
	const char *name;
	bool hasScale;
	double scale;
	bool hasOrigin;
	double origin[3];
	bool secondOrigin;   // a second CAMORIGN after the first, which must not be the one used
};

static Bytes LevelBytes(LevelSpec const &spec)
{
	Bytes environment = MakeChunk("RIFFNAME", NameBytes(spec.name));

	if (spec.hasScale)
	{
		environment = Join({ environment, MakeChunk("ENVSDSCL", Doubles({ spec.scale })) });
	}

	if (spec.hasOrigin)
	{
		Bytes special = MakeChunk("CAMORIGN", Doubles({ spec.origin[0], spec.origin[1], spec.origin[2] }));

		if (spec.secondOrigin)
		{
			special = Join({ special, MakeChunk("CAMORIGN", Doubles({ 9999.0, 9999.0, 9999.0 })) });
		}

		environment = Join({ environment, MakeChunk("SPECLOBJ", special) });
	}

	Bytes const inside = Join({ MakeChunk("REBENVDT", environment), MakeChunk("REBSHAPE", MakeChunk("SHPHEAD1", Bytes(68, 0))) });

	return MakeChunk("REBINFF2", inside);
}

static bool WriteTheFile(std::string const &path, Bytes const &bytes)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD written = 0;
	bool const whole = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &written, 0) && written == bytes.size();

	CloseHandle(file);
	return whole;
}

static std::string folderOfTest;

// Opens a level made from spec and runs what: its File_Chunk is only good while the open level is held
template <typename Run>
static void WithLevel(LevelSpec const &spec, Run run)
{
	std::string const path = folderOfTest + "\\level.rif";

	if (!WriteTheFile(path, LevelBytes(spec)))
	{
		Check(false, "a test level can be written");
		return;
	}

	OpenLevelFile open;

	if (open.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, std::string("the test level (") + spec.name + ") opens in the editor's own open");
		DeleteFileA(path.c_str());
		return;
	}

	run(open.Level());
	DeleteFileA(path.c_str());
}

static bool Same3(int32_t const a[3], int32_t x, int32_t y, int32_t z)
{
	return a[0] == x && a[1] == y && a[2] == z;
}

static void TestNames()
{
	puts("1. The file's name");

	std::string name;

	LevelSpec plain = { "Invasion", false, 0, false, { 0, 0, 0 }, false };
	WithLevel(plain, [&](File_Chunk *level) { Check(host::CameraFile_Name(level, &name) && name == "Invasion.cam", "the RIFFNAME and .cam"); });

	LevelSpec path = { "..\\..\\Windows\\System32\\evil name!", false, 0, false, { 0, 0, 0 }, false };
	WithLevel(path, [&](File_Chunk *level) { Check(host::CameraFile_Name(level, &name) && name == "evil_name_.cam", "a RIFFNAME with folders in it keeps only its last part, and its odd letters become _"); });

	LevelSpec dots = { "..", false, 0, false, { 0, 0, 0 }, false };
	WithLevel(dots, [&](File_Chunk *level) { Check(!host::CameraFile_Name(level, &name), "a RIFFNAME of just .. names no file"); });

	Check(!host::CameraFile_Name(nullptr, &name), "no level names no file");
}

static void TestOffset()
{
	puts("2. The CAMORIGN offset (FUN_00470950 0x470b97 to 0x470de1)");

	int32_t offset[3];

	LevelSpec none = { "a", false, 0, false, { 0, 0, 0 }, false };
	WithLevel(none, [&](File_Chunk *level) { host::CameraFile_Offset(level, offset); Check(Same3(offset, 0, 0, 0), "no SPECLOBJ and no CAMORIGN: the offset is 0 0 0"); });

	// no ENVSDSCL: the scale is inteng's default, the double 100.0, and the result is that times the doubles (not times 100 again)
	LevelSpec noScale = { "a", false, 0, true, { 1.5, -2.0, 0.25 }, false };
	WithLevel(noScale, [&](File_Chunk *level) { host::CameraFile_Offset(level, offset); Check(Same3(offset, 150, -200, 25), "no ENVSDSCL: 100.0 times the three doubles (1.5, -2.0, 0.25 give 150, -200, 25)"); });

	// ENVSDSCL 0.5 (the game's loader takes only 0.5 to 1.0): 0.5 x 100.0 x each, cut toward zero (12.5 is 12)
	LevelSpec scaled = { "a", true, 0.5, true, { 1.5, -2.0, 0.25 }, false };
	WithLevel(scaled, [&](File_Chunk *level) { host::CameraFile_Offset(level, offset); Check(Same3(offset, 75, -100, 12), "ENVSDSCL 0.5: 0.5 x 100.0 x the doubles (75, -100, 12)"); });

	// cut toward zero, as __ftol does
	LevelSpec cut = { "a", false, 0, true, { 0.019, -0.019, 0.0 }, false };
	WithLevel(cut, [&](File_Chunk *level) { host::CameraFile_Offset(level, offset); Check(Same3(offset, 1, -1, 0), "the numbers are cut toward zero (1.9 is 1, -1.9 is -1)"); });

	// the first CAMORIGN of the first SPECLOBJ
	LevelSpec two = { "a", false, 0, true, { 1.0, 1.0, 1.0 }, true };
	WithLevel(two, [&](File_Chunk *level) { host::CameraFile_Offset(level, offset); Check(Same3(offset, 100, 100, 100), "of two CAMORIGN chunks the first is used"); });

	// a number that is not a number, or too far for a whole number, is 0, never undefined
	LevelSpec tooFar = { "a", false, 0, true, { 1e300, -1e300, 5.0 }, false };
	WithLevel(tooFar, [&](File_Chunk *level) { host::CameraFile_Offset(level, offset); Check(Same3(offset, 0, 0, 500), "a place too far for a whole number is 0; the others are as they were"); });

	Check(true, "(no level: see 1)");
	host::CameraFile_Offset(nullptr, offset);
	Check(Same3(offset, 0, 0, 0), "no level: 0 0 0");
}

static void TestFile()
{
	puts("3. The file: places in and out with the offset");

	std::string const folder = folderOfTest + "\\camera\\";
	int32_t const offset[3] = { 150, -200, 25 };
	host::CameraFileSlot written[host::kCameraFileSlots];

	for (size_t s = 0; s < host::kCameraFileSlots; s++)
	{
		written[s].place[0] = (int)(1000 * (s + 1));
		written[s].place[1] = (int)(-500 * (s + 1));
		written[s].place[2] = (int)(7 * s);
		for (int i = 0; i < 9; i++)
		{
			written[s].matrix[i] = (i % 4 == 0) ? 65536 : 0;   // the identity turn
		}
	}

	Check(host::CameraFile_Write(folder, "a.cam", offset, written), "a camera file is written, the folder made");

	// its size
	WIN32_FILE_ATTRIBUTE_DATA info;

	Check(GetFileAttributesExA((folder + "a.cam").c_str(), GetFileExInfoStandard, &info) && info.nFileSizeLow == host::kCameraFileBytes && info.nFileSizeHigh == 0 && host::kCameraFileBytes == 528, "it is 528 bytes, as inteng's");

	// the bytes: slot 0's place has the offset taken off (FUN_004fd9d0), the first three numbers of the file
	{
		std::vector<char> bytes(host::kCameraFileBytes);
		HANDLE const file = CreateFileA((folder + "a.cam").c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		DWORD got = 0;

		Check(file != INVALID_HANDLE_VALUE && ReadFile(file, bytes.data(), (DWORD)bytes.size(), &got, 0) && got == bytes.size(), "it is read back");

		if (file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(file);
		}

		int32_t first[3];
		int32_t eleventh[3];

		memcpy(first, bytes.data(), 12);
		memcpy(eleventh, bytes.data() + 0x78 + 0x168, 12);
		Check(Same3(first, 1000 - 150, -500 + 200, 0 - 25), "slot 0's place in the file is the place minus the offset");
		Check(Same3(eleventh, 11000 - 150, -5500 + 200, 70 - 25), "slot 10's place (after the ten places and ten turns) is the place minus the offset");
	}

	// read back with the same offset: the places are as they were
	host::CameraFileSlot back[host::kCameraFileSlots];

	for (size_t s = 0; s < host::kCameraFileSlots; s++)
	{
		back[s].place[0] = back[s].place[1] = back[s].place[2] = -1;

		for (int i = 0; i < 9; i++)
		{
			back[s].matrix[i] = 0;
		}
	}

	Check(host::CameraFile_Read(folder + "a.cam", offset, back), "the file is read");

	bool same = true;

	for (size_t s = 0; s < host::kCameraFileSlots; s++)
	{
		same = same && back[s].place[0] == written[s].place[0] && back[s].place[1] == written[s].place[1] && back[s].place[2] == written[s].place[2] && back[s].matrix[4] == 65536 && back[s].matrix[1] == 0;
	}

	Check(same, "all eleven places come back as they went out (the offset taken off on write and added on read)");

	// read with no offset: the file's own numbers
	host::CameraFileSlot raw[host::kCameraFileSlots];
	int32_t const none[3] = { 0, 0, 0 };

	memcpy(raw, written, sizeof(raw));
	Check(host::CameraFile_Read(folder + "a.cam", none, raw) && raw[0].place[0] == 850 && raw[0].place[1] == -300 && raw[0].place[2] == -25, "read with offset 0 the places are the file's own numbers");

	// a file of another size isn't read, and the slots are as they were
	{
		HANDLE const file = CreateFileA((folder + "short.cam").c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		DWORD put = 0;
		char const little[100] = {};

		WriteFile(file, little, sizeof(little), &put, 0);
		CloseHandle(file);

		host::CameraFileSlot kept[host::kCameraFileSlots];

		for (size_t s = 0; s < host::kCameraFileSlots; s++)
		{
			kept[s].place[0] = 5; kept[s].place[1] = 6; kept[s].place[2] = 7;
		}

		Check(!host::CameraFile_Read(folder + "short.cam", none, kept) && kept[0].place[0] == 5 && kept[10].place[2] == 7, "a file of 100 bytes is not read, and the slots are as they were");
		Check(!host::CameraFile_Read(folder + "missing.cam", none, kept), "a file that isn't there is not read");
		DeleteFileA((folder + "short.cam").c_str());
	}

	DeleteFileA((folder + "a.cam").c_str());
	RemoveDirectoryA(folder.c_str());
}

int main()
{
	folderOfTest = "camera_file_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folderOfTest.c_str(), 0))
	{
		puts("  FAILED  the test's folder can't be made");
		return 1;
	}

	TestNames();
	TestOffset();
	TestFile();
	RemoveDirectoryA(folderOfTest.c_str());
	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
