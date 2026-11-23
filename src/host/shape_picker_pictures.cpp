// shape_picker_pictures.cpp: a placement picker tile's PICTURE read and decoded (shape_picker_screen.h ShapePickerScreen_ReadPicture). Written 2026-11-23 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// AS THE LEVEL'S OWN PICTURES ARE READ (Bishop 2's ruling, 2026-11-23, option A): the name already held to the level open's own rule (shape_thumb.h: avpx
// level_file_name.h, a refused or empty path never comes here, and is refused here again), then the game's RimLoader through its FileStream (a loose file under the
// data folder's graphics, else the member of that name in the game's fastfile archives, where every stock picture is), decoded into memory with RimLoader's own
// checks (its sizes, parts, colour table and body): the pixels go to the picker's own renderer only, never into the game's texture manager, so nothing the level
// owns is shared or freed. NAMED: inteng's Prefer .BMP over .RIM is not followed here (a RIM only).
// THE PICKER'S OWN ALLOWANCE: every picture is counted on ONE RimLoaderToolAllowance a visit, from the picker's opening to its closing (Bishop 2's ruling on Drake's
// review of #293: one a picture would bound nothing in all), so it neither spends the open level's nor starts the level's over; once it is used up, the tiles
// still to be drawn are shaded, each saying so. A tile's pictures together are at most kShapePickerMostTileBytes, and are held only while it is drawn.
#include "shape_picker_screen.h"

#include <memory>
#include <string>

#include "RimLoader.h"         // the game's own RIM reader, and RimLoaderToolAllowance (Melanikus/AvP #293)
#include "level_file_name.h"   // LevelFileName_WhyRefused: held again here, whatever the caller did
#include "shape_thumb.h"
#include "shape_thumb_render.h"

namespace
{
	std::unique_ptr<RimLoaderToolAllowance> allowance;   // the visit's one (none: no picture is read)
}

void ShapePickerPictures_Begin()
{
	allowance.reset();                                   // (a visit's own: never two at once)
	allowance.reset(new RimLoaderToolAllowance());
}

void ShapePickerPictures_End()
{
	allowance.reset();                                   // the open level's count put back, exactly
}

bool ShapePickerScreen_ReadPicture(host::ShapeThumbPicture const &picture, host::ShapeThumbTexture *out, std::string *why, uint64_t *tileBytes)
{
	std::string notSaid;

	why = why ? why : &notSaid;
	*out = host::ShapeThumbTexture();

	// a picture whose path was refused, or that has none, is never read: not even as "graphics/" alone (Frost on ec8a4d0e); and the rule held here again
	char const *const refused = picture.path.empty() ? nullptr : LevelFileName_WhyRefused(picture.path.c_str());

	if (picture.path.empty() || !picture.refused.empty() || refused)
	{
		*why = picture.path.empty() && picture.refused.empty() ? "it names no file" : "its name is refused: " + (picture.refused.empty() ? std::string(refused) : picture.refused);
		return false;
	}

	if (!allowance)
	{
		*why = "the picker isn't open, so no picture is read";
		return false;
	}

	if (kMostRimBytesPerLevel - RimLoader_BytesThisLevel() < kLeastRimChargeBytes)
	{
		*why = "the picker's allowance is used up (" + std::to_string((unsigned long long)(kMostRimBytesPerLevel / (1024 * 1024))) + " MB of pictures a visit): it is shaded";
		return false;
	}

	std::string file = "graphics/" + picture.path;   // as projload.cpp load_rif_bitmap makes it, its slashes the game's way

	for (char &c : file)
	{
		c = c == '\\' ? '/' : c;
	}

	RimLoader rim;

	if (!rim.Open(file))
	{
		*why = rim.Problem() == kRimLevelHasTooMany ? std::string("the picker's allowance is used up: it is shaded") : RimLoader_ProblemWords(rim.Problem());
		return false;
	}

	uint32_t width = 0, height = 0;

	rim.GetDimensions(width, height);

	uint64_t const bytes = (uint64_t)width * height * 4;

	if (width == 0 || height == 0 || width > host::kShapeThumbMostTextureSide || height > host::kShapeThumbMostTextureSide ||
		(tileBytes && *tileBytes + bytes > kShapePickerMostTileBytes))
	{
		*why = "it is " + std::to_string(width) + " x " + std::to_string(height) + ": more than one tile's pictures may hold together (" +
			std::to_string((unsigned long long)(kShapePickerMostTileBytes / (1024 * 1024))) + " MB)";
		return false;
	}

	out->bgra.assign((size_t)bytes, 0);   // (out of memory: the tile's drawing fails, in words: shape_picker_screen.cpp's guard)

	if (!rim.Decode(out->bgra.data(), width * 4))
	{
		*out = host::ShapeThumbTexture();
		*why = std::string("its picture couldn't be decoded whole: ") + RimLoader_ProblemWords(rim.Problem());
		return false;
	}

	out->width = width;
	out->height = height;

	if (tileBytes)
	{
		*tileBytes += bytes;
	}

	return true;
}
