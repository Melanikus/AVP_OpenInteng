// texture_shared.h: the Texture Map's SHARED calls, for every part of the mode that edits polygons (TM-A, texture-map-closeout-frost.md: Frost's; Dietrich's sub-mode
// frames, corners and Merge call them where inteng does). The pure maths they use, the merged pair as one quad (FUN_0055cdf0) and the wrap's move, are in
// src/model/texture_quad.h; the undo ring itself is src/model/undo_ring.h. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

class OpenLevelFile;
struct TexturePolygon;

namespace model
{
	struct PolygonUv;
}

// ----- the undo of the polygon screen's edits (TM-A, ruling 5: as deep as inteng's; texture-map-closeout-frost.md "RINGS' RECORDS READ") -----
// The EDITS ring (inteng DAT_00914ca8, FUN_0054be10: 25000 records, model::UndoRing). Each record is one polygon as it was: its partner in the REAL pairing (the file's
// merge data, also while the fake copy is on: FUN_00567ba0 reads DAT_00914d48 then), its type, flag word and colour word (its picture and list numbers), and its list
// of texture coordinates. Its undo swaps the partner back into the real pairing only when the pairing IN USE differs from it (FUN_00567ac0 0x567af2), so an Unmerge
// made in the fake copy is not undone (DECIDED: inteng's way; the fake copy is the mode's scratch). Inteng's record (FUN_00567ba0, 0x38 bytes)
// holds the same things as its own texture record keeps them; ours is 18 whole numbers (the same 25000 records, a larger slot: NAMED).

// FUN_00573570 (the selection): before an edit to the selected polygons, a new undo step (FUN_0057e940) holds each of them as it is (FUN_00567ba0 for every selected
// polygon). Nothing with no selection. Inteng calls it at the start of every edit command (FUN_0054d260's cases, FUN_00570f40, FUN_00570590, ...: the 65 places
// of reassess/texture-map-undo-sites.md): call it at the same place.
void TextureUndo_RecordSelection();

// FUN_00567ba0: one polygon of the shape of object id (the file's SHPPOLYS number) into the step being made (a new step only when BeginStep was asked: inteng's
// callers outside FUN_00573570 call FUN_0057e940 themselves). Nothing while an undo or redo runs.
void TextureUndo_RecordPolygon(uint8_t const id[8], int32_t polygon);

// FUN_0057e940 for a caller that records polygons one by one (TextureUndo_RecordPolygon): the next record starts a new step
void TextureUndo_BeginStep();

// 0x36 (FUN_0057e6d0 on the edits ring) / 0x37 (FUN_0057e770): the newest step undone, or the newest undone step done again. Each polygon of the step takes the
// state its record holds and the record takes what the polygon had (FUN_00567ac0), so the same record redoes it; after the step the pairing of every shape whose
// pairing changed is written again (FUN_00567c10 -> FUN_00566d60). Inteng says nothing; here the status line says how many polygons came back (NAMED: the owner,
// users are told). False when there was nothing to undo or redo.
bool TextureUndo_Undo();
bool TextureUndo_Redo();

// The edits ring emptied and made again (FUN_0057e630, at the mode's entry 0x54bcbe)
void TextureUndo_Reset();

// ----- the undo of the SELECTION (the ring DAT_00914c10: FUN_0054bf80, 100000 records, model::UndoRing; reset at entry 0x54bcc8) -----
// Every frame of the mode starts a step (FUN_00543fa0 0x543fa5: FUN_0057e940), so what one frame's clicks and keys select or deselect is one step. Each polygon
// added to the selection is recorded as selected (FUN_00575070 -> FUN_00567a80: [0] 0) and each taken out as deselected (FUN_005750e0 / FUN_00575170 ->
// FUN_00567aa0: [0] 1), with its object and polygon. 0x38 (Shift+,) undoes the newest step and 0x39 (Shift+.) does it again: each record's polygon is deselected
// or selected back and the record turned the other way (FUN_00567a30). NAMED: the status line says how many; inteng's deselect also brings the polygon's
// coordinates onto its picture (FUN_0056dea0 0x56df12, the wrap), which comes with the deselect's own wrap (TM-A). The record is our 6 whole numbers.
void TextureSelectionUndo_BeginStep();   // every frame while the mode is on (editor_main.cpp)
void TextureSelectionUndo_Record(TexturePolygon const &one, bool selected);
bool TextureSelectionUndo_Undo();
bool TextureSelectionUndo_Redo();
void TextureSelectionUndo_Reset();

// Both rings emptied: a command that changes a shape's polygon NUMBERS (polygons taken out) calls it, as a record names its polygon by number. NAMED: inteng keeps the
// records, and an undo then puts one onto another polygon
void TextureUndo_ForgetAll();

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

// FUN_0056dfa0 for every shape (FUN_0056e0a0, "Clearing list"): TextureWrapPolygon on each of `polygons` (and the partner still with it), each shape read once and
// written once (a list another polygon shares refuses a shape's whole write, so then each polygon is written alone). Gives back how many polygons were moved.
size_t TextureWrapPolygons(std::vector<TexturePolygon> const &polygons);

// Texture Alignment (I, 0x34, FUN_0055eeb0) waiting for a click: Esc stops it (inteng: the right button). True when it was waiting.
bool TextureAlignCancel();

// ----- THE MODIFIED OBJECTS (inteng's (DAT_00914dcc+8)[slot], read by Return's FUN_00572440 and its shared-shape question FUN_0055aa50) -----
// The objects a write has been noted for since the save point: TextureUndo_RecordSelection and TextureUndo_RecordPolygon (every built command records first), and
// ShowTheNewUv (every texture-coordinate write of the polygon screen, the wraps too). In the order first noted; each the object's 8-byte ID as a string. NAMED: the
// sub-modes' own writes (texture_submodes.cpp) are not noted, so Return does not ask about a shared shape only they changed (it stays written for every user, as All).
void TextureModified_Note(uint8_t const id[8]);
void TextureModified_Forget(uint8_t const id[8]);
void TextureModified_Clear();
std::vector<std::string> TextureModified_List();

// The type, flags, colour and texture coordinates of every polygon of object fromId's shape in `from` against object toId's shape in `to`: Same = all alike (the same
// polygon count); Copy = written into `to` (refused, with *why, when the counts differ: the polygon numbers moved). The pairing is not part of it. FUN_0055aa50's copy
// path: the shape the other users keep goes back to what the last save held (`from` = the level as saved).
bool TextureShapeFields_Same(OpenLevelFile *from, uint8_t const fromId[8], OpenLevelFile *to, uint8_t const toId[8]);
bool TextureShapeFields_Copy(OpenLevelFile *from, uint8_t const fromId[8], OpenLevelFile *to, uint8_t const toId[8], bool *showAgain, std::string *why);

// ----- the mode's own, shared between its files (texture_map_screen.cpp makes them) -----
int32_t TexturePartnerOf(OpenLevelFile *open, uint8_t const *id, int32_t polygon);   // the pairing in use: the fake merge data's while on, else the file's
void TextureFakePairSet(OpenLevelFile *open, uint8_t const *id, int32_t polygon, int32_t partner);   // the fake copy's pairing (only while it is on)
bool TextureFakeMergeOn();
void ShowTheNewUv(OpenLevelFile *open, TexturePolygon const &lead, std::vector<model::PolygonUv> const &polygons);   // the view's live preview of a UV write
