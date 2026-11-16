// inteng_help.h: the old editor's help texts, as inteng.exe holds them (inteng_help.cpp says where each comes from). Written 2026-11-15 by
// Melanikus (Dietrich) for AVP_OpenInteng (A6). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#pragma once

// Construction's Keys - Mode: the mouse and keyboard controls of the construction screen, with inteng's own tabs and new lines
extern const char kIntengConstructionKeysText[];

// Module Linking's Help: the keys of the Module Linking screen (inteng's own words, spelling included)
extern const char kIntengLinkHelpText[];
extern const char kIntengPlacementKeysText[];

// Texture Map's Help: General (0x27) and Movement (0x28), inteng's own words
extern const char kIntengTextureMapHelpText[];
extern const char kIntengTextureMapMovementText[];

// Strategy Data's Help (0x36, FUN_00522950): the 0xcf words (829 letters) at 0x5ed824, shown by FUN_004724f0
extern const char kIntengStrategyHelpText[];

// Light Sourcing's Help > General: the general keys, then the light states (two texts shown one after the other)
extern const char kIntengLightHelpGeneralText[];
extern const char kIntengPathKeysText[];

// Waypoint mode's Help (0x15), one text for each of its modes, each shown by FUN_004724f0: AI modules (FUN_00584e70, 0x602720), waypoints (FUN_00584ea0, 0x6027f0), extents (FUN_00585720, 0x602978)
extern const char kIntengWaypointModulesHelpText[];
extern const char kIntengWaypointWaypointsHelpText[];
extern const char kIntengWaypointExtentsHelpText[];
extern const char kIntengLightHelpStatesText[];
