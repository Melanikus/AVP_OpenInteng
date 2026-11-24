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
//
// Modified 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng: THE SITTING'S KEPT PICTURES (shape_picker_screen.h; the owner's "cache every texture once").
// A picture decoded whole is kept for the sitting, by its path as judged plus the identity of the very file RimLoader read (avpx FileStream.h FileStreamIdentity).
// Asked for again, its path is first opened with the same lookup RimLoader's Open makes, nothing read and nothing counted, to learn which file it leads to NOW:
// only the very same file (the same disk, number, place, size and time last written) is given from the kept ones; any other is decoded again, as before. So a
// kept picture costs no read and nothing on the allowance, and every picture it gives was decoded with RimLoader's checks from that same file.
#include "shape_picker_screen.h"

#include <map>
#include <memory>
#include <new>
#include <string>

#include "FileStream.h"        // the game's own file reader: its lookup, and which file it opened (FileStreamIdentity, Melanikus/AvP #298)
#include "RimLoader.h"         // the game's own RIM reader, and RimLoaderToolAllowance (Melanikus/AvP #293)
#include "level_file_name.h"   // LevelFileName_WhyRefused: held again here, whatever the caller did
#include "shape_thumb.h"
#include "shape_thumb_render.h"

namespace
{
	std::unique_ptr<RimLoaderToolAllowance> allowance;   // the visit's one (none: no picture is read)

	struct KeptPicture
	{
		FileStreamIdentity file;                 // the very file its pixels were decoded from (RimLoader's own handle's)
		host::ShapeThumbTexture texture;         // its pixels, 4 bytes each
		uint64_t given = 0;                      // when it was last given, as a running number (the least recent is let go first)
	};

	// by the path as judged, its letters made small: Windows and the game's archives (avpx FastFile.cpp FF_Search) read a name in any letters as the same one
	std::map<std::string, KeptPicture> keptPictures;
	uint64_t mostKeptBytes = kShapePickerMostKeptPictureBytes;
	uint64_t givenSoFar = 0;
	ShapePickerPictureCounts counts;

	std::string TooMuchForTheTile(uint32_t width, uint32_t height)
	{
		return "it is " + std::to_string(width) + " x " + std::to_string(height) + ": more than one tile's pictures may hold together (" +
			std::to_string((unsigned long long)(kShapePickerMostTileBytes / (1024 * 1024))) + " MB)";
	}

	void LetGo(std::map<std::string, KeptPicture>::iterator which)
	{
		counts.keptBytes -= which->second.texture.bgra.size();
		counts.kept--;
		keptPictures.erase(which);
	}

	// A picture just decoded whole, kept under its path: in place of what was kept there (another file, or one saved again since), then the least recently given
	// let go until all are under the most. Never kept: one whose file Windows wouldn't name (an identity that isn't known equals none), one larger than the most on
	// its own, and one there's no memory for (it is simply decoded again when next asked for).
	void Keep(std::string const &key, FileStreamIdentity const &file, host::ShapeThumbTexture const &texture)
	{
		auto const before = keptPictures.find(key);

		if (before != keptPictures.end())
		{
			LetGo(before);
		}

		if (!file.known || texture.bgra.size() > mostKeptBytes)
		{
			return;
		}

		try
		{
			KeptPicture made;
			made.file = file;
			made.texture = texture;   // (a copy: the tile's own goes to the renderer)
			made.given = ++givenSoFar;
			keptPictures[key] = std::move(made);
		}
		catch (std::bad_alloc const &)
		{
			keptPictures.erase(key);
			return;
		}

		counts.keptBytes += texture.bgra.size();
		counts.kept++;

		while (counts.keptBytes > mostKeptBytes)
		{
			auto oldest = keptPictures.end();

			for (auto it = keptPictures.begin(); it != keptPictures.end(); ++it)
			{
				oldest = it->first != key && (oldest == keptPictures.end() || it->second.given < oldest->second.given) ? it : oldest;
			}

			if (oldest == keptPictures.end())
			{
				break;   // (only the one just kept is left, and it fits: checked above)
			}

			LetGo(oldest);
			counts.letGo++;
		}
	}
}

ShapePickerPictureCounts ShapePickerPictures_Counts()
{
	return counts;
}

void ShapePickerPictures_Forget(uint64_t mostBytes)
{
	keptPictures.clear();
	mostKeptBytes = mostBytes;
	givenSoFar = 0;
	counts = ShapePickerPictureCounts();
}

void ShapePickerPictures_Begin()
{
	allowance.reset();                                   // (a visit's own: never two at once)
	allowance.reset(new RimLoaderToolAllowance());
}

void ShapePickerPictures_End()
{
	allowance.reset();                                   // the open level's count put back, exactly (the kept pictures stay, for the sitting)
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

	std::string file = "graphics/" + picture.path;   // as projload.cpp load_rif_bitmap makes it, its slashes the game's way

	for (char &c : file)
	{
		c = c == '\\' ? '/' : c;
	}

	// WHICH FILE THAT PATH LEADS TO NOW: the very lookup RimLoader's Open makes (FileStream::Open: a loose file under the data folder, else the archive member of
	// that name), only opened: nothing read, nothing counted. One that isn't there says RimLoader's own words, and isn't looked for a second time (the archives'
	// lookup would say so in the log twice).
	FileStreamIdentity now;

	{
		FileStream look;

		look.Open(file, FileStream::FileRead);

		if (!look.IsGood())
		{
			*why = RimLoader_ProblemWords(kRimNoFile);
			return false;
		}

		now = look.Identity();
	}

	std::string key = file;

	for (char &c : key)
	{
		c = c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
	}

	// KEPT, and the very same file: given from the kept ones, counted on the tile as a read one is (== is never true for an identity that isn't known)
	auto const kept = keptPictures.find(key);

	if (kept != keptPictures.end() && kept->second.file == now)
	{
		host::ShapeThumbTexture const &texture = kept->second.texture;
		uint64_t const bytes = texture.bgra.size();

		if (tileBytes && *tileBytes + bytes > kShapePickerMostTileBytes)
		{
			*why = TooMuchForTheTile(texture.width, texture.height);
			return false;
		}

		*out = texture;   // (out of memory: the tile's drawing fails, in words: shape_picker_screen.cpp's guard)
		kept->second.given = ++givenSoFar;
		counts.fromKept++;

		if (tileBytes)
		{
			*tileBytes += bytes;
		}

		return true;
	}

	// NOT KEPT, or another file now: read and decoded whole, as before, and kept
	if (kMostRimBytesPerLevel - RimLoader_BytesThisLevel() < kLeastRimChargeBytes)
	{
		*why = "the picker's allowance is used up (" + std::to_string((unsigned long long)(kMostRimBytesPerLevel / (1024 * 1024))) + " MB of pictures a visit): it is shaded";
		return false;
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
		*why = TooMuchForTheTile(width, height);
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
	counts.decoded++;

	// kept by the file RimLoader itself read (its own handle's identity): if the path led elsewhere between the look above and this read, it is that file's
	Keep(key, rim.FileIdentity(), *out);

	if (tileBytes)
	{
		*tileBytes += bytes;
	}

	return true;
}
