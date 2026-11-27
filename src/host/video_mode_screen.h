// video_mode_screen.h: THE OWNER'S VIDEO MODE, ITS SCREEN (alpha.5 item 2, his words 11-22: "let's add a new mode for video, it only has load and mode exit and displays
// a black screen 'Drag and drop .mp4 or use Load -> Browse', while video loads you'll put a loading message with % let them fit/crop/expand, cut start and ending, turn
// volume up and down, drawn a rectangle over the video to show how it'll display down (the '128p' aspect ratio)"; "Don't put it directly on strategy mode").
// Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHO DOES WHAT. Dietrich's engine (video_mode.h) holds the video and does the work; this is only its screen, in inteng's look (dialogs.cpp: a window BLACK with a
// WHITE 1-pixel frame, a button a WHITE frame filled BLUE, the words WHITE in inteng's own letters):
// - THE ENTRY: Construction's Mode > Video (menu-status.json's addition after Hierarchy, IntengWork::VideoModeEnter); the screen is the stand-in "Video", whose bar is
//   only Load > Browse... (IntengWork::VideoBrowse) and the generator's Mode > Exit, which goes back to the hub WITHOUT saving the level (Video touches no level).
// - THE SCREEN, in the level's view only (the bar, the status line and the panels stay as they are): the view black; with no video, the owner's words in the middle;
//   while a video is read or a film made, a big "Loading N%" / "Making N%" on a black box (item 1's big letters, the owner's pick 11-22) with Cancel; with a video,
//   the source's picture with THE RECTANGLE over it (the part that becomes the game's 128 x 96), its words at the top, and the controls in a frame at the foot.
// - A DROP of files on the window goes to the engine only while on this screen; on any other it is refused in words, never read (video_drop.h checks every path).
// - LEAVING the screen by any way (Exit, another screen): the engine's VideoMode_Leave, so its worker is stopped and the video let go.
// The places are video_mode_layout.h's, so a test can check them. The state line's fields: videowords (the black screen's words while it has no video, else ""),
// videocontrols ("<name> <x> <y> <w> <h>|..." in the editor's pixels, for the harness's click-video), videostate (VideoMode_StateWords) and videofit (the fit's words).
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#include "inteng_menus.h"

namespace ui
{
	struct Box;
}

namespace host
{
	struct PaintBox;
	struct PaintText;

	// The Video screen's number: the screen named "Video" (inteng_menus.cpp's last stand-in), or kIntengScreenCount when the menus have none
	inline size_t VideoModeScreen_Number()
	{
		for (size_t s = 0; s < kIntengScreenCount; s++)
		{
			if (strcmp(kIntengScreens[s].name, "Video") == 0)
			{
				return s;
			}
		}

		return kIntengScreenCount;
	}

	inline bool VideoModeScreen_Is(size_t screen)
	{
		return screen < kIntengScreenCount && screen == VideoModeScreen_Number();
	}

	// Each frame, from the panels' plan (editor_panels.cpp PlanThePanels), on the Video screen or not. On it (`on`): the engine's frame (VideoMode_Frame), then the
	// screen's words, message and controls added to the frame's boxes and lines (at most `most` of each), and the places kept for VideoModeScreen_DrawUnder and
	// VideoModeScreen_Press. Either way, the state line's four fields
	void VideoModeScreen_Panels(bool on, ui::Box const &view, PaintBox *boxes, size_t *boxCount, PaintText *lines, size_t *lineCount, size_t most);

	// Each frame on the Video screen, drawn before the panels (editor_main.cpp AddThePanels), so the bar's menus and a dialog stay over it: the view black, then the
	// source's picture and its rectangle
	void VideoModeScreen_DrawUnder();

	// A press of the left button at (x, y) in the editor's pixels, not taken by the bar: true when it was on one of the controls shown (that control pressed:
	// VideoMode_Press)
	bool VideoModeScreen_Press(int32_t x, int32_t y);

	// Every screen change (editor_main.cpp OnScreenChanged): entering the Video screen gives the engine the preview's place; leaving it by any way lets go of the video
	// (VideoMode_Leave) and of the picture drawn
	void VideoModeScreen_ScreenChanged(size_t was, size_t now);

	// FILES DROPPED on the editor's window: a real drop (WM_DROPFILES's handle, released here) or the harness's list (WM_COPYDATA with video_drop.h kVideoDropTag,
	// UTF-8, one path a line). screen: the screen shown (a place in kIntengScreens). On the Video screen they go to the engine (VideoMode_Dropped, which checks
	// every path first). Off it, WAVs are the sound import's (sound_import.h: loaded on Object Placement, said where they go on any other screen; Dietrich
	// 2026-11-27), and anything else is the texture import's (a picture), which says where a video goes
	void VideoModeScreen_DropHandle(void *drop, size_t screen);
	void VideoModeScreen_DropText(char const *text, size_t bytes, size_t screen);

	// The editor is closing: the engine's worker stopped and waited for, the picture let go
	void VideoModeScreen_Close();
}
