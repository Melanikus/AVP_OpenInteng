// one_editor.h: one editor at a time in each program folder. Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: two editors started from the same folder would share what the game's own code keeps for one program: the log and the game's other
// files in the editor's own folder (SAVE-FOLDER-1: <the program's folder>\settings\), and a level opened twice could be saved twice, the
// second save over the first. Melanikus's first "Open with", on 2026-11-14, started a second editor beside one already running. So an
// editor runs once per folder: a second one from the same folder says another is running, and stops before it reads or writes anything.
// An editor in another folder has files of its own, so it isn't in the way: a proof run from a build folder can run while the owner
// has his own editor open.
//
// HOW: the first editor holds a named mutex, a token Windows keeps for as long as the program that made it runs, and gives back when it
// ends, however it ends. A second editor asks Windows for the same name and is told it's taken. The name is in the "Local\" space, which
// is one user's own sign-in, and it ends in a number made from the program's folder: the folder's own full path as Windows finally
// resolves it (GetFinalPathNameByHandleW: no short 8.3 names, no junctions or links left in it), in capitals, since Windows' paths ignore
// case. So one folder gives one name, however its path was spelled.

#pragma once

#include <stddef.h>

// The start of every token's name; the rest is the folder's number (one_editor.cpp)
extern const wchar_t kOneEditorName[];

// What asking for the token gave, each with its own words for the box (OneEditor_Words): taken; another editor in this folder holds
// it; the folder couldn't be opened or named; or Windows wouldn't make the token at all. Only "taken" lets the editor go on.
enum class OneEditorAnswer
{
	Taken,
	AnotherIsRunning,
	FolderNotNamed,
	WindowsWouldNotSay
};

// The words for an answer, as plain text
const char *OneEditor_Words(OneEditorAnswer answer);

// The token's name for a folder, written into name. Gives back false if the folder can't be opened or its name doesn't fit.
bool OneEditor_NameFor(const wchar_t *folder, wchar_t *name, size_t nameCount);

// Takes the token for this program's own folder, for as long as it runs. Anything but Taken means this editor mustn't go on.
OneEditorAnswer OneEditor_Claim();

// The same, for a folder given by name (the test plays two editors with it)
OneEditorAnswer OneEditor_ClaimFor(const wchar_t *folder);
