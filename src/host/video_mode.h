// video_mode.h: THE OWNER'S VIDEO MODE's engine (alpha.5 item 2, his words 11-22: "let's add a new mode for video, it only has load and mode exit and displays a black
// screen 'Drag and drop .mp4 or use Load -> Browse', while video loads you'll put a loading message with % let them fit/crop/expand, cut start and ending, turn volume up
// and down, drawn a rectangle over the video to show how it'll display down (the '128p' aspect ratio)"; "Don't put it directly on strategy mode"). Written 2026-11-22 by
// Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHO DOES WHAT. This engine holds the video and does the work; the SCREEN (Drake's video_mode_screen: the Mode menu's entry, the black screen and its words, Load >
// Browse... and Mode > Exit, the controls and the rectangle drawn) only reads VideoMode_View() each frame and calls the calls below. Everything here is called from the
// editor's own thread; the reading, the encoding and the save run on a worker of their own (one at a time), whose progress VideoMode_Frame takes each frame.
// THE PATH IS HOSTILE: every file handed in goes through video_drop.h's check first, then video_import.h's own (Tao 3). Nothing here touches a level, a strategy or a Message.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "fmv_preview.h"
#include "video_import.h"

namespace host
{
	// The on-screen controls, by the names the screen shows, the harness clicks (click-video) and VideoMode_Press takes. Cancel stops a load or a film being made
	// (Bishop 2's brief: the % and Cancel); the others wait while one is going
	char const *const kVideoModeControls[] = { "Fit", "Crop", "Expand", "Start-", "Start+", "End-", "End+", "Volume-", "Volume+", "Preview", "Save", "Cancel" };
	size_t const kVideoModeControlCount = sizeof(kVideoModeControls) / sizeof(kVideoModeControls[0]);

	// The black screen's words (the owner's, with the types his ruling 11-22 widened)
	char const *const kVideoModeEmptyWords = "Drag and drop a video (.mp4, .mkv, .mov, ...) or use Load -> Browse";

	// The cut's step for Start- / Start+ / End- / End+: half a second (the nearest frame)
	double const kVideoModeCutStepSeconds = 0.5;

	enum class VideoModePhase
	{
		Empty,     // no video: the black screen and its words
		Loading,   // a video being read on the worker: "Loading N%" (percent below)
		Loaded,    // a video held: its picture, the rectangle, the controls
		Making,    // a film being made (Preview or Save) on the worker: "Making N%"
	};

	// What the screen draws and the state line says, as it is this frame
	struct VideoModeView
	{
		VideoModePhase phase = VideoModePhase::Empty;
		int32_t percent = 0;                  // Loading / Making: 0 .. 100

		// the video held (Loaded, Making)
		std::string name;                     // "clip.mp4"
		std::string sourceWords;              // "clip.mp4: 320 x 180, 6.0 s, with sound"
		uint32_t sourceWidth = 0;             // its picture as the file has it
		uint32_t sourceHeight = 0;

		// THE PICTURE UNDER THE RECTANGLE: the source's own first picture, R G B a pixel, the top row first, stillWidth x stillHeight (the source's
		// shape, at most kVideoModeMostStillSide a side); empty when there is none yet
		std::vector<uint8_t> still;
		uint32_t stillWidth = 0;
		uint32_t stillHeight = 0;

		// THE RECTANGLE: the part of the source's picture that becomes the game's 128 x 96, in the source's own pixels (left, top, width, height; Fit's reaches past
		// the picture where its black bars are): VideoImport_SourceRect's; and the same in words (VideoImport_FitWords)
		VideoImportFit fit = VideoImportFit::Fit;
		double sourceRect[4] = { 0.0, 0.0, 0.0, 0.0 };
		std::string fitWords;                 // "crop: the middle 240 x 180 of 320 x 180 becomes 128 x 96"

		// the cut and the volume
		double fromSeconds = 0.0;
		double toSeconds = 0.0;
		std::string cutWords;                 // "from 1.0 s to 4.0 s: 3.0 s, 45 frames"
		double volumeDb = 0.0;
		std::string volumeWords;              // "AvP's level +3 dB"

		// for the state line (acceptance-video-mode): videos loaded so far, and how many frames the screen drew "Loading N%" (VideoMode_LoadingDrawn)
		size_t loadedClips = 0;
		size_t loadingFrames = 0;
	};

	int32_t const kVideoModeMostStillSide = 512;

	// The view as it is now (the screen reads it each frame)
	VideoModeView const &VideoMode_View();

	// Each frame on the Video screen: the worker's progress and its end taken in (the screen draws after it)
	void VideoMode_Frame();

	// The screen drew "Loading N%" this frame (counted, for the state line)
	void VideoMode_LoadingDrawn();

	// FILES HANDED IN, by a drop on the window while on the Video screen (WM_DROPFILES's list, video_drop.h VideoDrop_FromHdrop) or the harness's drop-file
	// (VideoDrop_FromHarness): checked (video_drop.h), then read on the worker. `listed`: how many files were handed over in all. False when refused or busy, with the
	// reason on the status line
	bool VideoMode_Dropped(std::vector<std::wstring> const &paths, size_t listed);

	// Load > Browse...: Windows' own Open dialog, filtered to the one list of endings (video_import.h), over `owner`; the file picked is taken as a drop of one
	void VideoMode_Browse(void *owner);

	// One of kVideoModeControls pressed (by its name): Fit / Crop / Expand the picture, the cut a step, the volume a step, Preview (the film made in a scratch place,
	// then played by Barry's decoder where VideoMode_SetPreviewPlace says), Save (the number asked from the free list, then FMVs\message<n>.smk written, a film there
	// replaced only after a Yes and kept in FMVs\backup\ first). False for a name it doesn't know or a press it can't take now (no video, busy), with words
	bool VideoMode_Press(char const *control);

	// Where Preview plays (the screen's place for it, in the editor's own pixels), asked each frame while it plays
	void VideoMode_SetPreviewPlace(FmvPreviewPlace const &place);

	// Mode > Exit, or the editor closing: the worker stopped and waited for, the preview stopped, the video let go (the next entry starts Empty)
	void VideoMode_Leave();

	// The state line's fields (StateLine_Add): videostate "<phase>; loadedclips <n>; loadingframes <n>; <sourceWords>; <cutWords>; volume <volumeWords>" and videofit
	// (fitWords); the screen adds videowords and videocontrols
	std::string VideoMode_StateWords();
}
