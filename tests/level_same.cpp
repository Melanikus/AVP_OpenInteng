// level_same.cpp: two level files compared chunk by chunk, every byte but the version counters every save moves on. What it does and why are
// at the top of level_same.h. Written 2026-11-17 by Melanikus (Bishop) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "level_same.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "chunk_gsprites.hpp"
#include "enum_chunk.hpp"
#include "environment_chunk.hpp"
#include "huffman.hpp"
#include "level_file_sum.h"
#include "sha256_stream.h"

namespace
{

// Every chunk starts with a heading of 12 bytes: an 8-letter name, then its size, which counts the heading too
size_t const kHeadingBytes = 12;

// The most a level may be unpacked: 32 MiB, as the editor opens (level_file.cpp, MostUnpackedBytes)
size_t const kMostUnpackedBytes = 32u << 20;

// The most chunks a level may have: 2^18, as the editor opens (level_file.cpp, MostChunks; the game's own levels have at most 15,935)
size_t const kMostChunks = 1u << 18;

// How deep chunks may sit inside chunks: the game's loader keeps a chunk past 64 as plain bytes and refuses the level (chunk.cpp,
// most_chunks_inside_chunks, line 520), so no level it loads is deeper; this walk asks it again for itself
int const kMostDepth = 64;

// The most different names of chunk one chunk may hold. Each name is looked up in the library once (lookup_child, a walk of all of that
// chunk's chunks), so a chunk of a great many names could make the walk take hours; a real level has a few dozen at most in one place.
size_t const kMostNamesInOneChunk = 256;

// THE GAME'S LOADER'S TIME. The loader adds each chunk it makes to the end of the chunks of the chunk that holds it by walking them all
// from the first (avpx 3dc/win95/chunk.cpp, Chunk::Chunk), so its time grows as the square of the chunks inside one chunk: 100,000 inside
// one took it 24 s, 200,000 over a minute, and a 32 MiB level of 12-byte chunks inside one would take it hours (measured 2026-11-18). So
// before the loader is given a level, what it could make of it is counted (LoaderCouldMake), and the level is refused when more than
// kMostInOneChunk chunks could sit inside one chunk, or more than kMostChunks in all: the loader then takes at most about 11 s a level
// (2^18 in all, as 16 chunks of 16,383; 1.7 s for one chunk of 16,384; measured 2026-11-18). The game's own 86 files hold at most 1,674
// inside one chunk (the top of Nost03_M) and could hold 16,145 in all by that count (Nost03_M, of its 15,935 real ones; counted 2026-11-18).
size_t const kMostInOneChunk = 1u << 14;

size_t const kNoCounter = (size_t)-1;

// THE VERSION COUNTERS every save moves on (each class's prepare_for_output() adds one), and where each is, counted from the start of what
// the chunk holds (after its 12-byte heading), as the library's own reader and writer have it in avpx (at the pin):
//   OBJHEAD1, Object_Header_Chunk: flags 4, lock user 16, place 12 (ChunkVectorInt), turn 16 (ChunkQuat), index 4, then version_no: 52
//            (3dc/win95/chunk_object.cpp: read at 441, written at 508, moved on at 517-520);
//   SHPHEAD1, Shape_Header_Chunk: flags 4, lock user 16, file_id_num 4, the counts of points and polygons 8, radius 4, the box 24, then
//            version_no: 60 (3dc/win95/chunk_shape.cpp: read at 1598, written at 1698, moved on at 1743, the end of prepare_for_output);
//   ENDTHEAD, Environment_Data_Header_Chunk: flags 4, lock user 16, then version_no: 20 (3dc/win95/environment_chunk.cpp: read at 104,
//            written at 137, moved on at 140-143);
//   ENUMHEAD, Enum_Header_Chunk: the same, 20 (3dc/win95/enum_chunk.cpp: read at 73, written at 106, moved on at 109-112);
//   ASPRHEAD, AllSprites_Header_Chunk: the same, 20 (3dc/win95/chunk_gsprites.cpp: read at 72, written at 105, moved on at 108-111).
// The kind is the class the game's loader made of the chunk, not its name alone: a chunk of one of these names somewhere the library
// doesn't make it that class is plain bytes to the library, and a save never moves it on. These five are every prepare_for_output() in the
// library that changes a number (the others: the shape morphing chunks number a sub-shape only when it has no number, -1, and
// Chunk_With_Children's prepares the chunks inside).
size_t CounterPlace(Chunk *chunk)
{
	if (dynamic_cast<Object_Header_Chunk *>(chunk))
	{
		return 52;
	}

	if (dynamic_cast<Shape_Header_Chunk *>(chunk))
	{
		return 60;
	}

	if (dynamic_cast<Environment_Data_Header_Chunk *>(chunk) || dynamic_cast<Enum_Header_Chunk *>(chunk) || dynamic_cast<AllSprites_Header_Chunk *>(chunk))
	{
		return 20;
	}

	return kNoCounter;
}

// One chunk of a level, from its bytes, with what the game's loader made of it
struct Node
{
	char name[8] = {};
	size_t at = 0;                 // where its heading starts in the level's bytes
	size_t bytes = 0;              // its size, the 12-byte heading included
	bool holder = false;           // the library holds chunks inside it: they are in inside, end to end
	size_t counterAt = kNoCounter; // where its version counter is, from the start of what it holds; kNoCounter for none
	std::vector<Node> inside;
};

struct Level
{
	std::string path;
	std::vector<char> image;  // the level unpacked: its own 12-byte heading (REBINFF2), then its chunks
	Node root;
	size_t chunks = 0;        // every chunk walked, the level's own heading not counted
};

// A chunk's name as text for a line: up to its first zero byte, anything but a printable letter shown as '?' (the name is the file's,
// so it is hostile: it is never a format, and holds no control characters when printed)
std::string NameText(char const name[8])
{
	std::string text;

	for (int n = 0; n < 8 && name[n]; n++)
	{
		text += (name[n] > 32 && name[n] < 127) ? name[n] : '?';
	}

	return text.empty() ? std::string("?") : text;
}

std::string Number(long long value)
{
	return std::to_string(value);
}

int IntAt(std::vector<char> const &bytes, size_t at)
{
	int value = 0;
	memcpy(&value, bytes.data() + at, sizeof(value));
	return value;
}

// Reads a whole file, of the sizes a level file may be (level_file_sum.h: 12 bytes to 32 MiB)
bool ReadWhole(char const *path, std::vector<char> *out, std::string *why)
{
	HANDLE const file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		*why = "it can't be opened (Windows error " + Number(GetLastError()) + ")";
		return false;
	}

	LARGE_INTEGER size;
	bool good = false;

	if (GetFileType(file) != FILE_TYPE_DISK)
	{
		*why = "it isn't a file on a disk";
	}
	else if (!GetFileSizeEx(file, &size))
	{
		*why = "its size can't be found";
	}
	else if (!LevelFile_SizeIsUsable(size.QuadPart))
	{
		*why = "it is " + Number(size.QuadPart) + " bytes: a level file is 12 bytes to 32 MiB";
	}
	else
	{
		out->assign((size_t)size.QuadPart, 0);
		DWORD got = 0;
		good = ReadFile(file, out->data(), (DWORD)out->size(), &got, 0) && got == out->size();

		if (!good)
		{
			*why = "it couldn't be read whole";
		}
	}

	CloseHandle(file);
	return good;
}

// The level in a file: the file itself, or what the game's own checked decoder unpacks from a packed one (huffman.cpp,
// HuffmanDecompressChecked: the one its loader uses), each size in the package's heading asked first
bool LevelInFile(std::vector<char> const &file, std::vector<char> *image, std::string *why)
{
	// (ReadWhole took only files of 12 bytes or more)
	if (memcmp(file.data(), COMPRESSED_RIF_IDENTIFIER, 8) == 0)
	{
		if (file.size() < sizeof(HuffmanPackage))
		{
			*why = "it is packed (REBCRIF1), but its " + Number(file.size()) + " bytes are fewer than a package's heading, " + Number(sizeof(HuffmanPackage));
			return false;
		}

		int packedBytes = 0, unpackedBytes = 0;
		memcpy(&packedBytes, file.data() + offsetof(HuffmanPackage, CompressedDataSize), sizeof(packedBytes));
		memcpy(&unpackedBytes, file.data() + offsetof(HuffmanPackage, UncompressedDataSize), sizeof(unpackedBytes));

		// The decoder reads zero bits past the end of a package (huffman.cpp), so a cut package still unpacks to something: the heading's own
		// count of packed bytes is what says it was cut. Every packed file the game comes with, and every one the game's packer makes (the
		// editor's saves), holds exactly that many after its heading (all 86 of the game's own files, counted 2026-11-17).
		if (packedBytes < 0 || (size_t)packedBytes != file.size() - sizeof(HuffmanPackage))
		{
			*why = "its package's heading says it holds " + Number(packedBytes) + " bytes of packed data, but the file has " +
				Number(file.size() - sizeof(HuffmanPackage)) + " after the heading";
			return false;
		}

		if (unpackedBytes < (int)kHeadingBytes || (size_t)unpackedBytes > kMostUnpackedBytes)
		{
			*why = "its package's heading says the level is " + Number(unpackedBytes) + " bytes unpacked: a level is 12 bytes to 32 MiB";
			return false;
		}

		// the decoder's memory is its own (malloc), given back on every way out
		std::unique_ptr<char, void (*)(void *)> const unpacked(HuffmanDecompressChecked((HuffmanPackage const *)file.data(), (int)file.size()), free);

		if (!unpacked)
		{
			*why = "the game's unpacker refuses its package";
			return false;
		}

		image->assign(unpacked.get(), unpacked.get() + unpackedBytes);
	}
	else if (memcmp(file.data(), "REBINFF2", 8) == 0)
	{
		*image = file;
	}
	else
	{
		*why = "it is not a level: its first 8 bytes are neither REBINFF2 nor REBCRIF1";
		return false;
	}

	if (memcmp(image->data(), "REBINFF2", 8) != 0)
	{
		*why = "it is not a level: what it unpacks to doesn't start with REBINFF2";
		return false;
	}

	int const size = IntAt(*image, 8);

	if (size < 0 || (size_t)size != image->size())
	{
		*why = "its level's size field says " + Number(size) + " bytes, but the level is " + Number(image->size());
		return false;
	}

	return true;
}

// The chunks end to end between from and to in the level's bytes, into out, each one's size asked against the bytes left before it is used.
// False, with why, when they don't fill the room exactly, or the level has more chunks than the editor opens.
bool Tile(Level &level, size_t from, size_t to, std::string const &path, std::vector<Node> *out, std::string *why)
{
	for (size_t at = from; at < to; )
	{
		if (to - at < kHeadingBytes)
		{
			*why = path + ": only " + Number(to - at) + " bytes are left at offset " + Number(at) + ", fewer than a chunk's 12-byte heading";
			return false;
		}

		Node chunk;
		memcpy(chunk.name, level.image.data() + at, 8);
		int const size = IntAt(level.image, at + 8);

		if (size < (int)kHeadingBytes || (size_t)size > to - at)
		{
			*why = path + ": the chunk " + NameText(chunk.name) + " at offset " + Number(at) + " says its size is " + Number(size) + " bytes, " +
				(size < (int)kHeadingBytes ? std::string("less than its own heading") : "which goes past the end of " + path + " (" + Number(to - at) + " bytes left)");
			return false;
		}

		if (++level.chunks > kMostChunks)
		{
			*why = "it has more than " + Number(kMostChunks) + " chunks, more than the editor opens";
			return false;
		}

		chunk.at = at;
		chunk.bytes = (size_t)size;
		out->push_back(chunk);
		at += (size_t)size;
	}

	return true;
}

// The most chunks the game's loader could make of what one chunk holds, from "from" to "to" in the level's bytes, counted into *all: the
// chunks it holds one after another from the start, up to the first that doesn't fit (as the loader makes them: chunk.hpp,
// ChildChunkFits), and the chunks each of those could hold in turn, as deep as the loader makes chunks inside chunks (kMostDepth; depth is
// the holder's own, 0 for the level). Which chunks hold chunks only the loader knows (its kinds), so here every chunk is taken to hold
// them: the count is never less than what the loader makes. holderAt is where the holder's heading is (0: the level's own). False, with
// why, past kMostInOneChunk inside one chunk or kMostChunks in all.
bool LoaderCouldMake(Level const &level, size_t holderAt, size_t from, size_t to, int depth, size_t *all, std::string *why)
{
	size_t inThis = 0;

	for (size_t at = from; to - at >= kHeadingBytes; )
	{
		int const size = IntAt(level.image, at + 8);

		if (size < (int)kHeadingBytes || (size_t)size > to - at)
		{
			break;
		}

		if (++inThis > kMostInOneChunk)
		{
			std::string const holder = holderAt == 0 ? std::string("the level itself") :
				"the chunk " + NameText(level.image.data() + holderAt) + " at offset " + Number(holderAt);
			*why = "it could hold more than " + Number(kMostInOneChunk) + " chunks inside one chunk (" + holder + "), before the game's loader is "
				"given it (the loader's time grows as the square of the chunks inside one chunk)";
			return false;
		}

		if (++*all > kMostChunks)
		{
			*why = "it could hold more than " + Number(kMostChunks) + " chunks in all, before the game's loader is given it";
			return false;
		}

		if (depth < kMostDepth && !LoaderCouldMake(level, at, at + kHeadingBytes, at + (size_t)size, depth + 1, all, why))
		{
			return false;
		}

		at += (size_t)size;
	}

	return true;
}

// Pairs the chunks of one holder (node->inside, tiled from the bytes) with the chunks the game's loader made of them, as the editor pairs a
// level it opens (level_file.cpp, OpenLevelFile::Pair): the library keeps a holder's chunks in the order it read them and gives back those of a
// name in that order, so the n-th chunk of a name in the bytes is the n-th the library holds by that name. Each one then says whether it holds
// chunks (a Chunk_With_Children that isn't the library's plain bytes, Miscellaneous_Chunk) and where its version counter is; a holder's own
// chunks are tiled and paired in turn, no deeper than kMostDepth.
bool Pair(Level &level, Chunk_With_Children *holder, Node *node, std::string const &path, int depth, std::string *why)
{
	std::map<std::string, std::vector<size_t> > byName;

	for (size_t n = 0; n < node->inside.size(); n++)
	{
		byName[std::string(node->inside[n].name, 8)].push_back(n);
	}

	if (byName.size() > kMostNamesInOneChunk)
	{
		*why = path + ": it holds chunks of " + Number(byName.size()) + " different names, more than " + Number(kMostNamesInOneChunk);
		return false;
	}

	std::vector<Chunk *> made(node->inside.size(), nullptr);

	for (auto const &name : byName)
	{
		// the library looks a name up as text, so up to its first zero byte
		char asText[9];
		memcpy(asText, name.first.data(), 8);
		asText[8] = 0;

		List<Chunk *> found;
		holder->lookup_child(asText, found);

		std::vector<Chunk *> inOrder;

		for (LIF<Chunk *> each(&found); !each.done(); each.next())
		{
			inOrder.push_back(each());
		}

		if (inOrder.size() != name.second.size())
		{
			*why = path + ": the game's loader holds " + Number(inOrder.size()) + " chunks named " + NameText(asText) + " here, the file " + Number(name.second.size());
			return false;
		}

		for (size_t n = 0; n < inOrder.size(); n++)
		{
			if (!inOrder[n] || memcmp(inOrder[n]->identifier, name.first.data(), 8) != 0)
			{
				*why = path + ": a chunk named " + NameText(asText) + " isn't named so in the game's loader, all 8 bytes";
				return false;
			}

			made[name.second[n]] = inOrder[n];
		}
	}

	for (size_t n = 0; n < node->inside.size(); n++)
	{
		Node &chunk = node->inside[n];
		Chunk *const library = made[n];

		if (!library)
		{
			*why = path + ": the chunk " + NameText(chunk.name) + " at offset " + Number(chunk.at) + " has no chunk of the game's loader";
			return false;
		}

		Chunk_With_Children *const inner = dynamic_cast<Chunk_With_Children *>(library);
		chunk.holder = inner != nullptr && !library->r_u_miscellaneous();
		chunk.counterAt = CounterPlace(library);

		if (!chunk.holder)
		{
			continue;
		}

		std::string const inside = path + "/" + NameText(chunk.name);

		if (depth + 1 > kMostDepth)
		{
			*why = inside + ": chunks sit inside chunks deeper than " + Number(kMostDepth);
			return false;
		}

		if (!Tile(level, chunk.at + kHeadingBytes, chunk.at + chunk.bytes, inside, &chunk.inside, why) ||
			!Pair(level, inner, &chunk, inside, depth + 1, why))
		{
			return false;
		}
	}

	return true;
}

// One level read whole: the file, its level unpacked, the level's own chunks tiled (before the loader sees it), then the game's loader on the
// same file and the pairing; the library's tree is gone again when this gives back. Why says which file, and why it is refused.
bool ReadLevel(char const *path, Level *level, std::string *why)
{
	level->path = path ? path : "";

	std::vector<char> file;

	if (!path || !path[0] || !ReadWhole(path, &file, why) || !LevelInFile(file, &level->image, why))
	{
		*why = level->path + ": " + (why->empty() ? std::string("no file is named") : *why);
		return false;
	}

	memcpy(level->root.name, "REBINFF2", 8);
	level->root.at = 0;
	level->root.bytes = level->image.size();
	level->root.holder = true;

	size_t couldMake = 0;

	if (!Tile(*level, kHeadingBytes, level->image.size(), "REBINFF2", &level->root.inside, why) ||
		!LoaderCouldMake(*level, 0, kHeadingBytes, level->image.size(), 0, &couldMake, why))
	{
		*why = level->path + ": " + *why;
		return false;
	}

	// The game's own loader, as the other verifiers use it, on the same file. It makes itself the library's "file being loaded" (Parent_File),
	// which is put back as it was once it has loaded.
	Chunk *const before = Parent_File;
	std::unique_ptr<File_Chunk> const loaded(new File_Chunk(path));
	Parent_File = before;

	if (loaded->error_code != 0)
	{
		*why = level->path + ": the game's loader refuses it (error code " + Number(loaded->error_code) + ")";
		return false;
	}

	// The loader read the very bytes compared here: its SHA-256 of what it read is theirs (level_file_sum.h), else the file changed between
	Sha256Stream stream;
	unsigned char sum[32];
	Sha256_Start(&stream);
	Sha256_Add(&stream, file.data(), file.size());
	Sha256_Finish(&stream, sum);

	if (loaded->file_sum.bytes != file.size() || memcmp(loaded->file_sum.sha256, sum, sizeof(sum)) != 0)
	{
		*why = level->path + ": it changed while it was read (the game's loader read other bytes)";
		return false;
	}

	if (!Pair(*level, loaded.get(), &level->root, "REBINFF2", 0, why))
	{
		*why = level->path + ": " + *why;
		return false;
	}

	return true;
}

struct Comparison
{
	Level const &first;
	Level const &second;
	std::vector<std::string> const &ignored;
	std::vector<std::string> const &dropped;
	LevelSameReport &report;

	void Difference(std::string const &line)
	{
		report.differences++;

		if (report.lines.size() < kLevelSameMostLines)
		{
			report.lines.push_back(line);
		}
	}

	static bool Named(Node const &chunk, std::vector<std::string> const &names)
	{
		std::string const name(chunk.name, strnlen(chunk.name, 8));

		for (std::string const &each : names)
		{
			if (each == name)
			{
				return true;
			}
		}

		return false;
	}

	bool Ignored(Node const &chunk) const
	{
		return Named(chunk, ignored);
	}

	// --drop: the chunks inside a holder without those of the names dropped (counted), in the file's order
	std::vector<Node const *> Kept(Node const &holder)
	{
		std::vector<Node const *> kept;

		for (Node const &chunk : holder.inside)
		{
			if (Named(chunk, dropped))
			{
				report.chunksDropped++;
			}
			else
			{
				kept.push_back(&chunk);
			}
		}

		return kept;
	}
};

// What a level holds at a place, for a line: the 4-byte number there (the size of most of a chunk's fields), as a number and in hex; or
// one byte, when 4 don't fit before the end of what the chunk holds
std::string ValueAt(char const *holds, size_t at, bool fourBytes)
{
	char text[48];

	if (fourBytes)
	{
		int value = 0;
		memcpy(&value, holds + at, 4);
		snprintf(text, sizeof(text), "%d (0x%08x)", value, (unsigned)value);
	}
	else
	{
		snprintf(text, sizeof(text), "byte 0x%02x", (unsigned)(unsigned char)holds[at]);
	}

	return text;
}

void CompareChunks(Comparison &c, Node const &a, Node const &b, std::string const &path);

// The chunks inside two holders of the same place, side by side in the files' order, those of a name dropped left out of both. A chunk's path
// names it by its name, with #n (from 1) when the holder has more than one of that name in the first file.
void CompareInside(Comparison &c, Node const &a, Node const &b, std::string const &path)
{
	std::map<std::string, size_t> ofName, seen;
	std::vector<Node const *> const inA = c.Kept(a), inB = c.Kept(b);

	for (Node const *chunk : inA)
	{
		ofName[std::string(chunk->name, 8)]++;
	}

	size_t const both = inA.size() < inB.size() ? inA.size() : inB.size();

	for (size_t n = 0; n < both; n++)
	{
		Node const &one = *inA[n], &other = *inB[n];

		if (memcmp(one.name, other.name, 8) != 0)
		{
			// from here on the two hold other chunks, or the same in another order: one line says where it starts
			c.Difference(path + ": chunk " + Number(n + 1) + " inside is " + NameText(one.name) + " in the first, " + NameText(other.name) +
				" in the second (the chunks inside differ from there on)");
			return;
		}

		std::string const key(one.name, 8);
		size_t const number = ++seen[key];
		CompareChunks(c, one, other, path + "/" + NameText(one.name) + (ofName[key] > 1 ? "#" + Number(number) : std::string()));
	}

	if (inA.size() != inB.size())
	{
		bool const firstHasMore = inA.size() > inB.size();
		Node const &extra = firstHasMore ? *inA[both] : *inB[both];
		c.Difference(path + ": " + Number(inA.size()) + " chunks inside in the first, " + Number(inB.size()) + " in the second (the first more is " +
			NameText(extra.name) + ", in the " + (firstHasMore ? "first" : "second") + ")");
	}
}

// Two chunks of the same name at the same place: a holder's chunks inside, or every byte a chunk holds, but for its version counter
void CompareChunks(Comparison &c, Node const &a, Node const &b, std::string const &path)
{
	if (c.Ignored(a))
	{
		c.report.chunksSkipped++;
		return;
	}

	if (a.holder != b.holder)
	{
		c.Difference(path + ": the game's loader holds chunks inside it in the " + (a.holder ? "first" : "second") + " file only");
		return;
	}

	if (a.holder)
	{
		CompareInside(c, a, b, path);
		return;
	}

	if (a.bytes != b.bytes)
	{
		c.Difference(path + ": " + Number(a.bytes) + " bytes in the first, " + Number(b.bytes) + " in the second (the 12-byte heading included)");
		return;
	}

	size_t const holds = a.bytes - kHeadingBytes;
	char const *const one = c.first.image.data() + a.at + kHeadingBytes;
	char const *const other = c.second.image.data() + b.at + kHeadingBytes;

	// the version counter, when both are that kind and it lies inside what the chunk holds (a short chunk has none)
	size_t const counter = (a.counterAt != kNoCounter && a.counterAt == b.counterAt && a.counterAt + 4 <= holds) ? a.counterAt : kNoCounter;

	if (counter != kNoCounter && memcmp(one + counter, other + counter, 4) != 0)
	{
		c.report.versionsIgnored++;
	}

	size_t firstAt = kNoCounter, differ = 0;

	for (size_t n = 0; n < holds; n++)
	{
		if (counter != kNoCounter && n >= counter && n < counter + 4)
		{
			continue;
		}

		if (one[n] != other[n])
		{
			if (firstAt == kNoCounter)
			{
				firstAt = n;
			}

			differ++;
		}
	}

	if (differ)
	{
		// the 4-byte field the first difference is in, when it fits (fields are 4 bytes from the start of what a chunk holds)
		size_t const word = firstAt & ~(size_t)3;
		bool const fits = word + 4 <= holds;
		size_t const at = fits ? word : firstAt;
		c.Difference(path + " offset " + Number(at) + ": " + ValueAt(one, at, fits) + " in the first, " + ValueAt(other, at, fits) + " in the second (" +
			Number(differ) + " of " + Number(holds) + " bytes differ)");
	}
}

} // namespace

LevelSameReport CompareLevelFiles(char const *first, char const *second, std::vector<std::string> const &ignored, std::vector<std::string> const &dropped)
{
	LevelSameReport report;

	try
	{
		Level a, b;
		std::string why;

		if (!ReadLevel(first, &a, &why) || !ReadLevel(second, &b, &why))
		{
			report.lines.push_back(why);
			return report;
		}

		report.chunks = a.chunks;
		Comparison c{ a, b, ignored, dropped, report };
		CompareInside(c, a.root, b.root, "REBINFF2");
		report.answer = report.differences ? LevelSameAnswer::Different : LevelSameAnswer::Same;
	}
	catch (std::bad_alloc const &)
	{
		report = LevelSameReport();
		report.lines.push_back("there isn't the memory to read the two levels");
	}

	return report;
}

int PrintLevelSameReport(LevelSameReport const &report)
{
	if (report.answer == LevelSameAnswer::Refused)
	{
		printf("REFUSED %s\n", report.lines.empty() ? "" : report.lines[0].c_str());
		return 2;
	}

	std::string const skipped = (report.chunksSkipped ? ", " + Number(report.chunksSkipped) + " chunks skipped by --ignore" : std::string()) +
		(report.chunksDropped ? ", " + Number(report.chunksDropped) + " chunks dropped by --drop (both files)" : std::string());

	if (report.answer == LevelSameAnswer::Same)
	{
		printf("SAME (%zu chunks, %zu version counters ignored%s)\n", report.chunks, report.versionsIgnored, skipped.c_str());
		return 0;
	}

	std::string const shown = report.differences > report.lines.size() ? ", the first " + Number(report.lines.size()) + " below" : std::string();
	printf("DIFFERENT (%zu difference%s%s; %zu chunks, %zu version counters ignored%s)\n", report.differences, report.differences == 1 ? "" : "s", shown.c_str(),
		report.chunks, report.versionsIgnored, skipped.c_str());

	for (std::string const &line : report.lines)
	{
		printf("  %s\n", line.c_str());
	}

	return 1;
}
