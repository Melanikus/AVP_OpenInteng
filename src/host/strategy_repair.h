// strategy_repair.h: Strategy Data's REPAIR PASS (FUN_00536ad0: every record's strategy method +0x1c) and DELETE MULTIPLE's "Obsolete Switches" (FUN_00537030), both read from inteng.exe. Written 2026-11-16 by Melanikus (Dietrich) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RECORDS. inteng keeps an array of records (DAT_00914954), one for each object of the level and each virtual object: record +8 is its type (0 a placed object, 1 a virtual object), +0x14 its object data and +0x30 its strategy.
// "A record exists" for a target (FUN_005219f0 on an object ID, FUN_00529570 finding it): the ID is not null, a record has it, and that record is a virtual object, or a placed object that has a strategy. (FUN_005219f0's last branch, a
// placed object with no strategy whose shape has SHPMORPH, asks about the SELECTED object, FUN_004d0fd0, not the record's: it is not done here, named.)
//
// THE REPAIR (method +0x1c of each strategy class; every other class has the base class's, which does nothing: vtable +0x1c is 0x401b30). A target that no record answers to is reset to the null ID and its "turn on / off" number to 0:
//   BinSwitch (52) and LinkSwitch (61) FUN_0052dc20: the Switch's target (ID at 36) and request (44);  Placed Object (62) FUN_005315b0: Event Target (ID at 44) and target_request (40);  Enemy (69) FUN_00531cd0: Death target (ID at 28)
//   and request (24).  NAMED, NOT DONE: the repairs that take the dead entries out of a LIST and make the strategy shorter (MultiSwitch 66 and AreaSwitch 68 FUN_00530820, Mission Objective 70 FUN_005335f0, the two Track kinds 72 and 76
//   FUN_00534450, Track Destructable 80 FUN_005354a0): they change the chunk's size, which the level edit store does not do for a strategy yet. Neither Find Errors' answer nor Obsolete Switches depends on them (a dead entry adds
//   nothing to a closure either way).
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
