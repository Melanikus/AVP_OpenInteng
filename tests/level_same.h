// level_same.h: "nothing changed in the level": two level files compared chunk by chunk, every byte, except the version counters every save
// moves on. The check behind the scenario verifier verify_level_same (tests/scenarios/verify_level_same.cpp) and its unit test
// (tests/host_level_same_test.cpp). Written 2026-11-17 by Melanikus (Bishop) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHY IT IS NEEDED. Two saves of the same untouched level are never the same bytes, by design: every save runs inteng's prepare_for_output
// (File_Chunk::write_file, avpx 3dc/win95/mishchnk.cpp), which moves on the version counter of every object header (OBJHEAD1), shape header
// (SHPHEAD1), environment header (ENDTHEAD), enum header (ENUMHEAD) and sprite header (ASPRHEAD); and the game's levels are packed
// (Huffman-coded, "REBCRIF1"), so one changed byte spreads over the whole file. Two developers lost 45 minutes to this on 2026-11-17 and
// each wrote a check of their own for it; this is the one check for everyone.
//
// WHAT IT DOES. Each file is read whole (at most 32 MiB, the most the game reads), unpacked with the game's own checked decoder when it is
// packed, and loaded by the game's own loader, which says which chunks hold other chunks (its own kinds, as it loads them) and which kind
// each chunk is. The two files' chunks are then walked side by side, in the files' order: each chunk's name, its size, and every byte it
// holds are compared, except the 4 bytes of the version counter in a chunk of the five kinds above, which are taken from where the
// library's own reader and writer keep them (level_same.cpp says where, file and line). Nothing else is skipped unless the caller names a
// chunk to skip (ignored: the chunk must still be there, at the same place in both, but its size and what it holds are not compared).
//
// WHAT TO COMPARE: two saves, one made before the action and one after it ({save1} {save2} in a scenario). Not the game's own file against a
// save: the game's files hold stray bytes in the rounding after a name (the 1 to 3 bytes after its closing zero, up to the next 4), which
// every save writes as zeros (level_file.cpp), so a stock level and its first save differ there, and this check names each one (Invasion:
// 1,073 such bytes in 499 chunks, besides its 926 version counters; counted 2026-11-17).
//
// BOTH FILES ARE HOSTILE INPUT. Every size is checked against the bytes really there before it is used; the chunks inside chunks go no
// deeper than the loader's own limit (64); a level of more chunks than the editor opens (2^18) is refused; nothing is read past a buffer.
// The game's loader takes time that grows as the square of the chunks inside one chunk, so before it is given a level, a level that
// could hold more than 16,384 chunks inside one chunk, or 2^18 in all, is refused (level_same.cpp, kMostInOneChunk, says why).

#pragma once

#include <stddef.h>

#include <string>
#include <vector>

enum class LevelSameAnswer
{
	Same,        // every chunk the same, but for the version counters
	Different,   // at least one difference; lines says which
	Refused,     // a file isn't a level this can read (lines holds the one reason)
};

// At most this many differences are written out (the count of all of them is kept)
size_t const kLevelSameMostLines = 20;

struct LevelSameReport
{
	LevelSameAnswer answer = LevelSameAnswer::Refused;
	size_t chunks = 0;              // the chunks walked in the first file (the level's own heading, REBINFF2, not counted)
	size_t versionsIgnored = 0;     // the version counters that differ between the two, which were left out of the comparison
	size_t chunksSkipped = 0;       // the chunks of the names asked to be skipped, found at the same place in both
	size_t differences = 0;         // every difference found (lines holds the first kLevelSameMostLines of them)
	std::vector<std::string> lines; // each difference: the chunk's path, the offset in what it holds, and the two values; or why a file is refused
};

// Compares two level files as above. ignored: chunk names (1 to 8 letters) whose chunks are not compared, matched against a chunk's name up to
// its first zero byte, as the library looks names up. The report says how it came out.
LevelSameReport CompareLevelFiles(char const *first, char const *second, std::vector<std::string> const &ignored);

// The report as the verifier prints it: "SAME (n chunks, m version counters ignored)", or "DIFFERENT (...)" and a line for each difference
// written out, or "REFUSED ..."; gives back the exit code: 0 same, 1 different, 2 refused.
int PrintLevelSameReport(LevelSameReport const &report);
