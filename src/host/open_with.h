// open_with.h: opening a level file given to the editor, as "AVP_OpenInteng.exe <file.rif>", which is what Windows' "Open with" does.
// Written 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// inteng opens THE DOUBLE-CLICKED FILE ITSELF (WinMain: FUN_0042b8c0(path)): no copy, no new name, no refusal of a name another level has. The game's loader opens a level by a name under its folder (avp_rifs/<name>.rif), so
// the file is read from where it is and put, as a copy of its bytes, in a folder of the editor's own inside its data folder's avp_rifs, "OpenedByEditor", where only the editor puts files: the level is then named
// OpenedByEditor/<name> to the game (the rule for a level's name allows a folder part). A file with the same name as a stock level (Derelict.rif) is opened as ITSELF, never refused, and what is in the editor's own
// avp_rifs under that name is never touched. Whatever was in OpenedByEditor under that name from an earlier open is replaced: it is only ever the editor's own copy. (Modified 2026-11-16 by Melanikus (Dietrich), Bishop:
// "Melanikus can't open map .rifs"; it used to refuse a different level of the same name.) The path is hostile input (CODING-STANDARD.md, rule 1; the phase 2 plan, section 5), so these are the rules:
// - It has to be a regular file: not a folder, not a link or any other reparse point, not a device.
// - Its name has to end in .rif, in capitals or not, and it can be at most 16 MB.
// - The part of its name before .rif has to pass the game's own rule for a level file's name (level_file_name.h). That part, and only
//   that part, is used: nothing else is ever built from the path. The rule takes plain letters and digits and a few marks only, so a
//   colon (a hidden stream, "x.txt:y.rif") or a device's name is refused there too.
// - It's copied with CopyFile into <the data folder>\avp_rifs\OpenedByEditor\<that name>.rif (the folder made when it isn't there), over the editor's own earlier copy when there is one. The name the game gets is
//   OpenedByEditor/<that name>.
// - The path is read as the wide text Windows keeps for the command line, never as the program's narrow argv: turning it narrow can
//   change a letter into a different one Windows thinks is close ("best fit"), even into a slash.
// Nothing is written to the registry: the README says how to set "Open with" up by hand.

#ifndef AVP_OPENINTENG_OPEN_WITH_H
#define AVP_OPENINTENG_OPEN_WITH_H

#include <stddef.h>

// The largest level file the editor takes from outside its data folder
enum { kOpenWithMostBytes = 16 * 1024 * 1024 };

// Makes the level file at "path" (the command line's own wide text for it) ready to open: copies its bytes into the data folder's avp_rifs\OpenedByEditor. Gives back true, with the level's name for the game ("OpenedByEditor/<name>", without .rif) in levelName, or false, with why
// in "why". dataFolder is the editor's own data folder, from its settings.
bool OpenWith_TakeLevel(const wchar_t *path, const char *dataFolder, char *levelName, size_t levelNameSize, char *why, size_t whySize);

#endif
