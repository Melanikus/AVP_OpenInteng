// placement_special_alt.h: Object Placement's Alt Locations on the CURRENT SPECIAL object (reassess/object-placement.md OP-34..38, OP-91 for specials; V-28): the items and , / . do to a
// generator, a bad guy, a sound or a particle generator what they do to a placed object (editor_main.cpp AltLocationCommand), on its own ALTLOCAT (special_alt_edit.h). Written 2026-11-17 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// inteng: every class's vtable has the alternate locations (FUN_004df820 Add new, FUN_004df6b0 Delete Current, FUN_004df8c0 Next, FUN_004df990 Prev, FUN_004dfa60 Assign Group Number,
// FUN_004dfb90 Change shown location), kinds 0, 1, 2, 3 and 7 only (FUN_004df820 :8141); the current object (DAT_008f4f3c) is the special object just made or found, a placed hierarchy, else the
// selected placed object. A player start has none: its items do nothing.

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

class OpenLevelFile;

// Add new (0x64), Delete Current (0x65), Next (0x67), Prev (0x68) on the current special object: true when there is one (the command was its, done or refused with words), false when
// none is current (the placed object's path goes on)
bool SpecialAlt_Command(int32_t command);

// Assign Group Number's window starts at the current special's group: true and *group when one is current
bool SpecialAlt_CurrentGroup(int64_t *group);

// The number typed into that window for the current special (FUN_004dfa60: 0 or more): true when one is current
bool SpecialAlt_AssignGroup(int64_t typed);

// Change shown location (all objects, 0x69, FUN_004dfb90): every special object with locations shows the typed one; gives back how many did
size_t SpecialAlt_ChangeShown(OpenLevelFile *open, int64_t typed);

// Object Placement's text for a special (FUN_004df3c0 0x4df3cb-0x4df40e): "Location %d/%d" (the shown one from 0, the count) with 2 or more locations, then "Location group %d" when the
// group is not 0; none otherwise. kind is host::SpecialKind as an int (so this header needs no other).
std::vector<std::string> SpecialAlt_ShownLines(OpenLevelFile *open, int kind, size_t number);

// A level opened again or changed by another edit: the lists kept here are read again from it
void SpecialAlt_Forget();
