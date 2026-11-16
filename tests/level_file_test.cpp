// The test of the editor's save path: opening a level to edit, and saving it back (src/io/level_file.cpp, the phase 2 plan's F7).
// Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT TESTS, on level files it makes itself. No file the game comes with is used.
// - A save with nothing changed writes the file's very bytes, and makes a .bak of them. The .bak is made once: a later save never
//   writes over it, and nor does the first save if a .bak was there already.
// - A changed chunk is written by the game's chunk library into memory filled with zeros first. Every chunk nobody changed is written
//   as the very bytes it was read from. Every chunk holding a changed one, and the level itself, gets its size worked out again.
// - A packed level saved with nothing changed is its own bytes. A changed one is packed again, and loads back.
// - What opening refuses: a file too short or too long, a chunk whose size runs past the end of what holds it, a chunk size below a
//   heading, a level heading that doesn't give the file's size, chunks inside chunks deeper than the game allows, a package that
//   can't be unpacked, a name the library can't keep whole, and a file that isn't the same for the editor's read and the game's.
// - What stops a save, each time with the file the user picked left as it was and no ".saving" left behind: a level the game's
//   loader would refuse, a ".saving" already there (which isn't touched), a read-only file, a folder, a level too big for the game,
//   a .bak that can't be made, a file another program holds, and a new file the loader doesn't read back as it was written.
//
// HOW IT WORKS
// - The game's real chunk library, loader, packer and unpacker are built into the test (tests/level_file_test.args lists them).
// - Its files go in a folder it makes inside the folder it runs in, named by its own process number, and removes at the end. Each
//   part of the test has a folder of its own in there, so "nothing else is in the folder" can be checked exactly.
// - Every block of memory the program asks for with new is filled with 0xA5 before it's handed out (operator new, below). So a byte
//   that nobody wrote shows up as 0xA5 in a saved file, never as a zero by luck.
// - The game opens files through avp_CreateFile. Here that's Windows' own CreateFile, as in the game (avpx/3dc/utilities.cpp:293-295),
//   except that it refuses to open anything for writing, so every byte written here is written by level_file.cpp itself. It can also
//   be told to open another file in place of the one asked for: that's how a file that changes in the middle of an open, or a new
//   file that isn't read back as written, is made without a race.
//
// THE FILES IT MAKES, as the game reads them (avpx/3dc/win95/chunk.hpp and mishchnk.cpp): every chunk is an 8-byte name, then a
// 4-byte size, lowest byte first, that counts these 12 bytes too, then what it holds. A level file is one chunk named REBINFF2 whose
// size is the file's own. A packed level is the game's Huffman package of such a level (huffman.hpp). The kinds used here:
// - RIFFNAME (RIF_Name_Chunk, mishchnk.cpp:763): a name, its zero byte, then 1 to 4 bytes rounding it up to a multiple of 4
//   (RIF_Name_Chunk::size_chunk, mishchnk.hpp). The game's library never writes those rounding bytes, and the old tools left whatever
//   was in their memory there, so here they hold a byte that isn't zero: it shows whether a chunk was written from its own bytes;
// - AVPSTRAT holding a text message (AVP_Strategy_Chunk and TextMessageStrategy, strategy_chunk.cpp): the loader refuses a message
//   number below 0 or above 450 (strategy_chunk.cpp:26-34), which is how a level the game would refuse is made;
// - VIOBJECT (Virtual_Object_Chunk, strategy_chunk.cpp:255-256): a chunk that holds other chunks;
// - a name the game doesn't know, which the library keeps as plain bytes.
//
// WHAT IT CAN'T SHOW: that the disk keeps a file after a flush or a power cut, and that a short read or write is refused (a test
// can't make Windows give one). Nor the checks in level_file.cpp that no file can reach while the game's loader refuses the same
// files first: a chunk size that Pair or Write would refuse, a chunk the library holds that has no bytes, and sizes above INT_MAX.
//
// HOW IT RUNS: built with tests/level_file_test.args. The program gives back a non-zero result if any check failed.
// tests/takeouts/level_file_test.json breaks level_file.cpp one way at a time, to prove a check here fails each time.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "huffman.hpp"
#include "strategy_chunk.hpp"
#include "level_file_sum.h"
#include "level_file.h"
#include "crash_guard.h"

static int checksFailed = 0;

static void Check(bool good, std::string const & what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls

static std::string loaderAsksFor;          // when the game's loader asks for this file...
static std::string loaderIsGiven;          // ...it's given this one instead (empty: it's given what it asks for)
static int writesAskedOfTheGame = 0;       // how many times anything asked the game's own file call to write

HANDLE avp_CreateFile(const char * name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		writesAskedOfTheGame++;
		return INVALID_HANDLE_VALUE;
	}

	// "*" swaps every file the loader asks for: since L9 it reads the editor's private copy, whose name the test doesn't know
	bool const swapped = name && !loaderAsksFor.empty() && (loaderAsksFor == "*" || loaderAsksFor == name);
	return CreateFileA(swapped ? loaderIsGiven.c_str() : name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char * name)
{
	return GetFileAttributesA(name);
}

// The game's memory code stops the game through these if the computer runs out of memory. Here that stops the test, as a failure.
void avp_MessageBox(const char * message, int)
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

// The memory code tells the game's sounds about memory that's about to go. There are no sounds in this test.
void Sound_ForgetAddressesInside(const void *, size_t)
{
}

// ---- Memory that is never zero by luck

// What every block from new holds before anyone writes in it
static unsigned char const Unwritten = 0xA5;

void * operator new(size_t bytes)
{
	size_t const asked = bytes ? bytes : 1;
	void * const block = malloc(asked);

	if (!block)
	{
		throw std::bad_alloc();
	}

	memset(block, Unwritten, asked);
	return block;
}

void * operator new[](size_t bytes) { return operator new(bytes); }
void operator delete(void * block) noexcept { free(block); }
void operator delete[](void * block) noexcept { free(block); }
void operator delete(void * block, size_t) noexcept { free(block); }
void operator delete[](void * block, size_t) noexcept { free(block); }

// ---- Making level files

typedef std::vector<char> Bytes;
typedef std::vector<std::string> Names;

// A 4-byte number on the end, lowest byte first, as the game keeps every number in a level file
static void PutNumber(Bytes & out, int value)
{
	char four[4];
	memcpy(four, &value, sizeof(four));
	out.insert(out.end(), four, four + 4);
}

static void PutNumberAt(Bytes & bytes, size_t at, int value)
{
	memcpy(bytes.data() + at, &value, sizeof(value));
}

// Whether the 4-byte number at this place is this one. False for a place too near the end to hold one.
static bool NumberIs(Bytes const & bytes, size_t at, int wanted)
{
	if (at > bytes.size() || bytes.size() - at < 4)
	{
		return false;
	}

	int found;
	memcpy(&found, bytes.data() + at, sizeof(found));
	return found == wanted;
}

// A chunk: the 8 bytes of its name, its size counting this 12-byte heading, then what it holds. name must have 8 bytes to read.
static Bytes MakeChunk(const char * name, Bytes const & inside)
{
	Bytes out(name, name + 8);
	PutNumber(out, (int)(12 + inside.size()));
	out.insert(out.end(), inside.begin(), inside.end());
	return out;
}

static Bytes Join(std::initializer_list<Bytes> parts)
{
	Bytes out;

	for (Bytes const & part : parts)
	{
		out.insert(out.end(), part.begin(), part.end());
	}

	return out;
}

// A name chunk: the name, its zero byte, then "leftover" in the bytes that round it up to a multiple of 4, as the game sizes it
static Bytes NameBytes(const char * name, char leftover)
{
	size_t const length = strlen(name);
	Bytes inside(name, name + length);
	inside.push_back(0);

	while (inside.size() < length + 4 - length % 4)
	{
		inside.push_back(leftover);
	}

	return MakeChunk("RIFFNAME", inside);
}

// A text message: the chunk's own two numbers (index, spare2), then the strategy's three (its kind, StratMessage, then Type and
// ExtraData), then the message's number, its flags and one spare (TextMessageStrategy, strategy_chunk.cpp:1403-1418)
static Bytes MessageBytes(int message)
{
	Bytes inside;
	PutNumber(inside, 0);
	PutNumber(inside, 0);
	PutNumber(inside, StratMessage);
	PutNumber(inside, 0);
	PutNumber(inside, 0);
	PutNumber(inside, message);
	PutNumber(inside, 0);
	PutNumber(inside, 0);
	return MakeChunk("AVPSTRAT", inside);
}

static Bytes HolderBytes(Bytes const & inside)
{
	return MakeChunk("VIOBJECT", inside);
}

static Bytes WholeLevel(Bytes const & inside)
{
	// Every level the game loads holds a level environment (REBENVDT; mishchnk.cpp, File_Chunk(file), at the pin 491deb22), so this one does, last
	return MakeChunk("REBINFF2", Join({ inside, MakeChunk("REBENVDT", Bytes()) }));
}

// The level most of the test uses, 160 bytes:
//     0  REBINFF2, the level's heading
//    12  RIFFNAME "ab", rounding byte 0x55 at 27
//    28  VIOBJECT, holding:
//    40      RIFFNAME "cd", rounding byte 0x66 at 55
//    56      AVPSTRAT, message 5
//   100  AVPSTRAT, message 7 (its number at 132)
//   144  ZZPLAINZ, a kind the game doesn't know, holding x y z 0x77
// Each part can be set otherwise, to make the level a save is expected to write.
struct BaseLevel
{
	const char * topName = "ab";
	char topLeftover = 0x55;
	const char * innerName = "cd";
	char innerLeftover = 0x66;
	int innerMessage = 5;
	int topMessage = 7;

	Bytes Make() const
	{
		return WholeLevel(Join({
			NameBytes(topName, topLeftover),
			HolderBytes(Join({ NameBytes(innerName, innerLeftover), MessageBytes(innerMessage) })),
			MessageBytes(topMessage),
			MakeChunk("ZZPLAINZ", Bytes{ 'x', 'y', 'z', 0x77 }) }));
	}
};

size_t const TopNameAt = 12;
size_t const HolderAt = 28;
size_t const InnerNameAt = 40;
size_t const InnerMessageAt = 56;
size_t const TopMessageAt = 100;
size_t const PlainAt = 144;
size_t const BaseBytes = 172;

// A level of holders inside holders, this many deep, with "inside" in the deepest
static Bytes Nested(int holders, Bytes const & inside)
{
	Bytes level = inside;

	for (int i = 0; i < holders; i++)
	{
		level = HolderBytes(level);
	}

	return WholeLevel(level);
}

// The level packed the way the game packs its levels, as one file
static Bytes Packed(Bytes const & level)
{
	Bytes copy(level);
	HuffmanPackage * const package = HuffmanCompression((unsigned char *)copy.data(), (int)copy.size());
	char const * const start = (char const *)package;
	Bytes out(start, start + sizeof(HuffmanPackage) + package->CompressedDataSize);
	free(package);
	return out;
}

// The level inside a packed file, unpacked by the game's checked unpacker; empty if it can't be
static Bytes Unpacked(Bytes const & file)
{
	Bytes level;

	if (file.size() < sizeof(HuffmanPackage))
	{
		return level;
	}

	char * const unpacked = HuffmanDecompressChecked((HuffmanPackage const *)file.data(), (int)file.size());

	if (unpacked)
	{
		int size;
		memcpy(&size, file.data() + offsetof(HuffmanPackage, UncompressedDataSize), sizeof(size));
		level.assign(unpacked, unpacked + size);
		free(unpacked);
	}

	return level;
}

// ---- Files and folders

static std::string testFolder;

static bool WriteBytes(std::string const & path, Bytes const & bytes)
{
	FILE * const file = fopen(path.c_str(), "wb");

	if (!file)
	{
		return false;
	}

	bool const wrote = bytes.empty() || fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
	return fclose(file) == 0 && wrote;
}

// A file's bytes, or none if it can't be read
static Bytes Read(std::string const & path)
{
	Bytes bytes;
	FILE * const file = fopen(path.c_str(), "rb");

	if (!file)
	{
		return bytes;
	}

	char block[4096];
	size_t got;

	while ((got = fread(block, 1, sizeof(block), file)) > 0)
	{
		bytes.insert(bytes.end(), block, block + got);
	}

	fclose(file);
	return bytes;
}

static bool IsThere(std::string const & path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Whether another program could open the file now with nobody else sharing it: true only if no handle on it is left open
static bool NobodyHolds(std::string const & path)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	CloseHandle(file);
	return true;
}

// The names in a folder, in order
static Names NamesIn(std::string const & folder)
{
	Names names;
	WIN32_FIND_DATAA found;
	HANDLE const search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
	{
		return names;
	}

	do
	{
		if (strcmp(found.cFileName, ".") != 0 && strcmp(found.cFileName, "..") != 0)
		{
			names.push_back(found.cFileName);
		}
	}
	while (FindNextFileA(search, &found));

	FindClose(search);
	std::sort(names.begin(), names.end());
	return names;
}

// A folder of its own, inside the test's, for one part of the test
static std::string Room(const char * name)
{
	std::string const room = testFolder + "\\" + name;

	if (!CreateDirectoryA(room.c_str(), 0))
	{
		printf("  FAILED  the test couldn't make its folder %s\n", room.c_str());
		exit(1);
	}

	return room;
}

// Writes a level file for a part of the test, which stops if it can't
static void Make(std::string const & path, Bytes const & bytes)
{
	if (!WriteBytes(path, bytes))
	{
		printf("  FAILED  the test couldn't write %s\n", path.c_str());
		exit(1);
	}
}

// The level as inteng writes it (File_Chunk::write_file, mishchnk.cpp:391): the game's own library reads the level, prepare_for_output()s the whole tree
// and gives the tree's bytes. Done here straight on the library, in a file of the test's own, so the expected bytes of a save don't come from level_file.cpp.
// Empty if the library refuses the level.
static Bytes AsIntengWritesIt(Bytes const & level)
{
	static int count = 0;
	std::string const path = testFolder + "\\prepared-" + std::to_string(count++) + ".tmp";
	Bytes out;

	if (!WriteBytes(path, level))
	{
		printf("  FAILED  the test couldn't write %s\n", path.c_str());
		exit(1);
	}

	auto * const before = Parent_File;

	{
		std::unique_ptr<File_Chunk> const tree(new File_Chunk(path.c_str()));

		if (tree->error_code == 0)
		{
			tree->prepare_for_output();
			size_t const size = tree->size_chunk();
			Bytes block(size, 0); // the library's own block would be whatever the memory held (here 0xA5s) in a name's rounding bytes
			tree->fill_data_block(block.data());
			out.swap(block);
		}
	}

	Parent_File = before;
	DeleteFileA(path.c_str());
	return out;
}

static void RemoveFolder(std::string const & folder)
{
	for (std::string const & name : NamesIn(folder))
	{
		std::string const path = folder + "\\" + name;
		DWORD const attributes = GetFileAttributesA(path.c_str());

		if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			RemoveFolder(path);
		}
		else
		{
			SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
			DeleteFileA(path.c_str());
		}
	}

	RemoveDirectoryA(folder.c_str());
}

// ---- The loaded level's chunks

static Chunk * FirstNamed(Chunk_With_Children * holder, const char * name)
{
	return holder ? holder->lookup_single_child(name) : 0;
}

static Chunk_With_Children * HolderIn(Chunk_With_Children * holder)
{
	return dynamic_cast<Chunk_With_Children *>(FirstNamed(holder, "VIOBJECT"));
}

static RIF_Name_Chunk * NameIn(Chunk_With_Children * holder)
{
	return dynamic_cast<RIF_Name_Chunk *>(FirstNamed(holder, "RIFFNAME"));
}

static AVP_Strategy_Chunk * MessageChunkIn(Chunk_With_Children * holder)
{
	return dynamic_cast<AVP_Strategy_Chunk *>(FirstNamed(holder, "AVPSTRAT"));
}

static TextMessageStrategy * MessageOf(AVP_Strategy_Chunk * chunk)
{
	return chunk ? dynamic_cast<TextMessageStrategy *>(chunk->Strategy) : 0;
}

// Gives a name chunk a new name, as a mode that edits names would. The chunk gives the old one back with delete[], and so this one.
static void Rename(RIF_Name_Chunk * chunk, const char * name)
{
	size_t const length = strlen(name);
	char * const copy = new char[length + 1];
	memcpy(copy, name, length + 1);
	delete[] chunk->rif_name;
	chunk->rif_name = copy;
}

static bool NameIs(RIF_Name_Chunk * chunk, const char * name)
{
	return chunk && chunk->rif_name && strcmp(chunk->rif_name, name) == 0;
}

static const char * Said(LevelOpenResult result)
{
	switch (result)
	{
	case LevelOpenResult::Opened: return "Opened";
	case LevelOpenResult::NoPath: return "NoPath";
	case LevelOpenResult::CantRead: return "CantRead";
	case LevelOpenResult::LoaderRefused: return "LoaderRefused";
	case LevelOpenResult::ChangedWhileOpening: return "ChangedWhileOpening";
	case LevelOpenResult::DoesntPair: return "DoesntPair";
	}

	return "?";
}

static const char * Said(LevelSaveResult result)
{
	switch (result)
	{
	case LevelSaveResult::Saved: return "Saved";
	case LevelSaveResult::NothingOpen: return "NothingOpen";
	case LevelSaveResult::NoPath: return "NoPath";
	case LevelSaveResult::ReadOnly: return "ReadOnly";
	case LevelSaveResult::CantBuild: return "CantBuild";
	case LevelSaveResult::SavingFileInTheWay: return "SavingFileInTheWay";
	case LevelSaveResult::CantWrite: return "CantWrite";
	case LevelSaveResult::GameWouldRefuse: return "GameWouldRefuse";
	case LevelSaveResult::CantKeepBackup: return "CantKeepBackup";
	case LevelSaveResult::CantReplace: return "CantReplace";
	}

	return "?";
}

// The words of a check, with what the call really said on the end
static std::string Words(const char * what, const char * said)
{
	return std::string(what) + " (it said " + said + ")";
}

// ---- The tests

// The test's own level is laid out as the comment above BaseLevel says, so the places the checks look at are the right ones
static void TestTheTestsOwnLevel()
{
	Bytes const level = BaseLevel().Make();

	Check(level.size() == BaseBytes && memcmp(&level[0], "REBINFF2", 8) == 0 && NumberIs(level, 8, (int)BaseBytes) &&
		memcmp(&level[TopNameAt], "RIFFNAME", 8) == 0 && level[TopNameAt + 15] == (char)0x55 &&
		memcmp(&level[HolderAt], "VIOBJECT", 8) == 0 && NumberIs(level, HolderAt + 8, 72) &&
		memcmp(&level[InnerNameAt], "RIFFNAME", 8) == 0 && level[InnerNameAt + 15] == (char)0x66 &&
		memcmp(&level[InnerMessageAt], "AVPSTRAT", 8) == 0 && memcmp(&level[TopMessageAt], "AVPSTRAT", 8) == 0 &&
		NumberIs(level, TopMessageAt + 32, 7) && memcmp(&level[PlainAt], "ZZPLAINZ", 8) == 0,
		"the test's own level is laid out as its comment says: 160 bytes, each chunk where the checks look for it");
}

// With no level open, nothing can be changed, built or saved, and a path that's no path is refused
static void TestNothingOpen()
{
	std::string const room = Room("nothing-open");
	std::string const path = room + "\\level.rif";
	Make(path, BaseLevel().Make());

	OpenLevelFile none;
	Bytes built;
	Check(none.Level() == 0 && !none.HasChanges() && !none.MarkChanged(0) && !none.BuildLevel(built),
		"with no level open there's no level, nothing can be marked changed, and none can be built (level_file.cpp:328-333, 403-410)");
	Check(none.SaveAs(path.c_str()) == LevelSaveResult::NothingOpen && Read(path) == BaseLevel().Make(),
		"a save with no level open is refused, and the file is left alone (level_file.cpp:432-435)");
	Check(none.Open(0) == LevelOpenResult::NoPath && none.Open("") == LevelOpenResult::NoPath,
		"opening with no path, or an empty one, is refused as no path (level_file.cpp:181-184)");

	OpenLevelFile opened;
	LevelOpenResult const result = opened.Open(path.c_str());
	Check(result == LevelOpenResult::Opened, Words("the test's level opens", Said(result)));
	Check(opened.SaveAs(0) == LevelSaveResult::NoPath && opened.SaveAs("") == LevelSaveResult::NoPath,
		"saving with no path, or an empty one, is refused as no path (level_file.cpp:437-440)");

	// A failed open closes the level that was open before it
	LevelOpenResult const missing = opened.Open((room + "\\not-there.rif").c_str());
	Check(missing == LevelOpenResult::CantRead, Words("a file that isn't there can't be read", Said(missing)));
	Check(opened.Level() == 0 && opened.SaveAs(path.c_str()) == LevelSaveResult::NothingOpen,
		"and after it no level is open: the one open before was closed first, whatever happened (level_file.cpp:179)");
	Check(NamesIn(room) == Names{ "level.rif" }, "and nothing was written in the folder");
}

// A save with nothing changed writes the file's very bytes and makes a .bak of them
static void TestSaveWithNothingChanged()
{
	std::string const room = Room("nothing-changed");
	std::string const path = room + "\\level.rif";
	Bytes const original = BaseLevel().Make();
	Make(path, original);

	OpenLevelFile opened;
	LevelOpenResult const opening = opened.Open(path.c_str());
	Check(opening == LevelOpenResult::Opened,
		Words("a level the game loads opens for editing, every chunk paired with its own bytes (level_file.cpp:217-242)", Said(opening)));
	Check(NobodyHolds(path), "opening leaves no handle on the file: another program can have it to itself (level_file.cpp:88)");

	Bytes built;
	Check(opened.BuildLevel(built) && built == original,
		"with nothing changed, the level built for a save is the file's own bytes (level_file.cpp:412-416)");

	LevelSaveResult const result = opened.SaveAs(path.c_str());
	Check(result == LevelSaveResult::Saved, Words("a save with nothing changed is made", Said(result)));
	Bytes const asIntengWritesIt = AsIntengWritesIt(original);
	Check(!asIntengWritesIt.empty() && Read(path) == asIntengWritesIt,
		"and the file is what inteng writes for it: the whole tree prepared and written by the library (level_file.cpp, PrepareAsIntengDoes)");
	Check(Read(path + ".bak") == original, "and a .bak of the file as it was is made (level_file.cpp:534-554)");
	Check(NamesIn(room) == Names{ "level.rif", "level.rif.bak" },
		"and nothing else is left in the folder: no .saving, and no name from inside the level (level_file.cpp:442-443, 559)");
	Check(NobodyHolds(path) && NobodyHolds(path + ".bak"),
		"and no handle is left on the file or its .bak (level_file.cpp:507, 130)");
	Check(Parent_File == opened.Level(),
		"and the library's \"file being loaded\" is this level again after the save's check load (level_file.cpp:518-523)");
}

// The .bak is made once: never written over by a later save, and never by the first if one was there already
static void TestBackupIsMadeOnce()
{
	std::string const room = Room("backup-once");
	std::string const path = room + "\\level.rif";
	Bytes const original = BaseLevel().Make();
	Make(path, original);

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened && opened.SaveAs(path.c_str()) == LevelSaveResult::Saved &&
		Read(path + ".bak") == original, "the first save makes the .bak, of the file as it was");

	RIF_Name_Chunk * const name = NameIn(opened.Level());
	Check(name != 0, "the test finds the level's first name chunk");

	if (!name)
	{
		return;
	}

	Rename(name, "q1");
	Check(opened.MarkChanged(name) && opened.SaveAs(path.c_str()) == LevelSaveResult::Saved && Read(path) != original,
		"a second save, with a chunk changed, writes the changed level");
	Check(Read(path + ".bak") == original, "and leaves the .bak as it was: the file before the editor first touched it (level_file.cpp:539)");

	AVP_Strategy_Chunk * const message = MessageChunkIn(opened.Level());
	TextMessageStrategy * const text = MessageOf(message);
	Check(text != 0, "the test finds the level's message");

	if (!text)
	{
		return;
	}

	text->message_string = 9;
	Check(opened.MarkChanged(message) && opened.SaveAs(path.c_str()) == LevelSaveResult::Saved,
		"a third save, over a file that's no longer the first one, is made");
	Check(Read(path + ".bak") == original, "and the .bak is still the first file, not the one the third save wrote over (level_file.cpp:539-541)");

	// A .bak that was there before the editor ever saved is kept as it is
	std::string const room2 = Room("backup-there");
	std::string const path2 = room2 + "\\level.rif";
	Bytes const kept{ 'k', 'e', 'p', 't' };
	Make(path2, original);
	Make(path2 + ".bak", kept);

	OpenLevelFile second;
	LevelSaveResult const result = second.Open(path2.c_str()) == LevelOpenResult::Opened ? second.SaveAs(path2.c_str()) : LevelSaveResult::NothingOpen;
	Check(result == LevelSaveResult::Saved, Words("a save over a file with a .bak beside it already is made", Said(result)));
	Check(Read(path2 + ".bak") == kept, "and the .bak that was there is kept as it was, never replaced (level_file.cpp:532-541)");
}

// A changed chunk is written by the library into memory filled with zeros first; every other chunk is its very bytes
static void TestChangedChunkIsWrittenIntoZeros()
{
	std::string const room = Room("changed-chunk");
	std::string const path = room + "\\level.rif";
	Make(path, BaseLevel().Make());

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened, "the level opens");

	RIF_Name_Chunk * const name = NameIn(opened.Level());
	Check(name != 0 && opened.MarkChanged(name) && opened.HasChanges(), "its first name chunk, which holds no chunks, can be marked changed (level_file.cpp:328-344)");

	if (!name)
	{
		return;
	}

	BaseLevel expected;
	expected.topLeftover = 0;
	Bytes const wanted = expected.Make();

	Bytes built;
	Check(opened.BuildLevel(built) && built.size() == BaseBytes, "the level built with it changed is the same size");
	Check(built.size() == BaseBytes && built[TopNameAt + 15] == 0,
		"the changed chunk's rounding byte, 0x55 in the file, is a zero: the library wrote it into memory filled with zeros first, "
		"never its own memory (which here would show 0xA5) or the file's leftover (level_file.cpp:363-377)");
	Check(built.size() == BaseBytes && built[InnerNameAt + 15] == (char)0x66 && memcmp(&built[PlainAt + 12], "xyz\x77", 4) == 0,
		"every chunk nobody changed is written as its very bytes, the old tools' leftovers included (level_file.cpp:391-395)");
	Check(built == wanted, "and every other byte of the level is the file's own");

	LevelSaveResult const result = opened.SaveAs(path.c_str());
	Check(result == LevelSaveResult::Saved && Read(path) == AsIntengWritesIt(wanted),
		Words("the save writes that level as inteng writes it: the whole tree prepared and written by the library", Said(result)));

	OpenLevelFile again;
	Check(again.Open(path.c_str()) == LevelOpenResult::Opened && NameIs(NameIn(again.Level()), "ab"),
		"and the saved file opens for editing again, with the name as it was");
}

// A changed chunk that grows: the chunk holding it and the level itself get their new sizes
static void TestHoldersGetNewSizes()
{
	std::string const room = Room("holders");
	std::string const path = room + "\\level.rif";
	Make(path, BaseLevel().Make());

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened, "the level opens");

	Chunk_With_Children * const holder = HolderIn(opened.Level());
	RIF_Name_Chunk * const inner = NameIn(holder);
	Check(holder != 0 && inner != 0, "the test finds the name chunk inside the holder");

	if (!inner)
	{
		return;
	}

	Rename(inner, "cdefgh");
	Check(opened.MarkChanged(inner), "the name chunk inside the holder can be marked changed");

	BaseLevel expected;
	expected.innerName = "cdefgh";
	expected.innerLeftover = 0;
	Bytes const wanted = expected.Make();

	Bytes built;
	Check(opened.BuildLevel(built) && built == wanted,
		"the level built has the longer name, written by the library, and every chunk around it as its own bytes (level_file.cpp:347-401)");
	Check(NumberIs(built, HolderAt + 8, 76),
		"the holder's heading gives its new size, 76 for 72: its size is worked out again from what's now inside it (level_file.cpp:378-390)");
	Check(NumberIs(built, 8, (int)BaseBytes + 4), "the level's heading gives the level's new size, 164 (level_file.cpp:426)");

	LevelSaveResult const result = opened.SaveAs(path.c_str());
	Check(result == LevelSaveResult::Saved && Read(path) == AsIntengWritesIt(wanted),
		Words("the save is made as inteng writes it, and loads back through the game's loader", Said(result)));

	OpenLevelFile again;
	Check(again.Open(path.c_str()) == LevelOpenResult::Opened && NameIs(NameIn(HolderIn(again.Level())), "cdefgh"),
		"and the saved file opens again, with the new name inside the holder");
}

// MarkChanged takes only a chunk of this level that holds no chunks
static void TestWhatCanBeMarked()
{
	std::string const room = Room("marking");
	std::string const path = room + "\\level.rif";
	std::string const other = room + "\\other.rif";
	Make(path, BaseLevel().Make());
	Make(other, BaseLevel().Make());

	OpenLevelFile opened;
	OpenLevelFile another;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened && another.Open(other.c_str()) == LevelOpenResult::Opened, "two levels open");

	Check(!opened.MarkChanged(0), "no chunk at all can't be marked (level_file.cpp:330)");
	Check(!opened.MarkChanged(opened.Level()), "the level itself can't be marked: it holds chunks (level_file.cpp:330)");
	Check(!opened.MarkChanged(HolderIn(opened.Level())), "a chunk that holds chunks can't be marked: the one inside it that changed is (level_file.cpp:330)");
	Check(!opened.MarkChanged(NameIn(another.Level())), "a chunk of another level can't be marked (level_file.cpp:330)");
	Check(!opened.HasChanges(), "and none of those marked anything");

	Bytes built;
	Check(opened.BuildLevel(built) && built == BaseLevel().Make(), "so the level built is still the file's own bytes");
	Check(opened.MarkChanged(NameIn(opened.Level())) && opened.HasChanges(), "a name chunk of this level can be marked");
}

// A packed level: with nothing changed it's saved as its own bytes; a changed one is packed again, and loads back
static void TestPackedLevel()
{
	std::string const room = Room("packed");
	std::string const path = room + "\\level.rif";
	std::string const changedPath = room + "\\changed.rif";
	Bytes const level = BaseLevel().Make();

	// The game's packer, given the same level, gives the same package, so the file has 4 more bytes after its package, which the
	// game's unpacker never reads. Only the file's own bytes have them: a package made again can't, so these tell the two apart.
	Bytes packed = Packed(level);
	packed.insert(packed.end(), { 'T', 'A', 'I', 'L' });
	Make(path, packed);

	OpenLevelFile opened;
	LevelOpenResult const opening = opened.Open(path.c_str());
	Check(opening == LevelOpenResult::Opened, Words("a packed level opens for editing", Said(opening)));

	Bytes built;
	Check(opened.BuildLevel(built) && built == level, "and is unpacked to the level inside it (level_file.cpp:194-215)");

	LevelSaveResult result = opened.SaveAs(path.c_str());
	// inteng's save prepares the whole tree and writes it, so a packed level saved with nothing changed is packed again from the prepared tree
	Bytes const once = AsIntengWritesIt(level);
	// (this level has no object, environment or shape header, so no version number to go up: the real levels' test checks that; here the library's own writing of
	// the name chunks' rounding bytes, zeros, is what differs from the file)
	Check(!once.empty() && once != level, "the prepared tree is not the file's bytes: the library wrote the whole tree");
	Check(result == LevelSaveResult::Saved && Read(path) == Packed(once),
		Words("a packed level saved with nothing changed is the prepared tree, packed again (level_file.cpp, PrepareAsIntengDoes)", Said(result)));
	Check(Read(path + ".bak") == packed, "and its .bak is the file as it was");

	RIF_Name_Chunk * const name = NameIn(opened.Level());
	Check(name != 0 && opened.MarkChanged(name), "a chunk of the packed level is marked changed");

	// the editor now holds what the first save wrote, whose name chunk the library had already written into zeros
	Check(opened.BuildLevel(built) && built == once, "and the level built is the prepared tree, the changed chunk written into zeros");
	result = opened.SaveAs(changedPath.c_str());
	Bytes const changedFile = Read(changedPath);
	Check(result == LevelSaveResult::Saved, Words("the changed packed level is saved to a new file", Said(result)));
	Check(changedFile.size() >= 8 && memcmp(changedFile.data(), COMPRESSED_RIF_IDENTIFIER, 8) == 0,
		"and it's packed again, as the level it was opened from (level_file.cpp:475-487)");
	Check(Unpacked(changedFile) == AsIntengWritesIt(once), "and it unpacks to the level prepared a second time");

	OpenLevelFile again;
	Check(again.Open(changedPath.c_str()) == LevelOpenResult::Opened, "and it opens for editing again");
}

// A save the game's loader would refuse is refused, with the file left as it was and no .saving or .bak made
static void TestSaveTheGameWouldRefuse()
{
	std::string const room = Room("refused");
	std::string const path = room + "\\level.rif";
	Bytes const original = BaseLevel().Make();
	Make(path, original);

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened, "the level opens");

	AVP_Strategy_Chunk * const message = MessageChunkIn(opened.Level());
	TextMessageStrategy * const text = MessageOf(message);
	Check(text != 0, "the test finds the level's message");

	if (!text)
	{
		return;
	}

	// The game's loader takes message numbers 0 to 450 (strategy_chunk.cpp:26-34)
	text->message_string = 451;
	Check(opened.MarkChanged(message), "the message, set to 451, is marked changed");

	LevelSaveResult result = opened.SaveAs(path.c_str());
	Check(result == LevelSaveResult::GameWouldRefuse,
		Words("the save is refused, because the game's loader refuses the new file (level_file.cpp:515-530)", Said(result)));
	Check(Read(path) == original, "and the file the user picked is as it was");
	Check(NamesIn(room) == Names{ "level.rif" }, "and no .saving is left and no .bak was made: only the file is in the folder (level_file.cpp:527)");
	Check(Parent_File == opened.Level(), "and the library's \"file being loaded\" is this level again (level_file.cpp:523)");

	result = opened.SaveAs(path.c_str());
	Check(result == LevelSaveResult::GameWouldRefuse, Words("the same save tried again is refused the same way: the first left nothing in its way", Said(result)));

	text->message_string = 450;
	result = opened.SaveAs(path.c_str());
	Check(result == LevelSaveResult::Saved, Words("with the message set to 450, the largest the game takes, the save is made", Said(result)));
	Check(Read(path + ".bak") == original, "and only then is the .bak made, of the file as it was");

	OpenLevelFile again;
	Check(again.Open(path.c_str()) == LevelOpenResult::Opened && MessageOf(MessageChunkIn(again.Level())) &&
		MessageOf(MessageChunkIn(again.Level()))->message_string == 450, "and the saved file opens, with the message at 450");
}

// A ".saving" file that's already there isn't this save's: the save stops and leaves it alone
static void TestSavingFileInTheWay()
{
	std::string const room = Room("saving-in-the-way");
	std::string const path = room + "\\level.rif";
	Bytes const original = BaseLevel().Make();
	Bytes const mine{ 'm', 'i', 'n', 'e' };
	Make(path, original);

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened && opened.MarkChanged(NameIn(opened.Level())), "the level opens, and a chunk is marked changed");

	Make(path + ".saving", mine);
	LevelSaveResult result = opened.SaveAs(path.c_str());
	Check(result == LevelSaveResult::SavingFileInTheWay,
		Words("a .saving file already there stops the save: it's made only as a new file (level_file.cpp:499-504)", Said(result)));
	Check(Read(path + ".saving") == mine, "and the .saving file in the way is exactly as it was: never written over or deleted");
	Check(Read(path) == original && !IsThere(path + ".bak"), "and the file is as it was, and no .bak was made");

	DeleteFileA((path + ".saving").c_str());
	result = opened.SaveAs(path.c_str());
	Check(result == LevelSaveResult::Saved, Words("once it's gone, the save is made", Said(result)));
}

// A read-only file stops the save before anything is written
static void TestReadOnlyFile()
{
	std::string const room = Room("read-only");
	std::string const path = room + "\\level.rif";
	Bytes const original = BaseLevel().Make();
	Make(path, original);

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened && opened.MarkChanged(NameIn(opened.Level())), "the level opens, and a chunk is marked changed");

	SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_READONLY);
	LevelSaveResult const result = opened.SaveAs(path.c_str());
	SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);

	Check(result == LevelSaveResult::ReadOnly, Words("a file marked read-only stops the save (level_file.cpp:454-457)", Said(result)));
	Check(Read(path) == original && NamesIn(room) == Names{ "level.rif" }, "and the file is as it was, with no .saving and no .bak beside it");
}

// A file another program holds: the save stops, the file is as it was, and no .saving is left
static void TestFileHeldByAnotherProgram()
{
	std::string const room = Room("held");
	std::string const path = room + "\\level.rif";
	Bytes const original = BaseLevel().Make();
	Make(path, original);

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened && opened.MarkChanged(NameIn(opened.Level())), "the level opens, and a chunk is marked changed");

	// Held with nobody else let in: the .bak can't be copied from it
	HANDLE held = CreateFileA(path.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	Check(held != INVALID_HANDLE_VALUE, "another program holds the file with nobody else let in");
	LevelSaveResult result = opened.SaveAs(path.c_str());

	if (held != INVALID_HANDLE_VALUE)
	{
		CloseHandle(held);
	}

	Check(result == LevelSaveResult::CantReplace,
		Words("held with nobody else let in, the file can't be looked at as a plain file, so the save stops before anything is made "
			"(Drake's 5de1cdc: one rule for every file a save replaces)", Said(result)));
	Check(Read(path) == original && NamesIn(room) == Names{ "level.rif" }, "and the file is as it was, with no .saving and no .bak left beside it");

	// Held by a program that lets others read it but not delete or replace it: the .bak is made, the replace can't be
	held = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	Check(held != INVALID_HANDLE_VALUE, "another program holds the file, letting others only read it");
	result = opened.SaveAs(path.c_str());

	if (held != INVALID_HANDLE_VALUE)
	{
		CloseHandle(held);
	}

	Check(result == LevelSaveResult::CantReplace,
		Words("the new file can't be put in its place, so the save stops (level_file.cpp:557-563)", Said(result)));
	Check(Read(path) == original, "and the file is as it was");
	Check(!IsThere(path + ".saving"), "and no .saving is left (level_file.cpp:561)");
	Check(Read(path + ".bak") == original, "and the .bak made on the way is the file as it was");
}

// A folder at the path picked, and a level too big for the game, each stop the save before anything is written
static void TestFolderAndTooBig()
{
	std::string const room = Room("folder-and-too-big");
	std::string const path = room + "\\level.rif";
	std::string const folder = room + "\\a-folder";
	Make(path, BaseLevel().Make());
	CreateDirectoryA(folder.c_str(), 0);

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened, "the level opens");

	LevelSaveResult result = opened.SaveAs(folder.c_str());
	Check(result == LevelSaveResult::CantReplace, Words("a folder at the path picked stops the save (level_file.cpp:449-452)", Said(result)));
	Check(NamesIn(room) == Names{ "a-folder", "level.rif" } && NamesIn(folder).empty(), "and nothing was written: no .saving and no .bak");

	// A name as long as the largest level the game reads makes the level larger than that
	RIF_Name_Chunk * const name = NameIn(opened.Level());
	Check(name != 0, "the test finds the level's first name chunk");

	if (!name)
	{
		return;
	}

	size_t const length = (size_t)kLevelFileMostBytes;
	char * const longName = new char[length + 1];
	memset(longName, 'a', length);
	longName[length] = 0;
	delete[] name->rif_name;
	name->rif_name = longName;

	Check(opened.MarkChanged(name), "a name as long as the largest level file the game reads is marked changed");
	result = opened.SaveAs(path.c_str());
	Check(result == LevelSaveResult::CantBuild,
		Words("the level is now larger than any the game reads, so it isn't written (level_file.cpp:493-496)", Said(result)));
	Check(Read(path) == BaseLevel().Make() && NamesIn(room) == Names{ "a-folder", "level.rif" }, "and the file is as it was, with nothing beside it");
}

// Save As to a new name: no .bak (there was nothing to keep), the file opened from is left as it was, and the level then stands for
// the new file
static void TestSaveAsANewName()
{
	std::string const room = Room("new-name");
	std::string const path = room + "\\level.rif";
	std::string const newPath = room + "\\new.rif";
	Bytes const original = BaseLevel().Make();
	Make(path, original);

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened && opened.MarkChanged(NameIn(opened.Level())), "the level opens, and a chunk is marked changed");

	BaseLevel expected;
	expected.topLeftover = 0;

	LevelSaveResult result = opened.SaveAs(newPath.c_str());
	Bytes const first = AsIntengWritesIt(expected.Make());
	Check(result == LevelSaveResult::Saved && Read(newPath) == first, Words("a save as a new name writes the changed level there, as inteng writes it", Said(result)));
	Check(!IsThere(newPath + ".bak"), "and makes no .bak: there was no file to keep (level_file.cpp:534)");
	Check(Read(path) == original && !IsThere(path + ".bak"), "and leaves the file it was opened from as it was");
	Check(NamesIn(room) == Names{ "level.rif", "new.rif" }, "and leaves nothing else in the folder");

	result = opened.SaveAs(newPath.c_str());
	Check(result == LevelSaveResult::Saved && Read(newPath) == AsIntengWritesIt(first) && Read(newPath + ".bak") == first,
		Words("a save over the new file then prepares the tree again (version numbers up once more), and keeps the file as it was in its first .bak", Said(result)));
}

// After a save the level stands for the file just written: the next save builds on it
static void TestLaterSavesBuildOnEarlierOnes()
{
	std::string const room = Room("later-saves");
	std::string const path = room + "\\level.rif";
	Bytes const original = BaseLevel().Make();
	Make(path, original);

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened, "the level opens");

	RIF_Name_Chunk * const name = NameIn(opened.Level());
	AVP_Strategy_Chunk * const message = MessageChunkIn(opened.Level());
	TextMessageStrategy * const text = MessageOf(message);
	Check(name != 0 && text != 0, "the test finds the level's name and message");

	if (!name || !text)
	{
		return;
	}

	Rename(name, "q1");
	Check(opened.MarkChanged(name) && opened.SaveAs(path.c_str()) == LevelSaveResult::Saved, "a save with the name changed is made");

	text->message_string = 9;
	Check(opened.MarkChanged(message),
		"after it, a chunk can be marked again: the level was paired with the bytes just written (level_file.cpp:565-573)");
	Check(opened.SaveAs(path.c_str()) == LevelSaveResult::Saved, "a second save, with the message changed, is made");

	BaseLevel expected;
	expected.topName = "q1";
	expected.topLeftover = 0;
	expected.topMessage = 9;
	// each save prepares the tree it writes, so the file holds the level prepared twice, then three times after the next save
	Bytes const twice = AsIntengWritesIt(AsIntengWritesIt(expected.Make()));
	Check(Read(path) == twice, "and the file holds both changes: the first save's name is written back as the bytes it wrote (level_file.cpp:568)");

	Check(opened.SaveAs(path.c_str()) == LevelSaveResult::Saved && Read(path) == AsIntengWritesIt(twice),
		"a save with nothing changed after them builds on the file as the last save left it, not as it was opened, and prepares it once more (level_file.cpp:567)");
	Check(Read(path + ".bak") == original, "and the .bak is still the file as it was opened");

	OpenLevelFile again;
	Check(again.Open(path.c_str()) == LevelOpenResult::Opened && NameIs(NameIn(again.Level()), "q1") &&
		MessageOf(MessageChunkIn(again.Level())) && MessageOf(MessageChunkIn(again.Level()))->message_string == 9,
		"and the file opens again with the name q1 and the message 9");
}

// The file the game's loader reads has to be the one the editor read a moment before, and the new file has to be read back as written
static void TestTheLoaderReadsTheSameBytes()
{
	std::string const room = Room("same-bytes");
	std::string const path = room + "\\level.rif";
	std::string const other = room + "\\other.rif";
	Bytes const original = BaseLevel().Make();
	BaseLevel otherLevel;
	otherLevel.topMessage = 8;
	Make(path, original);
	Make(other, otherLevel.Make());

	// The two files are the same size and hold the same chunks, so only the loader's sum can tell them apart
	// Since L9 the loader reads the editor's own private copy of the bytes it read, not the file itself, so every file the loader asks
	// for is swapped: the loader's MD5 sum still has to catch that what it read isn't what the editor read
	OpenLevelFile opened;
	loaderAsksFor = "*";
	loaderIsGiven = other;
	LevelOpenResult const opening = opened.Open(path.c_str());
	loaderAsksFor.clear();
	loaderIsGiven.clear();

	Check(opening == LevelOpenResult::ChangedWhileOpening,
		Words("a file that isn't the same for the editor's read and the game's loader's is refused: the loader's MD5 sum differs (level_file.cpp:227-231)", Said(opening)));
	Check(opened.Level() == 0, "and no level is open after it");

	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened && opened.MarkChanged(NameIn(opened.Level())),
		"read the same both times, the file opens, and a chunk is marked changed");

	loaderAsksFor = "*";
	loaderIsGiven = other;
	LevelSaveResult const result = opened.SaveAs(path.c_str());
	loaderAsksFor.clear();
	loaderIsGiven.clear();

	Check(result == LevelSaveResult::CantWrite,
		Words("a new file that the game's loader doesn't read back as it was written stops the save (level_file.cpp:521-528)", Said(result)));
	Check(Read(path) == original && NamesIn(room) == Names{ "level.rif", "other.rif" }, "and the file is as it was, with no .saving and no .bak left");
}

// A chunk that holds nothing, and holders 40 deep, open and save; an edit at the bottom resizes every holder on the way out
static void TestEmptyChunksAndDeepLevels()
{
	std::string const room = Room("empty-and-deep");
	std::string const path = room + "\\empty.rif";
	Bytes const original = WholeLevel(Join({ NameBytes("ab", 0x55), MakeChunk("ZZEMPTYZ", Bytes()) }));
	Make(path, original);

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened, "a level with a chunk holding nothing, just its 12-byte heading, opens");
	Check(opened.MarkChanged(NameIn(opened.Level())), "and a chunk beside it is marked changed");

	Bytes built;
	Check(opened.BuildLevel(built) && built == WholeLevel(Join({ NameBytes("ab", 0), MakeChunk("ZZEMPTYZ", Bytes()) })),
		"and the level built keeps the empty chunk as it was");
	Check(opened.SaveAs(path.c_str()) == LevelSaveResult::Saved, "and the save is made");

	int const deep = 40;
	std::string const deepPath = room + "\\deep.rif";
	Make(deepPath, Nested(deep, NameBytes("ab", 0x55)));

	OpenLevelFile deepLevel;
	Check(deepLevel.Open(deepPath.c_str()) == LevelOpenResult::Opened, "a level with holders 40 deep opens");

	Chunk_With_Children * holder = deepLevel.Level();

	for (int i = 0; i < deep && holder; i++)
	{
		holder = HolderIn(holder);
	}

	RIF_Name_Chunk * const bottom = NameIn(holder);
	Check(bottom != 0, "the test finds the name at the bottom");

	if (!bottom)
	{
		return;
	}

	Rename(bottom, "abcdef");
	Check(deepLevel.MarkChanged(bottom), "the name at the bottom is marked changed");

	Bytes const wanted = Nested(deep, NameBytes("abcdef", 0));
	Check(deepLevel.BuildLevel(built) && built == wanted, "the level built has the new name, and every holder on the way out a new size");

	bool everySizeRight = built.size() == wanted.size();

	for (int k = 0; k < deep && everySizeRight; k++)
	{
		everySizeRight = NumberIs(built, 12 + 12 * (size_t)k + 8, 12 * (deep - k) + 20);
	}

	Check(everySizeRight, "each of the 40 holders gives its size 4 bytes larger, for the 4 bytes the name grew (level_file.cpp:338-341, 378-390)");
	Check(deepLevel.SaveAs(deepPath.c_str()) == LevelSaveResult::Saved, "and the save is made, and loads back");
}

// What opening refuses. Each one is refused, and leaves no level open.
static void TestWhatOpeningRefuses()
{
	std::string const room = Room("refused-at-open");

	struct Case
	{
		const char * file;
		Bytes bytes;
		LevelOpenResult wanted;
		const char * words;
	};

	std::vector<Case> cases;
	Bytes level;

	level = BaseLevel().Make();
	PutNumberAt(level, PlainAt + 8, 20);
	cases.push_back({ "past-the-end.rif", level, LevelOpenResult::LoaderRefused, "a chunk whose size runs past the end of the file is refused" });

	level = BaseLevel().Make();
	PutNumberAt(level, InnerMessageAt + 8, 48);
	cases.push_back({ "past-its-holder.rif", level, LevelOpenResult::LoaderRefused, "a chunk whose size runs past the end of the chunk holding it is refused" });

	level = BaseLevel().Make();
	PutNumberAt(level, PlainAt + 8, 8);
	cases.push_back({ "below-a-heading.rif", level, LevelOpenResult::LoaderRefused, "a chunk whose size is less than its own 12-byte heading is refused" });

	level = BaseLevel().Make();
	PutNumberAt(level, PlainAt + 8, 0);
	cases.push_back({ "size-zero.rif", level, LevelOpenResult::LoaderRefused, "a chunk whose size is 0 is refused" });

	level = BaseLevel().Make();
	PutNumberAt(level, PlainAt + 8, -16);
	cases.push_back({ "size-below-zero.rif", level, LevelOpenResult::LoaderRefused, "a chunk whose size is below 0 is refused" });

	level = BaseLevel().Make();
	PutNumberAt(level, 8, (int)BaseBytes + 4);
	cases.push_back({ "heading-too-big.rif", level, LevelOpenResult::LoaderRefused, "a level whose heading gives a size other than the file's is refused" });

	level = BaseLevel().Make();
	level.insert(level.end(), { 1, 2, 3, 4 });
	PutNumberAt(level, 8, (int)level.size());
	cases.push_back({ "scrap-at-the-end.rif", level, LevelOpenResult::LoaderRefused, "a level with 4 bytes after its last chunk, too few for a heading, is refused" });

	static const char zeroInside[8] = { 'R', 'I', 'F', 'F', 0, 'A', 'M', 'E' };
	level = WholeLevel(Join({ NameBytes("ab", 0x55), MakeChunk(zeroInside, Bytes{ 'x', 'y', 'z', 0x77 }) }));
	cases.push_back({ "zero-in-a-name.rif", level, LevelOpenResult::DoesntPair,
		"a chunk whose name has a zero byte inside it is refused: the library keeps only the letters before the zero, so it couldn't "
		"be written back as it was read (level_file.cpp:301)" });

	cases.push_back({ "too-deep.rif", Nested(70, NameBytes("ab", 0x55)), LevelOpenResult::LoaderRefused,
		"holders 70 deep, more than the game takes (64, avpx/3dc/win95/chunk.cpp:520), are refused" });

	cases.push_back({ "too-short.rif", Bytes{ 'R', 'E', 'B', 'I', 'N', 'F', 'F', '2' }, LevelOpenResult::CantRead,
		"a file of 8 bytes, too short to hold a level's heading, isn't read (level_file.cpp:79)" });

	// The package with only 4 bytes of its data left: it says it unpacks to 160 bytes, which 4 bytes can't hold
	Bytes cut = Packed(BaseLevel().Make());
	cut.resize(sizeof(HuffmanPackage) + 4);
	cases.push_back({ "packed-cut-short.rif", cut, LevelOpenResult::LoaderRefused,
		"a packed file cut short is refused: the game's checked unpacker can't unpack it (level_file.cpp:198-204)" });

	level = BaseLevel().Make();
	memcpy(level.data(), "XXXXXXXX", 8);
	cases.push_back({ "packed-not-a-level.rif", Packed(level), LevelOpenResult::DoesntPair,
		"a packed file whose level doesn't start with a level's heading is refused (the game's loader skips that heading unread) (level_file.cpp:235)" });

	cases.push_back({ "packed-too-short.rif", Packed(Bytes{ 'R', 'E', 'B', 'I', 'N', 'F', 'F', '2' }), LevelOpenResult::CantRead,
		"a packed file whose level is 8 bytes, shorter than a level's heading, is refused before it's unpacked: its package says less than "
		"a heading's 12 bytes (M1's size check)" });

	for (Case const & each : cases)
	{
		std::string const path = room + "\\" + each.file;
		Make(path, each.bytes);

		OpenLevelFile opened;
		LevelOpenResult const result = opened.Open(path.c_str());
		Check(result == each.wanted && opened.Level() == 0 && opened.SaveAs(path.c_str()) == LevelSaveResult::NothingOpen &&
			Read(path) == each.bytes, Words(each.words, Said(result)));
	}

	// One byte larger than the largest level file the game reads. Windows makes the file that long without writing its bytes.
	std::string const longPath = room + "\\too-long.rif";
	HANDLE const longFile = CreateFileA(longPath.c_str(), GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
	LARGE_INTEGER end;
	end.QuadPart = kLevelFileMostBytes + 1;
	bool const madeLong = longFile != INVALID_HANDLE_VALUE && SetFilePointerEx(longFile, end, 0, FILE_BEGIN) && SetEndOfFile(longFile);

	if (longFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(longFile);
	}

	OpenLevelFile opened;
	LevelOpenResult result = madeLong ? opened.Open(longPath.c_str()) : LevelOpenResult::Opened;
	Check(madeLong && result == LevelOpenResult::CantRead && opened.Level() == 0,
		Words("a file one byte larger than the largest level the game reads isn't read (level_file.cpp:79)", Said(result)));
	DeleteFileA(longPath.c_str());

	result = opened.Open(room.c_str());
	Check(result == LevelOpenResult::CantRead && opened.Level() == 0, Words("a folder can't be read as a level", Said(result)));
}

// No name from inside a level ever becomes a path: a save writes only the file picked, its .bak, and its own .saving on the way
static void TestNoNameFromTheLevel()
{
	std::string const room = Room("names-in-the-level");
	std::string const path = room + "\\level.rif";
	Make(path, WholeLevel(Join({ NameBytes("..\\made-by-the-level.rif", 0x55), NameBytes("made-by-the-level.rif", 0x55) })));

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened && opened.MarkChanged(NameIn(opened.Level())),
		"a level whose names look like file names opens, and a chunk is marked changed");
	Check(opened.SaveAs(path.c_str()) == LevelSaveResult::Saved, "the save is made");
	Check(NamesIn(room) == Names{ "level.rif", "level.rif.bak" } && !IsThere(testFolder + "\\made-by-the-level.rif"),
		"and the only files are the one picked and its .bak: no name from inside the level became a path (level_file.cpp:442-443)");
}

// The bytes of the top AVPSTRAT chunk's payload (message 7): 8 numbers, the message's number the sixth, at byte 20
static std::vector<unsigned char> MessagePayload(int message)
{
	Bytes const whole = MessageBytes(message);
	return std::vector<unsigned char>((unsigned char const *)whole.data() + 12, (unsigned char const *)whole.data() + whole.size());
}

// What PayloadOf says is in a chunk: the bytes after its heading, as they were read, until SetPayload puts others there
static void TestEditedPayload()
{
	std::string const room = Room("edited-payload");
	std::string const path = room + "\\level.rif";
	Make(path, BaseLevel().Make());

	OpenLevelFile opened;
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened, "the level opens");

	Chunk * const top = FirstNamed(opened.Level(), "AVPSTRAT");
	Chunk * const holder = FirstNamed(opened.Level(), "VIOBJECT");
	Chunk * const inner = MessageChunkIn(HolderIn(opened.Level()));
	RIF_Name_Chunk * const topName = NameIn(opened.Level());
	std::vector<unsigned char> payload;

	Check(top && holder && inner && topName, "the test finds the chunks it edits");

	if (!top || !holder || !inner || !topName)
	{
		return;
	}

	// PayloadOf: the bytes read, never the library's class
	Check(opened.PayloadOf(top, &payload) && payload == MessagePayload(7),
		"a chunk's payload is the 32 bytes after its heading as the file held them, with its message number, 7, at byte 20");
	Check(opened.PayloadOf(inner, &payload) && payload == MessagePayload(5), "so is a chunk inside a holder's");
	Check(!opened.PayloadOf(holder, &payload), "a chunk that holds other chunks has no payload of its own: it's refused");
	Check(!opened.PayloadOf(0, &payload), "no chunk at all is refused");
	bool refused = false;
	bool ran = Survives([&] { refused = !opened.PayloadOf(top, 0); });
	Check(ran && refused, "nowhere to put the payload is refused");
	Check(!opened.PayloadOf(opened.Level(), &payload), "the level itself is refused (it holds chunks)");

	// SetPayload refuses what isn't an edit of a field, and changes nothing
	std::vector<unsigned char> longer = MessagePayload(7);
	longer.push_back(0);
	std::vector<unsigned char> shorter = MessagePayload(7);
	shorter.pop_back();
	Check(!opened.SetPayload(top, longer) && !opened.SetPayload(top, shorter) && !opened.SetPayload(top, std::vector<unsigned char>()),
		"a payload of another size, one longer, one shorter or none at all, is refused");
	Check(!opened.SetPayload(holder, std::vector<unsigned char>(60, 0)) && !opened.SetPayload(opened.Level(), std::vector<unsigned char>(160, 0)),
		"a chunk that holds chunks, and the level, are refused even when the payload is as many bytes as they hold (60 and 160)");
	Check(!opened.SetPayload(0, MessagePayload(7)), "no chunk at all is refused");
	Check(!opened.HasChanges() && opened.PayloadOf(top, &payload) && payload == MessagePayload(7), "and none of those changed anything");

	OpenLevelFile other;
	Check(other.Open(path.c_str()) == LevelOpenResult::Opened && !other.SetPayload(top, MessagePayload(9)) && !other.PayloadOf(top, &payload),
		"a chunk of another open level is refused");

	// One edit
	Check(opened.SetPayload(top, MessagePayload(9)) && opened.HasChanges(), "the top message's payload, with 9 for its number, is put in, and the level has changes");
	Check(opened.PayloadOf(top, &payload) && payload == MessagePayload(9) && opened.PayloadOf(inner, &payload) && payload == MessagePayload(5),
		"it reads back as 9, and the other message as it was");

	BaseLevel expected;
	expected.topMessage = 9;
	Bytes built;
	Check(opened.BuildLevel(built) && built == expected.Make(),
		"the level built is the file's own bytes with only those four changed: the same size, every other chunk exactly as it was read");

	LevelSaveResult const saved = opened.SaveAs(path.c_str());
	Check(saved == LevelSaveResult::Saved && Read(path) == AsIntengWritesIt(expected.Make()),
		Words("the save writes that level as inteng writes it", Said(saved)));
	Check(!opened.HasChanges() && opened.PayloadOf(top, &payload) && payload == MessagePayload(9),
		"after the save nothing is changed, and the chunk holds 9 as the file's own bytes now");
	Check(!opened.MarkChanged(top) && !opened.HasChanges(),
		"and it can't be marked changed the library's way, even after the save: the library's class for it still says 7, and writing that would undo the edit");

	OpenLevelFile again;
	Check(again.Open(path.c_str()) == LevelOpenResult::Opened, "the saved file opens for editing again");
	Chunk * const afterOpen = FirstNamed(again.Level(), "AVPSTRAT");
	TextMessageStrategy * const message = MessageOf(dynamic_cast<AVP_Strategy_Chunk *>(afterOpen));
	Check(afterOpen && message && message->message_string == 9 && again.PayloadOf(afterOpen, &payload) && payload == MessagePayload(9),
		"and the game's own loader reads the edited number, 9, from it");

	// Two edits, one of them in a holder, and a chunk changed the library's way: all three are written
	Check(opened.SetPayload(inner, MessagePayload(8)) && opened.SetPayload(top, MessagePayload(6)), "the inner message and the top message are edited next");
	Check(topName && opened.MarkChanged(topName), "and a name chunk is marked changed, to be written the library's way");

	BaseLevel both;
	both.innerMessage = 8;
	both.topMessage = 6;
	both.topLeftover = 0;
	// the editor holds what the save wrote, which was prepared once, so the level built is the same edits on the prepared tree
	Check(opened.BuildLevel(built) && built == AsIntengWritesIt(both.Make()),
		"the level built has both edits, the name chunk's rounding byte a zero (the library's way), and nothing else changed from the prepared tree");

	// A second edit of the same chunk replaces the first
	Check(opened.SetPayload(top, MessagePayload(4)) && opened.PayloadOf(top, &payload) && payload == MessagePayload(4), "a second edit of the same chunk replaces the first");

	// A save the game's loader would refuse: SetPayload doesn't judge the bytes, the loader does, before anything is replaced
	Bytes const before = Read(path);
	Check(opened.SetPayload(top, MessagePayload(999999)), "a message number past the game's table, 999999, is a payload of the right size, so it's put in");
	LevelSaveResult const refusedSave = opened.SaveAs(path.c_str());
	Check(refusedSave == LevelSaveResult::GameWouldRefuse && Read(path) == before,
		Words("but the save is refused by the game's own loader, and the file is left as it was", Said(refusedSave)));
	Check(opened.HasChanges() && opened.SetPayload(top, MessagePayload(3)) && opened.SaveAs(path.c_str()) == LevelSaveResult::Saved,
		"the level still holds its changes, so putting a good number back saves it");

	// Closing forgets every edit
	Check(opened.SetPayload(top, MessagePayload(2)), "an edit is made once more");
	Check(opened.Open(path.c_str()) == LevelOpenResult::Opened && !opened.HasChanges() && opened.PayloadOf(FirstNamed(opened.Level(), "AVPSTRAT"), &payload) &&
		payload == MessagePayload(3), "opening the file again forgets it: the chunk holds what the file does");
}

int main()
{
	// The test's own folder is made in Windows' folder for temporary files, never in the repository: a take-out row that crashes the test
	// leaves it behind, and it mustn't be left in the tree
	char temporary[MAX_PATH + 1];
	DWORD const length = GetTempPathA(sizeof(temporary), temporary);
	if (length == 0 || length >= sizeof(temporary) - 32)
	{
		printf("  FAILED  the test couldn't find Windows' folder for temporary files\n");
		return 1;
	}
	char folder[MAX_PATH + 64];
	snprintf(folder, sizeof(folder), "%slevel_file_test_%lu", temporary, (unsigned long)GetCurrentProcessId());
	testFolder = folder;

	if (!CreateDirectoryA(testFolder.c_str(), 0))
	{
		printf("  FAILED  the test couldn't make its folder %s\n", testFolder.c_str());
		return 1;
	}

	TestTheTestsOwnLevel();
	TestNothingOpen();
	TestSaveWithNothingChanged();
	TestBackupIsMadeOnce();
	TestChangedChunkIsWrittenIntoZeros();
	TestHoldersGetNewSizes();
	TestWhatCanBeMarked();
	TestPackedLevel();
	TestSaveTheGameWouldRefuse();
	TestSavingFileInTheWay();
	TestReadOnlyFile();
	TestFileHeldByAnotherProgram();
	TestFolderAndTooBig();
	TestSaveAsANewName();
	TestLaterSavesBuildOnEarlierOnes();
	TestTheLoaderReadsTheSameBytes();
	TestEmptyChunksAndDeepLevels();
	TestWhatOpeningRefuses();
	TestNoNameFromTheLevel();
	TestEditedPayload();

	Check(writesAskedOfTheGame == 0,
		"nothing asked the game's own file call to write: every byte written was written by level_file.cpp, through its .saving file");

	RemoveFolder(testFolder);

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
