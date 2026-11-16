// reverse_links.h: Module Linking's AutoLink > Add Reverse Links (Module 0x12c, Environment 0x12d) on the open level, EXACTLY as inteng's FUN_004c86b0 and
// FUN_004c60a0 do it (AvP-docs\engine\inteng\drake-autolink-facts.md section 1; menu-reads-2.txt, menu-reads-4.txt). Written 2026-11-15 by Melanikus (Drake)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES. For every module (all of them, or the picked ones) it makes the module the current one and walks ITS tree: for each node met (a linked
// module B), B is made the current module and the path from the walked module down to B is put into B's tree in reverse (module_links.h ReverseVisits and
// AddReversePath). Making a module current always marks its tree "changed" (0x4c14df), so every module touched is written when it is left: its VMDARRAY when
// it has links (and the module flag 0x200 goes into its object header), else only the flag comes out (0x4c10e2-0x4c1430).
//
// WHAT THIS DOES. The same on trees kept in memory: each touched module's tree is loaded from the level the first time it is needed (as inteng loads it
// whenever it is made current: the written bytes and a load give the same tree), changed in the order inteng changes it, and written ONCE at the end, in
// the order the modules were first touched. Each write is one ReplaceLevel (the game's own loader takes the level or nothing changes for that module).
//
// NAMED: (1) the modules are taken in the order of their object numbers, inteng's in the order of its object table; (2) inteng's progress text and its right
// click that stops the loop are not built; (3) inteng's walk reads the nodes of the source tree while it makes other modules current; if one of them is the
// source itself, inteng frees the very nodes it is walking (a crash); here the walk is on a copy; (4) a module with no VMDARRAY and no link is still
// "touched" when it is made current, so its module flag is taken out when it has one (see the facts file: this is the reading of FUN_004c10c0).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	enum class ReverseLinksResult
	{
		Done,
		NothingOpen,     // no level
		WriteFailed,     // a module's links could not be written: the ones written before stay, *why says which
	};

	struct ReverseLinksCounts
	{
		size_t walked;      // modules made current as sources
		size_t touched;     // modules whose tree was made current (sources and the modules they link)
		size_t changed;     // modules whose list of links is different from what the level held
		size_t linksAdded;  // links that were not there before, summed
		size_t written;     // modules whose links were written
		size_t skipped;     // modules with links that could not be written (no MODULEDT: inteng makes one; named)
	};

	// What an answer means, in plain words: static text, never null
	char const *ReverseLinksWords(ReverseLinksResult result);

	// Adds the reverse links. sources: the modules (object numbers) to walk, or null for every module. counts (not null) and why (not null) are filled.
	ReverseLinksResult AddReverseLinks(OpenLevelFile *open, std::vector<int32_t> const *sources, ReverseLinksCounts *counts, std::string *why);
}
