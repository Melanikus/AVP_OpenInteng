// editor_icons.h: the ICONS inteng draws for its editor objects. Written 2026-11-16 by Vasquez (Sonnet 5.5) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (read: FUN_004e4ae0 0x4e4ae0 the placement object's constructor, FUN_004b2590 the map object builder, FUN_0042c700 0x42c711-0x42c7c0 the shape table, FUN_00521cb0 the Strategy
// Data scene, FUN_00539ee0, and the shapes' bytes in inteng.exe's data):
// - Every editor object has a display block made by FUN_004b2590 from the editor's OWN SHAPE, a built-in shape the program puts in the game's shape table at start (DAT_00931140 + 4 * index, filled by
//   FUN_0042c700). The object classes ask for their shape by that index (the argument before the place, in FUN_004e4ae0's call: PUSH index, then the place, then PUSH type):
//     shape 2  PARTICLE GENERATOR (type 7, FUN_004e65c0) and the LIGHT MARKER (descriptor 0x5e1c28, FUN_004ae5d0)   5 points, 5 polygons: a pyramid, apex at the place, base behind it (-z)
//     shape 3  AVP GENERATOR (type 1, FUN_004e4f90 / FUN_004dd4d0)                                                    28 points: a flat green silhouette
//     shape 4  PLACED HIERARCHY (PLACHIER records of Strategy Data, FUN_0053b5a0; type 9 FUN_004e7c30)               12 points, 20 polygons: red and blue
//     shape 7  AVP BAD GUY (type 2, FUN_004e57e0 / FUN_004dd600)                                                       56 points, 64 polygons: a head, blue and red
//     shape 8  SOUND (type 3, FUN_004ecb20; SOUNDOB2 records, FUN_0053ac30)                                           32 points, 30 polygons: a speaker, dark red and red
//     shape 9  PLAYER STARTING POSITION (type 8, FUN_004216d0)                                                         102 points, 64 polygons
//     shape 13 VIRTUAL OBJECT (FUN_00539ee0(0xd, x, y, z), Strategy Data's VIOBJECT records, FUN_005393c0)            41 points, 24 polygons: light blue
//   (the other numbers of Strategy Data's records: PARGENER shape 2, CUTSHEAD 0xc, MUMMTRAP 0x12, MUMSOUND 8: only the first is AvP's)
// - The shape's numbers are in inteng's memory units, 100 to a world unit (the place is the file's x 100 x the level's scale), so the world numbers are the shape's divided by 100. Each polygon has its
//   own flat colour (PolyColour, 0xRRGGBB: sound 0x961414 and 0xc81e1e, generator 0x00c832, bad guy 0x0000ff and 0xff1e1e, start 0xfc0000 and 0x0000fc, virtual object 0x007fff, pyramid 0x0080ff 0x00c800
//   0x008000 0x000080 0x808000).
// - The block's turn: the generator and the bad guy about the vertical axis by the object's heading (4096 to a turn, FUN_004e4ae0's last argument), the particle generator and the start by their
//   own orientation, the sound none.
// - SELECTED: the object's method +0x28 sets 0x40000000 in the block's flag word (FUN_004e4f40 / FUN_004e2fa0), cleared by +0x2c; the selected light is known to flash, and so does this (white and the
//   shape's own colours, one then the other every three frames, as the light markers do here).
// - WHICH MODES: the placement objects exist only while Object Placement (and its sounds screen, where only the sounds) is the mode (the list FUN_004d68e0 makes and FUN_004d9820 empties). Strategy Data
//   makes its records of the level's VIOBJECT, AVPGENER, SOUNDOB2 (MUMSOUND), PLACHIER, PARGENER ... each with the icon above, as long as that mode runs. Light Sourcing shows the light markers.
// NAMED: how the display blocks are lit (inteng's flags 0x22008 in the descriptor 0x5cda48) is not copied: the polygons are flat and opaque, as the light markers' are.
// WHERE THE SHAPES COME FROM (Melanikus's ruling via Bishop 11-16: inteng's binary data is never in our source): the points, polygons and colours above are read at run time from his own
// inteng.exe (inteng_shapes.h, EditorIcon_UseShapes); they were copied into editor_icon_shapes.inc before, which is gone. A shape not read is drawn as OUR OWN plain box, 500 mm across, in
// the editor's old marker colours (generator green, bad guy red, start white, sound cyan, particle generator orange, placed hierarchy magenta, virtual object blue), and EditorIcon_From says why.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "inteng_shapes.h"

namespace host
{
	struct IconWorldTriangle
	{
		int32_t a[3];
		int32_t b[3];
		int32_t c[3];
		uint32_t colour;   // 0xAARRGGBB, opaque
	};

	// Where the icons come from now, for the status line: "inteng.exe" or why our plain boxes are drawn
	char const *EditorIcon_From();

	// The shapes read from inteng.exe (inteng_shapes.h): each known one read there replaces our box; from = the file read. EditorIcon_NotRead keeps the boxes and says why.
	void EditorIcon_UseShapes(IntengShapes const &shapes, std::string const &from);
	void EditorIcon_NotRead(std::string const &why);

	// True for the shape indices above (2, 3, 4, 7, 8, 9, 13)
	bool EditorIcon_Known(int shape);

	// Adds the icon's triangles for the shape at the world place (millimetres), turned by the axes (three rows of three numbers: the shape's x, y and z axes in the world, unit length; null: none), all one
	// colour when flash is not 0 (0xAARRGGBB). Nothing is added for a shape that is not known.
	void EditorIcon_Add(int shape, double const place[3], double const *axes, uint32_t flash, std::vector<IconWorldTriangle> *out);

	// How many triangles the shape has (0 for an unknown one)
	size_t EditorIcon_TriangleCount(int shape);

	// The shape's radius in millimetres: the farthest of its points from its centre (0 for an unknown one). Object Placement's view distance takes it away from the icon's distance, as FUN_004dd2f0 takes the
	// block's radius (+0x100). NAMED: computed from the points; the radius word of inteng's built-in shape header is not read.
	double EditorIcon_Radius(int shape);
}
