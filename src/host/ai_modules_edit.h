// ai_modules_edit.h: the AI modules of Waypoint mode: Add Module (command 0x1a, FUN_00589310) and Remove Module (command 0x1b, FUN_00589570) on the open level. Written 2026-11-15 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Facts: AvP-docs\engine\inteng\drake-waypoint-facts.md.
//
// THE DATA (avpx/3dc/win95/chunk_waypoint.cpp): an AI module is a MASTER module (an AIMODMAS chunk in its MODULEDT, no data) and its SLAVES (an AIMODSLA chunk in theirs, the master's
// object number). The master's list of modules is made from the slaves when the level is read. The game's loader refuses a slave that names a module that is not a master or that is a
// slave itself. The waypoints of an AI module are the WAYPOINT chunk of its master.
//
// ADD MODULE (the game's own AI_Module_Master_Chunk::AddModule is the same code as inteng's FUN_00589310): the module x joins the master's list; when x is itself a master, every module of
// ITS list joins too (each given an AIMODSLA naming the new master, unless it already is there) and x's AIMODMAS is taken out; x gets an AIMODSLA naming the master; when x has a WAYPOINT
// chunk its waypoints are moved into the master's (made when the master has none) after the master's own, and x's chunk is taken out (when it had waypoints).
// REMOVE MODULE (FUN_00589570, first branch): the slave's AIMODSLA is taken out, so it is on its own again (a module that is not a master and not a slave).
//
// NAMED: (1) the master has to be a master or a module with no AI chunk at all (given an AIMODMAS, which inteng's Edit Module does before; here it is done by the Add); a slave is refused
// ("pick its master"); (2) Remove Module leaves the master's waypoints as they are (inteng's second branch of FUN_00589570, for a module with no AIMODSLA, is not read); (3) one image, one loader
// read for all the chunks of one command; the game's loader decides whether the result is a level it takes.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class OpenLevelFile;

namespace host
{
	enum class AiModulesResult
	{
		Done,
		NothingOpen,
		NotAModule,       // the master or the module is not a module of the level
		SameModule,
		AlreadyThere,     // the module is already in that master's list
		MasterIsASlave,
		NotASlave,        // Remove Module: the module is nobody's slave
		BadWaypoints,     // a WAYPOINT chunk could not be read
		WriteFailed,
	};

	struct AiModulesCounts
	{
		size_t slavesMoved;          // the modules of x's own list that joined the master (x was a master)
		size_t waypointsMoved;       // waypoints taken from x's chunk into the master's
		size_t chunksChanged;        // edits written
	};

	char const *AiModulesWords(AiModulesResult result);

	AiModulesResult AiModules_Add(OpenLevelFile *open, int32_t masterIndex, int32_t moduleIndex, AiModulesCounts *counts, std::string *why);
	AiModulesResult AiModules_Remove(OpenLevelFile *open, int32_t moduleIndex, AiModulesCounts *counts, std::string *why);

	// Entry points > Delete (FUN_0058a0e0, "Delete this entry point?"; the sides are made by FUN_0058a530): the AI module of module a (its master and every slave) and the AI module of module b:
	// every adjacency entry of a module of one that names a module of the other is taken out of its ADJMDLEP, both ways. *removed counts the entries taken out. Named: inteng picks the entry
	// point's marker in the 3D view; here the two modules are the ones the editor has picked (a, then b).
	AiModulesResult AiModules_DeleteEntryPoints(OpenLevelFile *open, int32_t a, int32_t b, size_t *removed, std::string *why);
}
