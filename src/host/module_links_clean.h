// module_links_clean.h: Module Linking's start takes every link that names no module out of the level's links (inteng's FUN_004c90c0, called by the screen's start FUN_004c3a00 at
// 0x4c3a15; row ML-E-12 of AvP-docs\engine\inteng\reassess\module-linking-map-dietrich.md, read on dump-full-2 64988-65200). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES: the level's objects (FUN_004be4f0) less the placed ones (+0x2c & 0x400) are the modules. For each module with a MODULEDT and a VMDARRAY (FUN_00431a00), its items in
// order: an item whose object index (+0xc) names one of those modules (FUN_004beb90, then the list) is KEPT and packed down to the next free slot (FUN_00430320); one that names none is
// DROPPED, and every item's branch_no above the number kept so far is lowered by one (0x4c9244-0x4c925e: all of the array, the items not yet read too). Then every kept item's branch_no
// above the number kept is held to it (0x4c928e-0x4c92a2: so a branch_no past the end, even with nothing dropped, is brought back). The count becomes the number kept, or with none
// kept the VMDARRAY is destroyed (0x4c92ab-0x4c92f5). Anything dropped: the object list is drawn again (FUN_004719e0, 0x4c9338).
// So a link to a placed object, or to an object index the level no longer has, never reaches the link tree or the game.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "module_links.h"

class OpenLevelFile;

namespace host
{
	// FUN_004c90c0 on ONE module's items (isModule: whether an object index names a module). True when the items changed (an item dropped, or a branch_no held to the count); with every
	// item dropped *items is empty (the VMDARRAY goes). *dropped (when not null) counts the items dropped.
	bool ModuleLinks_DropDangling(std::vector<LinkItem> *items, std::function<bool(int32_t)> const &isModule, size_t *dropped);

	struct ModuleLinksCleaned
	{
		size_t modules = 0;        // modules whose links changed
		size_t itemsDropped = 0;   // links taken out
		size_t emptied = 0;        // modules left with no link (their VMDARRAY destroyed)
	};

	// FUN_004c90c0 on the whole open level: each module's links read, cleaned, and the ones that changed written in one image (SetModuleLinksMany; an emptied one by DropModuleLinks).
	// False with *why when a write was refused (nothing of that write changed). Nothing is written when nothing changed.
	bool ModuleLinks_CleanLevel(OpenLevelFile *open, ModuleLinksCleaned *done, std::string *why);
}
