// The test for src/host/camera_file: inteng's camera file (<RIFFNAME>.cam, FUN_00470950 reads, FUN_00470610 writes) and the CAMORIGN offset. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: no stock level has a CAMORIGN chunk, so the offset path can't be seen in any real run. This builds small levels in memory the way the game reads them (REBINFF2 > REBENVDT > RIFFNAME, ENVSDSCL, SPECLOBJ > CAMORIGN
// with three known doubles) and checks, with the game's own chunk library: the file's name (the RIFFNAME's last part, cleaned), the offset (the first CAMORIGN of the first SPECLOBJ, times the level's scale times 100.0, cut
// toward zero; 0 with none), the offset ADDED to every place when a camera file is read, and TAKEN OFF again when one is written (FUN_004fd9d0), a file that isn't exactly 528 bytes refused, and a slot's place going
// out and coming back the same. THE FILE'S UNITS (Vasquez 11-18, Bishop's ruling: an inteng-written .cam must load right): the file holds inteng's view as FUN_004704d0 keeps it, the place in 1/100 mm and the
// matrix the game's VDB_Mat turned over (FUN_004c19a0); the slots hold millimetres and the game's form. Section 4 reads a file made byte by byte as inteng writes it.
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
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

// The game's view matrix (VDB_Mat) of a camera turned 45 degrees (512) and level: row 1 is (cos, 0, sin) (level_view.cpp LevelView_AnglesOfMatrix), x 65536; and as inteng's view keeps it, turned over
static int32_t const kTurned512[9] = { 46341, 0, 46341, 0, 65536, 0, -46341, 0, 46341 };
static int32_t const kTurned512Inteng[9] = { 46341, 0, -46341, 0, 65536, 0, 46341, 0, 46341 };

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
		memcpy(written[s].matrix, kTurned512, sizeof(kTurned512));
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
		int32_t firstTurn[9];

		memcpy(first, bytes.data(), 12);
		memcpy(eleventh, bytes.data() + 0x78 + 0x168, 12);
		memcpy(firstTurn, bytes.data() + 0x78, 36);
		Check(Same3(first, 1000 * 100 - 150, -500 * 100 + 200, 0 - 25), "slot 0's place in the file is in inteng's 1/100 mm (FUN_004704d0 keeps the view's +0x10), the offset taken off");
		Check(Same3(eleventh, 11000 * 100 - 150, -5500 * 100 + 200, 70 * 100 - 25), "slot 10's place (after the ten places and ten turns) likewise");
		Check(memcmp(firstTurn, kTurned512Inteng, sizeof(firstTurn)) == 0, "slot 0's turn in the file is inteng's view matrix, the game's TURNED OVER (FUN_004c19a0: +0x38 is VDB_Mat transposed)");
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
		same = same && back[s].place[0] == written[s].place[0] && back[s].place[1] == written[s].place[1] && back[s].place[2] == written[s].place[2] && memcmp(back[s].matrix, kTurned512, sizeof(kTurned512)) == 0;
	}

	Check(same, "all eleven places and turns come back as they went out (a save then a reload: the offset taken off on write and added on read, the turn turned over and back)");

	// read with no offset: the file's own numbers, in mm
	host::CameraFileSlot raw[host::kCameraFileSlots];
	int32_t const none[3] = { 0, 0, 0 };

	memcpy(raw, written, sizeof(raw));
	Check(host::CameraFile_Read(folder + "a.cam", none, raw) && raw[0].place[0] == 999 && raw[0].place[1] == -498 && raw[0].place[2] == 0,
		"read with offset 0 the places are the file's own numbers / 100, to the nearest mm (99850 is 999, halves away from 0; -49800 is -498; -25 is 0)");

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

static void PutLe(std::vector<uint8_t> *bytes, size_t at, int32_t value)
{
	for (int i = 0; i < 4; i++)
	{
		(*bytes)[at + i] = (uint8_t)((uint32_t)value >> (8 * i));
	}
}

// A file as INTENG writes it (FUN_00470610: the places of slots 0 to 9, their matrices, then slot 10's place and matrix; the view's +0x10 in 1/100 mm and its +0x38, VDB_Mat turned over), made here
// byte by byte from that format, read by the editor's reader: the camera must come where inteng's was
static void TestIntengFile()
{
	puts("4. A camera file written by inteng (its units: 1/100 mm, the view's matrix turned over)");

	std::string const folder = folderOfTest + "\\camera\\";
	std::vector<uint8_t> bytes(host::kCameraFileBytes, 0);
	int32_t const slot0[3] = { 123456, -200000, 5000050 };
	int32_t const slot10[3] = { -149, 150, -150 };

	for (int i = 0; i < 3; i++)
	{
		PutLe(&bytes, (size_t)i * 4, slot0[i]);
		PutLe(&bytes, 0x78 + 0x168 + (size_t)i * 4, slot10[i]);
	}

	for (int k = 0; k < 9; k++)
	{
		PutLe(&bytes, 0x78 + (size_t)k * 4, kTurned512Inteng[k]);
		PutLe(&bytes, 0x78 + 0x168 + 0xc + (size_t)k * 4, kTurned512Inteng[k]);
	}

	CreateDirectoryA(folder.c_str(), 0);

	HANDLE const file = CreateFileA((folder + "inteng.cam").c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	DWORD put = 0;

	Check(file != INVALID_HANDLE_VALUE && WriteFile(file, bytes.data(), (DWORD)bytes.size(), &put, 0) && put == bytes.size(), "a 528-byte file is made from inteng's format");

	if (file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
	}

	host::CameraFileSlot slots[host::kCameraFileSlots];
	int32_t const none[3] = { 0, 0, 0 };

	Check(host::CameraFile_Read(folder + "inteng.cam", none, slots), "the editor's reader takes it");
	Check(Same3(slots[0].place, 1235, -2000, 50001), "slot 0's place 123456 -200000 5000050 (1/100 mm) is 1235 -2000 50001 mm (to the nearest: 1234.56 is 1235, 50000.5 is 50001)");
	Check(memcmp(slots[0].matrix, kTurned512, sizeof(kTurned512)) == 0, "slot 0's matrix, inteng's form, becomes the game's VDB_Mat of a 45-degree turn (the one Link with AVP's module.bbb test takes as turn 512)");
	Check(Same3(slots[10].place, -1, 2, -2), "slot 10's place -149 150 -150 is -1 2 -2 mm (halves away from 0)");

	int32_t const offset[3] = { 150, -200, 25 };

	Check(host::CameraFile_Read(folder + "inteng.cam", offset, slots) && Same3(slots[0].place, 1236, -2002, 50001),
		"with a CAMORIGN offset 150 -200 25 (inteng's units, as CameraFile_Offset gives it) the offset is added BEFORE the change to mm: 123606 -200200 5000075 are 1236 -2002 50001");

	// the far ends: a file's number and an offset both at their largest never wrap round
	PutLe(&bytes, 0, INT32_MIN);
	PutLe(&bytes, 4, INT32_MAX);

	HANDLE const farFile = CreateFileA((folder + "far.cam").c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	Check(farFile != INVALID_HANDLE_VALUE && WriteFile(farFile, bytes.data(), (DWORD)bytes.size(), &put, 0) && put == bytes.size(), "a file with the largest numbers in slot 0's place is made");

	if (farFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(farFile);
	}

	int32_t const farOffset[3] = { -2147483000, 2147483000, 0 };

	Check(host::CameraFile_Read(folder + "far.cam", farOffset, slots) && slots[0].place[0] == -42949666 && slots[0].place[1] == 42949666,
		"the largest file number plus the largest offset (4294966648 and 4294966647) is summed in 64 bits, then made mm (-42949666 and 42949666, not wrapped)");

	host::CameraFileSlot out[host::kCameraFileSlots];

	memset(out, 0, sizeof(out));
	out[0].place[0] = 1 << 24;          // the camera's farthest place (level_view.h kFlyCameraMostPlace)
	out[0].place[1] = -(1 << 24);

	Check(host::CameraFile_Write(folder, "held.cam", farOffset, out), "a camera at its farthest is written with the largest offsets");

	HANDLE const held = CreateFileA((folder + "held.cam").c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	int32_t first[3] = {};
	DWORD got = 0;

	if (held != INVALID_HANDLE_VALUE)
	{
		ReadFile(held, first, sizeof(first), &got, 0);
		CloseHandle(held);
	}

	Check(got == sizeof(first) && first[0] == INT32_MAX && first[1] == INT32_MIN, "1677721600 + 2147483000 is held at the largest number the file holds, and its opposite at the smallest (inteng's 32 bits would wrap)");

	DeleteFileA((folder + "inteng.cam").c_str());
	DeleteFileA((folder + "far.cam").c_str());
	DeleteFileA((folder + "held.cam").c_str());
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
	TestIntengFile();
	RemoveDirectoryA(folderOfTest.c_str());
	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
