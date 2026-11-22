// The test for the track import's pool budget (tools/track-import/track_pool_budget.h): whether an import's tracks fit the level's free memory, packed as the game packs
// them. Written 2026-11-21 by Melanikus (Frost) for AVP_OpenInteng, Bishop's plan for the owner's track import, step 4 ("track-import --pool-free-mib N replaces the
// fixed 4 MiB"). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY packing and not a sum: the game gives each track's sections as ONE allocation from its level memory (object_setup.cpp:280, PoolAllocateMem), 40 blocks of 1 MiB
// (mempool.cpp). An allocation that doesn't fit what is left of the block in use takes the next block whole, and the rest of the old one is never used again
// (mempool.cpp:92-118). So six tracks of 0.6 MiB need six blocks, though they are only 3.6 MiB: a sum would let them through and, past the 40th block, Rebellion's own game
// writes past its memory. Each check's words say what it proves.
//
// Run from the repository's root through tools/run-unit-tests.ps1 (or Hicks's run-editor-tests.sh). It gives back a non-zero result if any check failed.

#include <stdio.h>
#include <string.h>

#include <string>

#include "track_pool_budget.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static size_t const kMib = 1024u * 1024u;

// The flag's words are hostile input: only a plain number of MiB above 0 and at most 40 is taken
static void TestTheWords()
{
	TrackPoolBudget budget;
	std::string why;

	Check(TrackPoolBudget_FromWords("34.63", &budget, &why) && budget.freeBlocks == 34 && budget.leftInBlock == (size_t)(0.63 * kMib),
		"\"34.63\" (stock Colony in the editor: taken 6 of 40, left 0.63) is 34 free blocks and 0.63 MiB left in the block in use, rounded down");
	Check(TrackPoolBudget_FromWords("40", &budget, &why) && budget.freeBlocks == 40 && budget.leftInBlock == 0, "\"40\" is the whole pool: 40 free blocks");
	Check(TrackPoolBudget_FromWords("0.5", &budget, &why) && budget.freeBlocks == 0 && budget.leftInBlock == kMib / 2, "\"0.5\" is no free block and half a block left");

	char const *const refused[] = { "", "0", "0.0", "40.01", "41", "-1", "+3", "1e3", "nan", "inf", "3.4.5", " 3", "3 ", "4x", ".", "3.", ".5", "123456789012" };
	for (char const *words : refused)
	{
		budget = TrackPoolBudget();
		why.clear();
		bool const took = TrackPoolBudget_FromWords(words, &budget, &why);
		Check(!took && !why.empty() && budget.freeBlocks == 0 && budget.leftInBlock == 0, std::string("refused, with why, the budget left empty: \"") + words + "\"");
	}

	budget = TrackPoolBudget();
	Check(!TrackPoolBudget_FromWords(nullptr, &budget, &why), "refused: no words at all");
}

// Without the flag the import keeps the old rule's room, 4 MiB, now packed: 4 free blocks
static void TestTheDefault()
{
	TrackPoolBudget const budget = TrackPoolBudget_Default();
	Check(budget.freeBlocks == 4 && budget.leftInBlock == 0, "without --pool-free-mib the budget is 4 free blocks, the old 4 MiB");
}

static void TestThePacking()
{
	std::string why;
	size_t blocks = 99;
	size_t const sixTenths = (size_t)(0.6 * kMib);

	TrackPoolBudget four;
	four.freeBlocks = 4;
	Check(!TrackPoolBudget_Fits(four, sixTenths, 6, &blocks, &why) && why.find("track 5 of 6") != std::string::npos,
		"6 tracks of 0.6 MiB don't fit 4 free blocks (one a block: two would be 1.2 MiB), though they are only 3.6 MiB; the 5th is named");
	Check(TrackPoolBudget_Fits(four, sixTenths, 4, &blocks, &why) && blocks == 4, "4 tracks of 0.6 MiB fit 4 free blocks, opening all 4");

	TrackPoolBudget lastOnly;
	lastOnly.leftInBlock = (size_t)(0.63 * kMib);
	Check(TrackPoolBudget_Fits(lastOnly, sixTenths, 1, &blocks, &why) && blocks == 0, "a 0.6 MiB track fits the 0.63 MiB left in the block in use, opening none");
	Check(!TrackPoolBudget_Fits(lastOnly, sixTenths, 2, &blocks, &why), "and a second doesn't: there is no free block");

	TrackPoolBudget oneAndHalf;
	oneAndHalf.freeBlocks = 1;
	oneAndHalf.leftInBlock = kMib / 2;
	size_t const fourTenths = (size_t)(0.4 * kMib);
	Check(TrackPoolBudget_Fits(oneAndHalf, fourTenths, 3, &blocks, &why) && blocks == 1,
		"3 tracks of 0.4 MiB in 1 free block and 0.5 MiB left: the 1st in what is left, the 2nd opens the block, the 3rd fits beside it");
	Check(!TrackPoolBudget_Fits(oneAndHalf, fourTenths, 4, &blocks, &why), "and a 4th doesn't");

	TrackPoolBudget whole;
	whole.freeBlocks = 1;
	Check(TrackPoolBudget_Fits(whole, kMib, 1, &blocks, &why) && blocks == 1, "a track of exactly one block fits one free block");
	Check(!TrackPoolBudget_Fits(whole, kMib + 1, 1, &blocks, &why), "a track over one block never fits: the game gives it memory outside the pool");
	Check(!TrackPoolBudget_Fits(whole, 0, 1, &blocks, &why), "a track of 0 bytes is refused");
	Check(!TrackPoolBudget_Fits(whole, 180, 0, &blocks, &why), "an import of no track is refused");
}

// THE RED CASE (Bishop's plan: "red: 10 MiB on Colony"): 20 tracks of 3,000 sections (180 bytes each, 540,000 bytes a track, 10.3 MiB in all), over the old fixed 4 MiB
static void TestColony()
{
	std::string why;
	size_t blocks = 0;
	size_t const track = 3000 * 180;
	TrackPoolBudget colony;

	Check(TrackPoolBudget_FromWords("34.63", &colony, &why) && TrackPoolBudget_Fits(colony, track, 20, &blocks, &why) && blocks == 19,
		"10.3 MiB of tracks fit stock Colony's 34.63 MiB: the 1st in the 0.63 MiB left, then one a block (two are 1.03 MiB), 19 blocks");

	TrackPoolBudget nine;
	Check(TrackPoolBudget_FromWords("9", &nine, &why) && !TrackPoolBudget_Fits(nine, track, 20, &blocks, &why) && why.find("track 10 of 20") != std::string::npos,
		"and not 9 MiB: 9 blocks hold 9 of them, the 10th is named");
	Check(!TrackPoolBudget_Fits(TrackPoolBudget_Default(), track, 20, &blocks, &why), "and not the default 4 blocks, as before");
}

int main()
{
	TestTheWords();
	TestTheDefault();
	TestThePacking();
	TestColony();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
