// game_folder.h: THE GAME'S FOLDER ABOVE THE EDITOR'S, so a first start works out of the box (Drake 2026-11-22, alpha.5; Bishop 2's order: "FIRST LAUNCH MUST WORK
// OUT OF THE BOX"). The editor is installed as inteng was, in the game's own Tools folder (<the game>\Tools\AVP_OpenInteng.exe), and the release zip's
// AVP_OpenInteng.ini names no data folder, so a fresh install used to stop at once, asking for DataFolder=.
//
// Now, when the ini has no DataFolder= line, the editor's start (editor_main.cpp) asks here for the folder above its own. When that folder holds a folder named
// avp_rifs, it is the game's folder, and it is used as the data folder: the very folder the owner's own Tools ini names by hand.
// - Only the program's own place is read (the path Windows gives for the running program), never the command line or a setting.
// - Only a plain path on a drive is looked at ("C:\..."): another computer's share or Windows' device forms are refused before anything is read, as the data
//   folder's own rule (save_folder.h) refuses them too.
// - The folder found is then held to exactly the checks a written DataFolder= is held to (editor_main.cpp: SaveFolder_WhyNotUsable, then its avp_rifs), so it is
//   trusted no more than a written one.
// - A DataFolder= line with something written after the "=" always wins, even one that can't be used: that one stops the editor with the reason, and is never
//   swapped for the folder found here.
// Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <string>

namespace host
{
	// programFolder: the program's own folder, ending in its last "\" (editor_main.cpp's ProgramFolder; one without it is taken the same). True, with the
	// folder above it in gameFolder (no last "\"), when that folder holds a folder named avp_rifs. False, with gameFolder empty, when it doesn't; when the folder
	// above would be a whole drive ("C:\Tools\" or "C:\": the data folder's rule refuses a whole drive); and when the path isn't a plain one on a drive
	bool GameFolder_AboveProgram(std::string const &programFolder, std::string *gameFolder);
}
