// Every family of chunk tables, in one place. Written 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// The editor describes the chunks it edits in several tables: chunk_fields.cpp's (lights, sounds, generators, the level's settings and
// the module flags) and, for each family added since, one of its own in its own file (field_model.h says why). The code that works on ANY
// chunk (the field panel's record for a chunk, chunk_record.h) asks for a chunk by its 8-letter name, and this file says which family
// holds it.
//
// TWO ASKS.
// - FindAnyChunkKind gives a chunk's ChunkKind and the names its fields use. It's for the families whose chunks are numbers counted from
//   the start of the payload: Base and Acoustics.
// - FindChunkFamily says which family holds a chunk, for EVERY family, including those that describe a chunk in PARTS (the numbers at its
//   head, and the numbers of one item of a list): a track, alternate locations and the AI chunks. Those can't be handed back as a
//   ChunkKind, and are shown one item at a time by code of their own.
//
// EVERY CHUNK IS IN ONE FAMILY. A name claimed by two families, or a name in a family's own tables that the family doesn't claim, is a
// mistake in the tables, and host_chunk_families_test fails on it.
//
// TO ADD A FAMILY: add it to ChunkFamily (field_model.h); add one case to ChunkFamilyClaims and the family to the order in FindChunkFamily
// below; and, if its chunks are ChunkKinds, one block to FindAnyChunkKind: ask the family's own finder, and when it has the chunk, give
// back its kind and, in *names, its lists of named bits and named choices (null and 0 for a list the family has none of). Then say its
// chunks' names in the test's table. Nothing else in the editor changes.

#include <string.h>

#include "acoustics_fields.h"
#include "ai_fields.h"
#include "field_model.h"

namespace model
{
	namespace
	{
		// The two chunks of chunk_fields_tracks.cpp. Its tables (tracks_fields.h) describe a chunk by its fields and say no name, so the
		// names are said here.
		char const TrackName[] = "OBJTRAK2";
		char const AlternateLocationsName[] = "ALTLOCAT";

		// Whether the 8 bytes at id are that name
		bool IsName(char const *id, char const *name)
		{
			return memcmp(id, name, 8) == 0;
		}
	}

	ChunkKind const *FindAnyChunkKind(char const *id, ChunkNames *names)
	{
		if (names)
		{
			names->bits = 0;
			names->bitCount = 0;
			names->choices = 0;
			names->choiceCount = 0;
		}

		// chunk_fields.cpp: lights, sounds, generators, particle generators, the player's start, the level's settings and module flags
		ChunkKind const *const base = FindChunkKind(id);

		if (base)
		{
			if (names)
			{
				names->bits = ChunkBits;
				names->bitCount = ChunkBitCount;
				names->choices = ChunkChoices;
				names->choiceCount = ChunkChoiceCount;
			}

			return base;
		}

		// chunk_fields_acoustics.cpp: module and level acoustics, ambience, the light set's header and its scale. Its tables name no bits.
		ChunkKind const *const acoustics = FindAcousticsKind(id);

		if (acoustics)
		{
			if (names)
			{
				names->choices = AcousticsChoices;
				names->choiceCount = AcousticsChoiceCount;
			}

			return acoustics;
		}

		return 0;
	}

	bool ChunkFamilyClaims(ChunkFamily family, char const *id)
	{
		if (!id)
		{
			return false;
		}

		switch (family)
		{
		case ChunkFamily::Base: return FindChunkKind(id) != 0;
		case ChunkFamily::Acoustics: return FindAcousticsKind(id) != 0;
		case ChunkFamily::Tracks: return IsName(id, TrackName);
		case ChunkFamily::AlternateLocations: return IsName(id, AlternateLocationsName);
		case ChunkFamily::Ai: return FindAiChunk(id) != 0;
		case ChunkFamily::None: return false;
		}

		return false;
	}

	ChunkFamily FindChunkFamily(char const *id)
	{
		ChunkFamily const families[] = { ChunkFamily::Base, ChunkFamily::Acoustics, ChunkFamily::Tracks, ChunkFamily::AlternateLocations,
		                                 ChunkFamily::Ai };

		for (ChunkFamily const family : families)
		{
			if (ChunkFamilyClaims(family, id))
			{
				return family;
			}
		}

		return ChunkFamily::None;
	}
}
