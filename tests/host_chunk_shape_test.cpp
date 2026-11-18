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
// morph data's prepare_for_output (which also runs the FRMMORPH's), then the FRMMORPH's make_data_block_from_chunk. Everything made with new or new[] in this program starts filled with
// 0xA5 bytes (operator new and new[] below), so a number that was never set reads as 0xA5A5A5A5 and can't pass as a real one by luck. It needs no game files.
//
// AND A LEVEL'S OWN FRAMES, READ AND SAVED AGAIN (TestReadAndSave): a FRMMORPH of 2 and one of 3 frames, made from bytes as a level holds them (the layout above, and after it the places no
// frame reaches, which hold the numbers inteng's save left there: made-up ones here, of the kind the game's levels hold), read by the loader (post_input_processing) and then saved twice,
// untouched. Each save must write the chunk back exactly as it was read, every byte: the frame numbers at their places, and the file's own numbers where no frame reaches.
//
// AND THOSE FRAMES EDITED BEFORE THE SAVE (TestEditAndSave): the same 2 and 3 frames, read by the loader, then the last frame changed (its second shape and its spare number), or taken
// out and a new one put in (the count is the file's again), or taken out (one frame fewer). The save must write the frames as they are after the edit, at places 0 to count+1 of the
// list, and at the places from count+2 on the file's own numbers while the count is the file's, 0 once it isn't. Each list wanted is written out by hand (SavedList).
//
// AND WHERE AN UNTOUCHED SAVE IS NOT THE FILE'S BYTES (TestLoaderLimits): two files whose numbers the loader's checks for the main shape and its shape lookups read at different places
// ("-1 1 2 -1 12 43" and "-1 1 -1 0 12 43"); what the loader makes of them and what the save writes are pinned. No stock level holds such a chunk.
//
// AND A MORPH MADE IN CODE (TestMadeInCode): a FRMMORPH made with the constructor that takes only its parent, given two frames made with a_frame's own constructor, their shapes set
// and their spare numbers not. Every number it writes must be set: the flags and the speed 0, the frames' shapes, and each spare number 0.
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

// ---- Everything made with new or new[] starts as 0xA5 bytes, and goes back with delete or delete[]

static unsigned char const kFill = 0xA5;
static int32_t const kFilled = (int32_t)0xA5A5A5A5;

static void *Filled(size_t bytes)
{
	void *const block = malloc(bytes ? bytes : 1);

	if (!block)
	{
		throw std::bad_alloc();
	}

	memset(block, kFill, bytes);
	return block;
}

void *operator new(size_t bytes)
{
	return Filled(bytes);
}

void *operator new[](size_t bytes)
{
	return Filled(bytes);
}

void operator delete(void *block) noexcept
{
	free(block);
}

void operator delete(void *block, size_t) noexcept
{
	free(block);
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

// A level's morph data as TestReadAndSave and TestEditAndSave make it: sub-shapes 1 to frameCount, then the FRMMORPH as the level holds it (LevelsList), read by the loader
struct LevelsMorph
{
	Bytes chunk;                                       // the FRMMORPH's bytes, as the level holds them
	Shape_Morphing_Data_Chunk *morph = 0;              // the caller deletes it
	Shape_Morphing_Frame_Data_Chunk *frames = 0;
	std::vector<Shape_Sub_Shape_Chunk *> subShapes;    // sub-shape 1 first
};

// Makes the morph data and runs the loader's reading of it (post_input_processing, as the end of a level's load runs it), checking that it reads frameCount frames, the main shape to
// sub-shape 1, then each sub-shape to the next. False when the bytes were not taken. The caller's file chunk must be Parent_File.
static bool ReadLevelsMorph(int frameCount, File_Chunk const &root, std::string const &what, LevelsMorph *level)
{
	// The FRMMORPH as the level holds it
	std::vector<int32_t> const list = LevelsList(frameCount);
	Bytes &chunk = level->chunk;

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

	level->morph = new Shape_Morphing_Data_Chunk((Shape_Chunk *)0, payload.data(), payload.size());
	level->frames = (Shape_Morphing_Frame_Data_Chunk *)level->morph->lookup_single_child("FRMMORPH");
	Shape_Chunk *const mainShape = reinterpret_cast<Shape_Chunk *>(&mainShapeStandIn);

	level->morph->parent_shape = mainShape;

	List<Chunk *> found;
	level->morph->lookup_child("SUBSHAPE", found);

	for (LIF<Chunk *> each(&found); !each.done(); each.next())
	{
		level->subShapes.push_back((Shape_Sub_Shape_Chunk *)each());
	}

	bool const made = root.error_code == 0 && level->frames && level->subShapes.size() == (size_t)frameCount;

	Check(made, what + ": the morph data's bytes were taken, with " + std::to_string(frameCount) + " sub-shapes and a FRMMORPH of " + std::to_string(frameCount) + " frames");

	if (!made)
	{
		return false;
	}

	// The loader's reading
	level->frames->post_input_processing();

	std::vector<a_frame *> read;

	for (LIF<a_frame *> each(&level->frames->anim_frames); !each.done(); each.next())
	{
		read.push_back(each());
	}

	bool shapesRight = read.size() == (size_t)frameCount;

	for (size_t k = 0; shapesRight && k < read.size(); k++)
	{
		bool const firstRight = k == 0 ? read[k]->shape1a == 0 && read[k]->shape1b == mainShape : read[k]->shape1a == level->subShapes[k - 1];

		shapesRight = firstRight && read[k]->shape2a == level->subShapes[k];
	}

	Check(shapesRight, what + ": the loader reads " + std::to_string(frameCount) + " frames, the main shape to sub-shape 1, then each sub-shape to the next (it read " +
		std::to_string(read.size()) + ")");

	return true;
}

static void TestReadAndSave(int frameCount)
{
	std::string const what = std::to_string(frameCount) + " frames read and saved";

	File_Chunk root;
	Parent_File = &root;

	LevelsMorph level;

	if (!ReadLevelsMorph(frameCount, root, what, &level))
	{
		delete level.morph;
		Parent_File = 0;
		return;
	}

	Bytes const &chunk = level.chunk;

	// Two saves of the untouched level: each writes the chunk back as it was read, every byte
	for (int save = 1; save <= 2; save++)
	{
		level.morph->prepare_for_output();

		char *const block = level.frames->make_data_block_from_chunk();
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

	delete level.morph;
	Parent_File = 0;
}

// ---- A level's frames edited after the loader read them, then saved (TestEditAndSave)

enum Edit
{
	kLastFrameChanged,    // the last frame's second shape pointed at sub-shape 1, and its spare number made 77: the count stays the file's
	kLastFrameReplaced,   // the last frame taken out, and a new one put in its place (the last sub-shape to sub-shape 1, spare 55): the count is the file's again
	kLastFrameRemoved     // the last frame taken out: one frame fewer than the file has
};

static char const *EditWords(Edit edit)
{
	return edit == kLastFrameChanged ? "the last frame changed" : edit == kLastFrameReplaced ? "the last frame taken out and a new one put in" : "the last frame taken out";
}

// What the save must write for the edit, worked out by hand from the layout (frame k at k, k+1 and k+2, each frame over the one before) and the file's list (LevelsList). After the loader's
// reading, frame 0 is the main shape to 1 with spare 2 (it reads frame 0's spare at place 2), frame 1 is 1 to 2 with spare 0 for 2 frames and spare 3 for 3 frames, and frame 2 is 2 to 3
// with spare 0. Where no frame reaches (the places from count+2 on) it is the file's own number while the count is the file's, and 0 when it isn't.
static std::vector<int32_t> SavedList(int frameCount, Edit edit)
{
	if (frameCount == 2)
	{
		if (edit == kLastFrameChanged)
			return { -1, 1, 1, 77, 12, 43 };

		if (edit == kLastFrameReplaced)
			return { -1, 2, 1, 55, 12, 43 };

		return { -1, 1, 2 };
	}

	if (edit == kLastFrameChanged)
		return { -1, 1, 2, 1, 77, 7, 9, 11, 13 };

	if (edit == kLastFrameReplaced)
		return { -1, 1, 3, 1, 55, 7, 9, 11, 13 };

	return { -1, 1, 2, 3, 0, 0 };
}

// The numbers from place `from` up to `to` of a list, in words
static std::string ListWords(std::vector<int32_t> const &list, size_t from, size_t to)
{
	std::string words;

	for (size_t p = from; p < to && p < list.size(); p++)
	{
		words += (p > from ? " " : "") + std::to_string(list[p]);
	}

	return words.empty() ? "nothing" : words;
}

// A FRMMORPH of 2 or 3 frames read by the loader as TestReadAndSave reads it, its frames edited as the editor can (a frame's shape or spare number changed, a frame taken out, one put in),
// then saved. The save must write the frames as they are after the edit, at their places of the list; and where no frame reaches, the file's own numbers while the count is the file's
// (12 43, or 7 9 11 13), and 0 once it isn't. So a save that kept the list as the file gave it whenever the count was unchanged, dropping the edit, can't pass.
static void TestEditAndSave(int frameCount, Edit edit)
{
	std::string const what = std::to_string(frameCount) + " frames read, " + EditWords(edit) + ", and saved";

	File_Chunk root;
	Parent_File = &root;

	LevelsMorph level;

	if (!ReadLevelsMorph(frameCount, root, what, &level))
	{
		delete level.morph;
		Parent_File = 0;
		return;
	}

	List<a_frame *> &frames = level.frames->anim_frames;
	a_frame *const last = frames.last_entry();

	if (edit == kLastFrameChanged)
	{
		last->shape2a = level.subShapes[0];
		last->shape2b = 0;
		last->spare = 77;
	}
	else
	{
		frames.delete_last_entry();
		delete last;
	}

	if (edit == kLastFrameReplaced)
	{
		a_frame *const fresh = new a_frame;

		fresh->shape1a = level.subShapes[frameCount - 1];
		fresh->shape2a = level.subShapes[0];
		fresh->spare = 55;
		frames.add_entry(fresh);
	}

	int const count = edit == kLastFrameRemoved ? frameCount - 1 : frameCount;
	std::vector<int32_t> const wanted = SavedList(frameCount, edit);
	size_t const given = (size_t)count + 2;   // the places a frame reaches: frame k reaches k, k+1 and k+2

	level.morph->prepare_for_output();

	char *const block = level.frames->make_data_block_from_chunk();

	Check(block != 0, what + ": the chunk made its block");

	if (!block)
	{
		delete level.morph;
		Parent_File = 0;
		return;
	}

	size_t const wantedSize = 12 + 12 + 12 * (size_t)count;
	bool const head = Read32(block + 8) == (int32_t)wantedSize && Read32(block + 12) == kFlags && Read32(block + 16) == kSpeed && Read32(block + 20) == count;

	Check(head, what + ": the chunk is " + std::to_string(wantedSize) + " bytes with the file's flags and speed and a count of " + std::to_string(count) + " (it says " +
		std::to_string(Read32(block + 8)) + " bytes, a count of " + std::to_string(Read32(block + 20)) + ")");

	if (head)
	{
		std::vector<int32_t> written((size_t)count * 3);

		for (size_t p = 0; p < written.size(); p++)
		{
			written[p] = Read32(block + 24 + p * 4);
		}

		bool givenSame = true, restSame = true;

		for (size_t p = 0; p < written.size(); p++)
		{
			if (written[p] != wanted[p])
			{
				if (p < given)
					givenSame = false;
				else
					restSame = false;
			}
		}

		Check(givenSame, what + ": places 0 to " + std::to_string(given - 1) + " hold the frames as they are after the edit, " + ListWords(wanted, 0, given) + " (it wrote " +
			ListWords(written, 0, given) + ")");

		if (written.size() > given)
		{
			Check(restSame, what + ": places " + std::to_string(given) + " on, where no frame reaches, hold " + (count == frameCount ? "the file's own numbers " : "0, as the count changed: ") +
				ListWords(wanted, given, wanted.size()) + " (it wrote " + ListWords(written, given, written.size()) + ")");
		}
	}

	delete[] block;
	delete level.morph;
	Parent_File = 0;
}

// ---- Where a save of an untouched level is not the file's bytes (TestLoaderLimits)

// The loader (post_input_processing) checks frame i for -1 (the main shape) at places i*2 and i*2+1 of the list, and takes its shapes from places i and i+1: the same places for frame 0
// only. Where they disagree, the frames it makes are not the ones the file's numbers would give back, so an untouched save writes other numbers, as inteng's save did. None of the game's
// 761 FRMMORPH chunks is such a one (tests/host_chunk_shape_stock_test). These two files, with sub-shapes 1 and 2 (A and B), are pinned here, so a change to the loader or the save that
// changes them is seen (avpx chunk_shape.cpp, Shape_Morphing_Frame_Data_Chunk::prepare_for_output, names both):
//   - "-1 1 2 -1 12 43" is read as the main shape to 1 (spare 2), then 1 to the main shape (spare -1), and written as "-1 1 -1 -1 12 43";
//   - "-1 1 -1 0 12 43" is read as one frame, the main shape to 1 (spare -1): the second frame's second shape is looked for as number -1 (place 2), which no sub-shape has, so the frame
//     is dropped, and the chunk is written with 1 frame, "-1 1 -1".
static void TestLoaderLimit(std::vector<int32_t> const &fileList, int framesRead, std::vector<int32_t> const &savedList)
{
	std::string const what = "the file " + ListWords(fileList, 0, fileList.size()) + " read and saved untouched";

	File_Chunk root;
	Parent_File = &root;

	Bytes payload;
	PutSubShape(&payload, 1);
	PutSubShape(&payload, 2);
	PutName(&payload, "FRMMORPH");
	Put32(&payload, 12 + 12 + 4 * (int32_t)fileList.size());
	Put32(&payload, kFlags);
	Put32(&payload, kSpeed);
	Put32(&payload, (int32_t)fileList.size() / 3);

	for (int32_t number : fileList)
	{
		Put32(&payload, number);
	}

	Shape_Morphing_Data_Chunk *const morph = new Shape_Morphing_Data_Chunk((Shape_Chunk *)0, payload.data(), payload.size());
	Shape_Morphing_Frame_Data_Chunk *const frames = (Shape_Morphing_Frame_Data_Chunk *)morph->lookup_single_child("FRMMORPH");

	morph->parent_shape = reinterpret_cast<Shape_Chunk *>(&mainShapeStandIn);

	Check(root.error_code == 0 && frames, what + ": the morph data's bytes were taken");

	if (root.error_code == 0 && frames)
	{
		frames->post_input_processing();

		Check(frames->anim_frames.size() == framesRead, what + ": the loader makes " + std::to_string(framesRead) + (framesRead == 1 ? " frame" : " frames") + " (it made " +
			std::to_string(frames->anim_frames.size()) + ")");

		morph->prepare_for_output();

		char *const block = frames->make_data_block_from_chunk();

		if (block)
		{
			int32_t const count = Read32(block + 20);
			std::vector<int32_t> written;

			if (Read32(block + 8) == 24 + 12 * count && count >= 0 && count <= 3)
			{
				for (int p = 0; p < count * 3; p++)
				{
					written.push_back(Read32(block + 24 + p * 4));
				}
			}

			Check(written == savedList, what + ": the save writes " + ListWords(savedList, 0, savedList.size()) + " (it wrote " + ListWords(written, 0, written.size()) + ")");
			delete[] block;
		}
		else
		{
			Check(false, what + ": the chunk made its block");
		}
	}

	delete morph;
	Parent_File = 0;
}

static void TestLoaderLimits()
{
	TestLoaderLimit({ -1, 1, 2, -1, 12, 43 }, 2, { -1, 1, -1, -1, 12, 43 });
	TestLoaderLimit({ -1, 1, -1, 0, 12, 43 }, 1, { -1, 1, -1 });
}

// ---- A morph made in code, not read from a file (TestMadeInCode)

// The morph data holds sub-shapes 1 and 2 and no FRMMORPH; the FRMMORPH is made with the constructor that takes only its parent, as code that builds a morph would make one, and given two
// frames made with a_frame's own constructor, the main shape to sub-shape 1, then 1 to 2, their spare numbers left as the constructor leaves them. The list written is -1 1 2 0 0 0: frame 0's
// three numbers at 0, 1 and 2, frame 1's at 1, 2 and 3 (its spare number, 0), and 0 at the 2 places no frame reaches, as the count isn't one a file gave.
static void TestMadeInCode()
{
	std::string const what = "a morph made in code";

	File_Chunk root;
	Parent_File = &root;

	Bytes payload;
	PutSubShape(&payload, 1);
	PutSubShape(&payload, 2);

	Shape_Morphing_Data_Chunk *const morph = new Shape_Morphing_Data_Chunk((Shape_Chunk *)0, payload.data(), payload.size());
	Shape_Chunk *const mainShape = reinterpret_cast<Shape_Chunk *>(&mainShapeStandIn);

	morph->parent_shape = mainShape;

	List<Chunk *> found;
	morph->lookup_child("SUBSHAPE", found);

	std::vector<Shape_Sub_Shape_Chunk *> subShapes;

	for (LIF<Chunk *> each(&found); !each.done(); each.next())
	{
		subShapes.push_back((Shape_Sub_Shape_Chunk *)each());
	}

	bool const made = root.error_code == 0 && subShapes.size() == 2 && !morph->lookup_single_child("FRMMORPH");

	Check(made, what + ": the morph data's bytes were taken, with 2 sub-shapes and no FRMMORPH");

	if (!made)
	{
		delete morph;
		Parent_File = 0;
		return;
	}

	// The FRMMORPH is made in memory this test fills with 0xA5 itself (placement new), not with plain new: made with plain new here, its flags and speed came out 0 though its constructor
	// sets neither. The tests, the game and the editor are all built with /sdl, which can clear an object made with plain new before its constructor runs, so a number the constructor
	// leaves unset could pass as 0 by luck of that setting.
	void *const room = Filled(sizeof(Shape_Morphing_Frame_Data_Chunk));
	Shape_Morphing_Frame_Data_Chunk *const frames = new (room) Shape_Morphing_Frame_Data_Chunk(morph);
	a_frame *const first = new a_frame;
	a_frame *const second = new a_frame;

	Check(first->spare == 0 && second->spare == 0 && !first->shape1a && !first->shape1b && !first->shape2a && !first->shape2b, what + ": a frame made with a_frame's constructor "
		"points at no shape and has a spare number of 0 (the spare numbers are " + std::to_string(first->spare) + " and " + std::to_string(second->spare) + ")");

	first->shape1b = mainShape;
	first->shape2a = subShapes[0];
	second->shape1a = subShapes[0];
	second->shape2a = subShapes[1];
	frames->anim_frames.add_entry(first);
	frames->anim_frames.add_entry(second);

	// The save's two steps
	morph->prepare_for_output();

	char *const block = frames->make_data_block_from_chunk();

	Check(block != 0, what + ": the chunk made its block");

	if (block)
	{
		Check(Read32(block + 8) == 48 && Read32(block + 20) == 2, what + ": the chunk is 48 bytes with a count of 2 (it says " + std::to_string(Read32(block + 8)) + " bytes, a count of " +
			std::to_string(Read32(block + 20)) + ")");
		Check(Read32(block + 12) == 0 && Read32(block + 16) == 0, what + ": the flags and the speed are 0 (it wrote " + std::to_string(Read32(block + 12)) + " and " +
			std::to_string(Read32(block + 16)) + ")");

		if (Read32(block + 8) == 48)
		{
			std::vector<int32_t> const wanted = { -1, 1, 2, 0, 0, 0 };
			std::vector<int32_t> written;
			int set = 0;

			for (int p = 0; p < 6; p++)
			{
				written.push_back(Read32(block + 24 + p * 4));
			}

			for (int n = 0; n < 9; n++)
			{
				set += Read32(block + 12 + n * 4) != kFilled;
			}

			Check(written == wanted, what + ": the list is " + ListWords(wanted, 0, 6) + " (it wrote " + ListWords(written, 0, 6) + ")");
			Check(set == 9, what + ": every one of the 9 numbers after the chunk's size was set (" + std::to_string(set) + " were)");
		}

		delete[] block;
	}

	// Its destructor deletes its frames and takes it out of the morph data's children, so the morph data's own destructor doesn't see it
	frames->~Shape_Morphing_Frame_Data_Chunk();
	free(room);

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

	for (int frameCount = 2; frameCount <= 3; frameCount++)
	{
		TestEditAndSave(frameCount, kLastFrameChanged);
		TestEditAndSave(frameCount, kLastFrameReplaced);
		TestEditAndSave(frameCount, kLastFrameRemoved);
	}

	TestLoaderLimits();
	TestMadeInCode();

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
