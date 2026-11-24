// level_objects.h: the open level's objects, as the game loaded them, for the object list (object_list.h). Written 2026-11-14 by
// Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHERE THE LEVEL COMES FROM: the editor's own open of it (OpenLevelFile, level_file.h), which the editor hands these lookups with
// LevelObjects_Use. That open holds the level file's bytes, and the chunks the game's own loader made from exactly those bytes (the sum
// the loader worked out from what it read is checked against the bytes' own, so the chunks are the bytes' chunks).
//
// It is NOT the game's own copy, because the game keeps none. As soon as a level has been copied into the game, copy_rif_data calls
// InitNPCs (projload.cpp:2852), and InitNPCs lets go of the level's file chunk ("at this point we no longer need the main level rif
// file", npcsetup.cpp:657-664, which calls unload_rif, chunk_load.cpp:720-731: delete h->fc; h->fc = 0). So once LevelView_Open is
// done, the game's env_rif (system.cpp:99) has no file chunk, and the first version of this module, which read it, found no objects in
// any real level: checkpoint 1's hidden run on 2026-11-14 said "object list: 0 rows". (Changed 2026-11-15 by Melanikus (Dietrich).)
//
// It reads the chunks the way the game's own loader does (projload.cpp:1468): walked in their own order. For each object it gives a
// pointer to its name's bytes and a copy of its 8-byte ID, which is what the game copies into a strategy's name (object_setup.cpp:
// `*(ObjectID *)xx->nameID = ob->object_data.ID`), so a pick in the view and a row of the list name the same object. The names are the
// level's own, hostile bytes: object_list.h reads them only through the safe text. Nothing is changed in the level.
//
// The pointers are good only while that level stays open, so the list copies them at once.

#pragma once

#include <stdint.h>

#include <vector>

#include "object_list.h"
#include "object_header.h"

class File_Chunk;      // the game's chunk library (mishchnk.hpp): the open level as it holds it
class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)
enum class LevelOpenResult;   // how that open went (src/io/level_file.h)

// Says which open level the lookups below read: the editor's own. Only its address is kept, and what it holds at the moment of each call
// is what's read, so a level opened in it later is the one read, and a level it has let go of is never. It has to stay alive for as long as
// it's given here: give null before it goes. Null, as it is at the start, means no level is open.
void LevelObjects_Use(OpenLevelFile const *open);

// THE LEVEL'S GENERATION: a number that steps every time LevelObjects_Use is called (a level given, opened again, or taken away). What was built from one level
// (the placement picker's list and its shape numbers) compares it with the number it was built under: another number, another level, even when the level was
// opened again into the same OpenLevelFile at the same address (Vasquez, 2026-11-23, Frost's lead: the picker stayed open across a reopen)
uint64_t LevelObjects_Generation();

// Holds the level LevelView has open, as the editor does when it opens a level: opens it in *open, by the path the game loaded it from
// (LevelView_FilePath), and hands it to the lookups (LevelObjects_Use). Gives back how the open went. Anything but Opened leaves *open
// holding no level, so the lookups find none. The editor and the stock proof both open levels through this, so what the proof checks on
// the game's own levels is what the editor does. A null *open gives back NoPath and hands the lookups none.
LevelOpenResult LevelObjects_Hold(OpenLevelFile *open);

// The open given to LevelObjects_Hold, to change: the edits that take an object out or add one (object_edit.h) work on it, and swap the
// level it holds, so everything the editor kept from the old one (the object list's rows, the field panel's record) is made again from the
// new one afterwards. Null when none was held, and after LevelObjects_Use. (Added 2026-11-15 by Melanikus (Drake), S5.)
OpenLevelFile *LevelObjects_OpenForEdit();

// The open level's objects, into objects (emptied first), at most one more than the list holds, so the list can say it was cut. Gives
// back false, with none, when no level is open.
bool LevelObjects_Read(std::vector<host::LevelObject> *objects);

// The header of the open level's object with that ID, copied into *header. When two objects share an ID (the ID comes from the name, so
// two of one name do), it's the first in the level's own order, the one the object list and a pick in the view find too. Gives back
// false, with *header as it was, when no level is open, no object has that ID, or the object has no header.
bool LevelObjects_FindHeader(const uint8_t *id, host::ObjectHeader *header);

// Every object of the open level whose shape is the shape of the object with that ID (Object_Chunk::get_assoc_shape, inteng's FUN_004d0fd0: its header's shape; the objects with
// no header share none), that object too, in the level's own order, into *objects (emptied first). For Strategy Data's collector, Ctrl + click (FUN_00526bf0 0x526e3b, S-79). When
// two objects share the ID, the first in the level's order is the one whose shape is taken. Gives back false, with none, when no level is open or no object of the level has that
// ID (a virtual object has none). (Added 2026-11-17 by Frost.)
bool LevelObjects_ShapeGroup(const uint8_t *id, std::vector<host::LevelObject> *objects);

// The open level, as the game's chunk library holds it (the editor's own open's file chunk, level_file.h), for lookups that walk it
// (strategy_lookup.h). Null when no level is open. It's good only while that level stays open.
File_Chunk *LevelObjects_Level();

// The place of every object of the open level, modules and placed objects alike, in the level's order and the file's own units (its header's
// location), three numbers an object, into *places (emptied first). False, with none, when no level is open. (Added 2026-11-17 by Drake: the
// rotation centre's average, camera-closeout-drake.md CAM-7.)
bool LevelObjects_Places(std::vector<int32_t> *places);

// The file place of the open level's object with that ID (the first in the level's order with it). False, with place as it was, when no level is
// open or no object has that ID. (Added 2026-11-17 by Drake: Scroll Lock's pick of the rotation centre, CAM-8.)
bool LevelObjects_PlaceOf(const uint8_t *id, int32_t place[3]);
