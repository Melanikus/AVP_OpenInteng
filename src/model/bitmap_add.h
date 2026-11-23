// bitmap_add.h: a new picture named in a level's BMPNAMES list, in the level's bytes (the RIM import's step 7: AvP-docs director/plan-rim-import.md). Written
// 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Pure: no Windows, no files.
//
// WHAT IT CHANGES. The level's bytes as the next save writes them (OpenLevelFile::BuildLevel's: REBINFF2, then its chunks) are given back with:
//   - one entry more at the end of BMPNAMES, inside the first REBENVDT, made as the game's own Chunk_With_BMPs::fill_data_block writes one
//     (avpx/3dc/win95/bitmap_names.cpp): flags, index, version, priority, the size word, then the name, its zero, and zeros to a multiple of 4;
//   - the list's count one more; BMNAMVER one more (or made, holding 1, at the end of REBENVDT when the level has none, as inc_version_num makes one);
//   - the sizes of BMPNAMES, REBENVDT and the whole file made right. Every other byte is the same, in the same place before the change and after it.
// THE ENTRY, as the 1,377 entries of the 63 stock levels all are (measured 2026-11-23, R2 in the plan):
//   flags     0x0100010C: IFF (the file holds its own palette and transparency), PriorityAndTransparencyAreValid, RequireToolsMipMaps, RequireGameMipMaps
//   index     the list's largest + 1 (a polygon picks its picture by it; the editor writes it in 12 bits, so at most 4095)
//   version   BMNAMVER << 8, as BMP_Name(name, gbnc_version) makes a new name's (every stock entry's version is below its level's BMNAMVER); enum 0
//   priority  6 (DEFAULT_BMPN_PRIORITY)
//   size word height << 16 | width (an IFF name's: bitmap_names.hpp SHIFT_W 0, SHIFT_H 16)
// REFUSED, nothing given back: bytes that can't be walked (every size is checked: they are hostile); no REBENVDT, no BMPNAMES, or two BMPNAMES in it; two
// BMNAMVER (the game keeps the largest and deletes the others as it reads them); a count word whose top 16 bits aren't 0 (the loader would set BMNAMVER from
// them: no stock level has any); a list that names the picture already (in any capitals, as the game compares names); no room for the index or the version;
// a name the game's rule for a name from a level refuses (LevelFileName_WhyRefused); a size of 0 or over 65535.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace model
{
	enum class BitmapAddResult
	{
		Added,
		BadCall,        // no room for the answer, a size of 0 or over 65535, or a name the game's rule refuses
		CantWalk,       // the bytes aren't a level whose chunks and list can be walked
		NoPictureList,  // no REBENVDT, or no BMPNAMES in it
		Unusual,        // two BMPNAMES, two BMNAMVER, or a count word with its top bits set: not a list this changes
		NameInList,     // the list names that picture already
		ListFull,       // the next index would be over 4095, the count over 65535, or the version over 4095
	};

	int32_t const kBitmapAddFlags = 0x0100010C;
	int32_t const kBitmapAddPriority = 6;
	int32_t const kBitmapAddMostIndex = 0xfff;

	// image: the level's bytes (REBINFF2 first). name: the entry's file name, as rim_save.h's RimSave_EntryName makes it ("Custom\Genshd1\Wall01.RIM"). width,
	// height: the picture's. Added: *out the level's bytes with the entry, *index the new entry's index. Else *out empty, *index -1, and *why saying why
	BitmapAddResult BitmapAdd_ToImage(std::vector<char> const &image, std::string const &name, uint32_t width, uint32_t height, std::vector<char> *out,
		int32_t *index, std::string *why);

	char const *BitmapAddResultWords(BitmapAddResult result);
}
