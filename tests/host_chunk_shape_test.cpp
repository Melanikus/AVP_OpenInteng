// The test of how a level's morph frames are written: the FRMMORPH chunk, Shape_Morphing_Frame_Data_Chunk in avpx/3dc/win95/chunk_shape.cpp. Written 2026-11-17 by Melanikus (Bishop)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT TESTS. A FRMMORPH chunk holds three numbers (flags, speed, the count of frames) and then a list of three numbers for each frame. Before a save, prepare_for_output makes that list
// from the frames the level holds, and fill_data_block writes all of it. A shape's number is its sub-shape's file_id_num, or -1 for the main shape.
//
// THE LAYOUT IS THE ONE THE GAME READS. prepare_for_output puts frame k's first shape, second shape and spare number at k, k+1 and k+2 of the list (each frame over the one before it), and
// the game's loader reads it that way (post_input_processing finds frame i's shapes at i and i+1). The game's own levels hold it that way too: each of their 47 FRMMORPH chunks with two
// frames is "-1 A B 0" and then two numbers no frame gives (tests/host_chunk_shape_stock_test). That layout is kept as it is, so a level the editor saves gives the game the frames it gave
// before. So for 1, 2 and 3 frames this checks:
//   - the chunk's head: its size, the flags, the speed and the count;
//   - every number of the list: the one the last frame to reach its place gives, and 0 where no frame gives one (from the 5th of 6, for 2 frames);
//   - that every number of the list was set;
//   - that the game's own reading (post_input_processing, on a FRMMORPH made from the written bytes) finds each frame's two shapes again.
//
// HOW. The morph data is made from bytes, as a level holds it: one sub-shape for each frame and one more, numbered 1, 2, 3 ..., and a FRMMORPH with no frames yet. The frames are added the
// way the loader keeps them (a_frame, pointing at the sub-shapes), as a door's: the main shape to sub-shape 1, then sub-shape 1 to 2, then 2 to 3. Then the save's own two steps run: the
// morph data's prepare_for_output (which also runs the FRMMORPH's), then the FRMMORPH's make_data_block_from_chunk. Every list made with new[] in this program starts filled with 0xA5 bytes
// (operator new[] below), so a number that was never set reads as 0xA5A5A5A5 and can't pass as a real one by luck. It needs no game files.
//
// AND A LEVEL'S OWN FRAMES, READ AND SAVED AGAIN (TestReadAndSave): a FRMMORPH of 2 and one of 3 frames, made from bytes as a level holds them (the layout above, and after it the places no
// frame reaches, which hold the numbers inteng's save left there: made-up ones here, of the kind the game's levels hold), read by the loader (post_input_processing) and then saved twice,
// untouched. Each save must write the chunk back exactly as it was read, every byte: the frame numbers at their places, and the file's own numbers where no frame reaches.
//
// HOW IT RUNS: tests/host_chunk_shape_test.args builds it; it takes no arguments. A non-zero result means a check failed.

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

// ---- Every list made with new[] starts as 0xA5 bytes, and goes back with delete[] (the plain new and delete stay the library's own)

static unsigned char const kFill = 0xA5;
static int32_t const kFilled = (int32_t)0xA5A5A5A5;

void *operator new[](size_t bytes)
{
	void *const block = malloc(bytes ? bytes : 1);

	if (!block)
	{
		throw std::bad_alloc();
	}

	memset(block, kFill, bytes);
	return block;
}

void operator delete[](void *block) noexcept
{
	free(block);
}

void operator delete[](void *block, size_t) noexcept
{
	free(block);
}

// ---- Stand-ins for the parts of the game the chunk code calls (as avpx/tests/chunk_memory_test.cpp). Nothing here opens a file.

HANDLE avp_CreateFile(const char *, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE)
{
	return INVALID_HANDLE_VALUE;
}

DWORD avp_GetFileAttributes(const char *)
{
	return INVALID_FILE_ATTRIBUTES;
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

// ---- The bytes of a level's morph data

typedef std::vector<char> Bytes;

static void Put32(Bytes *bytes, int32_t value)
{
	char four[4];

	memcpy(four, &value, 4);
	bytes->insert(bytes->end(), four, four + 4);
}

static void PutName(Bytes *bytes, char const *name)
{
	bytes->insert(bytes->end(), name, name + 8);
}

static int32_t Read32(char const *at)
{
	int32_t value;

	memcpy(&value, at, 4);
	return value;
}

// A sub-shape with only its header (SUBSHPHD, 44 bytes): flags 0, the number given, no points, no polygons, radius 0 and a box of 0
static void PutSubShape(Bytes *bytes, int32_t number)
{
	PutName(bytes, "SUBSHAPE");
	Put32(bytes, 12 + 12 + 44);
	PutName(bytes, "SUBSHPHD");
	Put32(bytes, 12 + 44);
	Put32(bytes, 0);
	Put32(bytes, number);

	for (int i = 0; i < 9; i++)
	{
		Put32(bytes, 0);
	}
}

static int32_t const kFlags = 5;
static int32_t const kSpeed = 6;

// A FRMMORPH with no frames: flags, speed, and a count of 0
static void PutNoFrames(Bytes *bytes)
{
	PutName(bytes, "FRMMORPH");
	Put32(bytes, 12 + 12);
	Put32(bytes, kFlags);
	Put32(bytes, kSpeed);
	Put32(bytes, 0);
}

// ---- The test

// One frame as the test makes it: the two sub-shapes it goes between (0 for the main shape, written as -1) and its spare number
struct Frame
{
	Shape_Sub_Shape_Chunk *first;
	Shape_Sub_Shape_Chunk *second;
	int32_t spare;
};

static int32_t NumberOf(Shape_Sub_Shape_Chunk *shape)
{
	return shape ? shape->get_header()->file_id_num : -1;
}

// The game's reading keeps the main shape as the morph data's parent_shape. This morph data is made on its own, with no shape chunk around it, so it is given this stand-in: the reading only
// keeps the pointer and never reads through it.
static char mainShapeStandIn;

static void TestFrames(int frameCount)
{
	std::string const what = std::to_string(frameCount) + (frameCount == 1 ? " frame" : " frames");

	// The level's file chunk is where the chunk code puts an error while it reads (Parent_File)
	File_Chunk root;
	Parent_File = &root;

	Bytes payload;

	for (int s = 0; s <= frameCount; s++)
	{
		PutSubShape(&payload, s + 1);
	}

	PutNoFrames(&payload);

	Shape_Morphing_Data_Chunk *const morph = new Shape_Morphing_Data_Chunk((Shape_Chunk *)0, payload.data(), payload.size());
	Shape_Morphing_Frame_Data_Chunk *const frames = (Shape_Morphing_Frame_Data_Chunk *)morph->lookup_single_child("FRMMORPH");
	Shape_Chunk *const mainShape = reinterpret_cast<Shape_Chunk *>(&mainShapeStandIn);

	morph->parent_shape = mainShape;

	List<Chunk *> found;
	morph->lookup_child("SUBSHAPE", found);

	std::vector<Shape_Sub_Shape_Chunk *> subShapes;

	for (LIF<Chunk *> each(&found); !each.done(); each.next())
	{
		subShapes.push_back((Shape_Sub_Shape_Chunk *)each());
	}

	bool const made = root.error_code == 0 && frames && subShapes.size() == (size_t)frameCount + 1;

	Check(made, what + ": the morph data's bytes were taken, with " + std::to_string(frameCount + 1) + " sub-shapes and a FRMMORPH");

	if (!made)
	{
		delete morph;
		Parent_File = 0;
		return;
	}

	// As a door's: frame 0 goes from the main shape to sub-shape 1, frame k from sub-shape k to sub-shape k+1
	std::vector<Frame> wanted;

	for (int k = 0; k < frameCount; k++)
	{
		Frame frame;

		frame.first = k == 0 ? 0 : subShapes[k - 1];
		frame.second = subShapes[k];
		frame.spare = 100 + k;
		wanted.push_back(frame);

		a_frame *const held = new a_frame;

		if (frame.first)
			held->shape1a = frame.first;
		else
			held->shape1b = mainShape;

		held->shape2a = frame.second;
		held->spare = frame.spare;
		frames->anim_frames.add_entry(held);
	}

	// The list as the game reads it: frame k's three numbers at k, k+1 and k+2, each frame over the one before; 0 where no frame reaches
	std::vector<int32_t> list((size_t)frameCount * 3, 0);
	std::vector<int> from((size_t)frameCount * 3, -1);

	for (int k = 0; k < frameCount; k++)
	{
		int32_t const three[3] = { NumberOf(wanted[k].first), NumberOf(wanted[k].second), wanted[k].spare };

		for (int j = 0; j < 3; j++)
		{
			list[k + j] = three[j];
			from[k + j] = k * 3 + j;
		}
	}

	// The save's two steps
	morph->prepare_for_output();

	char *const block = frames->make_data_block_from_chunk();
	size_t const wantedSize = 12 + 12 + 12 * (size_t)frameCount;

	Check(block != 0, what + ": the chunk made its block");

	if (block)
	{
		Check(!strncmp(block, "FRMMORPH", 8), what + ": the block starts with the chunk's name");
		Check(Read32(block + 8) == (int32_t)wantedSize, what + ": the block's size is " + std::to_string(wantedSize) + " (it says " + std::to_string(Read32(block + 8)) + ")");
		Check(Read32(block + 12) == kFlags && Read32(block + 16) == kSpeed, what + ": the flags and the speed are written as they were read");
		Check(Read32(block + 20) == frameCount, what + ": the count of frames is " + std::to_string(frameCount) + " (it says " + std::to_string(Read32(block + 20)) + ")");

		char const *const words[3] = { "first shape", "second shape", "spare number" };
		int set = 0;

		for (int p = 0; p < frameCount * 3; p++)
		{
			int32_t const got = Read32(block + 24 + p * 4);
			std::string const whose = from[p] < 0 ? "no frame gives it" : "frame " + std::to_string(from[p] / 3) + "'s " + words[from[p] % 3];

			Check(got == list[p], what + ": number " + std::to_string(p) + " of the list is " + std::to_string(list[p]) + ", " + whose + " (the block has " + std::to_string(got) + ")");

			if (got != kFilled)
			{
				set++;
			}
		}

		Check(set == frameCount * 3, what + ": every one of the " + std::to_string(frameCount * 3) + " numbers of the list was set (" + std::to_string(set) + " were)");

		// The game's own reading of the written bytes: a FRMMORPH made from them, in the same morph data, and its post_input_processing
		Shape_Morphing_Frame_Data_Chunk *const again = (Shape_Morphing_Frame_Data_Chunk *)morph->DynCreate(block);

		Check(root.error_code == 0 && again != 0, what + ": the game's reader takes the written bytes");

		if (again && root.error_code == 0)
		{
			again->post_input_processing();

			std::vector<a_frame *> read;

			for (LIF<a_frame *> each(&again->anim_frames); !each.done(); each.next())
			{
				read.push_back(each());
			}

			Check(read.size() == (size_t)frameCount, what + ": the game's reading finds " + std::to_string(frameCount) + (frameCount == 1 ? " frame" : " frames") + " (it found " +
				std::to_string(read.size()) + ")");

			for (size_t k = 0; k < read.size() && k < wanted.size(); k++)
			{
				bool const firstSame = wanted[k].first ? read[k]->shape1a == wanted[k].first : read[k]->shape1a == 0 && read[k]->shape1b == mainShape;
				bool const secondSame = read[k]->shape2a == wanted[k].second;

				Check(firstSame && secondSame, what + ": the game's reading finds frame " + std::to_string(k) + "'s two shapes again (" + std::to_string(NumberOf(wanted[k].first)) + " to " +
					std::to_string(NumberOf(wanted[k].second)) + ")");
			}
		}

		delete[] block;
	}

	delete morph;
	Parent_File = 0;
}

// The list of a FRMMORPH of 2 or 3 frames as a level holds it, for a door that goes from the main shape to sub-shape 1, then from 1 to 2 (and from 2 to 3): -1 1 2 (3), then the last
// frame's spare number 0, then the places no frame reaches. The game's levels hold numbers inteng's save never set there (Area52's door has "12 43"); these are made-up ones of that kind,
// none of them -1 (the loader reads some of those places for 3 frames or more, and takes a -1 for the main shape).
static std::vector<int32_t> LevelsList(int frameCount)
{
	if (frameCount == 2)
	{
		return { -1, 1, 2, 0, 12, 43 };
	}

	return { -1, 1, 2, 3, 0, 7, 9, 11, 13 };
}

static std::string NumbersWords(char const *list, int count)
{
	std::string words;

	for (int n = 0; n < count; n++)
	{
		words += (n ? " " : "") + std::to_string(Read32(list + n * 4));
	}

	return words;
}

static void TestReadAndSave(int frameCount)
{
	std::string const what = std::to_string(frameCount) + " frames read and saved";

	File_Chunk root;
	Parent_File = &root;

	// The FRMMORPH as the level holds it
	std::vector<int32_t> const list = LevelsList(frameCount);
	Bytes chunk;

	PutName(&chunk, "FRMMORPH");
	Put32(&chunk, 12 + 12 + 12 * frameCount);
	Put32(&chunk, kFlags);
	Put32(&chunk, kSpeed);
	Put32(&chunk, frameCount);

	for (int32_t number : list)
	{
		Put32(&chunk, number);
	}

	// The morph data: sub-shapes 1 to frameCount, then the FRMMORPH
	Bytes payload;

	for (int s = 0; s < frameCount; s++)
	{
		PutSubShape(&payload, s + 1);
	}

	payload.insert(payload.end(), chunk.begin(), chunk.end());

	Shape_Morphing_Data_Chunk *const morph = new Shape_Morphing_Data_Chunk((Shape_Chunk *)0, payload.data(), payload.size());
	Shape_Morphing_Frame_Data_Chunk *const frames = (Shape_Morphing_Frame_Data_Chunk *)morph->lookup_single_child("FRMMORPH");
	Shape_Chunk *const mainShape = reinterpret_cast<Shape_Chunk *>(&mainShapeStandIn);

	morph->parent_shape = mainShape;

	List<Chunk *> found;
	morph->lookup_child("SUBSHAPE", found);

	std::vector<Shape_Sub_Shape_Chunk *> subShapes;

	for (LIF<Chunk *> each(&found); !each.done(); each.next())
	{
		subShapes.push_back((Shape_Sub_Shape_Chunk *)each());
	}

	bool const made = root.error_code == 0 && frames && subShapes.size() == (size_t)frameCount;

	Check(made, what + ": the morph data's bytes were taken, with " + std::to_string(frameCount) + " sub-shapes and a FRMMORPH of " + std::to_string(frameCount) + " frames");

	if (!made)
	{
		delete morph;
		Parent_File = 0;
		return;
	}

	// The loader's reading, as the end of a level's load runs it
	frames->post_input_processing();

	std::vector<a_frame *> read;

	for (LIF<a_frame *> each(&frames->anim_frames); !each.done(); each.next())
	{
		read.push_back(each());
	}

	bool shapesRight = read.size() == (size_t)frameCount;

	for (size_t k = 0; shapesRight && k < read.size(); k++)
	{
		bool const firstRight = k == 0 ? read[k]->shape1a == 0 && read[k]->shape1b == mainShape : read[k]->shape1a == subShapes[k - 1];

		shapesRight = firstRight && read[k]->shape2a == subShapes[k];
	}

	Check(shapesRight, what + ": the loader reads " + std::to_string(frameCount) + " frames, the main shape to sub-shape 1, then each sub-shape to the next (it read " +
		std::to_string(read.size()) + ")");

	// Two saves of the untouched level: each writes the chunk back as it was read, every byte
	for (int save = 1; save <= 2; save++)
	{
		morph->prepare_for_output();

		char *const block = frames->make_data_block_from_chunk();
		std::string const which = what + ": save " + std::to_string(save);

		Check(block != 0, which + ": the chunk made its block");

		if (!block)
		{
			continue;
		}

		bool const sameSize = Read32(block + 8) == (int32_t)chunk.size();
		bool const same = sameSize && memcmp(block, chunk.data(), chunk.size()) == 0;

		Check(same, which + " writes the chunk as the level holds it, every byte: the list " + NumbersWords(chunk.data() + 24, frameCount * 3) + " (it wrote " +
			(sameSize ? NumbersWords(block + 24, frameCount * 3) : "a chunk of " + std::to_string(Read32(block + 8)) + " bytes") + ")");

		delete[] block;
	}

	delete morph;
	Parent_File = 0;
}

int main()
{
	setvbuf(stdout, 0, _IONBF, 0);

	TestFrames(1);
	TestFrames(2);
	TestFrames(3);
	TestReadAndSave(2);
	TestReadAndSave(3);

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
