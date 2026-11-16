// screen_picture.h: what Windows actually shows for the editor's window, saved as a picture. Written 2026-11-14 by Melanikus (Dietrich) for
// AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: a -picture run saves first-picture.png from the level's own back buffer, before it's shown. Melanikus's first build with the panels
// showed the view black on his screen while that picture, of the same level at the same size, was fine. A proof that reads only the back
// buffer can pass on a broken screen, and owner-intent.md says: "If a proof can pass on a broken picture, the proof is wrong. Fix the proof
// first." So a -picture run also saves first-screen.png: the editor's window as Windows shows it, the level's view and the panels together,
// asked of Windows' own screen composer with PrintWindow and PW_RENDERFULLCONTENT, which takes in what Direct3D presented.
//
// THE SELF-CHECK: a capture that came back blank, or showed something else, must never pass. So the picture is checked against something
// the editor knows it drew: a patch of the toolbar right of every menu title, which is the panels' own background colour. If the patch
// isn't that colour, the capture didn't show the editor's panels, and the run says so and fails.

#pragma once

#include <stdint.h>

#include <windows.h>
#include <d3d9.h>

#include "panel_layout.h"

struct ScreenPictureReport
{
	bool saved;           // the picture was taken and saved
	bool panelsSeen;      // every pixel of the toolbar's patch was the panels' colour, give or take kScreenPictureColourSlack
	double viewMean;      // in the view's box: the mean of the pixels' brightness, each pixel's red, green and blue averaged, 0 to 255
	double viewLitShare;  // and the share of its pixels whose brightness is above 16, 0 to 1
};

// How far each of red, green and blue may be from the panels' colour in the toolbar's patch, for the screen's own rounding
int32_t const kScreenPictureColourSlack = 3;

// The numbers above, from a picture held in memory (screen_measure.cpp, which a test compiles alone): width by height pixels of 32 bits
// each, 0xXXRRGGBB, rowPixels to a row. The view's box and the patch are cut to the picture; an empty patch is never seen, and an empty
// view box has a mean and a share of 0.
void ScreenPicture_Measure(const uint32_t *pixels, int32_t width, int32_t height, int32_t rowPixels, const ui::Box &view, const ui::Box &patch,
	uint32_t panelsColour, ScreenPictureReport *report);

// Takes window's inside as Windows shows it, saves it as a PNG at path, and measures it. A device is needed only to hold the picture and
// write the PNG. Gives back report->saved && report->panelsSeen.
bool ScreenPicture_Save(IDirect3DDevice9 *device, HWND window, const char *path, const ui::Box &view, const ui::Box &patch, uint32_t panelsColour,
	ScreenPictureReport *report);
