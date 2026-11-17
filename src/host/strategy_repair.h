// strategy_repair.h: Strategy Data's REPAIR PASS (FUN_00536ad0: every record's strategy method +0x1c) and DELETE MULTIPLE's "Obsolete Switches" (FUN_00537030), both read from inteng.exe. Written 2026-11-16 by Melanikus (Dietrich) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RECORDS. inteng keeps an array of records (DAT_00914954, FUN_00521cb0), one for each object of the level, each virtual object, enemy, generator, sound, placed hierarchy and particle generator
// (and the cut-scene heads and traps of Rebellion's other games): record +8 is its type (0 an object, 1 a virtual object, 2 a generator, 3 an enemy, 5 a sound, 7 a placed hierarchy, 8 a particle generator,
// 9 / 10 the other games'), +0x14 its object data and +0x30 its strategy. "A record exists" for a target (FUN_005219f0 on an object ID, FUN_00529570 finding the FIRST record with it): the ID is not null, a
// record has it, and that record is not an object's (type not 0), or is an object that has a strategy, or is an object with none whose OWN shape has SHPMORPH (FUN_005219f0 0x521a28-0x521a43:
// ECX = the record's +0x28, its object, into FUN_004d0fd0, a thiscall giving its OBJHEAD1's shape, +0x4c; then FUN_00431a00 "SHPMORPH". Read before 11-17 as the SELECTED object's, from the
// decompile, which hides ECX: OUR BUG, FIXED by Vasquez 11-17; 343 stock objects, mostly doors, are such targets). The cut-scene heads (CUTSHEAD, 9) and traps (MUMMTRAP, 10), and MUMSOUND
// (game number 4 only, FUN_0057aa00), are in no stock level (spec_survey 11-17: 0 of 63): not read here, named.
//
// THE REPAIR (method +0x1c of each strategy class; every other class has the base class's, which does nothing: vtable +0x1c is 0x401b30). A target that no record answers to is reset to the null ID and its "turn on / off" number to 0:
//   BinSwitch (52) and LinkSwitch (61) FUN_0052dc20: the Switch's target (ID at 36) and request (44);  Placed Object (62) FUN_005315b0: Event Target (ID at 44) and target_request (40);  Enemy (69) FUN_00531cd0: Death target (ID at 28)
//   and request (24).  The repairs that take the dead entries out of a LIST and make the strategy shorter: Mission Objective 70 FUN_005335f0, MultiSwitch 66 and AreaSwitch 68 FUN_00530820 (Vasquez 11-17: the same walk as 70's, RepairEntryList with the count at 36), the Track kinds 72 and 76 FUN_00534450, Track Destructable
//   80 FUN_005354a0 (strategy_repair.cpp says how). Neither Find Errors' answer nor Obsolete Switches
//   depends on them (a dead entry adds nothing to a closure either way). The cut-scene heads and traps are left out of the records (NAMED: the game's library does not read their chunks).
//
// OBSOLETE SWITCHES (FUN_00537030, which starts with the repair pass): for each VIRTUAL object whose strategy is a switch (kinds 52, 61, 66, 68: FUN_005218b0), the closure of the objects it switches is walked: the object itself, then, for
// each member, the objects it targets (52 and 61: the one target; 66 and 68: every target in its list, 12 bytes each). A member that is not a virtual object with a switch strategy makes the walk give up, and the object stays; when every
// member is, the object is destroyed (as Destroy Virtual Object does).
#pragma once

#include <stddef.h>

class OpenLevelFile;

namespace host
{
	// The repair pass: how many strategies it changed. Nothing is written when it changes none.
	size_t StrategyRepair_Run(OpenLevelFile *open);

	// Obsolete Switches: how many virtual objects it destroyed. *failed (when not null) says a destroy was refused (what was done stays).
	size_t StrategyRepair_DeleteObsoleteSwitches(OpenLevelFile *open, bool *failed);
}
