// The table of name kinds, and the one rule that checks a name's length. Written 2026-11-14 by Melanikus for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Every reader and every limit was read in the game's own code, first at AvPx 85704f7a (the name-use trace of 2026-11-14), and every
// citation was moved on 2026-11-15 to the editor's dev pin, AvPx 491deb22, by the git diff between the two, and each cited line was then
// read again there. Paths are under the game's 3dc folder. A name's most bytes is the game's own, or the format's (a fixed field), or
// the editor's: the pinned loader copies every name by its measured length and refuses none for being long. What the pinned loader does
// refuse is a name the game needs that's empty or missing, and each such row says so (requiredLimit). tests/model_names_test.cpp checks
// each limit against the game's own headers where it can, and every bound against its limit.

#include <string.h>

#include <string>

#include "name_model.h"

// The game's own rule for a file name from a level. It's a plain header that knows nothing about the rest of the game, so the
// editor uses it as it is, and the editor and the game can never disagree about which names may be opened.
#include "../../avpx/3dc/level_file_name.h"

namespace model
{
	namespace
	{
		// The reasons, in plain words. Several kinds share one.
		char const NoGameLimitWhy[] =
			"the game keeps the whole name, and the editor keeps to 255 bytes, more than any name in the game's own levels";
		char const ObjectNameWhy[] = "the game keeps only the first 63 bytes of an object's name";
		char const MatchesObjectNameWhy[] = "it's matched to an object's name, of which the game keeps only the first 63 bytes";
		char const LockUserWhy[] = "it's a fixed field of 16 bytes";
		char const Fixed8Why[] = "it's a fixed field of 8 bytes";
		char const Fixed20Why[] = "it's a fixed field of 20 bytes";
		char const Fixed100Why[] = "it's a fixed field of 100 bytes";
		char const FileNameWhy[] = "the game opens no file whose name is longer than 200 bytes";
		char const SoundFolderWhy[] =
			"the game builds folder/name for a sound in 200 bytes with its zero, so a folder leaves room for the / and a name only "
			"up to 197 bytes";
		char const RifNameWhy[] =
			"a shape's pictures are opened as rif name/picture name, at most 200 bytes, so a rif name leaves room for the / and a "
			"name only up to 198 bytes";

		char const SoundNeededWhy[] = "the game opens no file with an empty name, so the sound would be missing";
		char const PictureNeededWhy[] = "the game opens no file with an empty name, so the picture would be missing";
		char const FolderNeededWhy[] =
			"the game puts the folder and a / in front of the level's sound names, and opens no path that starts with a /";
		char const RifNeededWhy[] =
			"the game puts the rif name and a / in front of a shape's picture names, and opens no path that starts with a /";
		char const SectionNeededWhy[] = "the game refuses a level with a hierarchy section that has no object name";
		char const RandomTextureNeededWhy[] = "the game finds these by name, and can't find one with an empty name";

		// The limits' sources, where several kinds share one.
		char const FileNameLimit[] = "level_file_name.h:33";
		char const SoundPathLimit[] = "avp/win95/object_setup.cpp:138, :150; avp/win95/projload.cpp:506, :556";
		char const PicturePathLimit[] = "win95/chunk_load.cpp:846-851; avp/win95/projload.cpp:2869";

		NameStorage const E = NameStorage::Ended;
		NameStorage const F = NameStorage::Fixed;
		unsigned const Any = NameMostWithNoGameLimit;
	}

	char const PathTooLongReason[] = "with the name the game puts in front of it, the path is longer than the game opens";

	// chunk, label, reader, storage, gameMost, gameMostSource, most, mostWhy, pathPrefix, pathMostWithPrefix, pathMostSource,
	// required, requiredWhy, shown, requiredLimit and requiredSource (both left out when the name isn't required)
	NameKind const NameKinds[] =
	{
		// Objects and shapes
		{ "OBJHEAD1", "object name", "win95/chunk_object.cpp:449", E, 63, "win95/chunk_object_name.hpp:13", 63, ObjectNameWhy, 0, 0, "", false, "", true },
		{ "OBJHEAD1", "lock user", "win95/chunk_object.cpp:426", F, 16, "win95/chunk_object.cpp:421", 16, LockUserWhy, 0, 0, "", false, "", false },
		{ "SHPHEAD1", "lock user", "win95/chunk_shape.cpp:1506", F, 16, "win95/chunk_shape.cpp:1506", 16, LockUserWhy, 0, 0, "", false, "", false },
		{ "SHPHEAD1", "object using the shape", "win95/chunk_shape.cpp:1558", E, 63, "win95/chunk_shape.cpp:714", 63, MatchesObjectNameWhy, 0, 0, "", false, "", true },
		{ "SHPFNAME", "shape name", "win95/chunk_shape.cpp:2486", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "EXTOBJNM", "external object name", "win95/chunk_shape.cpp:1880", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "SHPEXTFN", "external file name", "win95/chunk_shape.cpp:1821", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "SHPTEXFN", "texture file name", "win95/chunk_shape.cpp:1400", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "SHPFRGTP", "fragment type name", "win95/chunk_shape.cpp:2648", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "FRGTYPDC", "fragment type name", "win95/chunk_fragments.cpp:47", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "FRGTYPSC", "fragment shape name", "win95/chunk_fragments.cpp:105", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "ANISEQDT", "sequence name", "win95/chunk_shape.cpp:2877", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "ANIFRADT", "frame file name", "win95/chunk_shape.cpp:2928", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },

		// Level-wide headers and fixed fields
		{ "ENDTHEAD", "lock user", "win95/environment_chunk.cpp:101", F, 16, "win95/environment_chunk.cpp:101", 16, LockUserWhy, 0, 0, "", false, "", false },
		{ "ENUMHEAD", "lock user", "win95/enum_chunk.cpp:70", F, 16, "win95/enum_chunk.cpp:70", 16, LockUserWhy, 0, 0, "", false, "", false },
		{ "ASPRHEAD", "lock user", "win95/chunk_gsprites.cpp:69", F, 16, "win95/chunk_gsprites.cpp:69", 16, LockUserWhy, 0, 0, "", false, "", false },
		{ "LTSETHDR", "light set name", "win95/chunk_light.cpp:62", F, 8, "win95/chunk_light.cpp:62", 8, Fixed8Why, 0, 0, "", false, "", true },
		{ "SHPVTINT", "light set name", "win95/chunk_light.cpp:234", F, 8, "win95/chunk_light.cpp:234", 8, Fixed8Why, 0, 0, "", false, "", true },
		{ "MAPBLOCK", "template name", "win95/chunk_map.cpp:19", F, 20, "win95/chunk_map.cpp:19", 20, Fixed20Why, 0, 0, "", false, "", true },
		{ "MAPBLOCK", "template notes", "win95/chunk_map.cpp:20", F, 100, "win95/chunk_map.cpp:20", 100, Fixed100Why, 0, 0, "", false, "", true },
		{ "STRATEGY", "strategy name", "win95/chunk_map.cpp:49", F, 20, "win95/chunk_map.cpp:49", 20, Fixed20Why, 0, 0, "", false, "", true },
		{ "STRATEGY", "strategy notes", "win95/chunk_map.cpp:50", F, 100, "win95/chunk_map.cpp:50", 100, Fixed100Why, 0, 0, "", false, "", true },
		{ "RIFFNAME", "rif name", "win95/mishchnk.cpp:803", E, 198, "win95/chunk_load.cpp:846", 198, RifNameWhy, 0, 0, "", true, RifNeededWhy, true,
		  Limit::GameMeaning, "win95/chunk_load.cpp:846-848; avp/win95/projload.cpp:2869-2877 (a file with a hierarchy and no RIFFNAME at all is refused, win95/mishchnk.cpp:320-323)" },
		{ "RANTEXID", "random texture set name", "win95/environment_chunk.cpp:280", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", true, RandomTextureNeededWhy, true,
		  Limit::Loader, "win95/environment_chunk.cpp:284-287" },

		// Things placed in a level
		{ "AVPGENER", "generator name", "avp/win95/avp_chunk.cpp:104", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "AVPGENNM", "generator extra name", "avp/win95/avp_chunk.cpp:221", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "AVPCABLE", "power cable name", "avp/win95/avp_chunk.cpp:506", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "PARGENDA", "particle generator name", "avp/win95/avp_chunk.cpp:840", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "VOBJPROP", "virtual object name", "avp/win95/strategy_chunk.cpp:312", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "PLACHIDT", "placed hierarchy name", "avp/win95/hierplace.cpp:108", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "AVPPATH2", "path name", "avp/win95/path_chunk.cpp:31", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "WAYPOINT", "module link name", "win95/chunk_waypoint.cpp:122", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "DUMOBJDT", "dummy object name", "win95/chunk_dummy.cpp:64", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "DUMOBJTX", "dummy object text", "win95/chunk_dummy.cpp:102", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },

		// Sounds
		{ "SOUNDOB2", "sound name", "win95/chunk_sound.cpp:73", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "SOUNDOB2", "sound file name", "win95/chunk_sound.cpp:75", E, 200, FileNameLimit, 200, FileNameWhy, "SOUNDDIR", 199, SoundPathLimit, true, SoundNeededWhy, true,
		  Limit::GameMeaning, "avp/win95/object_setup.cpp:141-144; avp/win95/jsndsup.cpp:106-116" },
		{ "INDSOUND", "sound file name", "win95/chunk_sound.cpp:237", E, 200, FileNameLimit, 200, FileNameWhy, "SOUNDDIR", 199, SoundPathLimit, true, SoundNeededWhy, true,
		  Limit::GameMeaning, "avp/win95/projload.cpp:529-532; avp/win95/jsndsup.cpp:106-116" },
		{ "TRAKSOUN", "sound file name", "win95/chunk_object.cpp:1105", E, 200, FileNameLimit, 200, FileNameWhy, "SOUNDDIR", 199, SoundPathLimit, true, SoundNeededWhy, true,
		  Limit::GameMeaning, "avp/win95/jsndsup.cpp:106-116" },
		{ "FRGSOUND", "sound file name", "win95/chunk_fragments.cpp:184", E, 200, FileNameLimit, 200, FileNameWhy, "SOUNDDIR", 199, SoundPathLimit, true, SoundNeededWhy, true,
		  Limit::GameMeaning, "avp/win95/jsndsup.cpp:106-116" },
		{ "SOUNDDIR", "sound folder", "win95/environment_chunk.cpp:244", E, 197, "avp/win95/object_setup.cpp:138, :150", 197, SoundFolderWhy, 0, 0, "", true, FolderNeededWhy, true,
		  Limit::GameMeaning, "avp/win95/object_setup.cpp:150; avp/win95/jsndsup.cpp:106-116" },
		{ "SOUNDNAM", "sound extra name", "win95/chunk_sound.cpp:204", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },

		// Pictures
		{ "BMPNAMES", "picture file name", "win95/bitmap_names.cpp:169", E, 200, FileNameLimit, 200, FileNameWhy, "", 0, "", true, PictureNeededWhy, true,
		  Limit::GameMeaning, "avp/win95/projload.cpp:2869-2877" },
		{ "BMPLSTST", "picture file name", "win95/bitmap_names.cpp:169", E, 200, FileNameLimit, 200, FileNameWhy, "RIFFNAME", 200, PicturePathLimit, true, PictureNeededWhy, true,
		  Limit::GameMeaning, "avp/win95/projload.cpp:2869-2877" },
		{ "SHBMPNAM", "picture file name", "win95/bitmap_names.cpp:169", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "SHBMPNAM", "rif name", "win95/bitmap_names.cpp:541", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "SHBMPNAM", "shape name", "win95/bitmap_names.cpp:543", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "BMPMD5ID", "picture name", "win95/bitmap_names.cpp:914", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "BMPMD5ID", "rif name", "win95/bitmap_names.cpp:917", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "BMPMD5ID", "shape name", "win95/bitmap_names.cpp:920", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "MATCHIMG", "picture file name", "win95/bitmap_names.cpp:703", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "MATCHIMG", "rif name", "win95/bitmap_names.cpp:708", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "MATCHIMG", "fixed palette rif name", "win95/bitmap_names.cpp:713", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "BMPENUMS", "enum type name", "win95/enumsch.cpp:107", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "BMPENUMS", "enum name", "win95/enumsch.cpp:76", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },

		// Hierarchies (the game's models)
		{ "OBHIERNM", "hierarchy name", "win95/hierarchy_chunk.cpp:351", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "OBJHIERD", "section object name", "win95/hierarchy_chunk.cpp:272", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", true, SectionNeededWhy, true,
		  Limit::Loader, "win95/hierarchy_chunk.cpp:290-294" },
		{ "OBHALTSH", "shape set name", "win95/hierarchy_chunk.cpp:407", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "OBHALTSH", "old object name", "win95/hierarchy_chunk.cpp:419", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "OBHALTSH", "new object name", "win95/hierarchy_chunk.cpp:422", E, 63, "avp/win95/projload.cpp:776", 63, MatchesObjectNameWhy, 0, 0, "", false, "", true },
		{ "HSETCOLL", "shape set collection name", "win95/hierarchy_chunk.cpp:524", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "OBASEQHD", "sequence name", "win95/animation_objects.cpp:196", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },

		// Palettes, lighting tables and game modes
		{ "GMODHEAD", "game mode name", "win95/chunk_palette.cpp:607", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "GMODHEAD", "rif file name", "win95/chunk_palette.cpp:621", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "RIFCHILD", "rif name", "win95/chunk_palette.cpp:802", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "RIFCHILD", "file name", "win95/chunk_palette.cpp:807", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "RIFCHILD", "picture file name", "win95/chunk_palette.cpp:829", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "SETPALST", "rif name", "win95/chunk_palette.cpp:858", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "TLTCONFG", "source rif name", "win95/chunk_palette.cpp:460", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "ENVTXLIT", "TLT file name", "win95/chunk_palette.cpp:311", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "CLRLOOKP", "colour lookup file name", "win95/chunk_palette.cpp:946", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
		{ "PRSETPAL", "palette name", "win95/chunk_palette.hpp:138", E, 0, "", Any, NoGameLimitWhy, 0, 0, "", false, "", true },
	};

	size_t const NameKindCount = sizeof(NameKinds) / sizeof(NameKinds[0]);

	NameKind const *FindNameKind(char const *chunk, char const *label)
	{
		for (size_t i = 0; i < NameKindCount; i++)
		{
			if (strncmp(NameKinds[i].chunk, chunk, 8) == 0 && strcmp(NameKinds[i].label, label) == 0)
			{
				return &NameKinds[i];
			}
		}

		return 0;
	}

	NameKind const *FirstNameKindIn(char const *chunk)
	{
		for (size_t i = 0; i < NameKindCount; i++)
		{
			if (strncmp(NameKinds[i].chunk, chunk, 8) == 0)
			{
				return &NameKinds[i];
			}
		}

		return 0;
	}

	bool NameLengthIsAllowed(NameKind const &kind, size_t length, size_t prefixLength, char const **why)
	{
		char const *refused = 0;

		if (length > kind.most)
		{
			refused = kind.mostWhy;
		}
		else if (kind.required && length == 0)
		{
			refused = kind.requiredWhy;
		}
		else if (kind.pathPrefix && kind.pathPrefix[0] && prefixLength > 0)
		{
			// The path is the name in front, a "/", then this name. Worked out without adding, so a huge prefixLength can't wrap
			// round to a small one.
			if (prefixLength >= kind.pathMostWithPrefix || length > kind.pathMostWithPrefix - prefixLength - 1)
			{
				refused = PathTooLongReason;
			}
		}

		if (refused && why)
		{
			*why = refused;
		}

		return refused == 0;
	}

	bool FileNameIsAllowed(NameKind const &kind, char const *name, size_t length, char const *prefix, size_t prefixLength,
	                       char const **why)
	{
		// The game puts the name in front only for a kind that has one, and only when that name is there.
		bool const prefixApplies = kind.pathPrefix && kind.pathPrefix[0] && prefix != 0;
		char const *reason = 0;

		// NameLengthIsAllowed sets reason when it refuses.
		bool const lengthIsAllowed = NameLengthIsAllowed(kind, length, prefixApplies ? prefixLength : 0, &reason);

		if (lengthIsAllowed && kind.pathPrefix)
		{
			if ((length && memchr(name, 0, length)) || (prefixApplies && prefixLength && memchr(prefix, 0, prefixLength)))
			{
				reason = kLevelNameCharacter;
			}
			else
			{
				// The bytes the game would open. Their length is already bounded by NameLengthIsAllowed.
				std::string path;

				if (prefixApplies)
				{
					path.append(prefix, prefixLength);
					path += '/';
				}

				if (length)
				{
					path.append(name, length);
				}

				reason = LevelFileName_WhyRefused(path.c_str());
			}
		}

		if (reason && why)
		{
			*why = reason;
		}

		return reason == 0;
	}
}
