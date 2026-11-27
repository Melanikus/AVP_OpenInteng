// export_map.h: EXPORT MAP, the owner's "we'll add a 'export map' option to inteng that will output the structure to be pasted at the root of AVP (avp_rifs +
// sounds, FMVs, textures, extra shape_rifs, etc)" (2026-11-27). The hub's File > Export map... writes the open level's tree into the editor's folder for exports.
// Written 2026-11-27 by Melanikus (Dietrich) for AVP_OpenInteng (AvP-docs director/design-dietrich-sound-import-export-map.md, section 5, approved by the owner
// via Bishop). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// PASTE AND PLAY, LOOSE FILES (the owner's choice, 2026-11-27, from data: "Loose WAVs, paste-and-play"). Steam's own fastfile\ffinfo.txt lists only stock
// packs, and the Universal Map Pack (community-maps/: ~225 maps for Steam and GOG) installs by pasting avp_rifs, graphics and sound, with 1,379 LOOSE WAVs
// and no ffinfo.txt edit. So the tree holds the level's own files loose, where the game looks for them. NO PACKS ("So cancel packing", the owner, the same
// day, from data: the community maps that shipped their own .FFL packs all told players to hand-edit ffinfo.txt, as Steam AvP opens only the packs it lists,
// FFSTDIO.CPP; the Universal Map Pack made them work by shipping every packed file loose too). His Xtranded retest (a loose sound no pack holds, in a copy of
// kens-co-op) confirms the loose route in Steam AvP itself.
// WHERE: <the folder for copies>\Export\<level>\ (the folder for copies is <Documents>\AVP_OpenInteng\, save_copy.h: every export of the editor keeps that
// rule; a hidden test run's is beside the program). The status line and the README name the folder from the Documents folder on, so the mapper can find
// it to paste it, and neither holds a whole path: the status line is in screenshots, and the README travels with the map to every player (the mapper's
// Windows name is in the whole path).
// WHAT (this part: the level, its sounds and its pictures; films, message lines and the source shapes follow, section 5):
//   avp_rifs\Custom\<level>.RIF     the level as the next save would write it. Steam AvP lists its custom levels from avp_rifs\custom\*.rif
//   sound\<SOUNDDIR>\<name>         each of the level's OWN sounds, as the game asks for it ("SOUND\" + SOUNDDIR + "\" + name)
//   graphics\<entry>                each of the level's OWN pictures, as the game asks for it ("graphics\" + its BMPNAMES entry; the RIM import's are
//                                   Custom\<level>\<texture>.RIM, rim_save.h)
//   README-<level>.txt              where the tree is, what to paste, and every file with its size and sha256 (its own name: pasted beside other maps')
// WHICH FILES: every sound name the level's chunks hold (SOUNDOB2 placed sounds, INDSOUND indexed ones (particle generators, placed hierarchies), FRGSOUND
// break sounds, TRAKSOUN track sounds) and every picture its BMPNAMES list names, read through the game's own chunk classes. One whose file is loose in the
// data folder is the level's own (the sound import, the RIM import put it there): copied. One only the game's fastfiles hold is stock (every install has
// it): left out. One that is neither is named in the words: the game shows or plays nothing for it. An own file with a stock name is copied with a WARNING:
// Steam AvP opens the game's pack before a loose file, so it plays the stock one.
// NEVER OVER ANOTHER FILE, NOTHING OUTSIDE THE TREE (Tao 1 / 3): <level> must not be there yet; every file is written by new_file_save.h (plain folders only,
// CREATE_NEW, flushed and read back byte for byte); every name in the tree is made from checked parts: the level's own name (letters, digits and '_' only,
// at most 25) and the game's rule for each file's name (LevelFileName_WhyRefused, at most 199 letters). A refusal leaves the tree's files that were made,
// and says so.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	// One file of the level's own, found loose in the data folder, for the tree (the gatherings hand these to ExportMap_Run, which writes them)
	struct OwnFile
	{
		std::string path;            // as the game asks for it, from the game's folder: sound\<SOUNDDIR>\<name>, graphics\<entry>, FMVs\<name>.smk
		std::vector<uint8_t> bytes;
		bool stockName = false;      // a stock file has that name too: Steam AvP plays the stock one for a packed sound or picture (a warning)
	};

	// File > Export map...: the open level's tree (above). The status line says where it went and what it holds, or why not
	void ExportMap_Run();

	// The level's part of the tree's names: its file's last part without ".rif" (any capitals), each letter but A-Z a-z 0-9 '_' made '_', at most 25 of them;
	// empty when nothing is left
	std::string ExportMap_LevelPart(std::string const &levelFile);
}
