// The names in a level file: where the game reads each kind, how long the game lets it be, and so how long the editor lets it be.
// Written 2026-11-14 by Melanikus for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT THIS IS. Many chunks in a level hold a name: an object's, a sound file's, a generator's. The editor shows each in a text box,
// and the box has to know how many bytes the name may have. The rule is that the editor never takes a longer name than the game
// itself keeps or uses, so a name typed in the editor is never cut short or refused when the game loads the level.
//
// WHERE THE FACTS COME FROM. Each kind of name gives the game's own reader for it at the pinned AvPx commit (file:line, under the
// game's 3dc folder), and, where the game keeps or uses fewer bytes than the chunk could hold, the line that sets that limit. The
// full study, with every place the game copies each name after reading it, is the name-use trace (AvP-docs, private, 2026-11-14).
//
// A NAME THE GAME OPENS AS A FILE (a sound, a picture) has a second limit: the game puts a folder or another name and a "/" in front,
// and the whole path has to fit what it opens. So such a name is checked together with the name that goes in front of it.
//
// Nothing here reads a file or uses the game's globals.

#pragma once

#include <stddef.h>

#include "field_model.h"

namespace model
{
	// How a name is kept in its chunk.
	enum class NameStorage
	{
		Ended,  // its letters, then a zero byte: only the chunk limits it, unless the game keeps fewer (gameMost)
		Fixed,  // a field of exactly gameMost bytes: the letters, then zeros when there are fewer, and no zero at all when it's full
	};

	// The most bytes the editor lets a name have when the game sets no lower limit. It keeps every name in the game's own levels
	// (the longest is 82 bytes, a generator's name) with room to spare, and it's the most one byte's worth of length can count.
	unsigned const NameMostWithNoGameLimit = 255;

	// One kind of name: one name field in one kind of chunk.
	struct NameKind
	{
		char const *chunk;           // the chunk it's in, as its 8-letter name ("OBJHEAD1")
		char const *label;           // what it is, in words; unique within its chunk
		char const *reader;          // where the game reads it at the pinned AvPx commit ("win95/chunk_object.cpp:440")
		NameStorage storage;
		unsigned gameMost;           // the most bytes (without the zero) the game keeps or uses; 0 when only the chunk limits it
		char const *gameMostSource;  // where that limit is in the game's code, at the pinned commit ("" when gameMost is 0)
		unsigned most;               // the most bytes the editor takes: gameMost when there is one, NameMostWithNoGameLimit otherwise
		char const *mostWhy;         // in plain words: why that's the most (shown when a name is refused, and tested)
		char const *pathPrefix;      // for a name the game opens as a file: the chunk whose name it puts in front, with a "/"
		                             // between, or "" when nothing goes in front. 0 for a name the game never opens.
		unsigned pathMostWithPrefix; // with that name and the "/" in front, the most bytes the whole path may have (0 if no prefix)
		char const *pathMostSource;  // where that limit is in the game's code ("" when there's no prefix)
		bool required;               // the game can't use it empty
		char const *requiredWhy;     // why ("" when it isn't required)
		bool shown;                  // false for a field the editor keeps byte for byte and never shows
		Limit requiredLimit;         // who makes it required (the same words as a field's limit): Limit::Loader when the pinned loader refuses a
		                             // level that has this name empty or missing; Limit::GameMeaning when the loader takes it and the game only goes
		                             // on without the file or sound it names; Limit::None (a row that leaves it out) when the name isn't required
		char const *requiredSource;  // where that is in the game's code, at the pinned commit; none (a row that leaves it out) when it isn't required
	};

	// ---- The table (name_kinds.cpp) ----

	extern NameKind const NameKinds[];
	extern size_t const NameKindCount;

	// The kind with that chunk and label, or 0 if the editor doesn't know it. chunk is the chunk's 8-letter name as a level file holds
	// it: exactly its 8 bytes are compared, and no zero is needed after them (a shorter string ending in a zero simply matches nothing).
	NameKind const *FindNameKind(char const *chunk, char const *label);

	// The first kind in that chunk, or 0. For a chunk that holds just one name (SOUNDDIR, RIFFNAME), that's the name. chunk is as
	// for FindNameKind.
	NameKind const *FirstNameKindIn(char const *chunk);

	// ---- The rule ----

	// Whether a name of length bytes (without its zero) may be stored as that kind. For a name the game opens with another name in
	// front, prefixLength is that name's length when the game will put it there, or 0 when it won't (the level has no sound folder,
	// or the picture is an IFF one, which the game opens by its own name). When it may not, *why (if not 0) says why: the kind's
	// mostWhy, its requiredWhy, or PathTooLongReason.
	bool NameLengthIsAllowed(NameKind const &kind, size_t length, size_t prefixLength, char const **why);

	// The reason NameLengthIsAllowed gives when the name fits on its own but not with the name the game puts in front of it.
	extern char const PathTooLongReason[];

	// Whether a name the game opens as a file may be stored: its length passes NameLengthIsAllowed, and the path the game would
	// build from it passes the game's own rule for a file name from a level (LevelFileName_WhyRefused, avpx/3dc/level_file_name.h):
	// only its letters, nothing that goes up a folder or starts at the top, no part ending in a dot or a space, no device's name.
	// - name and length are the name's bytes. A zero byte among them is refused, as the game would read the name only up to it.
	// - prefix and prefixLength are the name the game puts in front, with a "/" between, or prefix 0 when the game puts none there
	//   (as for NameLengthIsAllowed). An empty one counts: the game still puts the "/" in front.
	//   A shape with no RIFFNAME gives its pictures prefix "" (empty), not 0 (none): win95/chunk_load.cpp:761, :825-826.
	// - The rule is run on the whole path, never on the name alone.
	// When it may not, *why (if not 0) says why: a reason of NameLengthIsAllowed's, or the game's own. For a kind the game never
	// opens, only the length is checked.
	//
	// Every file the editor itself opens by a name from a level (a sound it plays, a picture it shows) passes this check first.
	bool FileNameIsAllowed(NameKind const &kind, char const *name, size_t length, char const *prefix, size_t prefixLength,
	                       char const **why);
}
