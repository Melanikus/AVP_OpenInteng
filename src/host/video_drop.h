// video_drop.h: A VIDEO FILE HANDED TO THE EDITOR, by a drag and drop onto its window in Video mode or by Load > Browse... (the owner, 11-22: "Drag and drop .mp4
// or use Load -> Browse"), checked before anything opens it. Written 2026-11-22 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// THE PATH IS HOSTILE INPUT (Tao 3; Bishop 2's brief). A path that names another computer is refused FROM ITS LETTERS, before any call that could reach it:
// opening \\server\share\... makes Windows contact that computer and can hand it the user's login hash. So, from the letters alone: a UNC name (\\server\...,
// //server/...), every \\?\ and \\.\ name (\\?\UNC\..., \\.\C:\..., \\?\GLOBALROOT\..., devices), the NT \??\ form, a name with no drive letter, and a name with a
// character below a space. Then the drive letter: what it stands for, read from this computer's own table of names (QueryDosDevice, which asks no other
// computer), must not be a network share (a mapped drive is \Device\LanmanRedirector\..., \Device\Mup\... or \??\UNC\...), and the drive must be one of this
// computer's own (GetDriveType: fixed, removable or CD; asked only once the table said it is no share). Then: exactly one file, whose ending is one of the ONE
// list video_import.h keeps (VideoImport_EndingsWords: the owner's ruling 11-22, Media Foundation's common containers).
// The letters can't show a FOLDER along the path that is itself a link to another computer (C:\Users\me\Videos\clips\a.mp4, where clips was made with mklink /D
// \\server\share: Vasquez's review, the MUST). That is video_import.h's walk: before its one open, every folder from the drive's root down to the file's own is
// read itself, each before anything goes through it, and a symbolic link or a link of any kind not known is refused (VideoImport_FoldersOnTheWayTaken).
// What is left (a regular file, no link, no stream, its size and length) is video_import.h's own check, which reads it.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include <windows.h>
#include <shellapi.h>

namespace host
{
	// The WM_COPYDATA tag the scenario harness's drop-file sends its paths with ('DROP'): one path a line, UTF-8, taken by the same check as a real drop
	unsigned long const kVideoDropTag = 0x44524f50ul;

	// The files a drop handed over (DragQueryFileW on the WM_DROPFILES handle), each as Windows gives it; at most `most` (the rest counted in *more); the handle is
	// not released here (the window's WM_DROPFILES does that)
	std::vector<std::wstring> VideoDrop_FromHdrop(HDROP drop, size_t most, size_t *more);

	// The harness's list: UTF-8 text, one path a line (no line, or a line that isn't UTF-8, gives no path for it)
	std::vector<std::wstring> VideoDrop_FromHarness(char const *text, size_t bytes);

	// Whether what a drive letter stands for (QueryDosDevice's answer for "X:") is another computer's share: \Device\LanmanRedirector\..., \Device\Mup\..., \??\UNC\...
	// (any capitals); a local volume (\Device\HarddiskVolume3) or a SUBST of a local folder (\??\C:\...) is not
	bool VideoDrop_TargetIsNetwork(std::wstring const &target);

	// The one video file the editor may read, from what was handed over: true with its full path in *path; false with the reason in words for the status line
	// ("Not taken: one video at a time (2 files were dropped)", "Not taken: \\server\share\a.mp4 is on another computer", ...). `listed` is how many files
	// were handed over in all (the list may hold fewer: VideoDrop_FromHdrop's most)
	bool VideoDrop_Check(std::vector<std::wstring> const &paths, size_t listed, std::wstring *path, std::string *why);

	// The UTF-8 of a path, as the status line writes it (a character that can't be written comes out as '?')
	std::string VideoDrop_Words(std::wstring const &path);
}
