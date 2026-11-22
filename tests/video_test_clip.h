// video_test_clip.h: a TEST VIDEO made with Windows' own Media Foundation sink writer, for the tests that read a user's video (video_import's own test, and the
// video editor's acceptance through tests/scenarios/verify_videomake.cpp). The repository holds no video (Bishop 11-18), so each test makes the one it reads, in its
// own folder. Moved here 2026-11-22 by Melanikus (Dietrich) from tests/host_video_import_read_test.cpp (Vasquez's, 11-18), unchanged, so both use the one maker.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

#include <functional>
#include <string>

#include <windows.h>

#include <mfapi.h>

// paints frame `index` into `pixels` (B, G, R, unused; the top row first; width x height)
typedef std::function<void(uint32_t index, uint8_t *pixels)> TestClipPainter;

// rows fromRow to toRow - 1 of a picture `width` wide, in one colour
void TestClip_Fill(uint8_t *pixels, uint32_t width, uint32_t fromRow, uint32_t toRow, uint8_t r, uint8_t g, uint8_t b);

// A video made with Media Foundation's sink writer (the container from the name's ending: .mp4 or .wmv), `frames` frames at `fps` a second, painted by `paint`;
// with `tone`, a 440 Hz tone, stereo, 44100 Hz, as AAC, over the whole length. Media Foundation must be started (MFStartup). False with the step that failed in *why
bool TestClip_Make(std::wstring const &path, GUID const &videoFormat, uint32_t width, uint32_t height, uint32_t fps, uint32_t frames, bool tone, TestClipPainter const &paint,
	std::string *why);
