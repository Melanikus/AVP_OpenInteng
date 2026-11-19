// fmv_files.h: the user's FMVs as the game finds them, for the editor to list and check (the owner's addition 11-18, through Bishop: "the editor reads the user's FMVs, the same
// way it reads custom sounds and textures"). Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// Facts: AvP-docs/engine/inteng/reassess/fmv-trace-dietrich.md.
//
// WHERE: the game opens "FMVs/message%d.smk" from its own folder (utilities.cpp avp_fopen: the name as it is, no search path, no fastfile; FMV-T-5), so the only place is the data
// folder's FMVs\ and never a folder under it or the level's own folder. inteng never lists or checks an FMV (FMV-I-1): this is ours.
// HOSTILE: every file is read as hostile input: only its first 104 bytes, by model::SmkHeader_Read's checks, before any decoder sees it (the preview's decoder is given only a file
// whose header passed).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "smk_header.h"

namespace host
{
	// The FMVs folder of a game folder: "<game folder>\FMVs\" ("" for no game folder)
	std::string FmvFiles_Folder(char const *gameFolder);

	// The folder's *.smk files, in the order Windows' finder gives them (FindFirstFile / FindNextFile, as Sound Edit's wav finder); a folder named *.smk, a name of 256 letters or
	// more and a name with a letter outside ' '..'~' are left out; at most `most`
	std::vector<std::string> FmvFiles_Names(std::string const &folder, size_t most = 4096);

	// The message numbers the folder holds an FMV for (model::MessageFmvNumber's names: the game's own "message%d.smk"), each once, lowest first
	std::vector<int32_t> FmvFiles_MessageNumbers(std::string const &folder);

	enum class FmvFileRead
	{
		Read,          // a Smacker header with possible numbers: *header holds it
		Missing,       // no such file
		NotAFile,      // a folder of that name, or a file that could not be opened (the reason in *why)
		TooShort,      // model::SmkRead's answers
		NotSmacker,
		Impossible,
	};

	// The header of folder + name (the first 104 bytes only, the file shared with any reader and writer)
	FmvFileRead FmvFiles_Header(std::string const &folder, std::string const &name, model::SmkHeader *header, std::string *why);

	// What the status line says about a Message strategy's number (its FMV, FMV-T-3: strategy n plays FMVs\message<n>.smk): playable with its numbers, playable but not AvP's
	// format (what differs), not a video the game can play, or no FMV (the game shows the message's text only); a number outside 0..450 is one the game refuses
	std::string FmvFiles_MessageWords(std::string const &folder, int32_t number);

	// THE VIDEO SCREENS, as the game's ScanImagesForFMVs finds them at every level load (FmvCutscenes.cpp:363-432; FMV-T-6), line for line: each picture name holding "FMVs" (those
	// capitals), in the order given; when 10 screens are made already (MAX_NO_FMVTEXTURES), it and every picture after it stay still; its .smk is the name from "FMVs" to the name's
	// FIRST '.' (std::string's own substr, so a '.' before "FMVs" takes the rest of the name), and ".smk"; that file THERE skips the picture (it stays still); else the picture must be
	// 128 x 128 (kFMVTextureSize) to be a screen, which the triggered messages then play on.
	enum class FmvScreen
	{
		Screen,
		StillItsSmkIsThere,
		StillWrongSize,
		StillPastTheTenth,
	};

	struct FmvScreenRow
	{
		std::string picture;   // the picture's name as the game holds it
		std::string smk;       // the .smk the game looks for
		FmvScreen verdict;
		uint32_t width, height;
	};

	int32_t const kFmvScreensMost = 10;
	uint32_t const kFmvScreenSide = 128;

	// `size(i, &w, &h)` gives the size the game asks for picture i (ScanImagesForFMVs's Tex_GetDimensions(i)); `smkThere(name)` whether the game's fopen of that name would open a file
	std::vector<FmvScreenRow> FmvFiles_JudgeScreens(std::vector<std::string> const &pictures, std::function<void(size_t, uint32_t *, uint32_t *)> const &size,
		std::function<bool(std::string const &)> const &smkThere);

	// The status line's words when a picture meant as a video screen stays still ("" when none does): one named with its reason, or how many and the first
	std::string FmvFiles_ScreensWords(std::vector<FmvScreenRow> const &rows);
}
