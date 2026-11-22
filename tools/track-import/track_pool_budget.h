// track_pool_budget.h: whether a track import's tracks fit the level's free memory, packed as the game packs them. Written 2026-11-21 by Melanikus (Frost) for
// AVP_OpenInteng, Bishop's plan for the owner's track import, step 4 ("track-import --pool-free-mib N replaces the fixed 4 MiB"). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// THE GAME'S RULE (avpx 3dc/avp/mempool.cpp): a level's memory is 40 blocks of 1 MiB, handed out in order. Each track's sections are ONE allocation (object_setup.cpp:280,
// PoolAllocateMem). One that doesn't fit what is left of the block in use takes the next block whole, and the rest of the old block is never used again (mempool.cpp:92-118).
// Past the 40th block, AvPx carries on in memory of its own, but Rebellion's 1999 game writes past its memory: the import must never need more than the level has free.
//
// WHERE THE NUMBER COMES FROM: open the level in the editor; its state line's "levelmemory" says "taken N of 40 left X ..." (level_view.cpp, LevelView_MemoryWords). The
// level's free memory is (40 - N) + X MiB: whole free blocks, and X left in the block in use. Stock Colony: taken 6, left 0.63, so 34.63.
//
// WHAT IS COUNTED: each track's sections, the one large allocation (180 bytes a section). Each track object also takes a few small pieces from the same memory (its
// controller, object_setup.cpp:262; its tools template, :934; its strategy's block and name, :607-619), and the tracks are allocated while the level is built, among its
// other things, so the rest of the build packs a little differently with them in. The import keeps a margin for both by being given a little less than the level's free
// memory (the README says how much, from the game's own count against the editor's). The proof of any import is the editor's own count of the level written: open it,
// and its "levelmemory" must say "outside 0.00".

#pragma once

#include <stddef.h>

#include <string>

// The level's free memory as the game hands it out: whole free 1 MiB blocks, and what is left in the block in use
struct TrackPoolBudget
{
	size_t freeBlocks = 0;
	size_t leftInBlock = 0;
};

size_t const kTrackPoolBlockBytes = 1024u * 1024u;   // mempool.cpp MEMORY_BLOCK_SIZE
size_t const kTrackPoolMostBlocks = 40;              // mempool.cpp MAX_NUM_MEMORY_BLOCK

// Without --pool-free-mib: 4 free blocks, the room the import always kept (the old rule's 4 MiB), now packed as the game packs it
TrackPoolBudget TrackPoolBudget_Default();

// The budget for "--pool-free-mib <words>". The words are hostile input: only a plain number of MiB is taken, digits with at most one '.' between digits, above 0 and at
// most 40, at most 11 characters. The whole MiB are free blocks, the rest what is left in the block in use, rounded down to a byte. False, and why, for anything else, with
// the budget left empty.
bool TrackPoolBudget_FromWords(char const *words, TrackPoolBudget *budget, std::string *why);

// Whether `tracks` tracks of `trackBytes` each fit the budget, each one allocation packed as the game packs it: in what is left of the block in use if it fits there, else
// in the next free block. blocksUsed: how many free blocks they open. False, and why (the first track that doesn't fit, as "track K of N"), when they don't, or when a track
// is 0 bytes or over one block (the game gives that memory outside its blocks), or there is no track.
bool TrackPoolBudget_Fits(TrackPoolBudget const &budget, size_t trackBytes, size_t tracks, size_t *blocksUsed, std::string *why);
