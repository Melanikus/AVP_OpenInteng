// autolink_edit.h: Module Linking's AutoLink > Module (0xa1), Environment (0xa2), Module (delete old) (0xa5) and Environment (delete old) (0xa6) on the open level: inteng's FUN_004c8930 over
// the visibility pass of autolink_pass.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The facts: AvP-docs\engine\inteng\drake-autolink-facts.md. NAMED DEVIATION (owner-intent item 15): the pass is drawn by OUR rasteriser, so the links are what that pass sees, not what inteng's
// display saw; checked against the stock levels (the oracle in tests\host_autolink_pass_test.cpp).
//
// WHAT inteng DOES (FUN_004c8930, menu-reads-2.txt; FUN_004c7ab0, FUN_004c6fb0, dump-all-2.txt): for each module of the list (every module when none is given) that has a record, FUN_004c10c0(module,
// deleteOld) makes it the current module (with deleteOld the old tree is thrown away and the old VMDARRAY destroyed), FUN_004c7ab0 draws the sample views and puts every module seen into the tree
// (as a link added at the end: LinkTree::Normal), kept only when the gap between the two modules' boxes on each axis is within the visibility distance DAT_00704378 (0 keeps all). Every
// module made current is written when it is left (VMDARRAY and the module flag, as WriteTheLinks).
//
// WHAT THIS DOES: the same, on trees kept in memory and written once at the end (one image, one loader read: object_edit.h SetModuleLinksMany). NAMED: the modules are taken in the order of their object
// numbers; no progress text and no right click to stop; a module with links but no MODULEDT is skipped and counted; inteng's leaving out of a SHPMORPH module and a strategy of kind 1000 is not copied.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "module_links.h"

class OpenLevelFile;

namespace host
{
	enum class AutoLinkResult
	{
		Done,
		NothingOpen,
		WriteFailed,
	};

	struct AutoLinkCounts
	{
		size_t sources;     // modules whose pass was run
		size_t portals;     // pairs of modules that meet, summed
		size_t views;       // pictures drawn
		size_t linksAdded;  // links put in the trees
		size_t cut;         // links not put in because of the visibility distance
		size_t written;     // modules whose links were written
		size_t skipped;     // modules with links that could not be written (no MODULEDT)
	};

	char const *AutoLinkWords(AutoLinkResult result);

	// sources: the modules (object numbers) to run, or null for every module. visibleDistance: DAT_00704378 (metres x 100000), 0 for no limit. levelScale: the level's scale (ENVSDSCL).
	AutoLinkResult AutoLink_Run(OpenLevelFile *open, std::vector<int32_t> const *sources, bool deleteOld, double levelScale, int32_t visibleDistance, AutoLinkCounts *counts, std::string *why);

	// The write at a run's end (the modules' trees by object number; FUN_004c10c0's write when each was left): every tree with a link in ONE new image and ONE loader read, then the
	// module flag of each that went in. NAMED: a module left with no link is not written, also with "delete old" (a missing link can make a module vanish in game). Also used by
	// autolink_runner.h (Dietrich's threaded driver, 2026-11-16).
	AutoLinkResult AutoLink_WriteTrees(OpenLevelFile *open, std::map<int32_t, std::unique_ptr<LinkTree> > const &trees, size_t *written, size_t *skipped, std::string *why);
}
