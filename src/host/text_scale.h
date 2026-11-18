// text_scale.h: the editor's TEXT SCALE, the owner's addition (Melanikus 2026-11-16: "you can guess how well I see those 16px letters" at 2560 x 1440; Bishop's order 11-18, "start
// now"). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT INTENG DOES: nothing of the kind. Its text is font16.fnt, 16 pixels, drawn one pixel a bit, and every place it puts a line, a bar or a dialog is a number of its own 640 x 480
// pixels (FUN_0046ce70, menu_font.h). So 1.0, the default, is inteng's look exactly (the owner's rule: the editor looks like inteng).
//
// HOW IT SCALES: the editor's panels keep inteng's numbers. They are laid out in a window of the client's size divided by the scale (the EDITOR'S OWN pixels), and drawn times the scale
// (the WINDOW'S pixels), letters too: each bit of a letter becomes a block of the scale's size. The mouse comes in in the window's pixels and is divided by it. So every bar, row,
// dialog and line grows together and nothing that fitted before overlaps. The 3D view itself is drawn in the window's pixels as before.
//
// THE NUMBERS: the scale is kept in thousandths (1000 to 3000), so the arithmetic is whole numbers and at 1000 both conversions give back exactly what they were given.
// FontScale= is read as hostile input (TextScale_Parse): only a plain number from 1 to 3, at most three decimals.
#pragma once

#include <stdint.h>

#include <string>

namespace host
{
	int32_t const kTextScaleLeast = 1000;    // thousandths: 1.0, inteng's own size
	int32_t const kTextScaleMost = 3000;     // 3.0
	int32_t const kTextScaleSteps[4] = { 1000, 1250, 1500, 2000 };   // the Options menu's steps
	size_t const kTextScaleMostLetters = 16; // a FontScale= value longer than this is not a number of ours

	// Reads a FontScale= value, in thousandths: digits, then at most one '.' and at most three more digits, from 1 to 3 (spaces round it are let go); false, *thousandths untouched,
	// for anything else (empty, a sign, a letter, an exponent, a second '.', more decimals, under 1 or over 3)
	bool TextScale_Parse(char const *text, int32_t *thousandths);

	// The words a scale is written as: "1", "1.25", "1.5", "2", "2.125"
	std::string TextScale_Words(int32_t thousandths);

	// The scale now, in thousandths (1000 until it is set), and setting it (held to 1000 .. 3000)
	int32_t TextScale_Get();
	void TextScale_Set(int32_t thousandths);

	// A place or a length in the editor's own pixels, in the window's: rounded to the nearest (half up); and a place in the window's pixels in the editor's own: rounded down (so a
	// press is in the own pixel whose window block holds it). Both give back their number at 1000.
	int32_t TextScale_ToWindow(int32_t own);
	int32_t TextScale_ToOwn(int32_t window);

	// A box in the editor's own pixels, in the window's: its two corners each scaled, so boxes that touch still touch and none overlaps its neighbour
	void TextScale_BoxToWindow(int32_t x, int32_t y, int32_t width, int32_t height, int32_t *windowX, int32_t *windowY, int32_t *windowWidth, int32_t *windowHeight);

	// FontScale= in the [Editor] part of the settings file: read (at most kTextScaleMostLetters + 1 letters of it) and the scale set. No line, or no file: 1.0 and true. A value that
	// isn't a scale: 1.0, false, and TextScale_Refusal says so for the status line. Saving writes the scale now as its words (TextScale_Words).
	bool TextScale_Load(char const *settingsFile);
	bool TextScale_Save(char const *settingsFile);
	char const *TextScale_Refusal();

	// The Options menu's step (1 to 4: kTextScaleSteps' 1, 1.25, 1.5, 2): set at once, kept in the settings file, and the status line's words for it (kept until the next call); any
	// other step changes nothing. The panels are laid out again on the next frame (the window's size divided by the new scale).
	char const *TextScale_Choose(int32_t step, char const *settingsFile);
}
