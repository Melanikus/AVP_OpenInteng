// The test of the block a chunk is written into before a save: Chunk::make_data_block_from_chunk in avpx/3dc/win95/chunk.cpp. Written 2026-11-17 by Melanikus (Bishop) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT TESTS. make_data_block_from_chunk makes a block of the chunk's size and has the chunk's fill_data_block write into it. A save writes the whole level that way (File_Chunk::write_file
// makes the level's block from its file chunk), and the editor reads a strategy's bytes that way (src/host/strategy_lookup.cpp). Some writers leave a few bytes of their block alone: a name
// is written with its one ending zero, while its chunk's size rounds it up to a whole number of 4 bytes. Those bytes must be 0 in the block. This checks it for:
//   - a chunk of the test's own, whose writer writes only its 12-byte head and leaves the 8 bytes after it;
//   - the game's own EXTOBJNM chunk (Shape_External_Object_Name_Chunk), with a name of 1 letter (2 bytes left) and of 4 letters (3 bytes left);
//   - the block of a whole file chunk holding both, as a save makes it.
// And the same for the block for processing (Chunk::make_data_block_for_process, beside it in chunk.cpp), which goes to the same writers: of the test's own chunk, and of the whole file
// chunk. Nothing in avpx or the editor makes one today; its block must start as zeros all the same.
//
// HOW. Every list made with new[] in this program starts filled with 0xA5 bytes (operator new[] below), so a byte the block was given nothing for can't read as 0 by luck. It needs no game
// files.
//
// HOW IT RUNS: tests/host_chunk_test.args builds it; it takes no arguments. A non-zero result means a check failed.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>
#include <string>

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

// ---- The chunks

// A chunk of the test's own: a 12-byte head and 8 bytes after it, which its writer never writes
class PartlyWrittenChunk : public Chunk
{
public:
	explicit PartlyWrittenChunk(Chunk_With_Children *parent)
	: Chunk(parent, "TESTPART")
	{
	}

	size_t size_chunk() override
	{
		return chunk_size = 12 + 8;
	}

	void fill_data_block(char *data_start) override
	{
		strncpy(data_start, identifier, 8);
		*((int *)(data_start + 8)) = (int)chunk_size;
	}
};

static int32_t Read32(char const *at)
{
	int32_t value;

	memcpy(&value, at, 4);
	return value;
}

// How many of the bytes from `from` up to `to` in the block are not 0
static int NotZero(char const *block, size_t from, size_t to)
{
	int count = 0;

	for (size_t i = from; i < to; i++)
	{
		count += block[i] != 0;
	}

	return count;
}

static void TestOwnChunk()
{
	File_Chunk root;
	PartlyWrittenChunk *const part = new PartlyWrittenChunk(&root);
	size_t const size = part->size_chunk();
	char *const block = part->make_data_block_from_chunk();

	Check(block != 0 && size == 20, "the test's own chunk made a block of 20 bytes");

	if (block)
	{
		Check(!strncmp(block, "TESTPART", 8) && Read32(block + 8) == 20, "its head is written");
		Check(NotZero(block, 12, 20) == 0, "the 8 bytes its writer leaves are 0 (" + std::to_string(NotZero(block, 12, 20)) + " are not)");
		delete[] block;
	}

	// The same chunk's block for processing (make_data_block_for_process), which goes to the same writer
	char *const process = part->make_data_block_for_process();

	Check(process != 0 && part->size_chunk_for_process() == 20, "the test's own chunk made a block for processing of 20 bytes");

	if (process)
	{
		Check(!strncmp(process, "TESTPART", 8) && Read32(process + 8) == 20, "the block for processing has its head written");
		Check(NotZero(process, 12, 20) == 0, "in the block for processing, the 8 bytes its writer leaves are 0 (" + std::to_string(NotZero(process, 12, 20)) + " are not)");
		delete[] process;
	}
}

// The game's EXTOBJNM chunk with the name given: its size rounds the name up to a whole number of 4 bytes (chunk_shape.hpp), its writer writes the name and its ending zero only
static void TestNameChunk(char const *name)
{
	File_Chunk root;
	Shape_External_Object_Name_Chunk *const chunk = new Shape_External_Object_Name_Chunk(&root, name);
	size_t const length = strlen(name);
	size_t const size = chunk->size_chunk();
	size_t const written = 12 + 4 + length + 1;
	char *const block = chunk->make_data_block_from_chunk();
	std::string const what = "EXTOBJNM named \"" + std::string(name) + "\"";

	Check(block != 0 && size == 12 + 4 + length + (4 - length % 4) && size > written, what + ": a block of " + std::to_string(size) + " bytes, " + std::to_string(size - written) + " after the name's zero");

	if (block)
	{
		Check(!memcmp(block + 16, name, length + 1), what + ": the name and its zero are written");
		Check(NotZero(block, written, size) == 0, what + ": the " + std::to_string(size - written) + " bytes after the name's zero are 0 (" + std::to_string(NotZero(block, written, size)) + " are not)");
		delete[] block;
	}
}

// Where the chunk with this name starts inside a block of `size` bytes, or size when it isn't there
static size_t FindName(char const *block, size_t size, char const *name)
{
	for (size_t i = 0; i + 8 <= size; i++)
	{
		if (!memcmp(block + i, name, 8))
		{
			return i;
		}
	}

	return size;
}

// The whole file chunk's block, as File_Chunk::write_file makes it: both chunks above are inside it, and the bytes they leave are 0 there too
static void TestFileBlock()
{
	File_Chunk root;

	// both are the file chunk's children: root owns them and deletes them with itself
	PartlyWrittenChunk *const partChunk = new PartlyWrittenChunk(&root);
	Shape_External_Object_Name_Chunk *const nameChunk = new Shape_External_Object_Name_Chunk(&root, "a");
	Check(partChunk != 0 && nameChunk != 0, "the two chunks were made inside the file chunk");

	size_t const size = root.size_chunk();
	char *const block = root.make_data_block_from_chunk();

	Check(block != 0 && size > 12, "the file chunk made a block of " + std::to_string(size) + " bytes");

	if (block)
	{
		size_t const part = FindName(block, size, "TESTPART");
		size_t const name = FindName(block, size, "EXTOBJNM");
		bool const both = part + 20 <= size && name + 20 <= size;

		Check(both, "both chunks are in the file's block");

		if (both)
		{
			Check(NotZero(block, part + 12, part + 20) == 0, "in the file's block, the 8 bytes the test's own chunk leaves are 0 (" + std::to_string(NotZero(block, part + 12, part + 20)) + " are not)");
			Check(NotZero(block, name + 18, name + 20) == 0, "in the file's block, the 2 bytes after EXTOBJNM's name are 0 (" + std::to_string(NotZero(block, name + 18, name + 20)) + " are not)");
		}

		delete[] block;
	}

	// The file chunk's block for processing (make_data_block_for_process): the same chunks, through the same writers
	size_t const processSize = root.size_chunk_for_process();
	char *const process = root.make_data_block_for_process();

	Check(process != 0 && processSize == size, "the file chunk made a block for processing of " + std::to_string(processSize) + " bytes");

	if (process)
	{
		size_t const part = FindName(process, processSize, "TESTPART");
		size_t const name = FindName(process, processSize, "EXTOBJNM");
		bool const both = part + 20 <= processSize && name + 20 <= processSize;

		Check(both, "both chunks are in the file's block for processing");

		if (both)
		{
			Check(NotZero(process, part + 12, part + 20) == 0, "in the file's block for processing, the 8 bytes the test's own chunk leaves are 0 (" +
				std::to_string(NotZero(process, part + 12, part + 20)) + " are not)");
			Check(NotZero(process, name + 18, name + 20) == 0, "in the file's block for processing, the 2 bytes after EXTOBJNM's name are 0 (" +
				std::to_string(NotZero(process, name + 18, name + 20)) + " are not)");
		}

		delete[] process;
	}
}

int main()
{
	setvbuf(stdout, 0, _IONBF, 0);

	TestOwnChunk();
	TestNameChunk("a");
	TestNameChunk("abcd");
	TestFileBlock();

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
