// export_films.h: EXPORT MAP'S FILMS, the level's own message films for the tree export_map.h writes. Written 2026-11-27 by Melanikus (Vasquez) for
// AVP_OpenInteng (AvP-docs director/design-vasquez-export-films.md, OK'd by Dietrich, Export map's owner, and Bishop). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// WHAT THE GAME PLAYS FROM A LEVEL (the avpx pin; Dietrich's FMV trace, AvP-docs engine/inteng/reassess/fmv-trace-dietrich.md):
// - MESSAGE FILMS. Only a Message strategy (kind 73) plays a film: its number N (the strategy payload's +20, 1 to 450) is both a text line and
//   FMVs\message<N>.smk, opened from the data folder's FMVs\ alone (FmvCutscenes.cpp StartTriggerPlotFMV; bh_mission.cpp). A mission objective (kind 70)
//   prints its strings and plays no film. A missing film is normal: the console prints the text only (233 of the 284 numbers stock levels name have none).
// - VIDEO SCREENS. A picture whose name holds "FMVs" (exact capitals) names FMVs\<from "FMVs" to the name's first '.'>.smk (FmvCutscenes.cpp:363-432
//   ScanImagesForFMVs). If that .smk EXISTS, AvPx skips the screen and the picture stays still; only when it's missing does the screen play the level's
//   message films. So NO screen film is exported (Bishop: the safe side, until the owner's retest says what Steam AvP does); the screens are named in
//   the README, and a screen film found in the data folder is a WARNING.
//
// WHICH FILMS ARE THE LEVEL'S OWN. Stock films are loose files (no packs), so "stock" means Rebellion's own BYTES: a film whose number, size and SHA-256
// are a stock film's (message_numbers.h MessageNumbers_IsStockFilm, from the depot-checked table) is left out and counted. Any other film there is the
// mapper's, and goes in the tree. One under a number that has a stock film is a WARNING: pasting it REPLACES Rebellion's film for every level that plays
// that number (named), and Steam's "Verify integrity of game files" puts Rebellion's back. A file our own Smacker reader refuses (smk_header.h: too
// short, no SMK2/SMK4, an impossible size or frame count) is LEFT OUT, with a WARNING saying why: the tree goes to every player, and their game's decoder
// (RAD's in Steam AvP, libsmackerdec in AvPx) would then read a file we already know is no film (Dietrich's review MUST, 11-27; the sound import refuses
// a WAV the game can't play the same way). A real Smacker film that merely differs from AvP's usual format is kept with a NOTE, as Video mode says it
// (stock message238 differs too): it may still play.
//
// THE FILES ARE HOSTILE (Tao 3). The level gives only numbers; each film's name is made from its number alone (MessageFmvName), so nothing from the level
// becomes a path. A screen's .smk name comes from the level, so it is held to the game's rule for a name (LevelFileName_WhyRefused) before the folder is
// even asked about it. The folders on the way must be plain (GameFolder_WhyNotPlainBelow), and each film is read by the editor's one reader for a file
// someone could have touched (VideoImport_ReadPlainFile), within kExportFilmMostBytes. A film there that can't be read stops the gathering, as a sound
// or a picture stops Export map: a tree without it would play the wrong film, or none.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "export_map.h"   // OwnFile

class File_Chunk;

namespace host
{
	uint64_t const kExportFilmMostBytes = 256ull * 1024 * 1024;   // the most a film is read to: Video mode's own (video_import.h kVideoImportMostBytes)

	// A video screen: its picture's name as the level holds it, and the film name the game makes of it ("FMVs\triggered_plot.smk")
	struct ExportFilmScreen
	{
		std::string picture;
		std::string film;
	};

	// One of the level's own films, as the gathering found it: what ExportMap_Run needs to list it, and to read it again, ONE AT A TIME, just before it
	// writes it (Dietrich's review NIT: holding every film's bytes at once, up to kExportFilmMostBytes each, in a 32-bit editor). ExportFilms_Read gives the
	// bytes back only if they are still the ones gathered (size and SHA-256).
	struct ExportFilm
	{
		std::string path;       // as the game asks for it: FMVs\message<N>.smk
		std::string whole;      // where it is: <data folder>\FMVs\message<N>.smk
		uint64_t size = 0;
		std::string sha256;     // 64 small hex letters
	};

	struct ExportFilms
	{
		std::vector<ExportFilm> own;        // the level's own films, for ExportMap_Run to write (through ExportFilms_Read) and list
		std::vector<int32_t> numbers;       // every number the level's Message strategies play, 1 to 450, ascending, each once
		size_t stock = 0;                   // films the level plays that are Rebellion's own bytes: left out
		std::vector<int32_t> textOnly;      // numbers with no film in the data folder (the console prints the text only)
		std::vector<ExportFilmScreen> screens;
		size_t replacing = 0;               // own films under a number Rebellion has a film for: each one of the warnings
		size_t unplayable = 0;              // films the game's decoder couldn't take (not a Smacker film): NOT exported, each one of the warnings
		size_t screenFilms = 0;             // video screen films found in the data folder (not exported): each one of the warnings
		std::vector<std::string> warnings;  // whole lines, for the README after "WARNING: "
		std::vector<std::string> notes;     // whole lines, for the README after "NOTE: "
	};

	// The numbers the level's Message strategies play (kind 73; every object's, virtual object's, enemy's and generator's and placed hierarchy's strategy,
	// as strategy_lookup.h finds each one the game reads), 1 to 450, ascending, each once
	std::vector<int32_t> ExportFilms_MessageNumbers(File_Chunk *level);

	// The level's video screens (REBENVDT > BMPNAMES, the pictures whose names hold "FMVs"), in the level's order, each once, with the film name made as
	// the game makes it
	std::vector<ExportFilmScreen> ExportFilms_Screens(File_Chunk *level);

	// The film name the game makes of a picture's name, as ScanImagesForFMVs does: from "FMVs" to the name's FIRST '.' (anywhere), plus ".smk"; empty
	// when the name holds no "FMVs"
	std::string ExportFilms_ScreenFilm(std::string const &picture);

	// The gathering, from the level and the data folder (the game's folder: theGameFolder), into *out (emptied first). False, with *why, when a film there
	// can't be read as a plain file, or when a folder on the way isn't a plain one
	bool ExportFilms_Gather(File_Chunk *level, std::string const &dataFolder, ExportFilms *out, std::string *why);

	// A gathered film's bytes, read again by the same reader, into *bytes; false, with *why, when it can't be read or isn't the film gathered any more
	// (another size or SHA-256: it changed between the gathering and the write)
	bool ExportFilms_Read(ExportFilm const &film, std::vector<uint8_t> *bytes, std::string *why);
}
