// shape_rescale_edit.h: Rescale (Object Placement, Object, Rescale; command 0x1a) on the open level: a placed object's shape is made the size typed.
// Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (out\menu-reads-35.txt:1395-1580 FUN_004e1a80, the prompt; FUN_004e1340, the work; Vasquez p11-shape-handlers.md sections 49 and 50; Apone's
// runs 8-13, apone-reads-3.md):
// - the typed text is read as a double and taken when 0.0 <= value <= 1,000,000.0 (0x4e1b10; _DAT_005ae590 = 1000000.0);
// - the shape chunk's SHPEXTFL child is looked up, then the SHPEXTFN chunks inside it; only when there is one does anything happen (0x4e1b57, 0x4e1c40);
// - the factor is typed / (_DAT_00607bd8 x _DAT_005ac438 x the first SHPEXTFN's rescale double) (0x4e1c69), where _DAT_00607bd8 is 100.0 x the level's
//   scale (ENVSDSCL; menu-reads-21.txt:554, 594) and _DAT_005ac438 is 0.01, so 100 x 0.01 x scale = the level's scale;
// - FUN_004e1340(factor): the chunk-side shape record's points, extents and radius are multiplied by the factor, each integer truncated toward zero
//   (FUN_00430210), the shape's chunks are made again (FUN_0050f570), and THE FIRST SHPEXTFN's rescale double is multiplied by the factor
//   (0x4e1832-0x4e1862, Apone's run 11).
//
// WHAT THIS DOES. The same to the level's bytes, through the edit store (the same sizes: no count changes): the points, extents and radius of the
// object's shape (host::ShapeEdit_Read / WriteSameCounts) and the first SHPEXTFN's rescale double.
//
// NAMED DEVIATIONS (drake-deviations.md): (1) inteng accepts a typed 0 (every point at 0); here 0 is refused. (2) A SHARED shape (used by other objects too)
// is refused: inteng rescales a shared shape once per user (Vasquez's card), which is not copied. (3) x87 extended precision in inteng, double here (see
// model::RescaleShape). (4) SHPCENTR (the centre from the new extents and the radius about it, FUN_0050f570) is written when the shape has the chunk; inteng
// makes one when there is none (the chunk's size would change): not done. (5) inteng's dialog, its title and default text are not read: the editor's
// own prompt asks. (6) The centre and record[10] that FUN_00430210 scales before the rebuild are not read (Apone's run 13 is queued); the rebuild sets the
// centre again, so only record[10] is unaccounted for.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class File_Chunk;      // the game's chunk library
class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	enum class ShapeRescaleResult
	{
		Done,
		BadCall,        // no level, no ID, or no place for an answer
		NoObject,       // no object has that ID
		BadValue,       // the typed number is not in 0 < value <= 1,000,000 (or is not a number)
		NoExternal,     // the shape has no SHPEXTFL with a SHPEXTFN in it: inteng does nothing then
		SharedShape,    // other objects use the shape too (a named deviation)
		BadShape,       // the shape or the SHPEXTFN could not be read, or its rescale is not a number above 0
		Overflow,       // a number would leave the 32-bit range
		Refused,        // the bytes wouldn't take the change
	};

	// What an answer means, in plain words for the status line: static text, never null
	char const *ShapeRescaleWords(ShapeRescaleResult result);

	// The most inteng accepts, and the 0.01 of its factor (_DAT_005ae590, _DAT_005ac438)
	double const kRescaleMost = 1000000.0;
	double const kRescaleHundredth = 0.01;

	// Makes the shape of the object whose 8-byte ID is id the size typed. levelScale is the level's scale (ENVSDSCL; 1.0 for a level with none: inteng's
	// _DAT_00607bd8 is then 100.0). *factorOut (when not null) gets the factor used. *why gets the model's words on BadShape or Refused.
	ShapeRescaleResult ShapeRescale_ByValue(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, double typed, double levelScale, double *factorOut,
		std::string *why);

	// The '=' and '-' keys of Object Placement (OP-14, FUN_004d9af0 0x4dc2c3-0x4dc308): the same work with the factor given (10/7 for '=', 0.7 for '-'): the points, extents and radius
	// times the factor and the first SHPEXTFN's rescale double times it, as ShapeRescale_ByValue does after working its factor out. The same refusals (NoExternal, SharedShape ...).
	ShapeRescaleResult ShapeRescale_ByFactor(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, double factor, std::string *why);

	// The two factors inteng pushes (0x3ff6db6db6db6db7 = 10/7, 0x3fe6666666666666 = 0.7)
	double const kScaleKeyUp = 10.0 / 7.0;
	double const kScaleKeyDown = 0.7;
}
