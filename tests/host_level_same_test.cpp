// The test of the "nothing changed in the level" check (tests/level_same.h, the verifier verify_level_same): two level files compared chunk by
// chunk, every byte but the version counters every save moves on. Written 2026-11-17 by Melanikus (Bishop) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_level_same_test.exe <a folder it may write its copies into> <a stock level .rif> [<more levels>]. The levels are only read; the
// copies in the folder are the only files written. Every chunk is compared, FRMMORPH too (above TestLevel says why that is worth saying).
// CHECKS, on every level named: two saves of the untouched level, made as inteng's save makes them (the game's library: load, then
// prepare_for_output and write, twice: the second from the first) are NOT the same bytes, and the check says SAME, with version counters
// ignored (a); the packed level and its unpacked copy are SAME with no counter ignored, and a packed save and an unpacked one too (d).
// On the first level also: one byte changed in a polygon's flags is DIFFERENT, named by its chunk's path and offset (b), and SAME when that
// chunk is skipped by name; two objects' chunks swapped is DIFFERENT though every chunk's name and size are still there (c); a truncated
// file (unpacked and packed), a chunk's size pointing past the end (at the top and inside a shape), a wrong heading and a level's size
// field that isn't its size are refused, each for its reason (e); a chunk holding 16,385 chunks, and 16 holding 16,384 each, are refused
// before the game's loader is given them, its time growing as the square of the chunks inside one chunk, and one holding 16,384 is read (f).

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "huffman.hpp"
#include "level_same.h"

static int checks = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// what the game's chunk code calls
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

static bool WriteWholeFile(std::string const &path, std::vector<char> const &bytes)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD put = 0;
	bool const good = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &put, 0) && put == bytes.size();
	return CloseHandle(file) && good;
}

// The level inside a file: the file itself, or what the game's checked unpacker makes of a packed one (empty if it can't)
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

// A level packed as the game packs it (the editor's save of a packed level: level_file.cpp, step 1)
static std::vector<char> Packed(std::vector<char> const &level)
{
	std::vector<char> out;
	std::unique_ptr<HuffmanPackage, void (*)(void *)> const package(HuffmanCompression((unsigned char *)level.data(), (int)level.size()), free);

	if (package)
	{
		size_t const bytes = (size_t)package->CompressedDataSize + sizeof(HuffmanPackage);
		out.assign((char const *)package.get(), (char const *)package.get() + bytes);
	}

	return out;
}

static std::string scratchFolder;

// What inteng's save writes for a level (File_Chunk::write_file, mishchnk.cpp: prepare_for_output() on the whole tree, then the tree's own
// bytes), done straight on the game's library, in a file of this test's own, into a block of zeros as the editor's save does
// (level_file.cpp, PrepareAsIntengDoes). Empty if the library refuses the level.
static std::vector<char> AsIntengSavesIt(std::vector<char> const &level)
{
	std::string const path = scratchFolder + "prepared.tmp";
	std::vector<char> out;

	if (!WriteWholeFile(path, level))
	{
		return out;
	}

	auto *const before = Parent_File;

	{
		std::unique_ptr<File_Chunk> const tree(new File_Chunk(path.c_str()));

		if (tree->error_code == 0)
		{
			tree->prepare_for_output();
			size_t const size = tree->size_chunk();
			std::vector<char> block(size, 0);
			tree->fill_data_block(block.data());
			out.swap(block);
		}
	}

	Parent_File = before;
	DeleteFileA(path.c_str());
	return out;
}

// One chunk's place in a level's bytes, found by walking them (the test's own walk, each size checked against the bytes there)
struct Place
{
	std::string name;
	size_t at;
	size_t bytes;
};

static std::vector<Place> ChunksBetween(std::vector<char> const &level, size_t from, size_t to)
{
	std::vector<Place> found;

	for (size_t at = from; at < to && to <= level.size(); )
	{
		int size = 0;

		if (to - at < 12)
		{
			return std::vector<Place>();
		}

		memcpy(&size, level.data() + at + 8, sizeof(size));

		if (size < 12 || (size_t)size > to - at)
		{
			return std::vector<Place>();
		}

		found.push_back(Place{ std::string(level.data() + at, 8), at, (size_t)size });
		at += (size_t)size;
	}

	return found;
}

static void PutInt(std::vector<char> &bytes, size_t at, int value)
{
	memcpy(bytes.data() + at, &value, sizeof(value));
}

// A copy of a level with chunks added at its end: holders chunks named SPECLOBJ (a kind the game's loader holds chunks in, wherever it
// is), each holding count empty chunks of 12 bytes named JUNKJUNK (plain bytes to the loader); the level's own size field made right
static std::vector<char> WithHoldersAtTheEnd(std::vector<char> const &level, size_t holders, size_t count)
{
	std::vector<char> out = level;
	std::vector<char> holder(12 + 12 * count, 0);
	memcpy(holder.data(), "SPECLOBJ", 8);
	PutInt(holder, 8, (int)holder.size());

	for (size_t n = 0; n < count; n++)
	{
		memcpy(holder.data() + 12 + 12 * n, "JUNKJUNK", 8);
		PutInt(holder, 12 + 12 * n + 8, 12);
	}

	for (size_t n = 0; n < holders; n++)
	{
		out.insert(out.end(), holder.begin(), holder.end());
	}

	PutInt(out, 8, (int)out.size());
	return out;
}

static size_t BytesThatDiffer(std::vector<char> const &a, std::vector<char> const &b)
{
	size_t count = a.size() > b.size() ? a.size() - b.size() : b.size() - a.size();

	for (size_t n = 0; n < a.size() && n < b.size(); n++)
	{
		count += a[n] != b[n];
	}

	return count;
}

// The check's answer, as the verifier's first line says it, and its first difference or reason
static std::string Said(LevelSameReport const &report)
{
	std::string words = report.answer == LevelSameAnswer::Same ? "SAME" : report.answer == LevelSameAnswer::Different ? "DIFFERENT" : "REFUSED";
	words += " (" + std::to_string(report.chunks) + " chunks, " + std::to_string(report.versionsIgnored) + " version counters ignored, " +
		std::to_string(report.differences) + " differences)";

	if (!report.lines.empty())
	{
		words += ": " + report.lines[0];
	}

	return words;
}

static bool Has(LevelSameReport const &report, char const *words)
{
	return !report.lines.empty() && report.lines[0].find(words) != std::string::npos;
}

static LevelSameReport Compare(std::string const &a, std::string const &b, std::vector<std::string> const &ignored = std::vector<std::string>())
{
	return CompareLevelFiles(a.c_str(), b.c_str(), ignored);
}

// A hostile copy in the scratch folder, and the check's answer for it against a good level
static LevelSameReport HostileAgainst(std::string const &good, std::string const &name, std::vector<char> const &bytes)
{
	std::string const path = scratchFolder + name;

	if (!WriteWholeFile(path, bytes))
	{
		Check(false, "the hostile copy " + name + " was written");
	}

	return Compare(path, good);
}

static std::string BaseName(std::string const &path)
{
	size_t const slash = path.find_last_of("\\/");
	std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
	size_t const dot = name.find_last_of('.');

	if (dot != std::string::npos)
	{
		name.resize(dot);
	}

	for (char &letter : name)
	{
		if (!((letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') || letter == '_' || letter == '-'))
		{
			letter = '_';
		}
	}

	return name;
}

// MORPH FRAMES (found by this test, 2026-11-17; fixed in avpx facfb49c): the library's save of a morph of two or more frames
// (Shape_Morphing_Frame_Data_Chunk::prepare_for_output, avpx 3dc/win95/chunk_shape.cpp) wrote 2 numbers a frame after the first that it never
// set, so two saves of the same level differed there and FRMMORPH had to be skipped on those levels. It now writes the file's own numbers
// there, so nothing is skipped; tests/host_level_same_test.runargs names the 10 stock levels with such a morph (Sulaco, Sulaco_a, Sulaco_p,
// Stat101, Stat101_M, Vaults, Vaults_M, Area52, furyall_a, odobenus).
static void TestLevel(std::string const &stockPath, bool everything)
{
	std::string const base = scratchFolder + BaseName(stockPath);
	printf("%s\n", stockPath.c_str());

	std::vector<char> file;
	Check(ReadWholeFile(stockPath, &file), "the level reads");

	std::vector<char> const stock = LevelIn(file);
	std::vector<char> const save1 = AsIntengSavesIt(stock);
	std::vector<char> const save2 = AsIntengSavesIt(save1);

	if (stock.empty() || save1.empty() || save2.empty())
	{
		Check(false, "the level unpacks, and the game's library saves it twice as inteng does");
		return;
	}

	// The reason this check exists: two saves of the untouched level are not the same bytes
	Check(save1.size() == save2.size() && save1 != save2, "two saves of the untouched level differ in " + std::to_string(BytesThatDiffer(save1, save2)) +
		" bytes, with the same size (the version counters moved on)");

	std::string const save1Path = base + "-save1.rif", save2Path = base + "-save2.rif", stockUnpacked = base + "-stock-unpacked.rif",
		save1Packed = base + "-save1-packed.rif";
	std::vector<char> const save1PackedBytes = Packed(save1);
	Check(WriteWholeFile(save1Path, save1) && WriteWholeFile(save2Path, save2) && WriteWholeFile(stockUnpacked, stock) && !save1PackedBytes.empty() &&
		WriteWholeFile(save1Packed, save1PackedBytes), "the copies are written");

	// (a) two saves of the untouched level: SAME, some version counters ignored
	LevelSameReport const a = Compare(save1Path, save2Path);
	Check(a.answer == LevelSameAnswer::Same && a.versionsIgnored > 0 && a.chunks > 0 && a.differences == 0, "(a) two saves of the untouched level: " + Said(a));

	// (d) packed and unpacked copies of the same level: SAME, nothing ignored; and a packed save against an unpacked one
	LevelSameReport const d1 = Compare(stockPath, stockUnpacked);
	Check(d1.answer == LevelSameAnswer::Same && d1.versionsIgnored == 0 && d1.chunks == a.chunks, "(d) the packed stock level and its unpacked copy: " + Said(d1));
	LevelSameReport const d2 = Compare(save1Packed, save1Path);
	Check(d2.answer == LevelSameAnswer::Same && d2.versionsIgnored == 0, "(d) a save packed as the game packs it, and the same save unpacked: " + Said(d2));
	LevelSameReport const d3 = Compare(save1Packed, save2Path);
	Check(d3.answer == LevelSameAnswer::Same && d3.versionsIgnored == a.versionsIgnored, "(d) the first save packed, the second unpacked: " + Said(d3));

	// What the file the editor opened and its first save are, for the record ({orig} against {copy} in a scenario)
	printf("  info  the stock file against its first save: %s\n", Said(Compare(stockPath, save1Path)).c_str());

	if (!everything)
	{
		return;
	}

	std::vector<Place> const top = ChunksBetween(save2, 12, save2.size());
	Check(!top.empty(), "the save's chunks are walked by the test");

	// (b) one byte of a polygon changed (the first polygon's flags, 8 bytes into the first shape's SHPPOLYS that holds one)
	{
		size_t polygons = 0, shapeNumber = 0, seen = 0;

		for (Place const &chunk : top)
		{
			if (chunk.name == "REBSHAPE")
			{
				seen++;

				for (Place const &inside : ChunksBetween(save2, chunk.at + 12, chunk.at + chunk.bytes))
				{
					if (inside.name == "SHPPOLYS" && inside.bytes >= 12 + 36 && !polygons)
					{
						polygons = inside.at;
						shapeNumber = seen;
					}
				}
			}
		}

		Check(polygons != 0, "the save has a shape with a polygon");

		if (polygons)
		{
			std::vector<char> changed = save2;
			changed[polygons + 12 + 8] ^= 0x10;
			std::string const path = base + "-save2-poly.rif";
			Check(WriteWholeFile(path, changed), "the copy with one polygon byte changed is written");

			// the path the difference must be named by
			std::string const where = "REBINFF2/REBSHAPE#" + std::to_string(shapeNumber) + "/SHPPOLYS offset 8:";
			LevelSameReport const b = Compare(save1Path, path);
			Check(b.answer == LevelSameAnswer::Different && b.differences == 1 && Has(b, where.c_str()),
				"(b) one polygon byte changed is DIFFERENT, named \"" + where + "\": " + Said(b));
			LevelSameReport const b2 = Compare(save2Path, path);
			Check(b2.answer == LevelSameAnswer::Different && b2.differences == 1 && b2.versionsIgnored == 0, "(b) against the save it was made from: " + Said(b2));
			LevelSameReport const b3 = Compare(save1Path, path, std::vector<std::string>{ "SHPPOLYS" });
			Check(b3.answer == LevelSameAnswer::Same && b3.chunksSkipped > 0, "(b) with SHPPOLYS skipped by name it is SAME: " + Said(b3) + ", " +
				std::to_string(b3.chunksSkipped) + " skipped");
		}
	}

	// (c) two objects' chunks swapped: every name and size is still there, only the order changed
	{
		size_t first = top.size();

		for (size_t n = 0; n + 1 < top.size(); n++)
		{
			if (top[n].name == "RBOBJECT" && top[n + 1].name == "RBOBJECT" &&
				(top[n].bytes != top[n + 1].bytes || memcmp(save1.data() + top[n].at, save1.data() + top[n + 1].at, top[n].bytes) != 0))
			{
				first = n;
				break;
			}
		}

		Check(first < top.size(), "the save has two objects side by side that differ");

		if (first < top.size())
		{
			// (save1 and save2 have the same places: the same size, the counters only moved on)
			Place const &one = top[first], &two = top[first + 1];
			std::vector<char> swapped(save1.begin(), save1.begin() + one.at);
			swapped.insert(swapped.end(), save1.begin() + two.at, save1.begin() + two.at + two.bytes);
			swapped.insert(swapped.end(), save1.begin() + one.at, save1.begin() + one.at + one.bytes);
			swapped.insert(swapped.end(), save1.begin() + two.at + two.bytes, save1.end());
			std::string const path = base + "-save1-swapped.rif";
			Check(swapped.size() == save1.size() && WriteWholeFile(path, swapped), "the copy with two objects swapped is written");

			LevelSameReport const c = Compare(save1Path, path);
			Check(c.answer == LevelSameAnswer::Different && Has(c, "REBINFF2/RBOBJECT#"), "(c) two objects swapped is DIFFERENT: " + Said(c));
		}
	}

	// (e) hostile copies, each refused (exit 2) for its reason
	{
		std::vector<char> truncated(save1.begin(), save1.begin() + save1.size() / 2);
		LevelSameReport const e1 = HostileAgainst(save1Path, "hostile-truncated.rif", truncated);
		Check(e1.answer == LevelSameAnswer::Refused && Has(e1, "size field"), "(e) a save cut in half is refused for its level's size field: " + Said(e1));

		std::vector<char> truncatedPacked(file.begin(), file.begin() + file.size() / 2);
		LevelSameReport const e2 = HostileAgainst(save1Path, "hostile-truncated-packed.rif", truncatedPacked);
		// (the game's unpacker reads zero bits past the end of a package, huffman.cpp, so a cut package still unpacks: its own heading's size is what tells)
		Check(e2.answer == LevelSameAnswer::Refused && Has(e2, "bytes of packed data"), "(e) a packed level cut in half is refused for its package's size: " + Said(e2));

		std::vector<char> pastTheEnd = save1;
		PutInt(pastTheEnd, 12 + 8, (int)(save1.size() - 12) + 4);
		LevelSameReport const e3 = HostileAgainst(save1Path, "hostile-past-the-end.rif", pastTheEnd);
		Check(e3.answer == LevelSameAnswer::Refused && Has(e3, "past the end"), "(e) the first chunk's size 4 bytes past the end is refused: " + Said(e3));

		// inside the first shape: its first chunk's size reaches 4 bytes past the shape's end (still inside the file)
		size_t shape = top.size();

		for (size_t n = 0; n < top.size(); n++)
		{
			if (top[n].name == "REBSHAPE")
			{
				shape = n;
				break;
			}
		}

		if (shape < top.size())
		{
			std::vector<char> inside = save1;
			PutInt(inside, top[shape].at + 12 + 8, (int)(top[shape].bytes - 12) + 4);
			LevelSameReport const e4 = HostileAgainst(save1Path, "hostile-inside-past-the-end.rif", inside);
			Check(e4.answer == LevelSameAnswer::Refused && Has(e4, "loader refuses"), "(e) a chunk inside a shape whose size goes past the shape's end is refused: " + Said(e4));
		}

		std::vector<char> heading = save1;
		heading[7] = '3';
		LevelSameReport const e5 = HostileAgainst(save1Path, "hostile-heading.rif", heading);
		Check(e5.answer == LevelSameAnswer::Refused && Has(e5, "not a level"), "(e) a file that starts REBINFF3 is refused: " + Said(e5));

		std::vector<char> levelSize = save1;
		PutInt(levelSize, 8, (int)save1.size() + 4);
		LevelSameReport const e6 = HostileAgainst(save1Path, "hostile-level-size.rif", levelSize);
		Check(e6.answer == LevelSameAnswer::Refused && Has(e6, "size field"), "(e) a level whose size field is 4 more than its bytes is refused: " + Said(e6));

		LevelSameReport const e7 = Compare(save1Path, scratchFolder + "not-there.rif");
		Check(e7.answer == LevelSameAnswer::Refused && Has(e7, "not-there.rif"), "(e) a file that isn't there is refused, named: " + Said(e7));
	}

	// (f) more chunks inside one chunk, or in all, than the game's loader makes in a few seconds: refused before the loader is given the
	// level (its time grows as the square of the chunks inside one chunk: level_same.cpp, kMostInOneChunk, 16,384)
	{
		DWORD const started = GetTickCount();
		LevelSameReport const f1 = HostileAgainst(save1Path, "hostile-one-holds-16385.rif", WithHoldersAtTheEnd(save1, 1, 16385));
		Check(f1.answer == LevelSameAnswer::Refused && Has(f1, "inside one chunk") && Has(f1, "SPECLOBJ"),
			"(f) a chunk holding 16,385 chunks is refused before the game's loader is given it, the chunk named: " + Said(f1));

		LevelSameReport const f2 = HostileAgainst(save1Path, "hostile-one-holds-16384.rif", WithHoldersAtTheEnd(save1, 1, 16384));
		Check(f2.answer == LevelSameAnswer::Different && Has(f2, "the first more is SPECLOBJ"),
			"(f) a chunk holding 16,384 chunks (the most) is read, and DIFFERENT for the chunk added: " + Said(f2));

		LevelSameReport const f3 = HostileAgainst(save1Path, "hostile-16-hold-16384.rif", WithHoldersAtTheEnd(save1, 16, 16384));
		Check(f3.answer == LevelSameAnswer::Refused && Has(f3, "in all, before the game's loader"),
			"(f) 16 chunks holding 16,384 each (more than 2^18 chunks in all) are refused before the game's loader is given them: " + Said(f3));
		printf("  info  (f) took %lu ms\n", (unsigned long)(GetTickCount() - started));
	}
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("usage: host_level_same_test <a folder it may write into> <a level .rif> [<more levels>]\n");
		return 1;
	}

	scratchFolder = std::string(argv[1]) + "\\";

	int tested = 0;

	for (int n = 2; n < argc; n++)
	{
		TestLevel(argv[n], tested++ == 0);
	}

	Check(tested > 0, "a level was tested");

	printf("host_level_same_test: %d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
