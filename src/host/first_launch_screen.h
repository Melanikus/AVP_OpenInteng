// first_launch_screen.h: THE FIRST-LAUNCH SETUP at the editor's start, its Windows part (first_launch_ui.h has its words, its form and the ini line it writes,
// and why). Written 2026-11-23 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// NOT FOUND (no DataFolder= written, and no game's folder above the editor's own): until the game is found there is no font to draw inteng's look with (the game's
// small font is a game file, graphics/Common/aa_font.RIM, and inteng's own font16.fnt is only beside Rebellion's old tools), so this one is said in Windows' own box,
// as the start's other stops are. It says the three things (where Open Inteng is, where it belongs, that it found no levels and where it looked), then "Set the
// game's folder now?":
//   - Yes: Windows' own folder picker. The folder picked is held to the data folder's own checks (save_folder.h SaveFolder_WhyNotUsable: a plain folder on a
//     drive, never a link) and its avp_rifs must be there and a plain folder (never a link: Vasquez's F-b). Only a folder that passes is written, as DataFolder=
//     under [Editor] in AVP_OpenInteng.ini, every other byte of that file kept (first_launch_ui.h FirstLaunch_IniWithLine), and the start goes on with it, where
//     every check of a written DataFolder= is made again. A folder refused says why, writes nothing, and offers to pick again.
//   - No (Not now): the editor closes, as it did (the owner, 11-23: "Close, as today"); the box has said how to fix it.
// With nobody at the screen (a hidden run) no box is shown: the words go to the error output, and the answer is Not now.
#pragma once

#include <string>

namespace host
{
	// programFolder: the editor's own folder (ending in its last "\" or not); settingsFile: AVP_OpenInteng.ini's path beside it. True, with *dataFolder the folder
	// picked, checked and now written as DataFolder=; false for Not now (the editor then closes)
	bool FirstLaunch_NotFound(char const *programFolder, char const *settingsFile, std::string *dataFolder);

	// AVP_OpenInteng.ini READ WHOLE before a line or a section of it is written (the DataFolder= line; the list of maps searched, shapes_scanned.h): judged on
	// its own handle, its link never followed (a plain file with one name), at most 1 MB. True with *text (empty, *there false, when there's no file yet);
	// false with *why
	bool FirstLaunch_ReadIni(char const *settingsFile, std::string *text, bool *there, std::string *why);

	// ... and WRITTEN back in one step: to "<ini>.writing" made new (never through a file or link already there), flushed, then put in the ini's place by
	// Windows (ReplaceFileA when it was there, else a move that never replaces). False with *why, and the ini as it was
	bool FirstLaunch_WriteIni(char const *settingsFile, std::string const &text, bool there, std::string *why);
}
