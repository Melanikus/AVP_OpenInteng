// shot_context.h: what a screenshot of the editor says about itself (GitHub issue #78: Print Screen or Shift+F12 saves the frame as shown, and the PNG carries the editor's context, so
// Melanikus can feed the manual's pictures with what they show, and anyone can send a picture as a bug report). Plain data and text, no window and no file, so a test can drive every
// part. Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE OWNER'S ADDITION: inteng has no such key and writes no such file (Print Screen and Shift+F12 do nothing on any of its screens: FUN_004705a0 saves camera slots with Shift + F1 to
// F10 only, and no code reads the Print Screen key's slot). The JSON goes into the PNG's iTXt chunk "AVP_OpenInteng" (png_meta.h, Dietrich).
//
// THE JSON ("format": 1; every field always there, empty when it does not apply; the field names are fixed, a reader keys on them):
//   format, privacy ("paths and user names removed"), build, map (the level's file NAME only), level_bytes, level_sha256, screen, pos {x, y, z} (game units: millimetres), pos_metres
//   {x, y, z}, turn, look (4096ths of a turn), view_distance, current (the module the camera is in), selected [names], visible [names of the modules drawn in that frame] and visible_more (how many more there were),
//   base, picks [names], branches [names] (Module Linking), light, strategy, strategy_kind, hierarchy, sequence, key_frame, status, log [the last lines], window {width, height},
//   display, gpu.
// THE LOG'S LINES are read from the end of the editor's own log file (avp_log.txt in the save folder), whose lines the log's writer has already left the player out of
// (log_files.h).
//
// PRIVACY (strangers will send these): no path anywhere. Every text the editor hands in has already had the user's and the computer's names and the user's folders left out the way the
// log does it (log_files.h, LogLeaveOutThePlayer); here, on top, Shot_ScrubPaths cuts every folder off any path that is left, keeping only the file's name. The map is a file NAME.
// HOSTILE NAMES: names come from the level (a file anyone can make). Each is cut to kShotMostNameBytes, and every byte that is not printable ASCII is written as \u00XX (read as
// Latin-1, the old tools' code page), so the JSON is plain ASCII whatever the level holds, and can't be broken out of. Lists are bounded (kShotMostVisible, kShotMostNames).

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	size_t const kShotMostNameBytes = 128;     // one name or one word list item
	size_t const kShotMostVisible = 1024;      // visible module names; past that, visible_more counts them
	size_t const kShotMostNames = 256;         // selected, picks, branches
	size_t const kShotMostLogLines = 20;
	size_t const kShotMostLineBytes = 400;     // one log line or the status words
	int const kShotNumbers = 100;              // <map>-<date>-<time>.png, then -1 .. -99 for more in the same second

	// When a shot was taken, the computer's own clock (SYSTEMTIME's fields, so this file needs no windows.h)
	struct ShotTime
	{
		int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
	};

	struct ShotContext
	{
		std::string build;                 // which program took it (the first 16 hex letters of its own sha256)
		std::string map;                   // the level's file name, no folder
		uint64_t levelBytes = 0;
		std::string levelSha256;           // 64 hex letters, or empty when the file could not be read
		std::string screen;
		int32_t pos[3] = { 0, 0, 0 };      // the camera, in the game's units (millimetres)
		int32_t turn = 0;
		int32_t look = 0;
		int32_t viewDistance = -1;         // the screen's view distance when it has one (Module Linking's "View Distance :%d"), -1 otherwise
		std::string current;               // the module the camera is in
		std::vector<std::string> selected;
		std::vector<std::string> visible;
		std::string base;                  // Module Linking's current module (the tree's root)
		std::vector<std::string> picks;
		std::vector<std::string> branches; // the open branches
		std::string light;
		std::string strategy;              // the object whose strategy is in view, and
		int32_t strategyKind = -1;         // its kind, -1 none
		std::string hierarchy;
		std::string sequence;
		int32_t keyFrame = -1;
		std::string status;
		std::vector<std::string> log;
		int32_t windowWidth = 0;
		int32_t windowHeight = 0;
		std::string display;               // "windowed 1920 x 1080" and the like
		std::string gpu;                   // the adapter's name as Direct3D gives it
	};

	// Every path in words cut to its file's name: a drive path (C:\... or C:/...) or a network one (\\server\...) becomes "<path>\" and the last part of it (the part after its last
	// folder; a folder's part is taken up to its next \ or /, spaces included, the last part up to the first space, quote, comma or end). Anything else is left as it is.
	std::string Shot_ScrubPaths(std::string const &words);

	// One JSON string (with its quotes) of text cut to most bytes: printable ASCII as it is (" and \ escaped), everything else \u00XX
	std::string Shot_JsonString(std::string const &text, size_t most);

	// The whole JSON object, format 1, every text through Shot_ScrubPaths and Shot_JsonString, every list bounded
	std::string Shot_Json(ShotContext const &context);

	// "<map>-<YYYYMMDD>-<HHMMSS>.png" (Bishop's brief for #78): the map's name without its ending, made safe as a file name (letters, digits, "_" and "-"; anything else "_"; at most 48;
	// "level" when nothing is left), the date and the time; number 1 to 99 adds "-<number>" before ".png" (more shots in the same second). Empty for a number outside 0 to 99 or a
	// time outside the calendar (year 1601 to 9999, as SYSTEMTIME has it).
	std::string Shot_FileName(std::string const &map, ShotTime const &when, int number);

	// The last most lines of a log file's tail (the bytes read from its end): split at line ends (a \r before one taken off), empty lines left out, and, when cut is true (the tail
	// did not start at the file's start), the first part left out too, since it may be the end of a line whose start was not read
	std::vector<std::string> Shot_LastLines(std::string const &tail, bool cut, size_t most);

	// The file's name without its folders (after the last \ or /)
	std::string Shot_NameOnly(std::string const &path);
}
