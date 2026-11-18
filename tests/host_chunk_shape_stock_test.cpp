// The guard on the morph frames the game's loader makes from the game's own levels: Shape_Morphing_Frame_Data_Chunk::post_input_processing in avpx/3dc/win95/chunk_shape.cpp. Written
// 2026-11-17 by Melanikus (Bishop) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT TESTS. The game loads a level's morph frames (a door's or a lift's shapes) from its FRMMORPH chunks: the reader keeps the chunk's numbers, three for each frame, and
// post_input_processing turns them into the frames the game uses (anim_frames: the frame's two shapes, a sub-shape or the main shape, and its spare number; chunk_load.cpp reads them).
// For every FRMMORPH chunk of every level given, this checks that the frames the loader made are exactly the frames of THE READING AS IT WAS, a copy of post_input_processing at avpx
// 587c09f3 (ReadingAsItWas below): the same count, and each frame with the same two shapes and the same spare number. So a change to the loader can't change any of the game's own
// levels without this going red.
//
// And THE SAVE: the whole level is then prepared as the editor's save prepares it (prepare_for_output on the whole tree, src/io/level_file.cpp PrepareAsIntengDoes), and for every FRMMORPH
// this checks that a save writes the chunk exactly as the file holds it, every byte: the numbers at the places of the list a frame gives, and the file's own numbers at the places no frame
// reaches (where inteng's save left numbers it never set, which the game's levels hold); and that the game's reading of the written numbers gives the frames the loader made. So a change
// to the writer (prepare_for_output) can't change a level the editor saves untouched, or what the game reads from it.
//
// It also says, as notes (never a check), how many chunks THE READER'S ORDER (frame i at i*3, i*3+1, i*3+2, each shape looked for from the first sub-shape: ReadingInReadersOrder below)
// would give other frames for, and which. That is what a change of the reading would change in the game's own levels.
//
// HOW. Each level is read by the game's own loader (File_Chunk), and every chunk of it is visited. A FRMMORPH's numbers are taken as the save path would write them before any
// prepare_for_output: its own make_data_block_from_chunk, so the numbers are the ones the reader kept. The levels are only read, opened so other tests can read them at the same time.
// The last lines give the count of chunks, the frames by count, and one sum over every frame (its shapes by their place in the morph data, and its spare number), to compare runs by.
//
// HOW IT RUNS: tests/host_chunk_shape_stock_test.args builds it; run as host_chunk_shape_stock_test <level> [<level> ...] (tests/host_chunk_shape_stock_test.runargs: {LEVELS}).

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
};

static int plainFrameChunks = 0;   // FRMMORPH chunks the loader kept as plain bytes (not in a SHPMORPH): the game never reads frames from those

static void Visit(Chunk *chunk, Chunk_With_Children *parent, std::vector<Found> *found, int depth)
{
	if (!strncmp(chunk->identifier, "FRMMORPH", 8))
	{
		Shape_Morphing_Frame_Data_Chunk *const frames = dynamic_cast<Shape_Morphing_Frame_Data_Chunk *>(chunk);
		Shape_Morphing_Data_Chunk *const morph = dynamic_cast<Shape_Morphing_Data_Chunk *>(parent);

		if (frames && morph)
		{
			found->push_back(Found { morph, frames });
		}
		else
		{
			plainFrameChunks++;
		}
	}

	Chunk_With_Children *const holder = dynamic_cast<Chunk_With_Children *>(chunk);

	if (!holder || depth > 64)
	{
		return;
	}

	for (Chunk *child = holder->*Get(FirstChild()); child; child = child->*Get(NextChunk()))
	{
		Visit(child, holder, found, depth + 1);
	}
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

// A FRMMORPH's list of numbers, from the block the chunk writes (make_data_block_from_chunk), and the whole block's bytes: false, with why, when there is no block or its size doesn't hold
// its count's numbers
static bool NumbersOf(Shape_Morphing_Frame_Data_Chunk *chunk, std::vector<int32_t> *numbers, std::string *why, std::vector<char> *bytes = 0)
{
	numbers->clear();

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

	numbers->resize((size_t)count * 3);

	if (count)
	{
		memcpy(numbers->data(), block + 24, numbers->size() * 4);
	}

	if (bytes)
	{
		bytes->assign(block, block + size);
	}

	delete[] block;
	return true;
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
	File_Chunk *const level = new File_Chunk(path);

	if (level->error_code != 0)
	{
		Check(false, name + ": the game's loader takes it (error " + std::to_string(level->error_code) + ")");
		delete level;
		return;
	}

	std::vector<Found> found;
	Visit(level, 0, &found, 0);

	int same = 0;
	int bad = 0;

	// What each chunk held as it was read, for the save below: the file's bytes and numbers and the frames the loader made from them (empty and not usable when the chunk's block was
	// refused)
	std::vector<std::vector<char>> fileBytes(found.size());
	std::vector<std::vector<int32_t>> fileNumbers(found.size());
	std::vector<std::vector<Frame>> loadersFrames(found.size());
	std::vector<bool> usable(found.size(), false);

	for (size_t k = 0; k < found.size(); k++)
	{
		Shape_Morphing_Frame_Data_Chunk *const chunk = found[k].frames;
		Shape_Morphing_Data_Chunk *const morph = found[k].morph;

		// The numbers the reader kept, as the chunk writes them before any prepare_for_output: the file's own bytes
		std::vector<int32_t> numbers;
		std::string why;

		if (!NumbersOf(chunk, &numbers, &why, &fileBytes[k]))
		{
			Check(false, name + ": FRMMORPH " + std::to_string(k) + " " + why);
			bad++;
			continue;
		}

		int const count = (int)(numbers.size() / 3);
		std::vector<Frame> const loaders = LoadersFrames(chunk);

		fileNumbers[k] = numbers;
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

			std::string numberWords;

			for (size_t n = 0; n < numbers.size(); n++)
			{
				numberWords += (n ? " " : "") + std::to_string(numbers[n]);
			}

			printf("  note  %s FRMMORPH %d (numbers %s; %d sub-shapes): the reading as it was gives %s; the reader's order would give %s\n", name.c_str(), (int)k, numberWords.c_str(),
				(int)list.size(), FramesWords(asItWas, morph).c_str(), FramesWords(readersOrder, morph).c_str());
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
		Check(true, name + ": all " + std::to_string(found.size()) + " FRMMORPH chunks give the frames of the reading as it was");
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

		std::vector<int32_t> saved;
		std::vector<char> savedBytes;
		std::string why;

		if (!NumbersOf(found[k].frames, &saved, &why, &savedBytes))
		{
			Check(false, name + ": FRMMORPH " + std::to_string(k) + " after the save's prepare_for_output: " + why);
			saveBad++;
			continue;
		}

		int const count = (int)(saved.size() / 3);
		size_t const given = count ? (size_t)count + 2 : 0;
		std::vector<Frame> const readAgain = ReadingAsItWas(saved, count, found[k].morph);
		bool const framesSame = Same(readAgain, loadersFrames[k]);
		bool const numbersSame = savedBytes == fileBytes[k];

		if (saved.size() > given)
		{
			notGiven += (int)(saved.size() - given);
		}

		saveChecked++;

		if (!framesSame || !numbersSame)
		{
			std::string savedWords, fileWords;

			for (size_t p = 0; p < saved.size(); p++)
			{
				savedWords += (p ? " " : "") + std::to_string(saved[p]);
			}

			for (size_t p = 0; p < fileNumbers[k].size(); p++)
			{
				fileWords += (p ? " " : "") + std::to_string(fileNumbers[k][p]);
			}

			saveBad++;
			Check(false, name + ": FRMMORPH " + std::to_string(k) + ": a save writes " + savedWords + " (the file has " + fileWords + "); the game's reading of it gives " +
				FramesWords(readAgain, found[k].morph) + "; the loader made " + FramesWords(loadersFrames[k], found[k].morph));
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
