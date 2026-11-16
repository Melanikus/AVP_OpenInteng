// screenshot_name.h: the name of a screenshot picture (Light Sourcing's Screenshot item, command 0x66, P6) and where it goes. Plain data and
// text, no window, no file touched, so a test can drive it. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (P6).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// MATCHES inteng's FUN_00472930 (AvP-docs\engine\inteng, out\menu-reads-2.txt:1395-1700): it takes the level's name, adds "P00.bmp", and tries
// the numbers 00, 01, ... 99 until it finds a file that isn't there yet; if all 100 are taken it saves nothing. It shows "Saving <name>" and
// writes the screen as a BMP. Here the same numbering and the same name shape ("<level name>P<nn>.bmp"). Named deviations (owner's rule: the
// effect on level data is copied exactly; a screenshot touches none, and the rest may be better):
//   1. Where: inteng put the file beside the level's name as given in the level (its RIFFNAME). The editor never writes beside a level or in the
//      game's folder (owner-intent 7), so the pictures go to "pictures" in the editor's own folder, the way a saved copy goes to "edited".
//   2. The file: inteng wrote its own 24-bit BMP, whose size field is 0x3e bytes more than the file's (a slip of the old code). The editor's
//      is the BMP the graphics library writes, a standard one, of the same picture: the window as it is on the screen.
//   3. All 100 numbers taken: inteng said nothing; the status line says so.

#pragma once

#include <stddef.h>

#include <string>

namespace host
{
	// How many numbers there are: 00 to 99
	int const kScreenshotNumbers = 100;

	// "<name>P<nn>.bmp" for the level opened at openedPath, with the name made safe the way a saved copy's is (save_copy.h: letters, digits,
	// "_" and "-", at most 64, "level" when nothing usable is left) and nn the number as two digits. Empty for a number outside 0 to 99.
	std::string ScreenshotName(char const *openedPath, int number);

	// The first number from 0 up whose name taken() says isn't used yet, or -1 when all 100 are (nothing is then saved, as in inteng).
	// taken is given each name in turn and the context as it was given here.
	int ScreenshotFirstFree(char const *openedPath, bool (*taken)(std::string const &name, void *context), void *context);

	// The folder for the pictures, from the folder SaveCopyPlace worked out for the copy ("...\edited\"): the same folder's sibling "pictures\".
	// Empty if copyFolder doesn't end in "edited\".
	std::string ScreenshotFolderFromCopyFolder(std::string const &copyFolder);
}
