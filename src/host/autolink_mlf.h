// autolink_mlf.h: AutoLink's "Load from file" (0xa3): the module views a ".mlf" file next to the level names, as a HOSTILE INPUT. Written 2026-11-15 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The facts: AvP-docs\engine\inteng\drake-autolink-facts.md section 3 (FUN_004c8be0, menu-reads-2.txt / dump-all-2.txt L64171).
//
// WHAT inteng DOES (FUN_004c8be0): opens the level's own file name with its extension replaced (".mlf"), reads the WHOLE file (no cap, no check) and walks records until the end or a right
// click. A record is the module's name as a NUL-terminated text padded to a multiple of 4 bytes, then a place (3 ints), nine ints (a 3 x 3 matrix) and the view's place (3 ints, times 100 in
// memory). The module with that name whose place is within 1 unit on each axis is made current, the visibility pass is run from the recorded view (FUN_004c6fb0), and what it sees is put
// in the module's tree (kept within the visible distance).
//
// WHAT THIS DOES: the same, but the file is treated as hostile (CODING-STANDARD: input is hostile). NAMED: (1) a file bigger than kMostBytes, or empty, is refused; (2) the whole file is parsed
// BEFORE anything is changed: a record that is cut short, a name that is empty, over 63 bytes, not plain text or with no NUL, a matrix number that no turn can have, more than kMostRecords records
// or bytes left over after the last record REFUSE THE FILE (the first reason is given, with the record's number) and nothing in the level is touched; (3) a record naming no module, or whose place
// is not within 1 unit of the module's, is skipped and counted (inteng does the same: it finds no module); (4) a name is matched ignoring case (the level's names are one name either case); (5) the
// place is compared as the level's file numbers or as memory units (the decompile compares the module's own place, in memory units; which one the file holds is not on disk), either is taken; (6)
// the pass is OUR rasteriser (autolink_pass.h), the links the editor's AutoLink makes; (7) no right click to stop. The .mlf's text and who writes it are not on disk: the layout is the decompile's.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "autolink_edit.h"
#include "autolink_scene.h"
#include "autolink_views.h"

namespace host
{
	size_t const kMlfMostBytes = 1u << 20;     // a bigger file is refused
	size_t const kMlfMostRecords = 5000;       // more records than this refuse the file
	size_t const kMlfMostName = 63;            // the game keeps an object's name to 63 bytes

	struct MlfRecord
	{
		std::string name;
		int32_t place[3];   // the module's place as the file names it
		int32_t turn[9];    // the view's 3 x 3 matrix, 16.16 numbers, the camera's right, up and forward as rows
		int32_t view[3];    // the view's place, in the file's numbers (times 100 in memory units)
	};

	// The ".mlf" file's path for a level opened at openedPath: its extension (the last '.' after the last folder mark) replaced, or ".mlf" added. Empty when openedPath is empty.
	std::string MlfPathFor(char const *openedPath);

	// Reads the whole file, at most kMlfMostBytes. False, with the reason, when it cannot be read, is empty or is bigger. Reads a file; changes nothing.
	bool MlfReadFile(char const *path, std::vector<uint8_t> *bytes, std::string *why);

	// Parses the bytes into records, all or none (see above). False, with the reason and the record's number (from 1), when the file is refused; *records is then empty.
	bool MlfParse(uint8_t const *bytes, size_t size, std::vector<MlfRecord> *records, std::string *why);

	struct MlfCounts
	{
		size_t records;     // records in the file
		size_t used;        // records whose module was found and whose view was drawn
		size_t unknown;     // records naming no module, or one whose place is not where the record says
		size_t linksAdded;
		size_t cut;
		size_t written;
		size_t skipped;     // modules with links but no MODULEDT
	};

	// Runs the records on the open level: the module of each is made current (its old links kept), the view drawn, what it sees put in its tree, then every changed tree written at once
	// (one image, one loader read). visibleDistance and levelScale as AutoLink_Run's.
	AutoLinkResult AutoLink_FromMlf(OpenLevelFile *open, std::vector<MlfRecord> const &records, double levelScale, int32_t visibleDistance, MlfCounts *counts, std::string *why);

	// FOR THE THREADED RUN (autolink_runner.h StartFromFile; Dietrich 2026-11-16). The module a record names (FUN_004c8be0 0x4c8e0c-0x4c8efa): the records are searched in their order
	// (scene.list, the modules in the file's order) and the FIRST whose name is the record's decides (inteng breaks there): its place within 1 unit of the record's on each axis (the
	// level's file numbers or memory units, as (5) above) and *object is it; else *moved is set ("%s has moved since module file created", 0x4c907d) and the record is skipped. False
	// when no module has that name. Names are matched ignoring case, as (4) above.
	bool MlfFindModule(AutoLinkScene const &scene, MlfRecord const &record, int32_t *object, bool *moved);

	// The record's view as AutoLink's own views are: the camera's place = the record's view x 100 (0x4c8dc3-0x4c8dd8); its matrix = the record's nine numbers TRANSPOSED (FUN_004b3710
	// swaps 1/3, 2/6, 5/7 before the camera's +0x38 takes them, 0x4c8dd4-0x4c8e02; AutoLinkView.turn is +0x38). False when the place x 100 does not fit 32 bits.
	bool MlfView(MlfRecord const &record, AutoLinkView *view);
}
