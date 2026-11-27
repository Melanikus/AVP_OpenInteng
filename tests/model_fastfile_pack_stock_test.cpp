// The .FFL pack writer and its strict checker (src/model/fastfile_pack.h) on the game's own packs. Written 2026-11-27 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". It needs Rebellion's files, so only a machine with the game runs it (GitHub's
// runner leaves out a test whose .runargs asks for the game's files); the hand-made checks are model_fastfile_pack_test's.
//
// No argument: the team's runner copies the game's fastfile folder beside this test's program ({FASTFILES}, tests/stock_beside_me.h), and the test
// opens only what is in that folder of its own. (Until 2026-11-27 it built the folder from a stock level's path on its command line: a Path Traversal
// as much as model_ffinfo_lines_stock_test's, which Snyk Code reported, cpp/PT/test; it didn't report this one only because the path went through a
// directory listing. Fixed the same way, not left because the scanner didn't see it.)
//   (1) THE POSITIVE CONTROL: every one of the game's 108 packs (2,548 files) passes the checker. A checker that refused stock would refuse ours for nothing,
//       and one that never ran would prove nothing: a missing folder, or fewer packs or files than the game's, FAILS.
//   (2) THE ROUND TRIP: each pack made again by FastFilePack_Build from what the checker listed comes out byte for byte the same, except the bytes after each
//       name's zero, which Rebellion's packer left holding whatever was in its memory (measured 2026-11-27: 2,256 bytes, none of them read by either reader)
//       and ours writes as zeros. That proves the writer's order (_stricmp's), its entry steps, its rounding and its zeros are stock's.
//   (3) common.ffl, in the same folder, is no pack (no "RFFL"; the game's ffinfo.txt doesn't name it): the checker refuses it, a negative control from stock.

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "fastfile_pack.h"
#include "stock_beside_me.h"   // StockFastfileBesideMe: the game's fastfile folder the runner copied beside the program

using namespace model;

typedef std::vector<uint8_t> Bytes;

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

static bool ReadAll(std::filesystem::path const &path, Bytes *bytes)
{
	std::ifstream in(path, std::ios::binary);

	if (!in)
	{
		return false;
	}

	bytes->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return true;
}

static uint32_t U32(Bytes const &b, size_t at)
{
	return (uint32_t)b[at] | (uint32_t)b[at + 1] << 8 | (uint32_t)b[at + 2] << 16 | (uint32_t)b[at + 3] << 24;
}

int main()
{
	std::filesystem::path const folder = StockFastfileBesideMe();
	std::error_code error;
	Check(!folder.empty() && std::filesystem::is_directory(folder, error), "the game's fastfile folder the runner copied is there, beside the test: " +
		folder.string());

	size_t packs = 0;
	size_t files = 0;
	size_t passed = 0;
	size_t same = 0;
	size_t padsDiffering = 0;
	bool commonRefused = false;
	std::string firstWrong;

	for (std::filesystem::directory_entry const &entry : std::filesystem::directory_iterator(folder, error))
	{
		std::string name = entry.path().filename().string();
		std::string small = name;

		for (char &c : small)
		{
			c = (char)tolower((unsigned char)c);
		}

		if (small.size() < 4 || small.compare(small.size() - 4, 4, ".ffl") != 0)
		{
			continue;
		}

		Bytes bytes;

		if (!ReadAll(entry.path(), &bytes))
		{
			Check(false, "read " + name);
			continue;
		}

		std::vector<PackListed> listed;
		std::string why;
		bool const ok = FastFilePack_Check(bytes.data(), bytes.size(), &listed, &why);

		if (small == "common.ffl")
		{
			commonRefused = !ok && why.find("RFFL") != std::string::npos;
			continue;
		}

		packs++;

		if (!ok)
		{
			if (firstWrong.empty())
			{
				firstWrong = name + ": " + why;
			}

			continue;
		}

		passed++;
		files += listed.size();

		// (2) made again from the list
		uint32_t const entryBytes = U32(bytes, 12);
		std::vector<PackFile> again;

		for (PackListed const &l : listed)
		{
			PackFile f;

			f.name = l.name;
			f.bytes.assign(bytes.begin() + 20 + entryBytes + l.offset, bytes.begin() + 20 + entryBytes + l.offset + l.length);
			again.push_back(f);
		}

		Bytes rebuilt;
		PackRefused refused = PackRefused::None;
		size_t which = 0;

		if (!FastFilePack_Build(again, &rebuilt, &refused, &which) || rebuilt.size() != bytes.size())
		{
			if (firstWrong.empty())
			{
				firstWrong = name + ": made again, " + FastFilePack_RefusedWords(refused) + " (" + std::to_string(rebuilt.size()) + " of " + std::to_string(bytes.size()) + " bytes)";
			}

			continue;
		}

		// every byte the same but those after each name's zero: those are put to stock's on the copy, counted, then the two compared whole
		size_t at = 20;

		for (PackListed const &l : listed)
		{
			size_t const step = 8 + ((l.name.size() + 4) & ~(size_t)3);

			for (size_t p = at + 8 + l.name.size() + 1; p < at + step; p++)
			{
				padsDiffering += rebuilt[p] != bytes[p] ? 1 : 0;
				rebuilt[p] = bytes[p];
			}

			at += step;
		}

		if (rebuilt == bytes)
		{
			same++;
		}
		else if (firstWrong.empty())
		{
			size_t p = 0;

			while (p < bytes.size() && rebuilt[p] == bytes[p])
			{
				p++;
			}

			firstWrong = name + ": made again, it differs first at byte " + std::to_string(p);
		}
	}

	Check(packs == 108 && files == 2548, "(1) the game's 108 packs and 2,548 files are there (" + std::to_string(packs) + " packs, " + std::to_string(files) + " files)");
	Check(packs > 0 && passed == packs, "(1) every one passes the checker (" + std::to_string(passed) + " of " + std::to_string(packs) + ")" +
		(firstWrong.empty() ? std::string() : "; the first wrong: " + firstWrong));
	Check(packs > 0 && same == packs, "(2) every one made again from its list is the same, byte for byte but the bytes after the names' zeros (" + std::to_string(same) +
		" of " + std::to_string(packs) + "; " + std::to_string(padsDiffering) + " of those bytes aren't zeros in stock)");
	Check(padsDiffering == 2256, "(2) the leftover bytes after the names' zeros are the 2,256 measured (" + std::to_string(padsDiffering) + ")");
	Check(commonRefused, "(3) common.ffl, no pack, is refused for its signature");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
