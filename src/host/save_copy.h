// save_copy.h: where the editor's Save puts the level: a COPY in the folder for copies, never the file that was opened and never anything in
// the game's folder. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (S3d, the edit path's save). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code". (Modified 2026-11-16 by Melanikus (Dietrich): the copies go to <Documents>\AVP_OpenInteng\.)
//
// THE OWNER'S RULING (owner-intent 7, through Bishop, 2026-11-15): a save NEVER overwrites the file that was opened, or anything in the
// player's game folder. It writes "<name>-edited.rif" in a folder of the editor's own, and the status line says where.
//
// THE FOLDER FOR COPIES (Melanikus, 2026-11-16 ~11:2x, through Bishop): the editor is now installed in the game's own Tools folder, as inteng was
// (<AvP>\Tools\), so the program's folder is inside the game's and nothing may be saved there. Every copy (and every export and screenshot made by
// the same rules) goes to <the user's Documents folder>\AVP_OpenInteng\, wherever the program is. The Documents folder is the one Windows gives (the
// shell's folder API), never a typed path; a hidden test run (-picture) uses a test Documents folder of its own instead (editor_main.cpp,
// CopiesDocumentsFolder), so a test never writes into the user's Documents or replaces a copy of theirs.
//
// THE RULES.
// - The folder is "edited" inside the folder for copies (SaveCopyBase: <Documents>\AVP_OpenInteng\), both made if they aren't there, and each has
//   to be a plain folder: not a link, not a file (save_folder.h, SaveFolder_WhyNotUsable, the check the editor's settings folder passes too).
// - The file's name is made from the opened file's name, and nothing else from the level: its letters, digits, "_" and "-" (anything else
//   becomes "_"), at most 64 of them, then "-edited.rif". Nothing a level holds ever becomes part of a path.
// - The path is worked out as a whole path (GetFullPathName) and refused when it is the file that was opened, or lies inside the game's
//   folder, or when the folder for copies lies inside the game's folder, whichever way the capitals of the letters are written. The
//   game's folder is the one holding avp_rifs that the editor runs from. Where the PROGRAM is doesn't matter.
// - A copy saved before is replaced by the next save: it is the editor's own file, in the editor's own folder for copies.
#pragma once

#include <stddef.h>

#include <string>

namespace host
{
	// The folder for copies: "<documentsFolder>\AVP_OpenInteng\" as a whole path, ending in one '\'. documentsFolder is the user's Documents
	// folder as Windows gives it. "" when none is given, or when the path would leave no room for a copy's folder and name.
	std::string SaveCopyBase(char const *documentsFolder);

	// The file's name for a save of the level at openedPath: its name without the folder and without the extension, made safe as above, then
	// "-edited.rif". Always a name: a path with no usable letters gives "level-edited.rif". A null path is the same as an empty one.
	std::string SaveCopyName(char const *openedPath);

	// Works out the whole path of the copy: <ownFolder>edited\<SaveCopyName(openedPath)> into *copyPath, and the folder it's in into
	// *copyFolder (ending in one '\', for the caller to make). ownFolder is the folder for copies (SaveCopyBase), a whole path ending in '\';
	// gameFolder is the game's. Gives back true when the copy may be written there. Otherwise it gives back false, with *why set to the reason
	// in plain words (static text) and the paths empty. It touches no file and makes no folder.
	bool SaveCopyPlace(char const *openedPath, char const *ownFolder, char const *gameFolder, std::string *copyPath, std::string *copyFolder,
	                   char const **why);

	// The same place and the same refusals for an EXPORT the mapper names (a .asc, a .c): <ownFolder>edited\<typed name made safe><extension>. The
	// typed name is only letters, digits, "_" and "-" (anything else becomes "_"), at most 64, "export" when none; the extension is the caller's
	// (".asc"). openedPath is the level's own path: the export is refused if it would be that file, as a copy is.
	std::string SaveExportName(char const *typedName, char const *extension);
	bool SaveExportPlace(char const *openedPath, char const *typedName, char const *extension, char const *ownFolder, char const *gameFolder,
	                     std::string *path, std::string *folder, char const **why);
}
