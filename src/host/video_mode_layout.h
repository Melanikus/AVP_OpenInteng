// video_mode_layout.h: WHERE THE VIDEO SCREEN PUTS THINGS (the owner's Video mode, alpha.5 item 2; Drake's screen, video_mode_screen.h), as plain numbers with no
// drawing, so a test can check every place. Inside the level's view, in the editor's own pixels:
// - at the top, the lines of words (the video's name and size, the fit, the cut, the volume), one under the other;
// - at the foot, the controls' frame: the buttons in a row, left to right, a new row when the next would not fit, each row centred, the frame round them all;
// - between the two, the source's picture with its shape kept, as large as fits, together with THE RECTANGLE: the part of the picture that becomes the game's
//   128 x 96 (video_mode.h VideoModeView::sourceRect, in the source's own pixels). Fit's rectangle reaches past the picture where its black bars go, so the two are
//   fitted as one: the picture and its rectangle always both show, whole.
// Hostile numbers (a size of 0, a rectangle that is not a number or has no size, a view too small) give no picture and no rectangle, never a box outside the view.
// Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace host
{
	struct VideoLayoutBox
	{
		int32_t x = 0;
		int32_t y = 0;
		int32_t width = 0;    // 0: there is no such box
		int32_t height = 0;
	};

	int32_t const kVideoLayoutMargin = 16;        // the room kept round everything, inside the view
	int32_t const kVideoLayoutButtonPadX = 8;     // a button's room left and right of its words
	int32_t const kVideoLayoutButtonPadY = 4;     // and above and below them
	int32_t const kVideoLayoutButtonGap = 6;      // between two buttons, and between two rows
	int32_t const kVideoLayoutFramePad = 6;       // the frame's room round the buttons
	int32_t const kVideoLayoutLineGap = 4;        // between two lines of words

	struct VideoModeLayout
	{
		std::vector<VideoLayoutBox> controls;   // one for each width given, in that order
		VideoLayoutBox frame;                   // round the controls (width 0 when there are none)
		std::vector<int32_t> lineTops;          // each line of words' top (its left is view.x + kVideoLayoutMargin)
		VideoLayoutBox picture;                 // the source's picture (width 0: none)
		VideoLayoutBox rectangle;               // the part that becomes 128 x 96, over the picture (width 0: none)
	};

	// view: the level's view. labelWidths: each control's words, as wide as the font draws them (0 or less: the control is left out, its box empty). fontHeight:
	// a line's height. lines: how many lines of words go at the top. sourceWidth x sourceHeight: the picture's own size; sourceRect: left, top, width, height of
	// the rectangle in the picture's pixels (nullptr, or no picture: none)
	VideoModeLayout VideoModeLayout_Plan(VideoLayoutBox const &view, std::vector<int32_t> const &labelWidths, int32_t fontHeight, size_t lines, uint32_t sourceWidth,
		uint32_t sourceHeight, double const *sourceRect);

	// The box that holds (x, y), by its place in boxes; -1 for none (a box of width or height 0 holds nothing)
	int32_t VideoModeLayout_At(std::vector<VideoLayoutBox> const &boxes, int32_t x, int32_t y);
}
