// sound_import.h: THE SOUND IMPORT, the owner's "import drag drop/load in the sound mode (under object placement?)" (2026-11-27). A WAV dropped on the editor's
// window while on Object Placement, or picked in Create > Load sound..., is checked and copied, byte for byte, into the level's own sound folder in the data
// folder. Written 2026-11-27 by Melanikus (Dietrich) for AVP_OpenInteng (AvP-docs director/design-dietrich-sound-import-export-map.md, section 4, approved by
// the owner via Bishop). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHERE IT GOES. The game opens a level's sound as "SOUND\" + the level's sound folder (REBENVDT > SOUNDDIR) + "\" + its name (Rebellion's PSNDPROJ.C:855,
// Objsetup.cpp:121; AvPx object_setup.cpp:130-155, psndproj.cpp:590-598). So the file goes to <data folder>\sound\<SOUNDDIR>\<its own name>, the working set the
// editor's Sound Edit lists and plays, and that Export map later packs (section 5). Only the file goes there: the level gets the name when a sound picks it in its
// Edit (inteng's own window, unchanged; the owner's answer to Q4: loading never places a sound).
// - A LEVEL WITH NO SOUND FOLDER (Base.RIF) is given its own, Custom\<level> (as the RIM import gives a texture graphics\Custom\<level>, rim_save.h), in the
//   level's bytes (sound_dir_add.h), swapped in by the game's own loader before the file is written. A level with a stock folder keeps it (the owner's Q3: it
//   gets a pack of its own at export).
// - A STOCK NAME (one the game's fastfiles hold in that folder) is loaded, with a warning: Steam AvP opens a pack before a loose file (FFSTDIO.CPP ffreadbuf,
//   PSNDPROJ.C:852-897), so it plays the stock sound; AvPx opens the loose file first (FileStream.cpp), so it plays this one. Modding: warn, never block.
// THE FILE IS HOSTILE (Tao 3): read by the editor's one reader for a file someone could have touched (video_import.h VideoImport_ReadPlainFile: a plain path,
// no link, plain folders), held to what both games play (model/wav_check.h), its name to the game's rule for a name from a level (LevelFileName_WhyRefused:
// the whole "sound\<folder>\<name>", at most 199 letters, as the game builds it in a 200-byte buffer). NEVER OVER ANOTHER FILE: written with CREATE_NEW into
// plain folders only (GameFolder_WhyNotPlainBelow), flushed and read back byte for byte, as rim_save.h saves a texture. The same file there already is taken
// as it is; other bytes under that name are never written over.
#pragma once

#include <stddef.h>
#include <string.h>

#include <string>
#include <vector>

#include "inteng_menus.h"   // kIntengScreens: the screens, by name

namespace host
{
	// The screen sounds are loaded on: Object Placement, found by its name
	inline bool SoundImport_IsPlacement(size_t screen)
	{
		return screen < kIntengScreenCount && strcmp(kIntengScreens[screen].name, "Object Placement") == 0;
	}

	// True when there is at least one path and every one ends in ".wav" (any capitals): a drop that is the sound import's, not the texture import's
	bool SoundImport_AllSounds(std::vector<std::wstring> const &paths);

	// The files handed in on Object Placement (a drop, or Load sound...'s Open dialog), at most 16 of them read (listed: how many were handed over in all):
	// each checked and copied (the top); the status line says what came of each
	void SoundImport_Dropped(std::vector<std::wstring> const &paths, size_t listed);

	// WAVs dropped on any other screen: not loaded, and the status line says where they go
	void SoundImport_DroppedElsewhere(std::vector<std::wstring> const &paths);

	// True when at least one path ends in ".wav" (any capitals). WAVs dropped TOGETHER with other files (a picture): nothing is taken, and the status line says
	// to drop them apart (Vasquez's review SHOULD 1, 11-27: the texture import's words told a WAV that a video goes in Video mode)
	bool SoundImport_AnySound(std::vector<std::wstring> const &paths);
	void SoundImport_DroppedMixed();

	// Object Placement's Create > Load sound... (ours, the owner's addition): Windows' own Open dialog, one or more WAVs, each taken as a drop is
	void SoundImport_Browse(void *owner);

	// The sound folder a level with none is given: "Custom\" and the level's own name (its file's last part, without a ".rif" in any capitals: "Base.RIF" is
	// "Custom\Base"). Empty when that name is empty, holds a slash, or the game's rule refuses the folder
	std::string SoundImport_FolderFor(std::string const &levelFile);
}
