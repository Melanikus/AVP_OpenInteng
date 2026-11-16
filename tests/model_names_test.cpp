// The test for src/model's name kinds: each name's bound is never more than the game's own limit, and the length rule refuses what
// the game would cut, refuse or fail to open. Written 2026-11-14 by Melanikus for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// Two of the game's limits are checked against the game's own headers, read here as they are in the pinned AvPx commit, so a change
// there fails this test instead of leaving the table behind: the most bytes of a file name the game opens (level_file_name.h) and of
// an object's name (chunk_object_name.hpp). Both headers are plain and know nothing about the rest of the game.
// It runs none of the editor and reads no file. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "name_model.h"

#include "../avpx/3dc/level_file_name.h"
#include "../avpx/3dc/win95/chunk_object_name.hpp"

using namespace model;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::string KindName(NameKind const &kind)
{
	return std::string(kind.chunk) + " " + kind.label;
}

// A reader or a limit's source names a file of the game's and a line in it: "win95/chunk_object.cpp:440".
static bool NamesAFileAndLine(char const *where)
{
	if (!where)
	{
		return false;
	}

	char const *const colon = strstr(where, ":");

	return colon && colon > where && colon[1] >= '0' && colon[1] <= '9' &&
	       (strstr(where, ".cpp:") || strstr(where, ".hpp:") || strstr(where, ".h:"));
}

// Whether a name of this length may be stored, and the reason given when it may not. A sentinel shows an allowed name leaves the
// reason as it was.
static char const Untouched[] = "untouched";

static char const *Verdict(NameKind const &kind, size_t length, size_t prefixLength)
{
	char const *why = Untouched;
	bool const allowed = NameLengthIsAllowed(kind, length, prefixLength, &why);

	return allowed ? (why == Untouched ? "" : "allowed, but the reason was changed") : why;
}

static void TableHoldsTogether()
{
	printf("The table holds together:\n");

	Check(NameKindCount > 0, "there are name kinds");

	std::set<std::string> seen;
	bool unique = true, readers = true, chunks = true, mostsKept = true, unlimited = true, fixed = true, whys = true, needed = true;
	bool files = true, prefixes = true, enforcers = true;

	for (size_t i = 0; i < NameKindCount; i++)
	{
		NameKind const &kind = NameKinds[i];
		std::string const name = KindName(kind);

		unique = seen.insert(name).second && unique;
		chunks = strlen(kind.chunk) == 8 && kind.label[0] && chunks;
		readers = NamesAFileAndLine(kind.reader) && readers;
		whys = kind.mostWhy && kind.mostWhy[0] && whys;
		needed = (kind.required == (kind.requiredWhy && kind.requiredWhy[0])) && needed;

		// A name the game needs says who makes it so, the loader or the game, and where; one it doesn't need says neither
		bool const enforced = kind.requiredLimit == Limit::Loader || kind.requiredLimit == Limit::GameMeaning;
		enforcers = (kind.required ? enforced && NamesAFileAndLine(kind.requiredSource)
		                           : kind.requiredLimit == Limit::None && (!kind.requiredSource || !kind.requiredSource[0])) &&
		            enforcers;

		// The bound against the game's limit: never more than the game keeps or uses, and never more than the editor's own most.
		if (kind.gameMost > 0)
		{
			mostsKept = NamesAFileAndLine(kind.gameMostSource) && kind.most >= 1 && kind.most <= kind.gameMost &&
			            kind.most <= NameMostWithNoGameLimit && mostsKept;
		}
		else
		{
			unlimited = kind.gameMostSource && !kind.gameMostSource[0] && kind.most == NameMostWithNoGameLimit &&
			            kind.storage == NameStorage::Ended && unlimited;
		}

		if (kind.storage == NameStorage::Fixed)
		{
			fixed = kind.gameMost > 0 && kind.most == kind.gameMost && fixed;
		}

		// A name the game opens: its own limit is the game's file-name limit, from the game's own header.
		if (kind.pathPrefix)
		{
			files = kind.gameMost == (unsigned)kLevelFileNameMost && kind.most <= (unsigned)kLevelFileNameMost && files;

			if (kind.pathPrefix[0])
			{
				// The name in front has to be a kind the table knows, and has to leave room for the "/" and at least one letter.
				NameKind const *const front = FirstNameKindIn(kind.pathPrefix);
				prefixes = front && front->pathPrefix == 0 && NamesAFileAndLine(kind.pathMostSource) &&
				           kind.pathMostWithPrefix <= (unsigned)kLevelFileNameMost && front->most + 2 == kind.pathMostWithPrefix &&
				           prefixes;
			}
			else
			{
				prefixes = kind.pathMostWithPrefix == 0 && kind.pathMostSource && !kind.pathMostSource[0] && prefixes;
			}
		}
		else
		{
			prefixes = kind.pathMostWithPrefix == 0 && kind.pathMostSource && !kind.pathMostSource[0] && prefixes;
		}
	}

	Check(unique, "each chunk and label is one kind, so a kind is found by them");
	Check(chunks, "every chunk is an 8-letter name, and every kind has a label");
	Check(readers, "every kind names the game's reader as a file and line");
	Check(whys, "every kind says why its most is what it is");
	Check(needed, "a kind the game needs filled says why, and only those do");
	Check(enforcers, "a kind the game needs filled says who makes it so, the loader or the game, and cites where; any other kind says neither");
	Check(mostsKept, "where the game keeps or uses fewer bytes, the kind cites where, and the editor's bound is no more than that");
	Check(unlimited, "where the chunk is the only limit, the editor's bound is its own 255, and nothing is cited");
	Check(fixed, "a fixed field's bound is exactly its size");
	Check(files, "a name the game opens has the game's own file-name limit (level_file_name.h), and the bound is no more");
	Check(prefixes, "a name with another in front cites that limit, which leaves the name in front room for the / and one letter");

	NameKind const *const objectName = FindNameKind("OBJHEAD1", "object name");
	Check(objectName && objectName->gameMost == OBJECT_NAME_BYTES - 1 && objectName->most == objectName->gameMost,
	      "an object name's bound is what the game's own header keeps: OBJECT_NAME_BYTES less its zero");

	NameKind const *const usingShape = FindNameKind("SHPHEAD1", "object using the shape");
	NameKind const *const newObject = FindNameKind("OBHALTSH", "new object name");
	Check(objectName && usingShape && newObject && usingShape->most == objectName->most && newObject->most == objectName->most,
	      "the names the game matches to an object's name have the object name's bound, so they can still match");

	bool lockUsers = true;
	int lockUserCount = 0;

	for (size_t i = 0; i < NameKindCount; i++)
	{
		if (strcmp(NameKinds[i].label, "lock user") == 0)
		{
			lockUserCount++;
			lockUsers = !NameKinds[i].shown && NameKinds[i].storage == NameStorage::Fixed && NameKinds[i].most == 16 && lockUsers;
		}
	}

	Check(lockUserCount == 5 && lockUsers, "the five lock users are fixed 16-byte fields the editor never shows");

	NameKind const *const folder = FirstNameKindIn("SOUNDDIR");
	NameKind const *const rifName = FirstNameKindIn("RIFFNAME");
	Check(folder && strcmp(folder->label, "sound folder") == 0 && folder->required && rifName && strcmp(rifName->label, "rif name") == 0 &&
	      rifName->required,
	      "the names put in front of others are the sound folder and the rif name, and neither may be empty");
}

static void LengthsAreChecked()
{
	printf("Lengths are checked:\n");

	NameKind const *const objectName = FindNameKind("OBJHEAD1", "object name");
	NameKind const *const lightSet = FindNameKind("LTSETHDR", "light set name");
	NameKind const *const generator = FindNameKind("AVPGENER", "generator name");
	NameKind const *const sound = FindNameKind("SOUNDOB2", "sound file name");
	NameKind const *const picture = FindNameKind("BMPNAMES", "picture file name");
	NameKind const *const shapePicture = FindNameKind("BMPLSTST", "picture file name");
	NameKind const *const randomTexture = FindNameKind("RANTEXID", "random texture set name");

	if (!objectName || !lightSet || !generator || !sound || !picture || !shapePicture || !randomTexture)
	{
		Check(false, "the kinds these checks use are in the table");
		return;
	}

	Check(strcmp(Verdict(*objectName, 63, 0), "") == 0, "an object name of 63 bytes is allowed");
	Check(Verdict(*objectName, 64, 0) == objectName->mostWhy, "an object name of 64 bytes is refused, as more than the game keeps");
	Check(strcmp(Verdict(*objectName, 0, 0), "") == 0, "an object may have no name");

	Check(strcmp(Verdict(*lightSet, 8, 0), "") == 0, "a light set name may fill its 8 bytes");
	Check(Verdict(*lightSet, 9, 0) == lightSet->mostWhy, "a light set name of 9 bytes is refused, as more than its field");

	Check(strcmp(Verdict(*generator, 255, 0), "") == 0, "a generator name of 255 bytes is allowed");
	Check(Verdict(*generator, 256, 0) == generator->mostWhy, "a generator name of 256 bytes is refused, as more than the editor keeps");

	Check(strcmp(Verdict(*sound, 200, 0), "") == 0, "a sound file name of 200 bytes, with no folder in front, is allowed");
	Check(Verdict(*sound, 201, 0) == sound->mostWhy, "a sound file name of 201 bytes is refused, as more than the game opens");
	Check(Verdict(*sound, 0, 0) == sound->requiredWhy, "an empty sound file name is refused, as the game would open nothing");
	Check(strcmp(Verdict(*sound, 188, 10), "") == 0, "a 10-byte folder, a / and a 188-byte name make 199 bytes, which fit");
	Check(Verdict(*sound, 189, 10) == PathTooLongReason, "a 10-byte folder, a / and a 189-byte name make 200 bytes, which don't fit");
	Check(Verdict(*sound, 1, 198) == PathTooLongReason, "a 198-byte folder leaves no room for a / and a name");
	Check(Verdict(*sound, 1, SIZE_MAX) == PathTooLongReason, "a folder length near the top of a size_t doesn't wrap round to a small one");

	Check(strcmp(Verdict(*shapePicture, 183, 16), "") == 0, "a 16-byte rif name, a / and a 183-byte picture name make 200 bytes, which fit");
	Check(Verdict(*shapePicture, 184, 16) == PathTooLongReason, "one byte more doesn't fit");
	Check(strcmp(Verdict(*picture, 200, 300), "") == 0, "a picture the game opens with nothing in front ignores a length given for one");

	Check(Verdict(*randomTexture, 0, 0) == randomTexture->requiredWhy, "an empty random texture set name is refused");
	Check(strcmp(Verdict(*randomTexture, 10, 0), "") == 0, "a random texture set name of 10 bytes is allowed");

	char const *why = 0;
	Check(NameLengthIsAllowed(*generator, 10, 0, 0) && !NameLengthIsAllowed(*generator, 256, 0, 0),
	      "the rule works with no place for a reason");
	Check(!NameLengthIsAllowed(*objectName, 64, 0, &why) && why == objectName->mostWhy, "a refusal gives the kind's own reason");

	Check(FindNameKind("OBJHEAD1", "no such name") == 0 && FindNameKind("NOSUCHID", "object name") == 0 && FirstNameKindIn("NOSUCHID") == 0,
	      "a chunk or label the table doesn't have finds nothing");
}

// Whether a file name may be stored, and the reason when it may not ("" when it may). The game's reasons are compared by their
// words, as each source file has its own copy of level_file_name.h's.
// prefix 0 means the game puts no name in front; otherwise prefixLength of its bytes are the name in front.
static std::string FileVerdict(NameKind const &kind, std::string const &name, char const *prefix, size_t prefixLength)
{
	char const *why = 0;
	bool const allowed = FileNameIsAllowed(kind, name.data(), name.size(), prefix, prefixLength, &why);

	return allowed ? "" : (why ? why : "refused with no reason");
}

static std::string FileVerdict(NameKind const &kind, std::string const &name, char const *prefix)
{
	return FileVerdict(kind, name, prefix, prefix ? strlen(prefix) : 0);
}

static void FileNamesAreTheGames()
{
	printf("File names are checked with the game's own rule, on the whole path:\n");

	NameKind const *const sound = FindNameKind("SOUNDOB2", "sound file name");
	NameKind const *const picture = FindNameKind("BMPNAMES", "picture file name");
	NameKind const *const shapePicture = FindNameKind("BMPLSTST", "picture file name");
	NameKind const *const objectName = FindNameKind("OBJHEAD1", "object name");

	if (!sound || !picture || !shapePicture || !objectName)
	{
		Check(false, "the kinds these checks use are in the table");
		return;
	}

	Check(FileVerdict(*sound, "door open.wav", "sound") == "", "a sound in the level's folder is allowed");
	Check(FileVerdict(*sound, "door open.wav", 0) == "", "a sound with no folder in front is allowed");
	Check(FileVerdict(*sound, "../door.wav", "sound") == kLevelNameGoesUp, "a sound name that goes up a folder is refused, as the game does");
	Check(FileVerdict(*sound, "door.wav", "..") == kLevelNameGoesUp,
	      "a folder that goes up is refused, though the name alone is fine: the whole path is checked");
	Check(FileVerdict(*sound, "door.wav", "") == kLevelNameStartsAtTheTop,
	      "an empty folder still puts a / in front, so the path starts at the top and is refused");
	Check(FileVerdict(*sound, "c:door.wav", "sound") == kLevelNameCharacter, "a colon is refused, as no game file's name has one");
	Check(FileVerdict(*sound, "nul.wav", "sound") == kLevelNameDevice, "a device's name is refused");
	Check(FileVerdict(*sound, "sub/COM1", "sound") == kLevelNameDevice, "a device's name in any part is refused");
	Check(FileVerdict(*sound, std::string("door\0.wav", 9), "sound") == kLevelNameCharacter,
	      "a zero byte inside the name is refused, as the game would stop reading there");
	Check(FileVerdict(*sound, "door.wav", "so\0und", 6) == kLevelNameCharacter, "a zero byte inside the folder is refused");
	Check(FileVerdict(*sound, std::string(201, 'a'), 0) == sound->mostWhy, "a name past its bound gets the bound's reason first");
	Check(FileVerdict(*sound, std::string(189, 'a'), "folder1234") == PathTooLongReason,
	      "a name that fits alone but not with its folder in front gets the path's reason");

	Check(FileVerdict(*picture, "Frame & Panels.RIM", 0) == "", "a picture name the game's own levels use is allowed");
	Check(FileVerdict(*picture, "frame.", 0) == kLevelNamePartEnding, "a name ending in a dot is refused");
	Check(FileVerdict(*picture, "a//b.rim", 0) == kLevelNameEmptyPart, "an empty part is refused");
	Check(FileVerdict(*picture, "/a.rim", 0) == kLevelNameStartsAtTheTop, "a name starting with a / is refused");
	Check(FileVerdict(*picture, "frame.rim", "..") == "",
	      "a picture the game opens with nothing in front ignores a name given for the front");

	Check(FileVerdict(*shapePicture, "frame.rim", "shapes") == "", "a shape's picture with its rif name in front is allowed");
	Check(FileVerdict(*shapePicture, "frame.rim", "c:") == kLevelNameCharacter, "a rif name with a colon is refused in the path");

	// A shape whose external file chunk (SHPEXTFL) holds no rif name still gets one in front of its pictures: the game starts from
	// an empty name and adds the "/" all the same (win95/chunk_load.cpp:761, :825-826). So an empty name in front ("") isn't the same
	// as none (0), which is only for a picture the game opens by its own name (an IFF one).
	Check(FileVerdict(*shapePicture, "frame.rim", "") == kLevelNameStartsAtTheTop,
	      "a shape's picture with an empty rif name in front still gets the game's /, so its path starts at the top and is refused");
	Check(FileVerdict(*shapePicture, "frame.rim", 0) == "",
	      "the same picture with nothing in front is allowed: an empty name in front isn't no name");

	Check(FileVerdict(*objectName, "../anything", 0) == "", "a name the game never opens is only measured, not checked as a path");

	// The editor's verdict on a picture with nothing in front is the game's own, name for name.
	char const *const names[] =
	{
		"frame.rim", "Pipe01-04@256.RIM", "sub\\dir\\a.rim", "a b.rim", "a..b.rim", "..", ".", "a. /b", "aux", "AUX.rim", "COM0.rim",
		"COM9", "lpt1.txt", "con .rim", "x*y", "x?y", "x\"y", "x|y", "x<y", "x>y", "tab\there", "\xC0name", "~name", "a,b",
	};
	bool agrees = true;

	for (char const *const name : names)
	{
		char const *const game = LevelFileName_WhyRefused(name);
		agrees = FileVerdict(*picture, name, 0) == std::string(game ? game : "") && agrees;
	}

	Check(agrees, "for " + std::to_string(sizeof(names) / sizeof(names[0])) +
	              " names, good and hostile, the editor refuses exactly what the game refuses, for the same reason");

	// A kind whose bound is past the game's file-name limit (none in the table is, which TableHoldsTogether checks) still can't
	// store a path the game won't open: the game's own rule refuses it as too long.
	NameKind wide = *picture;
	wide.most = NameMostWithNoGameLimit;
	Check(FileVerdict(wide, std::string(250, 'a'), 0) == kLevelNameTooLong, "a path longer than the game opens is refused by the game's rule");
}

// WHICH NAMES THE PINNED LOADER REFUSES (AvPx 491deb22). The loader copies every name by its measured length and refuses none for being long, so
// no name's most is a loader limit. What it does refuse is a name the game needs that's empty: a random texture set's
// (win95/environment_chunk.cpp:284-287) and a hierarchy section's object name (win95/hierarchy_chunk.cpp:290-294). Every other name the game
// needs the loader takes, and the game goes on without the file, sound or picture it names. (Added 2026-11-15 by Melanikus (Vasquez).)
static void LoaderLimitsAreTheLoaders()
{
	printf("The names the pinned loader refuses empty are the loader's limits:\n");

	NameKind const *const randomTexture = FindNameKind("RANTEXID", "random texture set name");
	NameKind const *const section = FindNameKind("OBJHIERD", "section object name");
	NameKind const *const rifName = FindNameKind("RIFFNAME", "rif name");
	NameKind const *const sound = FindNameKind("SOUNDOB2", "sound file name");

	if (!randomTexture || !section || !rifName || !sound)
	{
		Check(false, "the kinds these checks use are in the table");
		return;
	}

	Check(randomTexture->requiredLimit == Limit::Loader && strstr(randomTexture->requiredSource, "environment_chunk.cpp:284-287") &&
	      section->requiredLimit == Limit::Loader && strstr(section->requiredSource, "hierarchy_chunk.cpp:290-294"),
	      "a random texture set with no name and a hierarchy section with no object name are refused by the loader, at the lines cited");

	int loaders = 0;
	int games = 0;
	bool othersAreTheGames = true;

	for (size_t i = 0; i < NameKindCount; i++)
	{
		if (NameKinds[i].requiredLimit == Limit::Loader)
		{
			loaders++;
		}
		else if (NameKinds[i].required)
		{
			games++;
			othersAreTheGames = NameKinds[i].requiredLimit == Limit::GameMeaning && othersAreTheGames;
		}
	}

	Check(loaders == 2, "no other name is refused empty by the loader: those two are the only loader limits of the table");
	Check(games == 8 && othersAreTheGames,
	      "the other eight names the game needs (the rif name, the four sound file names, the sound folder and the two picture file names) are the game's: "
	      "it goes on without them");
	Check(rifName->requiredLimit == Limit::GameMeaning && strstr(rifName->requiredSource, "mishchnk.cpp:320-323"),
	      "a missing rif name chunk is the loader's refusal, but an empty rif name is the game's, and the row cites both");
	Check(sound->requiredLimit == Limit::GameMeaning && strstr(sound->requiredSource, "jsndsup.cpp:106-116"),
	      "a sound with no name is left out by the game (jsndsup.cpp:106-116), never refused by the loader");
}

int main()
{
	TableHoldsTogether();
	LengthsAreChecked();
	FileNamesAreTheGames();
	LoaderLimitsAreTheLoaders();

	printf("%s: %d check(s) failed\n", checksFailed ? "FAILED" : "PASSED", checksFailed);
	return checksFailed ? 1 : 0;
}
