// placement_hierarchy_fly.h: F on a PLACED HIERARCHY in Object Placement (reassess/object-placement.md OP-85 / V-6; reassess/placed-hierarchy-drake.md). Written 2026-11-17 by Melanikus (Drake)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// inteng: every special class has an object block +4 (FUN_004e4ae0); a placed hierarchy's is made from its PLACHIDT's place (0x4e8d31) and turned by its quaternion (FUN_004b3c80 0x4e8e45),
// and the Hierarchy player lays the model out from that block each frame (FUN_004e9cb0 -> ... FUN_0047ef70). F (vtable +0x48 = 0x4def60, the same for every class) flies block +4: the start
// gives it the engine's mover (+0x1c4 = 2, 0x4def7b), the stop writes nothing (0x4defe7); the class's save FUN_004e96d0 writes its place (block +0x10..0x18 / the scale, truncated,
// 0x4e96f1-0x4e971e) and turn into the PLACHIDT. Ours writes the place when F stops (as ours does for every object), through Vasquez's PlacedHierarchy_WritePlace (special_objects_edit.h),
// the turn kept. NAMED: the turn keys do not turn a flying hierarchy (inteng's mover turns its block; the turn written back would be FUN_004de070 of its matrix).

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class OpenLevelFile;

// The placed hierarchy's place in the world (its PLACHIDT's place x the level's scale), as the hub and Find put it (PlacedHierarchySelect, PlacementFind). False when there is no such one.
bool PlacedHierarchyFly_World(OpenLevelFile *open, size_t number, double scale, int32_t world[3]);

// F's stop: the hierarchy's place (file units) written into its PLACHIDT, its turn as it was; the status words. The level is swapped (the game's loader took it): the caller's list of placed
// hierarchies is read again on the generation's change.
std::string PlacedHierarchyFly_Stop(OpenLevelFile *open, size_t number, int32_t const place[3]);
