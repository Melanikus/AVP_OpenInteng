// shape_extract.h: Option C, "the shapes from the player's own levels" (the owner, 2026-11-22: "C seems to be our only safe choice, do it"; "Scan maps for
// shapes which we already place/move/load/save into their own rifs"). Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: Quick-create (shape_import.h) makes a level's outside shape from <name>.rif in a shape folder, and Steam ships no shape folder (Bishop 2 read all 4
// depots' manifests); the old tools' package can't be passed on (their EULA). But every stock level already holds a copy of each outside shape it places:
// the very bytes shape_import makes from the file (shape_import.h's proof: points, normals and centre equal in all 192 such shapes; polygons and UVs in
// 162). This is the way back: a level's outside shape made into the shape file it came from, so Quick-create can place it in any level.
//
// WHAT A LEVEL KEEPS, AND SO WHAT THE FILE GETS (measured 2026-11-23 on the package's 211 one-shape files against the 63 stock levels):
//   - the shape's own chunks (SHPRAWVT, SHPVNORM, SHPCENTR, SHPPNORM, SHPPOLYS, SHPUVCRD, SHPMRGDT, SHPPCINF, TEXTANIM, ...): copied as the level holds them,
//     but SHPFRAGS (the level's own fragment list: shape_import never copies it either) and SHPEXTFL (the outside file's data, below);
//   - SHPHEAD1: its counts and sizes (+24 to +59) as the level holds them (shape_import copied them from the file); the FILE'S version at +60, which the
//     level's SHPEXTFN keeps (the level's own +60 is the old editor's edit count: medbox01 is 27 in the package and in every SHPEXTFN, 1287 in Derelict's
//     header); its flags without 0x1000 (every
//     outside shape in a level has exactly 0x1000; 206 of the package's 211 files have 0, 5 have 0x2000, which a level doesn't keep); no lock user (171 of
//     the 211 hold the name of whoever last saved them in the old tools); shape number 0 (the package's are the old tools' counters: an import sets its own);
//     no object using it (each package file has one RBOBJECT placing the shape; an import makes its own object);
//   - REBENVDT: RIFFNAME, BMPNAMES (the level's BMPLSTST under the file's own name), BMNAMEXT and BMNAMVER as the level holds them, but each name's PAD
//     BYTES (after its end 0, up to a whole 4) set to 0, as the package's files have them: a level's copy holds whatever was in the old editor's memory there
//     ("Pack\0frg" for "Pack"). Not kept by a level, so not written: ENDTHEAD, LIGHTSET, SOUNDDIR, MATCHIMG, FRAGTYPE;
//   - RIFVERIN 0 (207 of the 211).
// So the file is not the package's byte for byte; what it must be is IMPORT-EQUAL: ShapeImport_Build of it makes the same level shape as of the package's
// file (tests/host_shape_extract_test.cpp checks the round trip; the stock comparison is AvP-docs review/placed-shapes-vs-shape_rifs.md).
//
// HOSTILE INPUT (Tao 3): the level may be a stranger's (avp_rifs\Custom). Every chunk size is checked inside its parent (shape_chunks), the file's name must
// be a plain one (letters, digits, '_' and '-', at most 63, then ".rif"; no device name), and a rescaled copy (rescale not 1) is refused: its points are
// the file's scaled, so it isn't the file. Pure: no file, no Windows; the host reads and writes.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace host
{
	enum class ShapeExtractResult
	{
		Done,
		NotOutside,     // the shape isn't an outside shape (no SHPEXTFL), so it came from no file
		Rescaled,       // its rescale isn't 1: its points are the file's scaled
		BadName,        // the file name it gives isn't a plain one
		BadShape,       // its chunks can't be walked, or a part the file needs is missing or short (the words say which)
	};

	char const *ShapeExtractWords(ShapeExtractResult result);

	// One outside shape a level places: where its REBSHAPE chunk is in the (unpacked) level, the file's name as the level gives it, its rescale
	struct ShapeExtractPlaced
	{
		size_t at = 0, size = 0;
		std::string fileName;     // as the level's SHPEXTFN holds it ("medbox01.rif"); not yet checked as a plain name
		std::string objectName;   // when the copy is ONE PART of that file: the object's name in it, as its EXTOBJNM holds it ("ductcove01"); else ""
		double rescale = 0;
	};

	// The outside shapes a level holds (its top-level REBSHAPE chunks with a SHPEXTFL and a SHPEXTFN), in the level's order. False when the level's chunks
	// can't be walked (nothing is given then).
	bool ShapeExtract_Placed(std::vector<char> const &level, std::vector<ShapeExtractPlaced> *placed);

	// The shape file one of them came from (above), as its bytes, and its plain name, in small letters. THE NAME (Bishop 2's ruling A, 11-23, the owner's
	// rule "call them by whatever game name we found them with"): a copy whose SHPEXTFL holds an EXTOBJNM is ONE PART of its file (a fragment file's piece:
	// 277 of the 1,160 stock outside shapes), which the game calls "<object>@<RIFFNAME>" (chunk_shape.cpp:1914); made plain, "<object>_<file>.rif"
	// ("ductcove01_missfrg.rif"), so no part ever takes the whole file's name (Rebellion's missfrg.rif is 19 shapes). A copy with no EXTOBJNM is the whole
	// file: its own name ("medbox01.rif"). *why, when not null, says what was wrong.
	ShapeExtractResult ShapeExtract_File(std::vector<char> const &level, ShapeExtractPlaced const &shape, std::vector<char> *file, std::string *fileName, std::string *why);

	// THE SCAN, over several levels at once (the host reads them; this part is pure).
	//
	// A level for the scan: its file's name ("Genshd1.RIF": for the provenance, and the names of conflicts), its bytes unpacked, and the SHA-256 of its file
	// as it is on disk (64 small hex letters; the host works it out)
	struct ShapeExtractLevel
	{
		std::string name;
		std::vector<char> bytes;
		std::string sha256;
	};

	// One shape file the scan makes
	struct ShapeExtractShape
	{
		std::string fileName;                  // the name it is written under: the plain name, or for a conflict <name>_<level> (then _2, _3, ...)
		std::string givenName;                 // the game's own name for it, from the first level ("spaceship bulb.rif"; a part's "ductcove01@Missfrg.RIF")
		std::string conflictOf;                // for a conflict, the plain name the first variant kept; "" otherwise
		std::string fileOf;                    // the plain name of the whole FILE it came from ("missfrg.rif"; for a whole-file copy, its own name). The same
		                                       // bytes as a part and as the whole file are one shape, by the file's name (a one-shape file whose only object
		                                       // its EXTOBJNM names: ndrum.rif's "drum")
		std::vector<char> file;                // its bytes, as ShapeExtract_File makes them
		std::string sha256;                    // of those bytes: the same stock bytes give the same file, so the same SHA-256, on every install
		std::vector<std::string> levels;       // every level holding a copy that makes these very bytes, in the order given (the first is where it was taken from)
		std::vector<std::string> levelSha256;  // each of those levels' SHA-256, in the same order
		std::vector<std::pair<std::string, size_t>> copies;   // every copy that makes these bytes: its level and where its REBSHAPE starts there
	};

	// A copy the scan didn't make into a file, and why (a rescaled copy, a name that isn't plain, ...)
	struct ShapeExtractSkipped
	{
		std::string level, givenName;
		ShapeExtractResult result = ShapeExtractResult::BadShape;
	};

	// Every outside shape the levels hold, made into its file, in the order the levels are given (the host gives them in a fixed order, so the same levels
	// always give the same files under the same names). THE OWNER'S NAMING RULE (2026-11-23): the same name with the same bytes is ONE shape, listed with every
	// level that holds it; the same name with different bytes is a CONFLICT: the first keeps the name, each other one is <name>_<level> ("medbox01_genshd1.rif",
	// the level's name made plain the same way), then _2, _3 ... if that is taken too. A level whose chunks can't be walked is left out whole (refusedLevels).
	void ShapeExtract_Scan(std::vector<ShapeExtractLevel> const &levels, std::vector<ShapeExtractShape> *shapes, std::vector<ShapeExtractSkipped> *skipped,
		std::vector<std::string> *refusedLevels);

	// A conflict's name, the n-th tried (n = 1: "<name>_<level>.rif", then "<name>_<level>_<n>.rif"), for the plain name ("medbox01.rif") and the level's
	// file name ("Genshd1.RIF" -> "genshd1"), cut to the 63 letters a plain name may have (the name's part is cut, the level's kept). The scan and the writer
	// both use it, so a conflict is named the same whichever of them meets it.
	std::string ShapeExtract_ConflictName(std::string const &plain, std::string const &levelName, int n);

	// The SHA-256 of some bytes as 64 small hex letters (avpx 3dc/sha256_stream.h, the loader's own)
	std::string ShapeExtract_Sha256(std::vector<char> const &bytes);

	// WHAT EACH SHAPE IS (wishlist 9, "auto kinds"; the owner: "tell Open Inteng 'this is a healthpack'"): what the GAME makes of every object a level
	// places with an outside shape, read as avpx's loader reads it (object_setup.cpp deal_with_placed_object and add_newsimpleobject, at the pin):
	//   - the object's shape: its OBJHEAD1's shape number (+56) is the shape's SHPHEAD1 number (+20) (chunk_object.cpp:211);
	//   - its kind: its OBJPRJDT > AVPSTRAT. +8 is the strategy's kind (strategy_chunk.hpp: 62 a placed object, 53 the old simple one, 75 a fan, ...).
	//     Kind 62 has its OWN Type and ExtraData at +20 and +24 (SimpleStrategy, after the 12-byte AvpStrat head); kind 53 has them at +12 and +16;
	//   - kind 62 with a PLOBJLIT is a placed light; Type 9 (IOT_ReservedForJohn) a video screen; otherwise Type is pvisible.h's object type, and for a
	//     weapon ExtraData is equipment.h's WEAPON_ID, for ammunition the FILE's number that add_newsimpleobject turns into the game's;
	//   - an object with no AVPSTRAT: a plain prop.
	// Measured on the 63 stock levels and 23 avp_huds files (AvP-docs review/placed-shapes-vs-shape_rifs.md): medbox01 "health" 186 of 186 placements,
	// pulse "weapon: pulse rifle" 159 of 159. inteng's own Quick-create gives every object Type 0 (static: object_make.h), so these kinds are the mappers'.
	struct ShapeExtractPlacement
	{
		std::string fileName;   // the shape's file name as its SHPEXTFN gives it, in small letters ("medbox01.rif"; not yet made plain)
		size_t shapeAt = 0;     // where that shape's REBSHAPE starts in the level (ShapeExtractPlaced::at): a level can hold two shapes of one name
		std::string kind;       // "health", "weapon: pulse rifle", "ammo: smartgun", "placed light", "prop (no strategy)", "door", ...
		// ITS OWN FIELDS (Dietrich 2026-11-24, typed placement's furniture: each shape's own stock placement), for a kind 62 object with no light whose strategy is
		// whole (52 bytes): simple, and its strategy's Type (+20), ExtraData (+24), mass (+28), integrity word (+32, its top bit and all) and flags word (+36: the
		// flags in bits 0x3, the explosion in 0x30), and whether it names a target (target_request or targetID not 0). Any other object: simple false, the rest 0.
		bool simple = false;
		int32_t type = 0, extra = 0, mass = 0;
		uint32_t integrity = 0, flags = 0;
		bool targeted = false;
	};

	// Every object of a level that places an outside shape, with its kind, in the level's order. False when the level's chunks can't be walked.
	bool ShapeExtract_Placements(std::vector<char> const &level, std::vector<ShapeExtractPlacement> *placements);

	// THE WORDS OF A KIND, as ShapeExtract_Placements gives them for a kind 62 or 53 object: the game's Type with its ExtraData ("health", "weapon: pulse rifle",
	// "ammo: smartgun"), the words the identity table holds. Typed placement (shape_identity.h ShapeIdentity_PlacedAs) matches a kind against these words.
	// (Added 2026-11-23 by Dietrich.)
	std::string ShapeExtract_KindWords(int32_t type, int32_t extra);

	// THE FILE EACH OF A LEVEL'S OUTSIDE SHAPES MAKES, BY ITS NUMBER (Dietrich 2026-11-23, typed placement: a picker tile drawn and placed from the level's own
	// shape is known the way the table knows a file). For the first outside shape of each SHPHEAD1 number, as ShapeExtract_Placements numbers them: the SHA-256
	// of the file ShapeExtract_File makes of it. A shape it makes no file of has no entry, so it is unknown: rescaled, a name that isn't plain, chunks not whole.
	// False when the level's chunks can't be walked (nothing is given then).
	bool ShapeExtract_FileSums(std::vector<char> const &level, std::map<int32_t, std::string> *sumOfNumber);

	// ONE MAP, AS ITS FILE HOLDS IT (the owner's rule, 2026-11-23: every map not yet searched is scanned, "avp_rifs and avp_huds at start, and every level
	// opened"; the first-launch screen keeps the SHA-256 of each map searched). The one call for it: a map's FILE bytes as they are on disk (packed, REBCRIF1,
	// or plain, REBINFF2) in, its shapes out. DETERMINISTIC: the same bytes always give the same shapes, names and SHA-256s.
	size_t const kShapeExtractMostMapBytes = 16 * 1024 * 1024;   // a map's file at most (open_with.cpp's cap on a level); unpacked at most 4 times that
	// THE WORK A MAP MAY ASK FOR (Vasquez's review, 11-23: the scan runs at every start on the editor's own thread, so a map whose scan never ends would
	// freeze every start): at most this many outside shapes and objects in one map, well above stock's most (45 outside shapes, Breakout; 885 objects,
	// Nost03_M). A map over either is refused in words, its bytes still read (so the list of maps searched keeps it, and it isn't tried again).
	size_t const kShapeExtractMostShapesInMap = 1024;
	size_t const kShapeExtractMostObjectsInMap = 16384;

	struct ShapeExtractMap
	{
		bool scanned = false;                         // false: the bytes couldn't be read as a map, and nothing of them is used
		std::string why;                              // when not scanned, why ("packed, but its packing doesn't decode safely", ...)
		std::string sha256;                           // of the file's bytes as given: what the list of maps already searched keeps
		std::vector<ShapeExtractShape> shapes;        // its shapes as ShapeExtract_Scan makes them for this map alone (two of one name in it: <name>_<map>)
		std::vector<ShapeExtractSkipped> skipped;     // its copies not made (rescaled, not a plain name, ...), each with why
		std::vector<ShapeExtractPlacement> placements;   // its objects placing outside shapes, with their kinds (ShapeExtract_Placements)
	};

	// mapName: the map file's own name ("Genshd1.RIF"), for the shapes' levels and a conflict's name
	ShapeExtractMap ShapeExtract_ScanMap(std::string const &mapName, std::vector<char> const &fileBytes);
}
