// shape_update.h: inteng's UPDATE OF AN OUTSIDE SHAPE FROM ITS OWN RIF FILE (the tools' Shape_External_File_Chunk::update_from_external_rif, inteng FUN_00512fc0), as plain
// functions on bytes. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (AvP-docs\engine\inteng\reassess\object-placement.md section 7, OP-137..OP-141 and 7a OP-138a..e):
// - It runs at EVERY LOAD for every outside shape (the SHPEXTFL's post-input method FUN_005149e0 -> FUN_00512fc0(0)) and from Rescale's "Take scale from shape rif" (FUN_004e1970 ->
//   FUN_00512fc0(1), "forced"). It runs only when the outside file is found (the SHPEXTFN name's last part, in the shape folder) and read, and its shape is found.
//   There is NO "changed" test: the copy is made every time.
// - The scale: forced, the rescale is 1.0, then the outside file's ENVSDSCL when it has one. Not forced: the outside ENVSDSCL when NO object uses the shape (the header's object count is 0),
//   else the rescale is kept.
// - The copy (OP-138a): the outside shape's points x the rescale (truncated, __ftol), its radius and box x the rescale; its normals, polygons, texture coordinates and texture file names as
//   they are. The centre is worked out again ((max + min) / 2), and the radius about it. The level's header keeps its flags, lock user, number and object names.
// - The extra chunks (OP-138b): the level shape's SHPZSPDT, SHPMRGDT, SHPMORPH, TEXTANIM, SHPPCINF, SHPFRAGS, ANIMSEQU, PNOTINBB, ANSHCEN2, ASALTTEX, CONSHAPE and SHPPRPRO go. The outside
//   shape's are copied in, all but SHPPRPRO. Inside SHPFRAGS, each SUBSHAPE's points, box and radius, and its FRAGLOCN place, are scaled too.
// - The SHPEXTFN gets the outside shape's version (its header's version_no) and the rescale (OP-138e).
// - With no EXTOBJNM and not exactly one shape in the file, only the SHPEXTFL's RIFFNAME is replaced by the outside file's (0x51328e-0x5133f1).
// - The texture list (OP-138c): with an outside BMPNAMES of at least one name, the SHPEXTFL keeps ONE BMPLSTST. It is left as it is when it holds the same names (any capitals), flags
//   (0xf0042) and versions, else it becomes the outside names with each one's flags masked to 0x10ffc3f. The polygons keep their own texture numbers. NAMED: the palette bit 0x40 and the
//   level's palette lists (the 8-bit tools' PRSETPAL / ENVPALET work) are not copied.
//
// EVERY OUTSIDE FILE IS HOSTILE (OP-141). It is read by shape_import.h's ShapeImport_Read (the plain-name rule, the 4 MB cap; Frost's), and walked by the same chunk helpers. Every count is
// measured against its chunk. The rescale must be a real number above 0 and at most 1,000,000 (Rescale's own rule, OP-13), and every scaled place within the game's 2^24 (chunk.hpp CheckPlace).
// The caller hands the new shape to the game's own loader before the level takes it, and keeps the level's copy when it refuses. A refusal changes nothing and says why.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	enum class ShapeUpdateResult
	{
		Done,          // the shape was brought up to date (newShape holds it)
		NameOnly,      // only the SHPEXTFL's RIFFNAME was replaced (no EXTOBJNM and not exactly one shape in the file)
		NotOutside,    // the shape has no SHPEXTFL / SHPEXTFN: not an outside shape (nothing to do)
		BadShape,      // the LEVEL's shape can't be walked, or its header / SHPEXTFN is too short
		BadFile,       // the outside file can't be walked, or its shape's chunks don't fit their counts
		NoShape,       // the outside file has no shape the EXTOBJNM names (nothing changes, as in inteng)
		BadScale,      // the rescale isn't a real number above 0 and at most 1,000,000, or a scaled place would be farther than 2^24
	};

	// In plain words, for the log and the status line: static text, never null
	char const *ShapeUpdateWords(ShapeUpdateResult result);

	// The outside file's name for a level shape (a whole REBSHAPE chunk): its SHPEXTFN name's last part (after the last ':', '/' or '\', as FUN_00567e40), with ".rif" taken off
	// (any capitals). false when the shape is not an outside one, or the name is empty.
	bool ShapeUpdate_FileName(std::vector<char> const &shapeChunk, std::string *name);

	// Whether a level shape (a whole REBSHAPE chunk) is the one the object of that name uses: its header's object list names it (the chunk library ties objects to shapes so)
	bool ShapeUpdate_UsedBy(std::vector<char> const &shapeChunk, std::string const &objectName);

	// FUN_00512fc0 on the bytes: `shapeChunk` is the level's whole REBSHAPE chunk, `outside` the outside file (a whole REBINFF2). forced: Take scale (1), else the load's update (0).
	// Done / NameOnly: *newShape is the whole new REBSHAPE chunk. Anything else: *newShape is empty and *why (when given) says what. *before / *after (when given) get the rescale before and
	// after: Take scale then rescales the shape's placed objects by after / before (FUN_004e1970).
	// levelUsers: how many of the level's objects are tied to this shape (ShapeUpdate_LevelUsers), for the scale's "no object uses it" test (FUN_0050e9f0). Follows FUN_00512fc0 step by step,
	// reassess/op138-fidelity-traces-vasquez.json (11-17): the outside shape found through the file's RBOBJECT of the EXTOBJNM's name, every SUBSHAPE updated, the SHPEXTFL's texture list,
	// BMNAMEXT, BMNAMVER and RIFFNAME done as inteng does them.
	ShapeUpdateResult ShapeUpdate_Build(std::vector<char> const &shapeChunk, std::vector<char> const &outside, bool forced, int32_t levelUsers, std::vector<char> *newShape, std::string *why,
		double *before = nullptr, double *after = nullptr);

	// The same, for a shape whose level is not at hand: its header's object count (+64) stands for the objects tied to it
	ShapeUpdateResult ShapeUpdate_Build(std::vector<char> const &shapeChunk, std::vector<char> const &outside, bool forced, std::vector<char> *newShape, std::string *why,
		double *before = nullptr, double *after = nullptr);

	// FUN_004bf1d0's tie of the level's objects to its shapes: for each of the level's top chunks (a whole REBINFF2 image), how many RBOBJECTs with an OBJHEAD1 are tied to it (each to the
	// FIRST REBSHAPE, in the file's order, whose SHPHEAD1 number is its OBJHEAD1 shape number; 0 for any other chunk). false when the level can't be walked.
	bool ShapeUpdate_LevelUsers(std::vector<char> const &level, std::vector<int32_t> *users);
}
