// shapes_scanned.h: THE MAPS ALREADY SEARCHED FOR OBJECT SHAPES, a list kept in AVP_OpenInteng.ini (the owner, 2026-11-23: "Open Inteng will save a list of
// map shas that it already searched assets from in his ini, whenever any new map not in its ini is found it'll scan it too"; the spec: AvP-docs/director/
// plan-first-launch-shapes.md, "The owner's new rule"). Written 2026-11-23 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code". Pure: no Windows, no files; the start reads and writes the ini (first_launch_screen.cpp).
//
// THE LIST is a section of its own, [ShapesScanned], one map a line: "<its SHA-256, 64 small hex letters>=<the file it was found as>" (the name is for people
// reading the ini; the SHA-256 is what counts: a changed map has another one, so it is searched again). Its being there at all means the player said Yes to
// the first search (the setup form): from then on a map not on it is searched without asking. "Not now" writes nothing (the owner: "Ask again next start").
// HOSTILE: anyone can write the ini. A line that isn't a 64-hex key, a repeat, or one past kShapesScannedMost is not taken (and named once, so the editor can
// say it); a name is kept to the editor's safe text rule when written. The section is the editor's own: writing it replaces its lines, every other byte of
// the file kept (first_launch_ui.h FirstLaunch_IniWithSection).
#pragma once

#include <stddef.h>

#include <string>
#include <vector>

namespace host
{
	size_t const kShapesScannedMost = 4096;
	size_t const kShapesScannedMostName = 64;      // a name kept on the list: at most 64 bytes, after the safe text rule (Vasquez's review)
	size_t const kShapesScannedMostRefused = 16;   // the section's lines not taken that are named; any more are only counted (refusedMore)
	extern char const kShapesScannedSection[];   // "ShapesScanned"

	struct ShapesScannedEntry
	{
		std::string sha256;   // 64 small hex letters
		std::string name;     // the file it was found as ("Genshd1.RIF"), for people reading the ini
	};

	struct ShapesScannedList
	{
		bool present = false;                       // the section is in the ini: the player said Yes once, so new maps are searched without asking
		std::vector<ShapesScannedEntry> entries;    // in the file's order, no SHA-256 twice
		std::vector<std::string> refused;           // the section's lines not taken (not a 64-hex key, a repeat, past the most), as the safe text rule shows them,
		                                            // at most kShapesScannedMostRefused of them
		size_t refusedMore = 0;                     // lines not taken past those
	};

	// The list, read from the ini's text (its first [ShapesScanned] section, any case; ';' lines are comments)
	ShapesScannedList ShapesScanned_Read(std::string const &ini);

	// A file's name as the list keeps it: the safe text rule, then at most kShapesScannedMostName bytes (the trigger and the reader both keep names by it)
	std::string ShapesScanned_Name(std::string const &name);

	// Whether a map's SHA-256 (any case) is on the list
	bool ShapesScanned_Has(ShapesScannedList const &list, std::string const &sha256);

	// The ini's text with its [ShapesScanned] section holding exactly these entries (a comment line first, then one line each), every other byte of the file
	// kept. False, with *out untouched, for more than kShapesScannedMost entries or one whose SHA-256 isn't 64 hex letters
	bool ShapesScanned_Write(std::string const &ini, std::vector<ShapesScannedEntry> const &entries, std::string *out);
}
