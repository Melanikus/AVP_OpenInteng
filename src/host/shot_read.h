// shot_read.h: a screenshot of the editor read back (GitHub #78: Print Screen or Shift+F12; its PNG carries the editor's context, shot_context.h), so the editor can open where the shot was
// (-shot <png>, open_at.h: the owner, 11-18, "make inteng take a map and coordinates so you can open it for me in a specific place") and a scenario can start there (the harness's from-shot).
// Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE PNG IS HOSTILE INPUT (a picture can come from anyone): png_meta.cpp reads it (every chunk checked, one iTXt "AVP_OpenInteng", at most 256 KB of UTF-8 holding one JSON object); the JSON is
// read again here by a parser of its own, bounded (depth 64, at most 4096 values, strings at most 4096 bytes, \u escapes 1 to 255 only, as the shot writes them), and each field taken is held
// to its own limits (ShotRead_Place's and the ones below). Anything else is refused with what and why. No window, no level: plain data, so a test drives every part.

#pragma once

#include <stdint.h>

#include <string>

namespace host
{
	// Where to open: the level by its file NAME, the camera (place in millimetres within 2^24, turn 0 to 4095, look -1024 to 1024), the screen, and the first selected object (empty: none)
	struct ShotPlace
	{
		std::string map;           // a plain file name: 1 to 128 printable bytes, none of \ / : * ? " < > |, not "." or ".."
		std::string sha256;        // 64 lower-case hex letters (a shot); empty for -at
		std::string screen;        // 1 to 64 printable bytes (a shot); empty for -at with no screen
		std::string selected;      // 1 to 128 printable ASCII bytes, or empty
		int32_t pos[3] = { 0, 0, 0 };
		int32_t turn = 0;
		int32_t look = 0;
	};

	int32_t const kShotReadMostPlace = 1 << 24;   // millimetres each way: the editor camera's own limit (level_view.h kFlyCameraMostPlace), far past any level's edge
	int32_t const kShotReadMostLook = 1024;        // a quarter turn up or down: the camera's own (kFlyCameraMostLook)

	// Whether a name is a plain file name as ShotPlace::map says
	bool ShotRead_PlainName(std::string const &name);

	// A shot's JSON (the context's text, as png_meta read it) into *out: format 1, every field above in its bounds; false with *why on anything else
	bool ShotRead_Json(std::string const &json, ShotPlace *out, std::string *why);

	// A shot's PNG file: png_meta's reader, then ShotRead_Json
	bool ShotRead_File(char const *path, ShotPlace *out, std::string *why);

	// The five numbers of "-at x y z turn look" (a scenario's "camera x y z turn" and "camera-look look"), each a whole number of at most 11 characters within its bounds
	bool ShotRead_Numbers(char const *const words[5], ShotPlace *out, std::string *why);
}
