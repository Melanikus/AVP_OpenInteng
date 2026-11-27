// new_file_save.h: a NEW file written safely, below a folder of the editor's own or the game's: the one way the sound import and Export map write a file.
// Written 2026-11-27 by Melanikus (Dietrich) for AVP_OpenInteng (moved out of sound_import.cpp when Export map needed the same). AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// THE STEPS, as rim_save.h's RimSave_New saves a texture:
// - THE FOLDERS: each from the root down is made when it's missing, one at a time, and held to GameFolder_WhyNotPlainBelow before anything is made in it, so a
//   link among them (a junction, a symbolic link) can't send the file (or a folder) somewhere else. The root itself is the caller's (the data folder, the
//   folder for copies), checked when the editor started.
// - THE FILE: made with CREATE_NEW, so a file there already (or a link of that name) is never written over or through; a file there that holds exactly these
//   bytes (read back as a plain file, VideoImport_ReadPlainFile) is AlreadyThere, taken as it is; other bytes are refused. A half-written file is deleted
//   through the very handle that made it. Flushed and closed, then READ BACK by its name and compared byte for byte.
// - The whole path within what Windows opens (MAX_PATH).
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	enum class NewFileSaved
	{
		Made,          // written, flushed and read back exactly
		AlreadyThere,  // a file of that name holds exactly these bytes: taken as it is
		Refused,       // *why says why: nothing was written over, and no half-written file is left
	};

	// bytes saved as root\folders[0]\...\folders[n-1]\name. Each folder part and the name are one part each (no slash), checked by the caller. shown: how the
	// file is named in the words ("sound\env\Invasion\boom.wav"). mostBytes: the most a file there already is read to compare it. what: the words' start for
	// the reader ("Load sound")
	NewFileSaved NewFile_Save(std::string const &root, std::vector<std::string> const &folders, std::string const &name, std::vector<uint8_t> const &bytes,
		std::string const &shown, uint64_t mostBytes, char const *what, std::string *why);
}
