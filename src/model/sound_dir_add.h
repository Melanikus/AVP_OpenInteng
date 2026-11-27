// sound_dir_add.h: a level with no sound folder of its own given one, in the level's bytes: the sound import's step for a level whose REBENVDT holds no SOUNDDIR
// (AvP-docs director/design-dietrich-sound-import-export-map.md section 4, step 5: "None, or a new level: set to Custom\<level>"). Written 2026-11-27 by
// Melanikus (Dietrich) for AVP_OpenInteng, the way bitmap_add.h adds a picture to the level's list. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code". Pure: no Windows, no files.
//
// WHAT IT CHANGES. The level's bytes as the next save writes them (OpenLevelFile::BuildLevel's: REBINFF2, then its chunks) are given back with one chunk more at
// the end of the first REBENVDT: SOUNDDIR, made as the game's own Sound_Directory_Chunk::fill_data_block writes one (avpx/3dc/win95/environment_chunk.cpp): its
// 12-byte heading, then the folder's name, its zero, and zeros to a multiple of 4 (size_chunk: 12 + ((letters + 4) & ~3)). The sizes of REBENVDT and the whole
// file are made right. Every other byte is the same, in the same place before the change and after it.
// THE FOLDER'S NAME is the part after "sound\" of where the game opens the level's sounds ("SOUND\" + it + "\" + a sound's name: Rebellion's PSNDPROJ.C:855 and
// Objsetup.cpp:121), so it is spelled with backslashes, and it passes the game's rule for a name from a level (LevelFileName_WhyRefused).
// REFUSED, nothing given back: bytes that can't be walked (every size is checked: they are hostile); no REBENVDT; a SOUNDDIR there already (one or more: this
// gives a folder only to a level that has none); a name that is empty, holds '/', or that the game's rule refuses.
#pragma once

#include <string>
#include <vector>

namespace model
{
	enum class SoundDirAddResult
	{
		Added,
		BadCall,        // no room for the answer, or a folder name refused (empty, a '/', or the game's rule)
		CantWalk,       // the bytes aren't a level whose chunks can be walked
		NoEnvironment,  // the level has no REBENVDT
		HasOne,         // the level has a SOUNDDIR already
	};

	// image: the level's bytes (REBINFF2 first). folder: the sound folder's name ("Custom\Base"). Added: *out the level's bytes with it. Else *out empty and *why
	// saying why
	SoundDirAddResult SoundDirAdd_ToImage(std::vector<char> const &image, std::string const &folder, std::vector<char> *out, std::string *why);

	char const *SoundDirAddResultWords(SoundDirAddResult result);
}
