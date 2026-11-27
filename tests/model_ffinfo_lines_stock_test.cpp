// An exported level's ffinfo.txt lines added to the game's own fastfile\ffinfo.txt (src/model/ffinfo_lines.h). Written 2026-11-27 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". It needs Rebellion's files, so only a machine with the game runs it
// (GitHub's runner leaves out a test whose .runargs asks for the game's files); the hand-made checks are model_ffinfo_lines_test's.
//
// No argument: the team's runner copies the game's fastfile\ffinfo.txt beside this test's program ({FASTFILE:ffinfo.txt}, tests/stock_beside_me.h), and
// the test opens only that file of its own, so nothing it opens comes from its command line (Snyk Code reported the first version, which built the path
// from a stock level's, as a Path Traversal: cpp/PT/test, 2026-11-27).
//   (1) THE POSITIVE CONTROL: the game's own file is the one measured (108 lines, 3,819 bytes), and our line goes into it: the 3,819 bytes kept byte for
//       byte, ours after them with CRLF. A missing file, or another one, FAILS: a check that never ran proves nothing.
//   (2) Install twice: the second adds nothing, and the file is the first one's byte for byte.
//   (3) the owner's Q3 on the real file: a level keeping stock's env\Temple gets its own pack after stock's Snd25.
// RED FIRST (2026-11-27): on a stand-in that refuses everything, 3 of the 4 checks failed; the one that passed only measures the file.

#include <stdint.h>
#include <stdio.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "ffinfo_lines.h"
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

int main()
{
	std::filesystem::path const folder = StockFastfileBesideMe();
	std::filesystem::path const path = folder.empty() ? std::filesystem::path() : folder / L"ffinfo.txt";
	std::ifstream in;

	if (!path.empty())
	{
		in.open(path, std::ios::binary);
	}

	Bytes const stock = in ? Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()) : Bytes();
	size_t lines = 0;

	for (size_t i = 0; i + 1 < stock.size(); i++)
	{
		lines += stock[i] == '\r' && stock[i + 1] == '\n' ? 1 : 0;
	}

	// every check below asks for this too: with no file, or another one, adding to its bytes would prove nothing about the game's
	bool const measured = stock.size() == 3819 && lines == 108;
	Check(measured, "(1) the game's own ffinfo.txt is the one measured: " + path.string() + " (" + std::to_string(stock.size()) + " bytes, " +
		std::to_string(lines) + " CRLF lines)");

	std::string line;
	std::string why;
	bool const made = FfInfo_Line("sound\\Custom\\MyLevel", "Custom_MyLevel_Snd.FFL", &line, &why);
	Bytes out;
	size_t added = 0;
	bool const done = made && FfInfo_Add(stock, { line }, &out, &added, &why);
	Bytes expected = stock;
	std::string const tail = line + "\r\n";
	expected.insert(expected.end(), tail.begin(), tail.end());
	Check(measured && done && added == 1 && out == expected, "(1) our line goes in: the game's 3,819 bytes kept byte for byte, ours after them with CRLF" +
		(why.empty() ? std::string() : " (" + why + ")"));

	Bytes twice;
	size_t addedAgain = 99;
	Check(measured && done && FfInfo_Add(out, { line }, &twice, &addedAgain, &why) && addedAgain == 0 && twice == out, "(2) Install again adds nothing, and the file is as it was");

	std::string temple;
	Bytes withTemple;
	size_t addedTemple = 0;
	bool const templeDone = FfInfo_Line("sound\\env\\Temple", "Custom_MyLevel_Snd.FFL", &temple, &why) && FfInfo_Add(stock, { temple }, &withTemple, &addedTemple, &why);
	Check(measured && templeDone && addedTemple == 1 && withTemple.size() == stock.size() + temple.size() + 2, "(3) the owner's Q3: " + temple + " goes in after stock's sound\\Env\\Temple;Snd25.FFL");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
