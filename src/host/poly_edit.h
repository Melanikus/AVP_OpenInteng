// poly_edit.h: Texture Map's POLY EDIT group, first piece: Select As > Vertex (command 0xfd) and Create poly (0x10e). Written 2026-11-15 by Melanikus (Hicks) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT INTENG DOES (AvP-docs\engine\inteng\vertex-mode-facts.md, read from the Ghidra dump: FUN_00565310, FUN_00564dc0, FUN_00564fc0, case 0x10e of FUN_0054d260, FUN_00565460):
// - VERTEX MODE. A click takes the CORNER NEAREST THE CLICK ON THE SCREEN among the corners of the polygon under the click and of that polygon's quad partner. At most four corners
//   are held, all of ONE shape. Shift+click takes a held corner out. A corner already held, or one of another shape, does nothing. The fourth corner is taken back at once when the
//   four cannot make a flat convex quad (FUN_00564fc0); when they can, the held list is put in the order that goes round the quad.
// - CREATE POLY. With three corners held, one triangle; with four, two triangles (v0,v1,v3) and (v1,v2,v3), paired as one quad. A triangle is a plain-colour polygon (engine type 2,
//   colour 0x7fff, flags 0) with the next polygon number as its normal number and a new unit normal (cross of the first edge with the second); when the camera looks along that normal
//   (dot above 0) the polygon is turned round (corners 2 and 3 swapped, normal negated) so it faces the viewer. A shape with a SHPMORPH is not edited (inteng's own rule).
//
// NAMED DEVIATIONS (each is in the card list, unfinished-in-inteng.md):
// - The convexity test of FUN_00564fc0 is worked in doubles with inteng's thresholds (500 and 400 of 65536); inteng works in 16.16 whole numbers. The two can differ for four
//   corners that sit right on a threshold.
// - The camera test uses the view direction in the SHAPE's own space. For a module that is the world; for a placed object that has a turn the caller gives the direction turned back.
// - The pairing of a quad's two triangles is kept as SHPMRGDT numbers only when the shape HAS a SHPMRGDT chunk (inteng's pairing, FUN_0055a360, is not read). Without one the two
//   triangles stay two triangles, which is how the game's loader treats a shape with no merge data.
//
// THE RULES (a level's bytes are hostile; Tao 1 and 3): every corner number is checked against the shape's points; the count of polygons is checked against kMostShapePolygons; the
// level is changed only through OpenLevelFile::ReplaceLevel, which reads it with the game's own loader and swaps it in only if the loader takes it.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "shape_model.h"
#include "uv_model.h"

class OpenLevelFile;

namespace host
{
	size_t const kPolyVertexMost = 4;

	// ---- The pick

	// One corner that could be taken: its point number in the shape, and where it is on the screen (onScreen false when it does not project; inteng skips those)
	struct PolyCornerSpot
	{
		int32_t point;
		bool onScreen;
		double x, y;
	};

	// The corner nearest (cx, cy) by squared distance, the first of equals; none farther than a squared distance of 1e9 (inteng's start value). False when there is none.
	bool PolyEdit_NearestCorner(PolyCornerSpot const *spots, size_t count, double cx, double cy, int32_t *point);

	// ---- The list of held corners

	struct PolyVertexList
	{
		uint8_t id[8];         // the object (or module) whose shape the corners are of: all of the one shape
		int32_t point[kPolyVertexMost];
		size_t count;
	};

	enum class PolyVertexClick
	{
		Added,             // taken
		AddedAndOrdered,   // the fourth, taken, and the four are now in the order round the quad
		Removed,           // Shift: taken out
		OtherShape,        // not the shape the held corners are of
		AlreadyHeld,       // a plain click on a corner already held
		NotHeld,           // Shift on a corner not held
		NotAQuad,          // the fourth corner would not make a flat convex quad, so it was not taken
		ListFull,          // four are held already
		BadCall,           // a null pointer, or a point number the shape does not have
	};

	// What an answer means, in plain words for the status line: static text, never null
	char const *PolyVertexClickWords(PolyVertexClick result);

	// Whether four corners (point numbers into points) make a flat convex quad, and if so in what order they go round it (FUN_00564fc0). The order is the permutation (p, q, r, s)
	// the first accepted by inteng's search: p over 0..3, then q, then r, s the one left.
	bool PolyEdit_QuadOrder(std::vector<model::ShapePoint> const &points, int32_t const four[4], int32_t order[4]);

	// One click of Vertex mode, on the corner `point` of the shape of the object `id`. shift true takes it out.
	PolyVertexClick PolyEdit_VertexClick(PolyVertexList *list, uint8_t const *id, int32_t point, bool shift, std::vector<model::ShapePoint> const &points);

	void PolyEdit_VertexClear(PolyVertexList *list);

	// ---- Create poly

	enum class PolyCreateResult
	{
		Done,
		BadCall,        // not three or four corners, a corner the shape does not have, a corner twice, or a null pointer
		Degenerate,     // the corners are in a line, or two are at one place: no polygon with a normal
		TooMany,        // the shape would have more polygons than kMostShapePolygons
		NoObject,       // no object has that ID
		NoShape,        // the object has no shape the editor can read
		Morphing,       // the shape has a SHPMORPH, which inteng does not edit
		Unreadable,     // a chunk of the shape could not be read or put together
		CantBuild,      // the level could not be put together from its bytes
		LoaderRefused,  // the game's loader would not take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the status line: static text, never null
	char const *PolyCreateWords(PolyCreateResult result);

	// The numbers of the change, no level needed. count is 3 or 4; the corners are point numbers in the order the list holds them (for 4, already round the quad). viewForward is the
	// way the camera looks, in the shape's space (any length; 0,0,0 means no flipping). mergeData is the shape's SHPMRGDT numbers, or null when it has none; it grows by one number
	// for a triangle, two for a quad (which are then each other's partner). On anything but Done the shape and mergeData are as they were. *first is the number of the first new polygon.
	PolyCreateResult PolyEdit_CreateInModel(model::ShapeModel *shape, std::vector<int32_t> *mergeData, int32_t const *corners, size_t count, double const viewForward[3], size_t *first);

	// The same on a level: the object whose 8-byte ID is id, its shape's five chunks (and SHPMRGDT when it has one) made again at the new counts, the level put in by ReplaceLevel.
	// After a success every Chunk * the editor held is gone (object_edit.h says the same). *first and *added (when not null) say which polygons are new.
	PolyCreateResult PolyEdit_Create(OpenLevelFile *file, uint8_t const *id, int32_t const *corners, size_t count, double const viewForward[3], size_t *first, size_t *added, std::string *why);

	// ---- Polygon TYPE changes: textured <-> plain (Select Colour 0x40 / 0x41, FUN_00570490 with FUN_0055a8a0; Anim > Add Polys on a plain polygon)

	enum class PolyTypeResult
	{
		Done,
		BadCall,          // no polygons, a polygon twice, a null pointer, or a colour word the game's loader would refuse
		NoObject,         // no object has that ID
		NoShape,          // the object has no shape the editor can read
		NoPolygon,        // a polygon number the shape does not have
		Animated,         // MakePlain: a polygon is in the shape's TEXTANIM; take it out of the animation first (Anim > Remove Polys): inteng does that itself (FUN_005413d0), here it is the caller's step
		TooManyCorners,   // MakeTextured: a polygon with 5 corners (a list of texture coordinates holds 4)
		NoPicture,        // MakeTextured: the shape has its own list of picture names (SHPTEXFN) and none has that number, or it has no SHPUVCRD chunk to put the list in. A shape with no SHPTEXFN takes the LEVEL's pictures: the number is the caller's (a picture that does not exist is shown by the game as a line)
		Unreadable,       // a chunk of the shape could not be read or put together
		CantBuild,        // the level could not be put together from its bytes
		LoaderRefused,    // the game's loader would not take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the status line: static text, never null
	char const *PolyTypeWords(PolyTypeResult result);

	// MAKE PLAIN. Every polygon of the object's shape named in polygons (FILE polygon numbers, as the Texture Map selection holds them; any mix of textured and plain ones) becomes the plain
	// colour type (engine type 2) with colourWord as its colour; a textured one loses its list of texture coordinates (the list is taken out of SHPUVCRD and the lists after it are renumbered in
	// the polygons' colour words, unless another polygon still uses it). Flags and corners are not touched. colourWord is what the game's loader takes as a plain polygon's colour, any whole
	// number but one with the bit 0x80000000 (negative: refused). On success *changed (when not null) is how many polygons were TEXTURED and are plain now. One new image of the level through ReplaceLevel;
	// on any answer but Done the level is as it was.
	PolyTypeResult PolyEdit_MakePlain(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, int32_t colourWord, size_t *changed, std::string *why);

	// MAKE TEXTURED. Every PLAIN polygon among polygons becomes engine type 7 (the 3D textured polygon) showing the shape's picture number pictureNumber (-1: picture 0) with a NEW list of texture
	// coordinates of its own, every corner at (0, 0); a polygon that is textured already is left as it is. *changed (when not null) is how many were plain. Same rules as MakePlain.
	PolyTypeResult PolyEdit_MakeTextured(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, int32_t pictureNumber, size_t *changed, std::string *why);

	// ---- Taking polygons out (what Delete polys 0xc3 and Delete 0-normal polys 0xc9 flag, and inteng carries out when the Texture Map mode is left: FUN_00563d90 with FUN_00435080 and FUN_00435600)

	enum class PolyRemoveResult
	{
		Done,
		BadCall,        // no polygons, a polygon twice, or a null pointer
		AllPolygons,    // every polygon of the shape: inteng deletes the whole object then, which is Object Placement's Delete, not this
		NoObject,       // no object has that ID
		NoShape,        // the object has no shape the editor can read
		NoPolygon,      // a polygon number the shape does not have
		Unsupported,    // the shape has morphing data, an animation, or another per-polygon chunk this does not carry over (*why names it)
		Unreadable,     // a chunk of the shape could not be read or put together
		CantBuild,      // the level could not be put together from its bytes
		LoaderRefused,  // the game's loader would not take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the status line: static text, never null
	char const *PolyRemoveWords(PolyRemoveResult result);

	// What a removal did to the numbers: for each old polygon its new number (-1: taken out), for each old point its new number (-1: no polygon left uses it, so it is taken out).
	struct PolyRemoveMap
	{
		std::vector<int32_t> polygon;
		std::vector<int32_t> point;
	};

	// The numbers of a removal, no level needed (FUN_00435600 without the recentring): the polygons named are taken out; the others keep their order, their normal and their normal number (made their own
	// new number, as inteng does); a point stays only when a kept polygon uses it (the order of the rest kept, corners and point normals follow); the extents are worked out again from the points left and the
	// radius is the longest distance of a point from the origin. mergeData (null when the shape has none) is renumbered through the map (a partner that is gone becomes -1). On anything but Done the shape
	// and mergeData are as they were.
	PolyRemoveResult PolyEdit_RemoveInModel(model::ShapeModel *shape, std::vector<int32_t> *mergeData, int32_t const *polygons, size_t count, PolyRemoveMap *map);

	// The same on a level: the object whose 8-byte ID is id, its shape's chunks made again at the new counts (header, points, polygons, both normal arrays, SHPMRGDT, the list of texture coordinates when a
	// list is left without a user, TEXTANIM with the polygons renumbered, SHPCENTR from the new extents), the level put in by ReplaceLevel. NAMED: inteng asks "would you like to have the affected shapes
	// recentred"; this answers no. Shapes with SHPMORPH, ANIMSEQU, SHPPCINF, SHPFRAGS or SHPFRGTP are refused (inteng refuses the first two for Delete itself). After a success the polygon numbers of that
	// shape have moved (see the map) and every Chunk * the editor held is gone.
	PolyRemoveResult PolyEdit_Remove(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, PolyRemoveMap *map, std::string *why);

	// ---- Unmerge selected polygons (0x11a, FUN_00573e20): each polygon named that is paired (the shape's SHPMRGDT numbers) is parted from its partner, both numbers made -1 (the partner need not be named).
	// The merge data keeps its size, so it is put back with SetPayload and the level is read again by the loader (ReplaceLevel) to make the game's items the triangles. On a refusal the numbers are put back.

	enum class PolyUnmergeResult
	{
		Done,
		BadCall,
		NoObject,
		NoShape,
		NoPolygon,      // a polygon number the shape does not have
		NoMergeData,    // the shape has no SHPMRGDT: nothing in it is merged
		Unreadable,     // the merge data is not one number for each polygon
		CantBuild,
		LoaderRefused,
	};

	char const *PolyUnmergeWords(PolyUnmergeResult result);

	// *parted (when not null) is how many pairs were parted
	PolyUnmergeResult PolyEdit_Unmerge(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, size_t *parted, std::string *why);

	// ---- Merge selected polygons (0x119, FUN_00573620 with FUN_0055a360 / FUN_0055a0d0 / FUN_00573b70): pairs the selected triangles into quads in the shape's merge data (SHPMRGDT)

	// Whether triangles a and b of the shape can be one quad, inteng's own test (constants read from the exe: 0.9998, 0.6, -1.0, 1.0): both are triangles, exactly two corners are shared, the normals' dot is at least
	// 0.9998 or else the free corner of b lies within 1 unit of a's plane (with fake merge data: the dot is at least 0.6), and the quad is convex (each of the two turns round the free corners is 0 or less). *score
	// (when not null) is FUN_00573b70's: the larger cosine of the angle at the two free corners. NAMED: inteng works in 16.16 whole numbers, this in doubles.
	bool PolyEdit_Joinable(model::ShapeModel const &shape, int32_t a, int32_t b, bool fake, double *score);

	// The pairs one Merge makes, on the numbers: the unpaired ones of polygons in selection order; each is tried with every one after it, the joinable pairs are kept lowest score first (a tie in front) and taken
	// while both are still unpaired; mergeData (one number per polygon) gets the pairs. Gives how many pairs were made. NAMED: the list order of inteng's own insertion is read from FUN_00575960 / FUN_00569d60.
	size_t PolyEdit_MergeInModel(model::ShapeModel const &shape, std::vector<int32_t> *mergeData, int32_t const *polygons, size_t count, bool fake);

	enum class PolyMergeResult
	{
		Done,
		BadCall,
		NoObject,
		NoShape,
		NoPolygon,
		Unreadable,
		CantBuild,
		LoaderRefused,
	};

	char const *PolyMergeWords(PolyMergeResult result);

	// On the level: the shape's SHPMRGDT gets the pairs (same size: put in place; a shape that has none gets the chunk, as inteng's save writes it), the loader reads the level again. *joined: how many pairs.
	PolyMergeResult PolyEdit_Merge(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, size_t *joined, std::string *why);

	// Texture Map's Merge sub-mode's save (FUN_0055b900, TM-B 2c): the shape's whole pairing written as `numbers` (one per polygon, the partner or -1), and the same numbers into each SUBSHAPE of
	// its SHPMORPH / ASALTTEX whose counts are the shape's; the loader reads the level again. NoPolygon: a number that is no polygon of the shape. TM-B 2e: the pairs are checked first
	// (PolyEdit_CheckMergedPairs, FUN_0055d500 at 0x55bb73): the parted ones are written parted, and the shape's polygons take the flags, types and colours the checks gave them. *parted (when not
	// null): how many pairs the checks parted.
	PolyMergeResult PolyEdit_SetMergeData(OpenLevelFile *file, uint8_t const *id, std::vector<int32_t> const &numbers, std::string *why, size_t *parted = nullptr);

	// FUN_0055d500, Merge's save checking its pairs (TM-B 2e; reassess/tm-b-merge-map.md). For each pair, j the partner of i and j < i, in the order of i:
	//  - flags (SHPPOLYS +8) that differ: each polygon gets the two OR'd;
	//  - types, as Texture Map holds them (6 / 0x15 / 0x16 are 5, 0x17-0x19 are 7, a textured polygon with no list or an empty one is 2), that differ: an untextured one parts the pair; both
	//    textured, i takes j's type;
	//  - i untextured, or the pair parted: i takes j's colour, only when the file holds both untextured (INTENG BUG, FIXED: inteng copies a textured polygon's picture number as a colour, or a
	//    colour as a picture number);
	//  - both textured: another picture (colour & 0x7fff) or another place on it, in whole texels, at a corner they share (the same point) parts the pair.
	// A parted pair is -1 both in *merge. *polygonsChanged (when not null): whether a polygon's flags, type or colour changed. Gives how many pairs were parted. lists: the shape's SHPUVCRD.
	size_t PolyEdit_CheckMergedPairs(model::ShapeModel *model, std::vector<model::UvList> const &lists, std::vector<int32_t> *merge, bool *polygonsChanged);

	// The names of the objects that use the shape of the object `id` (its SHPHEAD1's list, in order); false when it has none or the list can't be read
	bool PolyEdit_ShapeUserNames(OpenLevelFile *file, uint8_t const *id, std::vector<std::string> *names);

	// Merge's save of a SHARED shape that not all its users take (FUN_0055aa50's copy path, TM-B 2d): the objects named in `movers` (the current one first) get a COPY of the shape (FUN_0050e880:
	// numbered one past the level's highest, the file's last chunk, its users the movers) holding `numbers` as its SHPMRGDT; the other users keep the shape as it was, less the movers' names. One
	// new image, read by the game's loader; nothing changes on any other answer.
	PolyMergeResult PolyEdit_SetMergeDataOnCopy(OpenLevelFile *file, uint8_t const *id, std::vector<std::string> const &movers, std::vector<int32_t> const &numbers, std::string *why,
		size_t *parted = nullptr);

	// ---- Split Object > As Placed Obj (0xc8, FUN_005746b0(1), carried out when the mode is left: FUN_00563d90's neighbour FUN_00575b60): the polygons named leave the object's shape and become a NEW SHAPE with a NEW PLACED OBJECT
	// around it, at the old object's place and turn, so they stay where they are. The new shape is the old one made from those polygons only (points they use, normals, merge data, lists of texture coordinates, TEXTANIM, SHPCENTR,
	// extents, radius); the object is made by object_make.h (named Split<n>, a placed object's default strategy, NAMED: inteng's own naming and strategy defaults of the split are not read); then the polygons are taken out of the old
	// shape (PolyEdit_Remove). NAMED: As Module is not built (a module changes the level's module list); no recentring; a shape with SHPMORPH, ANIMSEQU, SHPPCINF, SHPFRAGS, SHPFRGTP or SHPEXTFL is refused.

	enum class PolySplitResult
	{
		Done,
		BadCall,
		NoObject,
		NoShape,
		NoPolygon,
		AllPolygons,    // inteng: "Cannot remove all polygons from shape"
		Unsupported,
		Unreadable,
		CantBuild,
		LoaderRefused,
		OldNotCut,      // the new object was made but the old shape could not lose the polygons (they are in both; *why says why)
		BadName,        // the name given is not usable: empty is the default, else plain letters up to 63 that no object of the level has
		NotModule,      // As Module: the new object was made and the polygons taken out, but it could not be made a module (it stays a placed object)
	};

	char const *PolySplitWords(PolySplitResult result);

	PolySplitResult PolyEdit_SplitAsObject(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, uint8_t *newId, std::string *newName, std::string *why, bool asModule = false, char const *name = nullptr);

	// Whether polygons of the object's shape may be flagged for deletion: inteng's Delete polys refuses a shape with a SHPMORPH or an ANIMSEQU ("Can't alter shapes with morphing data", FUN_0056fd40). False for an object that is not there.
	bool PolyEdit_CanFlagForDeletion(OpenLevelFile *file, uint8_t const *id);

	// The first <base><n> (n from 0) that no object of the level has and that is not in alsoTaken ("" when none is found)
	std::string PolyEdit_NextFreeObjectName(OpenLevelFile *file, char const *base, std::vector<std::string> const &alsoTaken);
}
