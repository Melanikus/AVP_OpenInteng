// The guard on the morph frames the game's loader makes from the game's own levels: Shape_Morphing_Frame_Data_Chunk::post_input_processing in avpx/3dc/win95/chunk_shape.cpp. Written
// 2026-11-17 by Melanikus (Bishop) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT TESTS. The game loads a level's morph frames (a door's or a lift's shapes) from its FRMMORPH chunks: the reader keeps the chunk's numbers, three for each frame, and
// post_input_processing turns them into the frames the game uses (anim_frames: the frame's two shapes, a sub-shape or the main shape, and its spare number; chunk_load.cpp reads them).
// For every FRMMORPH chunk of every level given, this checks that the frames the loader made are exactly the frames of THE READING AS IT WAS, a copy of post_input_processing at avpx
// 587c09f3 (ReadingAsItWas below): the same count, and each frame with the same two shapes and the same spare number. So a change to the loader can't change any of the game's own
// levels without this going red.
//
// THE FILE'S OWN BYTES are what every chunk is compared with: the level as it is in the file, unpacked by the game's checked unpacker (HuffmanDecompressChecked) when it is packed, and in
// it every FRMMORPH chunk's bytes, in the order the level holds them. Before any save, the chunk's own block (make_data_block_from_chunk: the numbers the reader kept, written by the chunk's
// writer) must be those bytes exactly, and the loader's frames must be those THE READING AS IT WAS gives from the file's numbers.
//
// And THE SAVE: the whole level is then prepared as the editor's save prepares it (prepare_for_output on the whole tree, src/io/level_file.cpp PrepareAsIntengDoes), and for every FRMMORPH
// this checks that a save writes the chunk exactly as the file holds it, every byte: the numbers at the places of the list a frame gives, and the file's own numbers at the places no frame
// reaches (where inteng's save left numbers it never set, which the game's levels hold); and that the game's reading of the written numbers gives the frames the loader made. So a change
// to the writer (prepare_for_output) can't change a level the editor saves untouched, or what the game reads from it.
//
// It also says, as notes (never a check), how many chunks THE READER'S ORDER (frame i at i*3, i*3+1, i*3+2, each shape looked for from the first sub-shape: ReadingInReadersOrder below)
// would give other frames for, and which. That is what a change of the reading would change in the game's own levels.
//
// HOW. Each level's file is read, then the level is read by the game's own loader (File_Chunk), and every chunk of it is visited. The FRMMORPH chunks the loader made are matched with the
// file's in order (the loader keeps a chunk's children in the order the file holds them), and there must be as many of each. Every list made with new[] in this program starts filled
// with 0xA5 bytes (operator new[] below), so a number a save never set reads as 0xA5A5A5A5 and can't match the file's by luck of what the memory held. The levels are only read, opened
// so other tests can read them at the same time. The last lines give the count of chunks, the frames by count, and one sum over every frame (its shapes by their place in the morph data,
// and its spare number), to compare runs by.
//
// HOW IT RUNS: tests/host_chunk_shape_stock_test.args builds it; run as host_chunk_shape_stock_test <level> [<level> ...] (tests/host_chunk_shape_stock_test.runargs: {LEVELS}).

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_shape.hpp"
#include "huffman.hpp"
#include "mishchnk.hpp"

static int checks = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checks++;

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Every list made with new[] starts as 0xA5 bytes, and goes back with delete[] (the plain new and delete stay the library's own), as in host_chunk_shape_test.
// The lists come from the process's own Windows heap and every delete[] gives its list back there once (GiveBack, HeapFree): nothing paired with the C
// library's malloc / free (as tests/level_file_test.cpp).

static unsigned char const kFill = 0xA5;

void *operator new[](size_t bytes)
{
	size_t const asked = bytes ? bytes : 1;
	void *const block = HeapAlloc(GetProcessHeap(), 0, asked);

	if (!block)
	{
		throw std::bad_alloc();
	}

	memset(block, kFill, asked);
	return block;
}

static void GiveBack(void *block) noexcept
{
	if (block)
	{
		HeapFree(GetProcessHeap(), 0, block);
	}
}

void operator delete[](void *block) noexcept
{
	GiveBack(block);
}

void operator delete[](void *block, size_t) noexcept
{
	GiveBack(block);
}

// ---- Stand-ins for the parts of the game the chunk code calls (as host_strategy_stock_roundtrip_test). The game's loader opens a level with no sharing (mishchnk.cpp: share 0), so two
// tests reading one stock level at the same time would refuse each other; an open for reading only is shared for reading here, which changes nothing the loader reads.

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (!(access & (GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE)))
	{
		share |= FILE_SHARE_READ;
	}

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

// ---- Visiting every chunk. A chunk's first child (Chunk_With_Children::children) and its next sibling (Chunk::next) are the chunk library's own; C++ lets an explicit instantiation
// name them, so these two give this test read access to them without changing the library.

template <typename Tag, typename Tag::type Member>
struct Reach
{
	friend typename Tag::type Get(Tag)
	{
		return Member;
	}
};

struct FirstChild
{
	typedef Chunk *Chunk_With_Children::*type;
	friend type Get(FirstChild);
};

struct NextChunk
{
	typedef Chunk *Chunk::*type;
	friend type Get(NextChunk);
};

template struct Reach<FirstChild, &Chunk_With_Children::children>;
template struct Reach<NextChunk, &Chunk::next>;

struct Found
{
	Shape_Morphing_Data_Chunk *morph;          // the SHPMORPH it is in
	Shape_Morphing_Frame_Data_Chunk *frames;
	size_t place;                              // which FRMMORPH of the level it is, counting every one (plain ones too) in the order the level holds them
};

static int plainFrameChunks = 0;   // FRMMORPH chunks the loader kept as plain bytes (not in a SHPMORPH): the game never reads frames from those

// Each chunk before its children, and the children in the loader's order, which is the file's (Chunk::Chunk adds a child at the end of its parent's list): so the FRMMORPH chunks are
// met in the order the level holds them. seen counts every one met.
static void Visit(Chunk *chunk, Chunk_With_Children *parent, std::vector<Found> *found, size_t *seen, int depth)
{
	if (!strncmp(chunk->identifier, "FRMMORPH", 8))
	{
		Shape_Morphing_Frame_Data_Chunk *const frames = dynamic_cast<Shape_Morphing_Frame_Data_Chunk *>(chunk);
		Shape_Morphing_Data_Chunk *const morph = dynamic_cast<Shape_Morphing_Data_Chunk *>(parent);

		if (frames && morph)
		{
			found->push_back(Found { morph, frames, *seen });
		}
		else
		{
			plainFrameChunks++;
		}

		(*seen)++;
	}

	Chunk_With_Children *const holder = dynamic_cast<Chunk_With_Children *>(chunk);

	if (!holder || depth > 64)
	{
		return;
	}

	for (Chunk *child = holder->*Get(FirstChild()); child; child = child->*Get(NextChunk()))
	{
		Visit(child, holder, found, seen, depth + 1);
	}
}

// ---- The file's own bytes

static bool ReadWholeFile(char const *path, std::vector<char> *out)
{
	HANDLE const file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

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

// The level inside a file: the file itself, or what the game's checked unpacker makes of a packed one (empty if it can't), as host_level_same_test's LevelIn
static std::vector<char> LevelIn(std::vector<char> const &file)
{
	if (file.size() < sizeof(HuffmanPackage) || memcmp(file.data(), COMPRESSED_RIF_IDENTIFIER, 8) != 0)
	{
		return file;
	}

	std::vector<char> level;
	char *const unpacked = HuffmanDecompressChecked((HuffmanPackage const *)file.data(), (int)file.size());

	if (unpacked)
	{
		int size;
		memcpy(&size, file.data() + offsetof(HuffmanPackage, UncompressedDataSize), sizeof(size));
		level.assign(unpacked, unpacked + size);
		free(unpacked);
	}

	return level;
}

// Every FRMMORPH chunk in a level's bytes, in the order the level holds them: the 8 letters FRMMORPH, and the chunk's bytes from there, as many as its size says. Empty for one whose size
// doesn't hold its count's numbers (24 bytes and 12 for each frame) or doesn't fit in the level. The caller checks there are as many as the loader made, so the letters inside some other
// chunk's data can't be taken for a chunk unnoticed.
static std::vector<std::vector<char>> FrameChunksIn(std::vector<char> const &level)
{
	std::vector<std::vector<char>> found;

	for (size_t at = 0; at + 24 <= level.size(); at++)
	{
		if (level[at] != 'F' || memcmp(level.data() + at, "FRMMORPH", 8) != 0)
		{
			continue;
		}

		int32_t size = 0, count = 0;
		memcpy(&size, level.data() + at + 8, 4);
		memcpy(&count, level.data() + at + 20, 4);

		if (count < 0 || count > 32767 || size != 24 + 12 * count || (size_t)size > level.size() - at)
		{
			found.push_back(std::vector<char>());
			continue;
		}

		found.push_back(std::vector<char>(level.begin() + at, level.begin() + at + size));
	}

	return found;
}

// ---- The two readings

// A frame as the loader keeps it (chunk_type.hpp a_frame)
struct Frame
{
	Shape_Sub_Shape_Chunk *shape1a;
	Shape_Chunk *shape1b;
	Shape_Sub_Shape_Chunk *shape2a;
	Shape_Chunk *shape2b;
	int spare;
};

static bool Same(std::vector<Frame> const &a, std::vector<Frame> const &b)
{
	if (a.size() != b.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.size(); i++)
	{
		if (a[i].shape1a != b[i].shape1a || a[i].shape1b != b[i].shape1b || a[i].shape2a != b[i].shape2a || a[i].shape2b != b[i].shape2b || a[i].spare != b[i].spare)
		{
			return false;
		}
	}

	return true;
}

// The morph data's sub-shapes, in its own order (as post_input_processing lists them)
static std::vector<Shape_Sub_Shape_Chunk *> SubShapesOf(Shape_Morphing_Data_Chunk *morph)
{
	std::vector<Shape_Sub_Shape_Chunk *> list;
	List<Chunk *> children;

	morph->lookup_child("SUBSHAPE", children);

	for (LIF<Chunk *> each(&children); !each.done(); each.next())
	{
		list.push_back((Shape_Sub_Shape_Chunk *)each());
	}

	return list;
}

static bool HasNumber(Shape_Sub_Shape_Chunk *shape, int number)
{
	return shape->get_header() && shape->get_header()->file_id_num == number;
}

static void Keep(std::vector<Frame> *frames, Shape_Sub_Shape_Chunk *sh1a, Shape_Chunk *sh1b, Shape_Sub_Shape_Chunk *sh2a, Shape_Chunk *sh2b, int spare)
{
	if ((sh1a || sh1b) && (sh2a || sh2b))
	{
		Frame frame = { 0, 0, 0, 0, spare };

		if (sh1a)
			frame.shape1a = sh1a;
		else if (sh1b)
			frame.shape1b = sh1b;

		if (sh2a)
			frame.shape2a = sh2a;
		else if (sh2b)
			frame.shape2b = sh2b;

		frames->push_back(frame);
	}
}

// THE READING AS IT WAS: a copy of Shape_Morphing_Frame_Data_Chunk::post_input_processing at avpx 587c09f3, line for line. It reads frame i's numbers at i*2, i, i*2+1, i+1 and i+2, and
// its first-shape search goes on from where the last search stopped (only the second-shape search starts again from the first sub-shape).
static std::vector<Frame> ReadingAsItWas(std::vector<int32_t> const &frame_store, int num_frames, Shape_Morphing_Data_Chunk *pchnk)
{
	std::vector<Shape_Sub_Shape_Chunk *> const shplist = SubShapesOf(pchnk);
	std::vector<Frame> frames;
	size_t sl = 0;

	for (int i = 0; i < num_frames; i++)
	{
		Shape_Chunk *sh1b = 0, *sh2b = 0;
		Shape_Sub_Shape_Chunk *sh1a = 0, *sh2a = 0;

		if (frame_store[i * 2] == -1)
		{
			sh1b = pchnk->parent_shape;
		}
		else
		{
			for (; sl < shplist.size(); sl++)
			{
				if (HasNumber(shplist[sl], frame_store[i]))
					break;
			}

			if (sl < shplist.size())
			{
				sh1a = shplist[sl];
			}
		}

		if (frame_store[i * 2 + 1] == -1)
		{
			sh2b = pchnk->parent_shape;
		}
		else
		{
			for (sl = 0; sl < shplist.size(); sl++)
			{
				if (HasNumber(shplist[sl], frame_store[i + 1]))
					break;
			}

			if (sl < shplist.size())
			{
				sh2a = shplist[sl];
			}
		}

		Keep(&frames, sh1a, sh1b, sh2a, sh2b, frame_store[i + 2]);
	}

	return frames;
}

// THE READER'S ORDER: frame i's numbers at i*3 (first shape), i*3+1 (second shape) and i*3+2 (spare), as the chunk's reader keeps them, each shape looked for from the first sub-shape
static std::vector<Frame> ReadingInReadersOrder(std::vector<int32_t> const &frame_store, int num_frames, Shape_Morphing_Data_Chunk *pchnk)
{
	std::vector<Shape_Sub_Shape_Chunk *> const shplist = SubShapesOf(pchnk);
	std::vector<Frame> frames;

	for (int i = 0; i < num_frames; i++)
	{
		Shape_Chunk *sh1b = 0, *sh2b = 0;
		Shape_Sub_Shape_Chunk *sh1a = 0, *sh2a = 0;

		for (int which = 0; which < 2; which++)
		{
			int32_t const number = frame_store[i * 3 + which];
			Shape_Sub_Shape_Chunk **const sub = which ? &sh2a : &sh1a;
			Shape_Chunk **const main = which ? &sh2b : &sh1b;

			if (number == -1)
			{
				*main = pchnk->parent_shape;
				continue;
			}

			for (size_t s = 0; s < shplist.size(); s++)
			{
				if (HasNumber(shplist[s], number))
				{
					*sub = shplist[s];
					break;
				}
			}
		}

		Keep(&frames, sh1a, sh1b, sh2a, sh2b, frame_store[i * 3 + 2]);
	}

	return frames;
}

// The frames the game's loader made
static std::vector<Frame> LoadersFrames(Shape_Morphing_Frame_Data_Chunk *chunk)
{
	std::vector<Frame> frames;

	for (LIF<a_frame *> each(&chunk->anim_frames); !each.done(); each.next())
	{
		frames.push_back(Frame { each()->shape1a, each()->shape1b, each()->shape2a, each()->shape2b, each()->spare });
	}

	return frames;
}

// ---- Words and the sum

// A shape in words: "sub-shape <place> (number <n>)", "the main shape", or "none"
static std::string ShapeWords(Shape_Sub_Shape_Chunk *sub, Shape_Chunk *main, Shape_Morphing_Data_Chunk *morph, std::vector<Shape_Sub_Shape_Chunk *> const &list)
{
	if (sub)
	{
		for (size_t s = 0; s < list.size(); s++)
		{
			if (list[s] == sub)
			{
				return "sub-shape " + std::to_string(s) + " (number " + std::to_string(sub->get_header() ? sub->get_header()->file_id_num : -999) + ")";
			}
		}

		return "a sub-shape not in this morph data";
	}

	if (main)
	{
		return main == morph->parent_shape ? "the main shape" : "another shape";
	}

	return "none";
}

static std::string FramesWords(std::vector<Frame> const &frames, Shape_Morphing_Data_Chunk *morph)
{
	std::vector<Shape_Sub_Shape_Chunk *> const list = SubShapesOf(morph);
	std::string words = std::to_string(frames.size()) + (frames.size() == 1 ? " frame" : " frames");

	for (size_t i = 0; i < frames.size(); i++)
	{
		words += (i ? "; " : ": ") + ShapeWords(frames[i].shape1a, frames[i].shape1b, morph, list) + " to " + ShapeWords(frames[i].shape2a, frames[i].shape2b, morph, list) +
			", spare " + std::to_string(frames[i].spare);
	}

	return words;
}

static uint64_t sum = 1469598103934665603ull;   // FNV-1a over every frame of every level, in order

static void AddToSum(int32_t value)
{
	for (int b = 0; b < 4; b++)
	{
		sum ^= (uint8_t)(value >> (b * 8));
		sum *= 1099511628211ull;
	}
}

static int32_t ShapeCode(Shape_Sub_Shape_Chunk *sub, Shape_Chunk *main, Shape_Morphing_Data_Chunk *morph, std::vector<Shape_Sub_Shape_Chunk *> const &list)
{
	if (sub)
	{
		for (size_t s = 0; s < list.size(); s++)
		{
			if (list[s] == sub)
			{
				return (int32_t)s;
			}
		}

		return -4;
	}

	return main ? (main == morph->parent_shape ? -1 : -2) : -3;
}

// ---- The test

// The block a FRMMORPH writes (make_data_block_from_chunk), every byte: false, with why, when there is no block or its size doesn't hold its count's numbers
static bool BlockOf(Shape_Morphing_Frame_Data_Chunk *chunk, std::vector<char> *bytes, std::string *why)
{
	bytes->clear();

	char *const block = chunk->make_data_block_from_chunk();

	if (!block)
	{
		*why = "made no block";
		return false;
	}

	int32_t size = 0, count = 0;
	memcpy(&size, block + 8, 4);
	memcpy(&count, block + 20, 4);

	if (count < 0 || count > 32767 || size != 24 + 12 * count)
	{
		*why = "made a block that doesn't hold its count's numbers (size " + std::to_string(size) + ", count " + std::to_string(count) + ")";
		delete[] block;
		return false;
	}

	bytes->assign(block, block + size);
	delete[] block;
	return true;
}

// A FRMMORPH's list of numbers, three for each frame, from the chunk's bytes (BlockOf's, or the file's)
// The 32-bit number at this place of the bytes, lowest byte first, read a byte at a time (0 past the end): numbers, never text (Snyk Code took a memcpy of
// a chunk's numbers into an array for an unterminated text, as in 20558ba8)
static int32_t NumberAt(std::vector<char> const &bytes, size_t at)
{
	if (at > bytes.size() || bytes.size() - at < 4)
	{
		return 0;
	}

	uint32_t value = 0;

	for (size_t b = 4; b > 0; b--)
	{
		value = (value << 8) | (unsigned char)bytes[at + b - 1];
	}

	return (int32_t)value;
}

static std::vector<int32_t> ListIn(std::vector<char> const &bytes)
{
	std::vector<int32_t> numbers;

	if (bytes.size() >= 24)
	{
		numbers.resize((bytes.size() - 24) / 4);

		if (!numbers.empty())
		{
			memcpy(numbers.data(), bytes.data() + 24, numbers.size() * 4);
		}
	}

	return numbers;
}

static std::string ListWords(std::vector<int32_t> const &numbers)
{
	std::string words;

	for (size_t n = 0; n < numbers.size(); n++)
	{
		words += (n ? " " : "") + std::to_string(numbers[n]);
	}

	return words;
}

static int notGiven = 0;       // places of the lists no frame gives (a save must write the file's own number there too)

static int allChunks = 0;
static int framesByCount[4] = { 0, 0, 0, 0 };   // chunks with 0, 1, 2, and 3 or more numbered frames
static int differInReadersOrder = 0;
static int levelsWithMorphs = 0;

static std::string NameOf(char const *path)
{
	char const *name = path;

	for (char const *at = path; *at; at++)
	{
		if (*at == '/' || *at == '\\')
		{
			name = at + 1;
		}
	}

	return name;
}

static void TestLevel(char const *path)
{
	std::string const name = NameOf(path);

	// The file's own bytes, read before the loader opens it
	std::vector<char> file;

	if (!ReadWholeFile(path, &file))
	{
		Check(false, name + ": the file is read");
		return;
	}

	std::vector<std::vector<char>> const fileChunks = FrameChunksIn(LevelIn(file));

	File_Chunk *const level = new File_Chunk(path);

	if (level->error_code != 0)
	{
		Check(false, name + ": the game's loader takes it (error " + std::to_string(level->error_code) + ")");
		delete level;
		return;
	}

	std::vector<Found> found;
	size_t seen = 0;
	Visit(level, 0, &found, &seen, 0);

	// The loader's FRMMORPH chunks are matched with the file's in order, so there must be as many of each
	if (fileChunks.size() != seen)
	{
		Check(false, name + ": the level's bytes hold " + std::to_string(fileChunks.size()) + " FRMMORPH chunks and the loader made " + std::to_string(seen) +
			", so they can't be matched");
		delete level;
		return;
	}

	int same = 0;
	int bad = 0;

	// What each chunk held in the file, for the save below, and the frames the loader made from it (not usable when the chunk's bytes or block were refused)
	std::vector<std::vector<Frame>> loadersFrames(found.size());
	std::vector<bool> usable(found.size(), false);

	for (size_t k = 0; k < found.size(); k++)
	{
		Shape_Morphing_Frame_Data_Chunk *const chunk = found[k].frames;
		Shape_Morphing_Data_Chunk *const morph = found[k].morph;
		std::vector<char> const &fileBytes = fileChunks[found[k].place];

		if (fileBytes.empty())
		{
			Check(false, name + ": FRMMORPH " + std::to_string(k) + ": the level's bytes there are not a whole chunk that holds its count's numbers");
			bad++;
			continue;
		}

		// The numbers the reader kept, as the chunk writes them before any prepare_for_output: they must be the file's own bytes
		std::vector<char> readersBytes;
		std::string why;

		if (!BlockOf(chunk, &readersBytes, &why))
		{
			Check(false, name + ": FRMMORPH " + std::to_string(k) + " " + why);
			bad++;
			continue;
		}

		std::vector<int32_t> const numbers = ListIn(fileBytes);

		if (readersBytes != fileBytes)
		{
			std::vector<int32_t> const kept = ListIn(readersBytes);
			int32_t fileHead[3] = { 0, 0, 0 }, keptHead[3] = { 0, 0, 0 };

			// the flags, speed and count after the chunk's 12-byte head, a number at a time
			for (size_t n = 0; n < 3; n++)
			{
				fileHead[n] = NumberAt(fileBytes, 12 + 4 * n);
				keptHead[n] = readersBytes.size() >= 24 ? NumberAt(readersBytes, 12 + 4 * n) : 0;
			}

			Check(false, name + ": FRMMORPH " + std::to_string(k) + ": before any save the chunk writes other bytes than the file holds: flags, speed, count " +
				std::to_string(keptHead[0]) + " " + std::to_string(keptHead[1]) + " " + std::to_string(keptHead[2]) + " and the list " + ListWords(kept) + " (the file has " +
				std::to_string(fileHead[0]) + " " + std::to_string(fileHead[1]) + " " + std::to_string(fileHead[2]) + " and " + ListWords(numbers) + ")");
			bad++;
		}

		int const count = (int)(numbers.size() / 3);
		std::vector<Frame> const loaders = LoadersFrames(chunk);

		loadersFrames[k] = loaders;
		usable[k] = true;
		std::vector<Frame> const asItWas = ReadingAsItWas(numbers, count, morph);
		std::vector<Frame> const readersOrder = ReadingInReadersOrder(numbers, count, morph);
		std::vector<Shape_Sub_Shape_Chunk *> const list = SubShapesOf(morph);

		allChunks++;
		framesByCount[count < 3 ? count : 3]++;

		if (Same(loaders, asItWas))
		{
			same++;
		}
		else
		{
			bad++;
			Check(false, name + ": FRMMORPH " + std::to_string(k) + ": the loader made " + FramesWords(loaders, morph) + "; the reading as it was gives " + FramesWords(asItWas, morph));
		}

		if (!Same(readersOrder, asItWas))
		{
			differInReadersOrder++;

			printf("  note  %s FRMMORPH %d (numbers %s; %d sub-shapes): the reading as it was gives %s; the reader's order would give %s\n", name.c_str(), (int)k,
				ListWords(numbers).c_str(), (int)list.size(), FramesWords(asItWas, morph).c_str(), FramesWords(readersOrder, morph).c_str());
		}

		AddToSum((int32_t)loaders.size());

		for (size_t f = 0; f < loaders.size(); f++)
		{
			AddToSum(ShapeCode(loaders[f].shape1a, loaders[f].shape1b, morph, list));
			AddToSum(ShapeCode(loaders[f].shape2a, loaders[f].shape2b, morph, list));
			AddToSum(loaders[f].spare);
		}
	}

	if (!found.empty())
	{
		levelsWithMorphs++;
	}

	if (bad == 0)
	{
		Check(true, name + ": all " + std::to_string(found.size()) + " FRMMORPH chunks write the file's own bytes before a save, and give the frames the reading as it was gives from the "
			"file's numbers");
	}

	// THE SAVE, as the editor's (src/io/level_file.cpp PrepareAsIntengDoes, as inteng's File_Chunk::write_file): the whole tree is prepared, then each FRMMORPH's block is made. The game's
	// reading of the written numbers must give the frames the loader made from the file, spare numbers too, and the block written must be the file's own, every byte: at the places of the
	// list a frame gives (frame k reaches k, k+1 and k+2, so the first count+2 places) and at the places no frame reaches.
	level->prepare_for_output();

	int saveBad = 0;
	int saveChecked = 0;

	for (size_t k = 0; k < found.size(); k++)
	{
		if (!usable[k])
		{
			continue;
		}

		std::vector<char> const &fileBytes = fileChunks[found[k].place];
		std::vector<char> savedBytes;
		std::string why;

		if (!BlockOf(found[k].frames, &savedBytes, &why))
		{
			Check(false, name + ": FRMMORPH " + std::to_string(k) + " after the save's prepare_for_output: " + why);
			saveBad++;
			continue;
		}

		std::vector<int32_t> const saved = ListIn(savedBytes);
		int const count = (int)(saved.size() / 3);
		size_t const given = count ? (size_t)count + 2 : 0;
		std::vector<Frame> const readAgain = ReadingAsItWas(saved, count, found[k].morph);
		bool const framesSame = Same(readAgain, loadersFrames[k]);
		bool const bytesSame = savedBytes == fileBytes;

		if (saved.size() > given)
		{
			notGiven += (int)(saved.size() - given);
		}

		saveChecked++;

		if (!framesSame || !bytesSame)
		{
			saveBad++;
			Check(false, name + ": FRMMORPH " + std::to_string(k) + ": a save writes " + ListWords(saved) + (bytesSame ? "" : ", not the file's bytes") + " (the file has " +
				ListWords(ListIn(fileBytes)) + "); the game's reading of it gives " + FramesWords(readAgain, found[k].morph) + "; the loader made " +
				FramesWords(loadersFrames[k], found[k].morph));
		}
	}

	if (saveBad == 0)
	{
		Check(true, name + ": a save of all " + std::to_string(saveChecked) + " FRMMORPH chunks writes each as the file holds it, every byte, and the game's reading of them gives the same "
			"frames");
	}

	delete level;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("usage: host_chunk_shape_stock_test <level> [<level> ...]: no level was named, so nothing was tested\n");
		return 1;
	}

	for (int i = 1; i < argc; i++)
	{
		TestLevel(argv[i]);
	}

	Check(allChunks > 0, "the levels hold FRMMORPH chunks to test (" + std::to_string(allChunks) + " in " + std::to_string(levelsWithMorphs) + " of " + std::to_string(argc - 1) + " levels)");

	printf("  note  %d FRMMORPH chunks: %d with no frames, %d with 1, %d with 2, %d with 3 or more; %d kept as plain bytes\n", allChunks, framesByCount[0], framesByCount[1], framesByCount[2],
		framesByCount[3], plainFrameChunks);
	printf("  note  the reader's order would give other frames for %d of them\n", differInReadersOrder);
	printf("  note  %d places of the lists are ones no frame gives (each checked above: a save writes the file's own number there)\n", notGiven);
	printf("  note  the sum of every frame the loader made: %016llx\n", (unsigned long long)sum);
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
