// object_make.h: inteng's "make a placed object from a shape" as one function on the open level: the maker behind Object Placement's Quick-create
// lists (the 22 items of the Weapon, Ammo, Light and Other lists) and, later, Create with shape. Written 2026-11-15 by Melanikus (Frost) for
// AVP_OpenInteng (P3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (FUN_004de260, out\menu-reads-15.txt:2280-2854; the pieces it uses are read in out\menu-reads-3.txt and menu-reads-18.txt, and
// engine\inteng\p11-shape-handlers.md sections 25 and 28):
//  1. The shape is found BY NAME among the shapes the level holds; a name with an `@` is split there. No shape: inteng asks to load it from the
//     "RifSearchPath" (FUN_004bef40, unread): THAT BRANCH IS NOT COPIED, the answer here is NoSuchShape ("shape not in this level").
//  2. The object's NAME is FUN_004d5620(base): "%s%d" with the name typed (the shape's name) and a counter from 0, the first name no object of the
//     level has (menu-reads-18.txt:740-1175: stock shows it: flam010, flam011, ... flam014, pulse0, pulse1, medbox010).
//  3. The object is made by FUN_004e0ed0 at the VIEW's place with the view's 9-int matrix, and the shape. Its save (FUN_004e1f90, menu-reads-3.txt:405-995)
//     writes an RBOBJECT holding: OBJHEAD1 (flags 0xFC00: placed, the three game modes, the platform and PC load; the place; the turn as four floats made
//     from the matrix by FUN_004de070; the number; the shape's number; the name), OBINTDT holding one OBJNOTES "Enter notes here" (FUN_004d2450), and
//     OBJPRJDT holding an AVPSTRAT of kind 62 (a placed object) with Type 0, ExtraData 0, mass 50, integrity 0x80000002, flags 1 (FUN_0052b120(0x3e), :597-611).
//  4. The shape's own header (SHPHEAD1) lists the names of the objects that use it (the library's assoc_with_shape): the new name is added there.
//
// WHAT IS NOT COPIED, each a NAMED GAP (the bytes stay as the stock levels' have them, or are left plain):
//  - The shape found only in the "RifSearchPath" (step 1): copied as shape_import.h says, from the editor's one shape folder, NOT the search path's list
//    (a name with a path, or a file past the cap, is refused). inteng's question "import it?" is not asked: a shape in the folder is taken (F-IMPORT-ASK).
//  - A name with an `@`: refused, SubNameNotCopied, a named gap with NO CALLER (no screen of ours makes such a name: Quick-create's 22 have none, and the
//    chooser and Create Duplicate make by the shape's number, below). Inteng's handling is READ (Vasquez 2026-11-23, AvP-docs engine/inteng
//    create-object-from-shape.md, "THE '@' NAME"): the part BEFORE the '@' is the object, the part after it the file. The level's own shape matches when its
//    SHPEXTFN's base is the file AND its EXTOBJNM is the object. Failing that, inteng loads <file>.rif, the WHOLE fragment file, from the RifSearchPath and
//    takes its object of that name. That is the spec for whoever gives it a caller (an Option C shape browser, say): there, Option C's own part file
//    "<object>_<file>.rif" first (Frost's choice), the new shape holding SHPEXTFN <file>.rif, EXTOBJNM <object> and RIFFNAME as a stock part does.
//  - How a shape's name is compared (FUN_0053ea00 is the name, FUN_005909e0 the compare): the name is the shape's outside file name without its extension
//    (SHPEXTFN: "Flam01.rif" is flam01), compared without regard to capitals, and the FIRST outside shape whose file has that name is taken, a fragment
//    file's part or not: INTENG'S OWN LOOKUP, kept exactly (FUN_004de260 0x4de315-0x4de3de walks the shapes in order). So a plain name can match a part of a
//    fragment file, the file's first; a caller that knows WHICH shape it means makes it by number (CreateObjectFromShapeNumber). STOCK EVIDENCE: the 22
//    names inteng's cases pass (smrtam01, smart, armour01...) match the stock files' names (Smrtam01.RIF, Smart.RIF, Armour01.RIF) only so.
//  - The object's lock user and number (index_num) are inteng's save's (V-27, object_number.h): "Player" and the lowest number no object has, as object_edit.h's copy gives.
//  - FUN_004d21b0's header bit 0x100 for a shape with an outside file: stock objects of such shapes (174 of them in Invasion) do not have it, so it is not set.
//  - The order of an object's chunks: the library's creation order (header, interface data, project data); stock has the reverse. The game reads both.
//  - The 3 padding bytes after the notes' text: zeros (stock has what was in memory).
//  - The view's matrix is taken as the CALLER gives it (nine 16.16 whole numbers, row by row, the camera's own turn); the conversion to four floats is
//    FUN_004de070 exactly, but its threshold constant (_DAT_005ab360) is read as 0.0, the usual one (it is not shown in the reads).
//
// HOW (as object_edit.h does it): the level is written out as the next save would write it (BuildLevel), the object is put in after the level's last
// object, the shape's header is made again with the name added, the sizes are made right, and OpenLevelFile::ReplaceLevel reads the result with the game's own
// loader. If the loader wouldn't take it, nothing has changed. After a success every Chunk * the editor held is gone.
//
// What the level's bytes hold is hostile: every size is checked as the bytes are walked, and a level that can't be walked is refused.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

class OpenLevelFile;

namespace host
{
	enum class ObjectMakeResult
	{
		Done,
		NothingOpen,       // no level is open (or none that's paired with its bytes)
		BadShapeName,      // the name is empty, longer than 63 letters, or has a letter that isn't plain text
		SubNameNotCopied,  // the name has an `@`: the sub-name compare isn't read
		NoSuchShape,       // the level holds no shape with that name, and none is in the shape folder (or no folder is set)
		ShapeFileRefused,  // the shape's file in the shape folder couldn't be used (shape_import.h: too big, not a shape file, ...)
		NoNameFree,        // no counter gave a name no object has
		PlaceOutOfRange,   // a coordinate of the place is farther than 2^24 millimetres from the middle
		TooManyObjects,    // no object number is left, or a size no longer fits
		CantBuild,         // the level couldn't be put together from the bytes (not walkable, or too big)
		LoaderRefused,     // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *ObjectMakeWords(ObjectMakeResult result);

	// FUN_004de070: the nine 16.16 whole numbers of a matrix (row by row: m[0..2] the first row) as the four floats of a turn (x, y, z, w)
	void ObjectMake_TurnFromMatrix(int32_t const *matrix, float *turn);

	// THE GAME OBJECT A NEW OBJECT IS MADE AS (typed placement, Dietrich 2026-11-23): its strategy's Type and ExtraData (the AVPSTRAT's +20 and +24). The
	// caller takes them from shape_identity.h's closed list (ShapeIdentity_PlacedAs) only. Every other field is the maker's own, as below: mass 50, integrity 2,
	// flags 1, no target. Every stock placement of each kind on that list carries exactly those fields. No type given: Type 0 and ExtraData 0, a static object,
	// as inteng's own Quick-create makes every object.
	struct ObjectStrategyType
	{
		int32_t type = 0;
		int32_t extra = 0;
	};

	// Makes the object. place: three whole numbers in millimetres (the view's place). matrix: nine whole numbers (the view's turn). newId (8 bytes) and
	// newName, if not null, say what it is called. The status of the maker is in the result.
	ObjectMakeResult CreateObjectFromShape(OpenLevelFile *file, char const *shapeName, int32_t const *place, int32_t const *matrix, uint8_t *newId,
	                                       std::string *newName, char const *shapeFolder = nullptr, std::string *importWhy = nullptr,
	                                       ObjectStrategyType const *as = nullptr);
	// shapeFolder (null or empty: none): where a shape the level doesn't hold is looked for, as <name>.rif (shape_import.h). Then a shape that is found
	// and taken is put in the level with the new object, in the same one change (the loader reads the whole result before anything is swapped in).
	// importWhy, when not null, says what was wrong with the shape's file (or that it isn't in the folder), when the answer is ShapeFileRefused or NoSuchShape.
	// as: the game object it is made as (ObjectStrategyType, above); null: a static one.

	// Makes the object of the level's shape with that NUMBER (its SHPHEAD1's own), as inteng's Create Duplicate (FUN_004e0ed0 is given the shape's number) and its
	// Shape Chooser (it answers with the chosen shape's number) make it; never found by its file's name, which every part of one fragment file shares (office's
	// offbits parts Aerosol, CDBox, Game ... are all "offbits": made by the name, a copy of Game came out Aerosol, OUR bug, 2026-11-23). nameBase: what the new
	// object's first free "<base><n>" name is made of (the caller may rename it). Otherwise as CreateObjectFromShape: NoSuchShape when the level holds no shape
	// of that number (nothing is imported). as: the game object it is made as (ObjectStrategyType, above); null: a static one.
	ObjectMakeResult CreateObjectFromShapeNumber(OpenLevelFile *file, int32_t shapeNumber, char const *nameBase, int32_t const *place, int32_t const *matrix, uint8_t *newId,
	                                             std::string *newName, ObjectStrategyType const *as = nullptr);

	// Makes a placed object around a shape the CALLER made (Split Object: poly_edit.h): shapeChunk is a whole REBSHAPE chunk with its own number (a number no shape of the level has) and a SHPHEAD1 that lists no
	// users yet. It goes in after the level's last object and the object is made as above, named <objectBase><n> (the first no object has), at place (millimetres) and turned as turn (four floats, an object
	// header's own), in the same one change. newId / newName as above.
	ObjectMakeResult CreateObjectWithShapeChunk(OpenLevelFile *file, std::vector<char> const &shapeChunk, char const *objectBase, int32_t const *place, float const *turn, uint8_t *newId, std::string *newName, bool exactName = false);

	// The parts of an object its caller has worked out: Light Sourcing's paste of a placed light (light_paste.h; inteng's FUN_004abb00). shapeNumber: the level's shape with that number (its SHPHEAD1's
	// own), not a name; headerFlags: the OBJHEAD1's flags; strategy: the AVPSTRAT's bytes after its heading (52: a kind 62); after: one whole chunk, its heading and all, put after the project data
	// (the PLOBJLIT). The header's other numbers and the notes are a new object's, as CreateObjectFromShape makes them. (Added 2026-11-17 by Frost.)
	struct ObjectParts
	{
		int32_t shapeNumber = -1;
		uint32_t headerFlags = 0;
		std::vector<char> strategy;
		std::vector<char> after;
	};

	// Makes the object of those parts, named `name` exactly (NoNameFree when an object has it), at place (the file's units) turned as matrix (nine 16.16 whole numbers, FUN_004de070), in one change.
	// NoSuchShape when the level holds no shape of that number; CantBuild when a part isn't whole (the strategy not 52 bytes, the chunk after not a chunk). newId / newName as above.
	ObjectMakeResult CreateObjectFromParts(OpenLevelFile *file, char const *name, int32_t const *place, int32_t const *matrix, ObjectParts const &parts, uint8_t *newId, std::string *newName);
}
