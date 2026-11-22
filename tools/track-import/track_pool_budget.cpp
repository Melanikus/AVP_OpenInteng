// track_pool_budget.cpp: whether a track import's tracks fit the level's free memory, packed as the game packs them (track_pool_budget.h says the game's rule and where
// the number comes from). Written 2026-11-21 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "track_pool_budget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TrackPoolBudget TrackPoolBudget_Default()
{
	TrackPoolBudget budget;
	budget.freeBlocks = 4;
	return budget;
}

bool TrackPoolBudget_FromWords(char const *words, TrackPoolBudget *budget, std::string *why)
{
	*budget = TrackPoolBudget();

	// the shape first, character by character, so nothing else ever reaches strtod: digits, then at most one '.' with digits on both sides
	size_t const length = words ? strlen(words) : 0;
	size_t digitsBefore = 0, digitsAfter = 0, dots = 0;

	for (size_t i = 0; i < length && length <= 11; i++)
	{
		if (words[i] == '.')
		{
			dots++;
		}
		else if (words[i] >= '0' && words[i] <= '9')
		{
			(dots ? digitsAfter : digitsBefore)++;
		}
		else
		{
			dots = 2;   // any other character: refused below
			break;
		}
	}

	if (length == 0 || length > 11 || digitsBefore == 0 || dots > 1 || (dots == 1 && digitsAfter == 0))
	{
		*why = "--pool-free-mib takes a plain number of MiB, as 34.63 (digits, at most one '.' between digits, at most 11 characters)";
		return false;
	}

	double const mib = strtod(words, nullptr);

	if (!(mib > 0.0 && mib <= (double)kTrackPoolMostBlocks))
	{
		*why = "--pool-free-mib must be above 0 and at most 40, the level's whole memory";
		return false;
	}

	// whole MiB are free blocks; the rest is what is left in the block in use, rounded down so the budget is never more than was said
	size_t const blocks = (size_t)mib;
	double const rest = (mib - (double)blocks) * (double)kTrackPoolBlockBytes;
	budget->freeBlocks = blocks;
	budget->leftInBlock = rest > 0.0 ? (size_t)rest : 0;
	return true;
}

bool TrackPoolBudget_Fits(TrackPoolBudget const &budget, size_t trackBytes, size_t tracks, size_t *blocksUsed, std::string *why)
{
	*blocksUsed = 0;
	char words[200];   // snprintf always ends the text inside the array, cutting it if it must

	if (tracks == 0 || trackBytes == 0 || trackBytes > kTrackPoolBlockBytes)
	{
		snprintf(words, sizeof(words), "%zu tracks of %zu bytes: an import is at least one track, each at least 1 byte and at most one 1 MiB block", tracks, trackBytes);
		*why = words;
		return false;
	}

	// the game's own order (mempool.cpp PoolAllocateMem): in what is left of the block in use if it fits there, else the next block whole
	size_t left = budget.leftInBlock;
	size_t freeBlocks = budget.freeBlocks;

	for (size_t k = 0; k < tracks; k++)
	{
		if (trackBytes <= left)
		{
			left -= trackBytes;
			continue;
		}

		if (freeBlocks == 0)
		{
			snprintf(words, sizeof(words), "track %zu of %zu does not fit: %zu bytes a track, and the level's free memory (%zu free blocks and %zu bytes left in the block "
				"in use) is used up", k + 1, tracks, trackBytes, budget.freeBlocks, budget.leftInBlock);
			*why = words;
			return false;
		}

		freeBlocks--;
		(*blocksUsed)++;
		left = kTrackPoolBlockBytes - trackBytes;
	}

	return true;
}
