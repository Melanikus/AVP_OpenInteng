// open_at.h: the editor opened at a place (the owner, 11-18: "make inteng take a map and coordinates so you can open it for me in a specific place you want me to test later"):
//   AVP_OpenInteng.exe <level> -at <x> <y> <z> <turn> <look> [<screen>]   the level, a NAME of the data folder's levels (avp_rifs), with the camera there (the numbers a scenario's
//                                                                          "camera x y z turn" and "camera-look look" lines take) and that screen (as -screen names it)
//   AVP_OpenInteng.exe -shot <png>                                        the same from a screenshot's context (#78, shot_read.h): its map, only when the data folder's file has the shot's
//                                                                          sha256, its camera, its screen and its first selected object
// The harness's from-shot (tests/drive_scenario/from_shot.h) starts the editor with -shot. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// THE COMMAND LINE IS INPUT: every number bounded (shot_read.h: the camera's own limits), the level a plain name found in the data folder's avp_rifs (a path, or a name not there, is refused),
// the screen checked by -screen's own reader, a refusal said in words and the editor not started. The place and the selection are put after the level's first frame is drawn (the level's own
// start puts the camera first, then this).

#pragma once

#include <stdint.h>

#include <string>

#include "shot_read.h"

namespace host
{
	struct OpenAt
	{
		bool at = false;           // -at was given
		bool shot = false;         // -shot was given
		bool applied = false;      // the camera and the selection have been put
		std::string shotPath;
		ShotPlace place;
	};

	// One word of the command line at *i: -at (with its five numbers and, when the word after them does not start with '-', a screen) or -shot (with its picture's path). True when it was one
	// of them: *i is then on the last word taken, and *why says a refusal (empty when taken). The words are the command line's, read as hostile.
	bool OpenAt_Argument(wchar_t **args, int count, int *i, OpenAt *out, std::string *why);

	// With -at or -shot: the level to open, *levelName (as the settings' Level= names one), from the data folder's avp_rifs: -at's level word (a plain name, its .rif ending optional) or the
	// shot's map. For a shot, the file's sha256 (sha256: the editor's own) must be the shot's: else the refusal names both. levelWord: the command line's level word (-at), empty for -shot.
	bool OpenAt_Level(OpenAt const &at, std::wstring const &levelWord, char const *dataFolder, char *levelName, size_t size, std::string (*sha256)(uint8_t const *, size_t), std::string *why);

	// The screen -at or the shot names into *startScreen, by -screen's own reader (IntengMenus_ScreenFromCommandWord: exactly one screen's name, as the shot writes it); refused when -screen gave one
	// too (screenGiven)
	bool OpenAt_Screen(OpenAt const &at, bool screenGiven, size_t *startScreen, std::string *why);

	// Once the level is up (the first frame drawn): the camera put where the place says, the selected object (a placed object or a module of the level, by its name) selected. *words say what was
	// done, for the log and the status line. Runs once.
	void OpenAt_Apply(OpenAt *at, std::string *words);
}
