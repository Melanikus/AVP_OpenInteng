// level_reopen.h: close the level the editor holds and open another, in the same sitting (All stock maps). Written 2026-11-22 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The close is main()'s own leaving and the open is main()'s own open, in their order (editor_main.cpp CloseTheLevel and OpenTheLevel). Between
// the two it lets go of what nothing else resets from one level to the next (Vasquez's plan, AvP-docs director/plan-all-stock-maps.md, step 4):
// Module Linking's tree and picks (left over, every save's WriteTheLinks would write level A's links into level B), AutoLink's kept module, the
// Delete Links run, the selection, the level's source path, and the open dialogs. The camera file of the level before is written by the close,
// and the next level's is read by the open.
//
// ONLY WHEN NOTHING RUNS ON THE LEVEL: no plot, AutoLink, Delete Links or Video job (the caller checks, as AllStockMaps_Start does). It saves
// nothing: a level with changes is let go of as it is. All stock maps does that ON PURPOSE when a level's plot failed or the file changed on
// disk: that level is not saved. So never "fix" this by saving first. Waypoint mode is set as its screen is entered (lastOpen let go).
// The level is opened by its name from the editor's data folder (avp_rifs\<name>.rif), as main() opens one named with Level=.
#pragma once

#include <string>

// False with *why when the next level can't be opened: the editor then holds NO level, and the caller opens the one before again, or stops.
bool LevelReopen_Open(char const *levelName, std::string *why);
