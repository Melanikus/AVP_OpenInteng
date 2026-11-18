// texture_shared.h: the Texture Map's SHARED calls, for every part of the mode that edits polygons (TM-A, texture-map-closeout-frost.md: Frost's; Dietrich's sub-mode
// frames, corners and Merge call them where inteng does). The pure maths they use, the merged pair as one quad (FUN_0055cdf0) and the wrap's move, are in
// src/model/texture_quad.h. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

#include <string>

// ----- the undo of the polygon screen (TM-A, ruling 5: as deep as inteng's) -----
// FUN_00573570 (the selection): before an edit to the selected polygons, a new undo step (FUN_0057e940) holds each of them as it is (FUN_00567ba0 for every selected
// polygon of every shape). Inteng calls it at the start of every edit command (FUN_0054d260's cases, FUN_00570f40, FUN_00570590, ...): call it at the same place.
// STUB: the undo ring (DAT_00914ca8, FUN_0057e5c0) is not built yet, so nothing is kept; the call is where it belongs, and the ring fills it in later.
void TextureUndo_RecordSelection();

// FUN_00567ba0: one polygon of the shape of object id (the file's SHPPOLYS number) and its partner into the step being made, for an edit to a polygon that isn't in
// the selection. STUB as above.
void TextureUndo_RecordPolygon(uint8_t const id[8], int32_t polygon);

// ----- FUN_00566b90: a polygon's texture coordinates brought onto its picture -----
enum class TextureWrapResult
{
	Moved,         // the coordinates (and the partner's) were moved by whole picture sizes and written
	AlreadyOn,     // the smallest u and v were on the picture already: nothing changed
	NotTextured,   // not a textured polygon as the game drew it (0x566bae: inteng's record says type 2 for one drawn without a picture): nothing changed
	NoPicture,     // the shape isn't in play, or the game has no size for its picture: nothing changed. Inteng reads its picture's record anyway
	Refused,       // the level, the polygon or the write was refused, or a corner is or would go past the texel rule: no coordinate changed (a partner already
	               // parted stays parted, as in inteng, which parts first); *why says which
};

// FUN_00566b90 (shape, polygon) on the open level, for the polygon of object id (isModule / moduleIndex as the selection holds them): when its partner (the merge data's,
// or the fake merge data's while that is on) is not textured or shows another picture, the two are parted first, as inteng parts them (0x566bf6-0x566c30, both -1 and
// FUN_00566d60 writes the merge data again: here PolyEdit_Unmerge, or the fake copy). Then the smallest u and v over the polygon (and the partner still with it) are
// brought into the picture's width and height, every corner moved by the same (model::TextureWrapUv), and written, the view shown at once. Inteng also sets its
// "texture changed" flag DAT_00914d18 and the shape's modified flag: here the write marks the level edited. Inteng runs it on every deselect (FUN_0056dfa0 /
// FUN_0056dea0), before each Anim step, first in Find invalid PSX UV and on leaving sub-mode 3.
TextureWrapResult TextureWrapPolygon(uint8_t const id[8], bool isModule, int32_t moduleIndex, int32_t polygon, std::string *why);
