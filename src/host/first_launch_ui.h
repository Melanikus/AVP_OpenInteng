// first_launch_ui.h: THE FIRST-LAUNCH SETUP FORM, its words and its parts, and the one ini line it writes. Pure: no Windows, no files, no editor state.
// Written 2026-11-23 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY (the owner, through Bishop 2, 2026-11-22 and 11-23): the community can't work on maps without the object shapes the old avptools gave (its
// shape_rifs), and doesn't know how the folders should be set up. So when Open Inteng starts, ONE form in inteng's own look ALWAYS says three things:
//   1. where Open Inteng is (its own folder);
//   2. where it belongs: the game's Tools folder, Steam's ...\steamapps\common\Aliens versus Predator Classic\Tools or the original Fox install's
//      (inteng's manual: c:\program files\fox\aliens versus predator; "Program Files (x86)" on 64-bit Windows);
//   3. whether it found the game's levels around it ("found 63 levels and 23 HUD files", or "found no levels", with where it looked).
// With levels found and object shapes to make (Frost's scan, shape_extract.h), it asks: make them now? (Yes / Not now). With none found, it says how to
// fix it and offers to name the game's folder (Browse... / Not now); the screen (first_launch_screen.cpp) checks the folder chosen before anything is written.
// The plan and the owner's draft words: AvP-docs/director/plan-first-launch-shapes.md.
//
// HOSTILE NAMES: every folder and file name is shown by the editor's safe text rule (panel_paint.h PanelPaint_SafeText: the space to the tilde, every other
// byte a '?', at most 255 letters each), and at most kFirstLaunchMostNames names of a list are named, the rest counted. Paths are shown WHOLE here (the
// folder is the message), wrapped to the window; elsewhere the editor cuts them to names.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "dialog_box.h"

namespace host
{
	// What a press answers (DialogWidget::answer): Yes (FOUND) and Return are kDialogOk; Not now and Escape are kDialogEscape; Browse... (NOT FOUND) its own
	int32_t const kFirstLaunchYes = kDialogOk;
	int32_t const kFirstLaunchNotNow = kDialogEscape;
	int32_t const kFirstLaunchBrowse = 4;

	size_t const kFirstLaunchMostNames = 8;   // a list's names shown; any more are counted ("and 12 more")

	// What the start knows when the form opens (the screen fills it from the folders and Frost's scan)
	struct FirstLaunchFacts
	{
		std::string programFolder;            // where Open Inteng is: its own folder
		bool found = false;                   // the game's levels were found
		bool fromIni = false;                 // found at the folder DataFolder= names (else around Open Inteng, the folder above its own)
		std::string gameFolder;               // the folder that holds avp_rifs, when found
		std::vector<std::string> looked;      // where it looked, when not found
		size_t levels = 0;                    // the game's own levels found (avp_rifs)
		size_t huds = 0;                      // the game's own HUD files found (avp_huds)
		std::vector<std::string> notOwn;      // stock files there that aren't Steam's own bytes
		std::vector<std::string> missing;     // stock files that aren't there
		size_t shapesToWrite = 0;             // the object shapes Yes would make (none there is ever replaced)
	};

	extern char const kFirstLaunchTitle[];    // "Open Inteng"

	// The form's words, paragraphs separated by an empty line, not yet wrapped (the three things, then the question)
	std::string FirstLaunch_Words(FirstLaunchFacts const &facts);

	// THE FORM: the words wrapped to the window by widthOf (a letter's width as drawn), a line fontHeight high, then its two buttons (Yes / Not now when found,
	// Browse... / Not now when not); the window as wide as screenWidth allows, between 400 and 760 pixels
	DialogFormSpec FirstLaunch_Form(FirstLaunchFacts const &facts, std::function<int32_t(unsigned char)> const &widthOf, int32_t fontHeight, int32_t screenWidth);

	// THE INI LINE (DataFolder=<folder>): ini's text with key=value under [section], every other byte kept: the first key= line of that
	// section replaced (its own line ending kept), or the line added at the end of the section's own lines, or the section added at the end of the file when it
	// isn't there. Sections and keys are matched as Windows matches them (any case, spaces around them dropped; ';' starts a comment line). New lines end as the
	// file's do (CRLF when it has any, else LF; CRLF for an empty file). False, with *out untouched, when value or key holds a new line, a 0 or a '=' in key, or
	// section holds a ']'
	bool FirstLaunch_IniWithLine(std::string const &ini, std::string const &section, std::string const &key, std::string const &value, std::string *out);

	// THE INI SECTION (a list the editor keeps, shapes_scanned.h): ini's text with the FIRST [section] holding exactly `lines`, every other byte kept: the lines
	// from its header down to its last line that isn't empty are replaced (the empty lines after it, and the next section, stay as they were), or the section
	// is added at the end of the file when it isn't there. The section is matched as Windows matches it (any case, spaces dropped). New lines end as the file's
	// do. False, with *out untouched, when a line holds a new line or a 0 or starts (after spaces) with a '[', or section holds a ']' or a new line
	bool FirstLaunch_IniWithSection(std::string const &ini, std::string const &section, std::vector<std::string> const &lines, std::string *out);
}
