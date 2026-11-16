// shape_model.h: a shape's points, polygons, normals and extents, read from the bytes of its chunks and put back into bytes. P11, the base of
// the shape-editing items (Merge Objects, Fix tears, Recalculate Normals and Extents, Recentre Shape, Replace shape, Rescale, the Texture Map
// mode's face selection). Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHAT IT IS. A shape (a REBSHAPE chunk) holds, among others, these chunks (avpx 3dc/win95/chunk_shape.cpp):
//   SHPHEAD1  the header: after its 12-byte heading, flags (4), the user who locked it (16), the shape's number (4), the number of points (4),
//             the number of polygons (4), the radius about the origin (a float, 4), the extents (six whole numbers: max x, min x, max y, min y,
//             max z, min z), the version (4), and the names of the objects that use it (a count, then each name) (chunk_shape.cpp:1603-1650);
//   SHPRAWVT  the points: three whole numbers each (x, y, z), in the shape's own units;
//   SHPPOLYS  the polygons: 36 bytes each: the engine's type, the normal's number, the flags, the colour, then five corner numbers, the unused
//             ones -1 (chunk_shape.cpp:1111-1132);
//   SHPPNORM  one normal (three floats) for each polygon, and SHPVNORM one for each point.
// This module is only about those numbers. It holds no pointer into the game's chunk classes: the host hands it the BYTES of the payloads (what
// OpenLevelFile::PayloadOf gives) and takes bytes back (SetPayload), the same way every other edit works. It changes nothing by itself and
// has no idea what an edit means: what Merge, Fix tears or Recentre do to these numbers is each item's own code, copied from inteng's handler.
//
// THE RULES (a level's bytes are hostile; Tao 1 and 3):
// - Every count is checked against the bytes BEFORE anything is sized from it, and against kMostPoints and kMostPolygons, so a count a file
//   makes up can't make a huge allocation or read past a payload's end.
// - The counts the header holds must equal the counts the payloads' sizes give; if they disagree the shape is refused, not guessed.
// - A polygon has 3 to 5 corners (the game's five slots), the unused slots -1, and every corner is a point that exists. A normal number is
//   0 or more and below the number of normals, or the polygon says "none" with -1 only where the game does (it is kept as read).
// - Putting numbers back keeps the chunk's SIZE: the writers refuse a change in the number of points or polygons, because an edit never
//   changes a chunk's size (OpenLevelFile::SetPayload). A change in the count is a whole new chunk, which is the host's business.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace model
{
	// The most points or polygons a shape is read to. The game's own limit for a module is far below (maxpolys is 4000); this only stops a made-up
	// count from sizing a buffer, so it is generous.
	size_t const kMostShapePoints = 1u << 20;
	size_t const kMostShapePolygons = 1u << 20;

	size_t const kShapePolygonBytes = 36;     // one polygon in SHPPOLYS
	size_t const kShapePointBytes = 12;       // one point in SHPRAWVT
	size_t const kShapeNormalBytes = 12;      // one normal in SHPPNORM or SHPVNORM

	struct ShapePoint
	{
		int32_t x, y, z;
	};

	struct ShapeNormal
	{
		float x, y, z;
	};

	struct ShapePolygon
	{
		int32_t engineType;
		int32_t normalIndex;
		int32_t flags;
		int32_t colour;          // for a textured polygon its low 15 bits are the bitmap and its high 16 bits the UV list (avp/win95/..., the plugin's geometry.py)
		int32_t corners[5];      // point numbers, the unused ones -1
		int cornerCount;         // 3 to 5
	};

	struct ShapeExtents
	{
		int32_t maxX, minX, maxY, minY, maxZ, minZ;   // as the header holds them, in this order
	};

	// A whole shape's numbers.
	struct ShapeModel
	{
		int32_t flags;
		int32_t number;                    // the shape's own number (file_id_num)
		float radius;                      // about the origin
		ShapeExtents extents;
		std::vector<ShapePoint> points;
		std::vector<ShapePolygon> polygons;
		std::vector<ShapeNormal> polygonNormals;   // one per polygon
		std::vector<ShapeNormal> pointNormals;     // one per point
	};

	enum class ShapeModelResult
	{
		Ok,
		BadCall,          // a payload pointer is null for a size above 0
		HeaderTooShort,   // SHPHEAD1 is shorter than its fixed part
		TooBig,           // a count above kMostShapePoints or kMostShapePolygons
		SizesDisagree,    // a payload's size isn't a whole number of its records, or the counts the header holds aren't the counts the sizes give
		BadCorner,        // a polygon with fewer than 3 corners, a corner that isn't -1 after the last, or a point number that doesn't exist
		BadNormalNumber,  // a polygon's normal number is outside the normals
		NotFinite,        // a float in the header or a normal is not finite
		WrongCount,       // a writer was handed a model whose count differs from the bytes it would overwrite
	};

	// What an answer means, in plain words for the status line: static text, never null.
	char const *ShapeModelWords(ShapeModelResult result);

	// The bytes of each chunk's payload (after the 12-byte heading). The normals chunks may be absent (size 0, null): the shape then has no normals.
	struct ShapeBytes
	{
		uint8_t const *header;      size_t headerSize;       // SHPHEAD1
		uint8_t const *points;      size_t pointsSize;       // SHPRAWVT
		uint8_t const *polygons;    size_t polygonsSize;     // SHPPOLYS
		uint8_t const *polyNormals; size_t polyNormalsSize;  // SHPPNORM
		uint8_t const *ptNormals;   size_t ptNormalsSize;    // SHPVNORM
	};

	// Reads a shape. *model is emptied first and holds the shape only when the answer is Ok.
	ShapeModelResult ReadShapeModel(ShapeBytes const &bytes, ShapeModel *model);

	// Puts numbers back, each into bytes of exactly the size they came from (the size is the vector's own size on entry; a count that differs is
	// WrongCount and the bytes are left as they were). Each leaves every byte it has no number for as it was, so unknown trailing bytes survive.
	ShapeModelResult WriteShapePoints(ShapeModel const &model, std::vector<uint8_t> *pointsBytes);
	ShapeModelResult WriteShapePolygons(ShapeModel const &model, std::vector<uint8_t> *polygonBytes);
	ShapeModelResult WriteShapeNormals(std::vector<ShapeNormal> const &normals, std::vector<uint8_t> *normalBytes);
	// The header's counts are the model's, its radius and extents are the model's; the lock, the version and the names stay as they were.
	ShapeModelResult WriteShapeHeader(ShapeModel const &model, std::vector<uint8_t> *headerBytes);
	// The same, for an edit that CHANGES the counts (Merge Objects): the header's counts are made the model's, then everything WriteShapeHeader
	// puts in is put in. The header chunk keeps its size (the counts are fixed-size numbers). The caller makes the other chunks' sizes match.
	ShapeModelResult WriteShapeHeaderSettingCounts(ShapeModel const &model, std::vector<uint8_t> *headerBytes);
}
