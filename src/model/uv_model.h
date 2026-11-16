// uv_model.h: a shape's texture coordinates (the SHPUVCRD chunk) and the names of its pictures (SHPTEXFN), read from the bytes of the chunks and put
// back into bytes, and the pure maths of the Texture Map mode on one polygon's coordinates. P11, the data side of Texture Map. Written 2026-11-15 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT IS. A textured polygon of a shape (shape_model.h) keeps two numbers in its colour word (avpx 3dc/win95/chunk_load.cpp:2770-2776, the game's
// loader): the low 15 bits are the number of its picture in the shape's own list of picture names (SHPTEXFN), and the high 16 bits are the number of
// its LIST of texture coordinates in SHPUVCRD. A list is a count of corners (0, 3 or 4) and, for each corner, two floats u and v: the corner's place
// on the picture in texels (chunk_shape.cpp:1230-1284). The list's corners go in the same order as the polygon's corners. The game turns each float
// into a whole number of texels when it loads the shape (chunk_load.cpp:2789), so a coordinate with a fraction loses it there.
//
// THE RULES (a level's bytes are hostile; Tao 1 and 3). They are the game loader's own, so a file the editor writes is one the game takes:
// - The chunk's count of lists must fit the bytes (each list takes at least 4 bytes), each list's count of corners is 0 to 4 (the game's own levels have
//   0, 3 or 4) and fits the bytes, and
//   every u and v of every corner is a real number (told by its bits: real_numbers.h) between -kMostTexel and +kMostTexel (texture_animation.h's
//   kTextureAnimationFurthestTexel, 16383; the furthest in the game's own levels is 4,715).
// - A textured polygon's colour must not have bit 0x8000 (the loader refuses the level), its list number must be a list that exists, and the list has
//   either no corners (the game shows the polygon as a plain line) or at least as many as the polygon has (chunk_shape.cpp:310-334).
// - Putting numbers back keeps the chunk's SIZE (OpenLevelFile::SetPayload never changes it): every list must keep its count of corners.
//
// NAMED DEVIATION from inteng (a card in unfinished-in-inteng.md): inteng works on a quad as two triangles (three (u, v) pairs each, with the merge data
// SHPMRGDT to join them again). The file holds one list of 4 corners for the quad. Every move here is applied to every corner of the list, which is
// what inteng's per-corner flips do to both triangles; a move that is NOT per corner (a rotation, wrap, tile) is not in this module yet.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "shape_model.h"

namespace model
{
	size_t const kMostUvLists = 1u << 20;
	int const kMostUvCorners = 4;               // ChunkUV_List::vert has 4 places
	float const kMostTexel = 16383.0f;          // texture_animation.h kTextureAnimationFurthestTexel, either way
	size_t const kMostTextureNames = 1u << 15;  // the polygon's picture number has 15 bits
	size_t const kMostTextureNameBytes = 4096;  // one name, without its end; a bound the editor puts, not the game's

	struct UvCorner
	{
		float u, v;
	};

	struct UvList
	{
		int cornerCount;                // 0 to 4 (0 = an empty list; the game's own levels hold 0, 3 or 4)
		UvCorner corners[kMostUvCorners];
	};

	enum class UvModelResult
	{
		Ok,
		BadCall,          // a pointer is null for a size above 0, or no place for the answer
		TooShort,         // the chunk is shorter than its count
		TooBig,           // a count above kMostUvLists or kMostTextureNames
		CountDoesNotFit,  // the chunk says more lists or names than its bytes can hold
		BadCornerCount,   // a list with a negative count of corners, more than 4, or more than the bytes after it hold
		BadTexel,         // a u or v that is not a real number, or is beyond +-kMostTexel
		WrongShape,       // a writer was handed lists whose number or corner counts differ from the bytes it would overwrite
		NotTextured,      // the polygon is not a textured one
		BadColour,        // a textured polygon whose colour has bit 0x8000
		NoList,           // the polygon's list number is not a list the shape has
		ListTooShort,     // the polygon's list has fewer corners than the polygon (and is not empty)
		BadName,          // a name with no end inside the chunk, or longer than kMostTextureNameBytes
		NoPolygon,        // a polygon number outside the shape
		BadFlag,          // a flag change that would set bit 0x40000000, which only the game's loader sets
		BadWindow,        // a texture window whose width or height is 1 or less, for a turn that divides by it; or a window side with no length, or a tile scale of 0
		BadPlane,         // the chosen polygon has an edge of no length, so no plane can be made
	};

	// What an answer means, in plain words for the status line: static text, never null.
	char const *UvModelWords(UvModelResult result);

	// SHPUVCRD's payload (after the 12-byte heading) -> the lists. *lists is emptied first and holds the lists only when the answer is Ok. An empty chunk
	// (size 0) is the game's own "no lists" only when the game accepts it: the game reads the count first, so a chunk shorter than 4 bytes is TooShort.
	UvModelResult ReadUvLists(uint8_t const *payload, size_t size, std::vector<UvList> *lists);

	// The lists -> the bytes. The size is the vector's own size on entry (it came from ReadUvLists' payload); every list must have the corner count the
	// bytes hold (WrongShape otherwise, bytes left as they were). The coordinates are checked as ReadUvLists checks them.
	UvModelResult WriteUvListsSameShape(std::vector<UvList> const &lists, std::vector<uint8_t> *payload);

	// SHPTEXFN's payload: a count, then that many names, each ended by a 0 byte (chunk_shape.cpp:1373-1403). A name with no end inside the chunk is
	// BadName; as the game does, a count the bytes can't hold even at one byte a name is CountDoesNotFit.
	UvModelResult ReadTextureNames(uint8_t const *payload, size_t size, std::vector<std::string> *names);

	// ----- the polygon's colour word, as the game's loader splits it (chunk_load.cpp:2770-2776) -----
	bool EngineTypeIsTextured(int32_t engineType);                       // chunk_type.cpp is_textured: the eight textured types (5, 6, 7, 21-25)
	int32_t PolygonPictureNumber(int32_t colour);                         // colour & 0x7fff
	int32_t PolygonUvListNumber(int32_t colour);                          // (unsigned)colour >> 16
	int32_t MakePolygonColour(int32_t pictureNumber, int32_t uvListNumber);  // the two put together; the caller keeps each in range (0..0x7fff, 0..0xffff)

	// One polygon's texture coordinates as the Texture Map mode holds them: the picture, the corners, and the polygon's flag word (SHPPOLYS +8, the
	// game's iflag_* bits: the selection-by-kind commands set and clear exactly these, p11-shape-handlers.md section 51).
	struct PolygonUv
	{
		int32_t polygonNumber;      // the polygon's number in the shape's SHPPOLYS (the file's, quads unsplit)
		int32_t engineType;
		int32_t flags;              // SHPPOLYS +8
		int32_t colour;             // SHPPOLYS +12 as read
		int cornerCount;            // the polygon's own corners, 3 to 5
		int32_t pictureNumber;      // the number in the shape's picture names (-1 for a polygon that is not textured)
		int32_t uvListNumber;       // the number of its list in SHPUVCRD (-1 for a polygon that is not textured)
		UvList uv;                  // a copy of the list (cornerCount 0 when not textured or when the list is empty)
		bool textured;
		std::string pictureName;    // the name at pictureNumber in the shape's SHPTEXFN: the HOST fills it (this module only reads the polygon); empty when none
	};

	// Reads polygon number polygonNumber of the shape, with its list, by the rules above. *out is set only when the answer is Ok. A polygon that is not
	// textured is Ok with textured = false and an empty list (the Texture Map mode may give it a texture later; that is its own command).
	UvModelResult ReadPolygonUv(ShapeModel const &shape, std::vector<UvList> const &lists, int32_t polygonNumber, PolygonUv *out);

	// The shape's own list number of every polygon that uses list uvListNumber (a textured polygon), in order: so a mode can say that a write changes
	// several. Empty for an unused number.
	std::vector<int32_t> PolygonsUsingList(ShapeModel const &shape, int32_t uvListNumber);

	// Puts the polygon's new coordinates into lists[uvListNumber]: the corner count must be the list's own (WrongShape otherwise) and every coordinate must
	// pass the texel rule. Nothing is changed unless the answer is Ok.
	UvModelResult PutPolygonUv(PolygonUv const &polygon, std::vector<UvList> *lists);

	// ----- the maths inteng's Texture Map commands do on one polygon's coordinates (p11-shape-handlers.md section 51) -----
	// The window the flips work over: inteng's texture screen keeps left, top, right, bottom (state +8, +0xc, +0x10, +0x14) and after FUN_0055d290 they are
	// 0, 0, the picture's width and height. FUN_00570f40 lowers right and bottom by 1 for the call, so the flip is u' = left + (right - 1) - u.
	struct TextureWindow
	{
		int32_t left, top, right, bottom;   // state +8, +0xc, +0x10, +0x14 (NOT yet lowered by 1: the commands do that)
		int32_t sizeX, sizeY;               // state +0x48, +0x4c: the picture's width and height (the rotations scale by sizeX - 1 and sizeY - 1)
	};

	// inteng's coordinates are WHOLE numbers (FUN_00570f40 works on ints, and the game's loader makes ints of the file's floats too): every command below first
	// makes an int of each coordinate by TRUNCATING toward zero (the x87 __ftol of FUN_0059008c), so a fraction in the file is lost by any move (named deviation
	// in unfinished-in-inteng.md: how inteng's own load turns a float into its int is not read). The results are whole numbers, written back as floats.
	//
	// Command 6: u becomes left + (right - 1) - u on every corner. Command 7: v becomes top + (bottom - 1) - v. (FUN_00570f40 0x57101e / 0x571030.)
	// Command 0x19 ("turn A", 0x571084): u' = trunc((v - top) / (sizeY - 1) x (sizeX - 1) + left) and v' = trunc(float32((right - 1 - u) / (sizeX - 1) x
	// (sizeY - 1) + top)). Command 0x1b ("turn B", 0x571045): u' = trunc((bottom - 1 - v) / (sizeY - 1) x (sizeX - 1) + left) and v' = trunc(float32((u - left) /
	// (sizeX - 1) x (sizeY - 1) + top)). (The x87 keeps extended precision except that the second result goes through a 32-bit float; here a double stands for
	// the extended: a result that falls exactly on a half can differ in the last place. Named, p11 section 57.)
	// All four then keep the texel rule: a result beyond +-kMostTexel is refused (BadTexel), the polygon is left as it was. A sizeX or sizeY of 1 or less is
	// BadWindow for the two turns (the code divides by it).
	// The selection-by-kind commands (FUN_0056fb70(flags, set), p11-shape-handlers.md section 51, called by the commands 0x4d-0x66): set (true) turns
	// the bits of mask on in the polygon's flag word, set false turns them off. The bit 0x40000000 (iflag_txanim) is never set: BadFlag, polygon unchanged.
	// Clearing it is allowed (a file with it is refused by the loader, so it is never there to clear).
	UvModelResult SetPolygonFlags(PolygonUv *polygon, int32_t mask, bool set);

	UvModelResult FlipU(PolygonUv *polygon, TextureWindow const &window);
	UvModelResult FlipV(PolygonUv *polygon, TextureWindow const &window);
	UvModelResult TurnA(PolygonUv *polygon, TextureWindow const &window);   // command 0x19
	UvModelResult TurnB(PolygonUv *polygon, TextureWindow const &window);   // command 0x1b

	// inteng's rounding of a float to a whole number, FUN_00570ef0 (used by wrap and tile): the whole number toward zero when the part after the point is at most
	// a half, else one more away from zero (x + 0.5 for x > 0, x - 0.5 otherwise, then toward zero). Round half toward zero.
	int32_t RoundHalfTowardZero(double x);
}
