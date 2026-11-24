// shape_thumb.h: A SHAPE FILE MADE READY TO DRAW SMALL, for the placement picker's tiles (shape_picker.h), pure: the file's bytes in (as ShapeImport_Read read
// them, on one handle), its first shape's points and polygons out, each polygon with the picture it is painted with and its corners' places in that picture,
// then the triangles to draw, lit, and the camera that frames them. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// EVERY FILE IS HOSTILE (Tao 3): a stranger's shape in <game>\shape_rifs, or anything a folder holds. Every chunk is walked inside its parent (shape_import.h's
// shape_chunks); the shape is read by model::ReadShapeModel (each count against its chunk's size, each corner a point that exists) and its texture coordinates by
// model::ReadUvLists; the texture list by shape_chunks::ReadBitmaps (the outside-shape update's own reader). A picture draws at most kShapeThumbMostPoints points
// and kShapeThumbMostPolygons polygons (16 times stock's most: predship.rif, 719 points and 981 polygons, measured 2026-11-23 over the old tools' 322 files and
// the 343 Option C makes); a file past them is refused with its counts. Nothing here reads a picture (the host does, as the level's open does) or touches the
// drawing device.
//
// WHICH SHAPE: the file's FIRST shape (REBSHAPE). A file of several is a fragment file: its first part is the picture, and parts says how many (the picker
// marks it "N parts"; the import takes a file of one shape: shape_import.h).
// WHICH PICTURE A POLYGON USES, as the game's loader finds it for an outside shape (avpx chunk_load.cpp setup_tex_conv_array, 851-936): a textured polygon
// (model::EngineTypeIsTextured) has its picture's NUMBER in its colour's low 15 bits; that number is a texture list entry's own "index" (not its place in the
// list; two entries of one index: the LAST, as the game's loop leaves it); the entry's file is its name when it has the IFF flag (0x01000000), else
// "<the file's RIFFNAME>/<its name>"; an entry with the NotInPC flag (0x800) sets no picture (the game's loop skips it), so its polygons are drawn shaded.
// The path is the game's: under its graphics folder, as the level's own textures are read (projload.cpp load_rif_bitmap). IT IS THE FILE'S OWN BYTES, so it
// is held to the level open's own rule for every texture name (avpx level_file_name.h LevelFileName_WhyRefused: plain letters in plain parts; no "..",
// drive, stream, "\\server" or device): a refused one has no path, only why, and is never read (Frost's review, 2026-11-23, MUST 1).
// Its corners' places come from the polygon's texture coordinate list (colour >> 16), in the picture's own texels: the triangles take them over its size.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	size_t const kShapeThumbMostPoints = 16384;
	size_t const kShapeThumbMostPolygons = 16384;
	int32_t const kShapeThumbFlagIff = 0x01000000;      // avpx bitmap_names.hpp ChunkBMPFlag_IFF: the name is the picture's whole path under graphics
	int32_t const kShapeThumbFlagNotInPc = 0x800;       // avpx bitmap_names.hpp ChunkBMPFlag_NotInPC: not a picture for the PC

	// A picture the shape's polygons use
	struct ShapeThumbPicture
	{
		int32_t index = 0;        // the texture list entry's own index (a polygon's picture number)
		int32_t flags = 0;
		std::string name;         // as the list holds it ("Common\Pikup.rim"): hostile bytes, shown only through a safe text rule
		std::string path;         // the file under the game's graphics folder: the name, or "<RIFFNAME>/<name>" ("Common\Pikup.rim"); "" when refused
		std::string refused;      // why the path is refused (LevelFileName_WhyRefused), "" when it may be read
	};

	struct ShapeThumbPolygon
	{
		int32_t corners[5] = { -1, -1, -1, -1, -1 };   // point numbers
		int cornerCount = 0;                            // 3 to 5
		int picture = -1;                               // in ShapeThumbModel::pictures, -1 when drawn shaded (not textured, or its picture or list isn't there)
		float texels[5][2] = {};                        // each corner's place in its picture, in the picture's own texels
	};

	struct ShapeThumbModel
	{
		size_t parts = 0;                               // the shapes the file holds (more than one: a fragment file, its first drawn)
		std::vector<float> points;                      // x, y, z of each point (the world's mm, y down)
		std::vector<ShapeThumbPolygon> polygons;
		std::vector<ShapeThumbPicture> pictures;        // each picture the polygons use, once, in the order first used
		float centre[3] = { 0, 0, 0 };                  // the middle of the points' box
		float radius = 0;                               // the farthest point from it
	};

	// The model of a shape file's bytes. False, with *why in plain words, when the file can't be drawn: not a rif, no shape, chunks that run past their parents,
	// a shape model::ReadShapeModel refuses, past the limits, no point at all. A polygon whose texture list or picture entry is missing or wrong is drawn shaded
	// (its picture -1), never a reason to refuse the file.
	// *noGeometry (when given): true when the file was refused because it holds NO GEOMETRY to draw: no shape in it, a shape with no header, points or polygons,
	// or a shape with no point; false otherwise (drawn, or refused as broken or too big). The placement picker leaves such a file out of its grid (the owner,
	// 2026-11-23: "No picture means no geometry? if yes don't show!"), while a broken or too big one keeps its tile and says why.
	bool ShapeThumb_Read(std::vector<char> const &file, ShapeThumbModel *model, std::string *why, bool *noGeometry = nullptr);

	// WHAT IS DRAWN, as bytes (the placement picker folds files that look the same into one tile: the owner, 2026-11-23, through Bishop 2): the points' exact
	// values, then each polygon's corners, its texture places and its picture's path in small letters ("" when shaded): never the file's other bytes (its own
	// number, its list's numbering, its other chunks), so two copies extracted from two levels with the same look give the same bytes, and a moved point or
	// another picture gives others. Empty for a fragment file (more than one part: only its first is drawn, so it never folds)
	std::string ShapeThumb_LookBytes(ShapeThumbModel const &model);

	// One corner of a triangle to draw: its place, its shade (0 to 1, from the light), its place in its picture (0 to 1 across it; 0 when shaded)
	struct ShapeThumbCorner
	{
		float x, y, z;
		float shade;
		float u, v;
	};

	// Triangles to draw with one picture (picture -1: shaded only)
	struct ShapeThumbBatch
	{
		int picture = -1;
		std::vector<ShapeThumbCorner> corners;   // three a triangle
	};

	// The triangles, grouped by picture: each polygon a fan of its corners, shaded by how its face turns to the light (both sides lit alike: a picture shows
	// the shape whichever side faces it), its picture's places divided by that picture's size. sizes: each picture's width and height in texels, as the host
	// read it (0 by 0 when it couldn't be read: its polygons are drawn shaded).
	std::vector<ShapeThumbBatch> ShapeThumb_Triangles(ShapeThumbModel const &model, std::vector<std::pair<uint32_t, uint32_t> > const &sizes);

	// THE CAMERA: from the 45-degree corner (the owner's test camera, 2026-11-18: 45 degrees round and 45 up, a cube's corner), looking at the shape's
	// middle, as far as frames its whole radius in a view fieldOfView radians across. eye, at: the world's mm; y down, as the level's.
	void ShapeThumb_Camera(ShapeThumbModel const &model, float fieldOfView, float eye[3], float at[3]);

	// The light's direction (toward the light, unit length): from over the camera's left shoulder, the same for every tile
	void ShapeThumb_Light(float toLight[3]);
}
