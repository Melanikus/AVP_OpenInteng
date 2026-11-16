// path_mode.h: inteng's Path mode (Construction > Mode > Path, command 0xce) data effects on the level, as plain functions. Written 2026-11-15 by Melanikus (Frost)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES is in AvP-docs\engine\inteng\path-mode-facts.md (every line cited from dump-all-3.txt). The menu of the mode is not wired: Vasquez pairs it.
//
// WRITE A PATH (FUN_004fab40, dump-all-3.txt:25699-25912): in REBENVDT > SPECLOBJ (SPECLOBJ is made, last in REBENVDT, when it is missing: FUN_00404560 at 0x4fab9e),
// the FIRST AVPPATH2 whose number is the path's is destroyed; with no node that is all; else a new AVPPATH2 goes LAST in SPECLOBJ holding the name, the number, the flags,
// the spare number and the nodes in order (path_chunk.cpp: name padded to 4 with its end, four numbers, then 24 bytes per node: module, place, flags, spare).
// The places here are FILE units (inteng converts the editor's with FUN_0042f9b0 and the level's scale before it writes; the caller does that).
//
// NEXT FREE NUMBER (FUN_004fb300 0x4fb62a-0x4fb666): inteng walks the level's AVPPATH2 in FILE ORDER with n = 0, a chunk whose number is n makes n go up by one, any other chunk is passed; with the
// chunks out of order that returns a number a path ALREADY HAS (stock Breakout: 2) and Create Path replaces that path. NAMED DEVIATION (Melanikus' ruling 2026-11-15): PathMode_NextNumber
// returns the smallest number no path uses. Card: AvP-docs/engine/inteng/unfinished-in-inteng.md, "Path mode".
//
// NAMED POINTS: inteng does not check the path number; the game's loader refuses one outside 0..255 (path_chunk.cpp), so this refuses it (BadNumber) rather than write a
// level the game won't load. The change is made as one new image read by the game's loader first (as object_edit.h): if the loader won't take it, nothing has changed.
// THE WORKING PATH (the editor's own list of nodes; Add / Delete last module, FUN_004fa6a0 and FUN_004fa950, path-mode-facts.md):
// ADD (key L): with an empty path the picked module is the one node; else the modules BETWEEN the last node's module and the picked one on a breadth-first route over the
// module adjacency (FUN_004f8c10 on the graph FUN_004f89f0 searched from the last node) become nodes with flags 1 (put in by the route), then the picked module with flags 0.
// All of a new node's numbers but the module and the flags are 0 (FUN_004fbe70 zeroes the node): a path made in inteng has places 0,0,0.
// READING (named, F-PATH-ROOT): the search's hidden store gives the root depth 0, so a module next to the root has depth 1 and no module between (the route loop runs while
// the depth is above 1 and pushes each module's parent); an unreachable module (depth -1) gives no module between. Confirm with the raw 0x4f89f0 when listing-all lands.
// The tie-break is the search's own: modules in the order of the adjacency lists (the graph FUN_004f8240: each ADJMDLEP entry's first matching module).
// DELETE LAST (key D): removes the last node, then every earlier node put in by the route (flags & 1) until a node that was not.
// BACK AND FORTH (case 9, "Path Properties": "Loop path" / "Go back and forth"): the flags' bit 2 is set when the "Loop path" choice is not the one picked, else cleared; bit 1 is not touched.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

class OpenLevelFile;

namespace host
{
	struct PathNode
	{
		int32_t module;     // the module's number in the level (inteng: the module chunk's +0x98)
		int32_t place[3];   // FILE units
		int32_t flags;      // bit 0: put in by the route (inteng), others as the node had them
		int32_t spare;
	};

	enum class PathModeResult
	{
		Done,
		BadCall,        // no level, or a null node list with a count
		BadNumber,      // not 0..255
		NoEnvironment,  // the level has no REBENVDT
		NothingToDo,    // no node, and no chunk with that number: the level stays as it is
		TooMany,        // more nodes than a chunk can hold
		CantBuild,      // the level couldn't be put together from its bytes
		LoaderRefused,  // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *PathModeWords(PathModeResult result);

	// The most nodes one path holds here (a hostile count is refused before anything is built)
	size_t const kPathModeMostNodes = 100000;

	// Writes the path as above. name: null or empty = no name. count 0 = delete the path's chunk.
	PathModeResult PathMode_Write(OpenLevelFile *file, int32_t number, char const *name, int32_t flags, int32_t spare, PathNode const *nodes, size_t count);

	// The module adjacency the route searches: neighbours[i] = the module indexes next to module i, in the adjacency lists' order
	struct PathGraph
	{
		std::vector<std::vector<int>> neighbours;
	};

	// THE GRAPH FROM THE LEVEL (FUN_004f8240, dump-all-3.txt:23728-23898): modules = the objects with the module bit (header flags & 0x200) and a MODULEDT, in the file's order; module i has
	// numbers[i] = its object number (OBJHEAD1 payload +48: inteng's chunk +0x98) and name[i]; its neighbours are, for each entry of its ADJMDLEP (a count, then 20 bytes each: the
	// neighbour's object number, flags, the entry point), the FIRST module with that number (an entry with no such module is passed over). False when the level can't be walked.
	bool PathMode_BuildGraph(OpenLevelFile *file, PathGraph *graph, std::vector<int32_t> *numbers, std::vector<std::string> *names);

	// The modules between module "from" and module "to" (indexes, in order from "from" outward), by the breadth-first search above. False for a bad index.
	bool PathMode_Route(PathGraph const &graph, int from, int to, std::vector<int> *between);

	// The path's module NEAREST module "start" (FUN_004f9650, used by an Enemy's / Generator's view end, S-K69-end): the breadth-first distances from start over the graph (FUN_004f89f0); over the
	// modules in the graph's order, one closer than the best so far (from 1,000,000) that is the FIRST module with a step's number (FUN_004beb90) becomes the answer (ties: the earlier). Gives
	// its graph index, or -1 (a bad start, no steps, or no step reachable).
	int PathMode_NearestStep(PathGraph const &graph, std::vector<int32_t> const &numbers, int start, std::vector<PathNode> const &steps);

	// Add the picked module to the working path. numbers[i] = the module chunk number of module i (PathNode::module). lastIndex = the last node's module index (ignored when the path is empty).
	bool PathMode_AddModule(std::vector<PathNode> *path, PathGraph const &graph, std::vector<int32_t> const &numbers, int lastIndex, int pickedIndex);

	// Delete the last node and the route's nodes before it.
	void PathMode_DeleteLast(std::vector<PathNode> *path);

	// The path's flags after "Path Properties": loopChosen = the "Loop path" choice was the one picked.
	int32_t PathMode_FlagsAfterProperties(int32_t flags, bool loopChosen);

	// THE LEVEL'S PATHS (the chooser of Select Path, FUN_004fb6a0, and the load FUN_004faf40): every AVPPATH2 of SPECLOBJ in file order, as the game's loader reads one (path_chunk.cpp: name padded
	// to 4 with its end, then number, flags, spare, the node count, then 24 bytes a node). A chunk too short for its own node count is cut to the nodes it holds. At most kPathModeMostPaths.
	struct PathInfo
	{
		int32_t number;
		int32_t flags;
		int32_t spare;
		std::string name;
		std::vector<PathNode> nodes;
	};

	size_t const kPathModeMostPaths = 4096;

	bool PathMode_ReadPaths(OpenLevelFile *file, std::vector<PathInfo> *paths);

	// The number a new path gets, as above. *number is the answer; false when the level can't be walked.
	bool PathMode_NextNumber(OpenLevelFile *file, int32_t *number);
}
