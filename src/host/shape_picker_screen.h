// shape_picker_screen.h: THE PLACEMENT PICKER on the screen (shape_picker.h says what it is and why): opened by Object Placement > Create > With shape > Choose,
// in place of inteng's one-at-a-time Shape Chooser (Replace shape keeps that one), over the view as the texture chooser's grid is (texture_map_screen.cpp), its
// tiles drawn a few a frame by the picker's own renderer (shape_thumb_render.h), kept by each file's SHA-256 for the sitting (at most kShapePickerMostKept), and
// its words on the state line for the harness (picker, pickerboxes, pickertiles, pickernotes). Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// EVERY FILE IS HOSTILE (Tao 3): each folder judged as the lookup order judges it (shape_search.h), each file read on one handle by ShapeImport_Read (a plain
// file, one name, at most kShapeImportMostBytes), made ready by shape_thumb.h (every count and corner checked, at most 16,384 points and polygons), its pictures
// read as the level's own are (shape_picker_pictures.cpp). The work a frame is bounded: at most kShapePickerDrawsAFrame pictures, and none once
// kShapePickerFrameMs has gone.
#pragma once

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace host
{
	struct ShapeThumbPicture;
	struct ShapeThumbTexture;
}

size_t const kShapePickerMostKept = 256;      // drawings kept for the sitting (each 96 x 96 x 4 bytes, and a texture of the game's): about 9 MB of each
int const kShapePickerDrawsAFrame = 3;         // pictures drawn in one frame at most
unsigned const kShapePickerFrameMs = 40;       // and no more once this much of the frame has gone on them

bool ShapePickerScreen_IsOpen();

// Opens the picker: the lookup order's folders listed, the level's outside shapes' file names marked, the selection on the first tile. False, with the words on
// the status line, when no level is held to work on
bool ShapePickerScreen_Open();

// Closes it, the drawings kept for the next time (Escape: the words say no shape was chosen)
void ShapePickerScreen_Close(bool sayNothingChosen);

// Each frame while it is open (inside the game's frame, as TextureChooserDraw): the grid laid out and a few tiles drawn. ownWidth, ownHeight: the frame in the
// editor's own pixels (text_scale.h)
void ShapePickerScreen_Frame(int32_t ownWidth, int32_t ownHeight);

// THE PAINT, apart from the work (Vasquez 2026-11-23, for Dietrich's overPanels hook: the owner's "Gray thingie over my scroll bar!", the object card kept in
// inteng's look UNDER the picker): the tiles, their names, the selection's yellow frame, the scroll bar and the picker's words added to the frame's panels, as
// ShapePickerScreen_Frame laid them out this frame (its own size, whatever the caller gives: kept so a hook's signature fits). Call it after the frame's work, at
// the place in the panels' pass where the picker must stand: over the card and the object list, under the bar, its drop-downs and the dialogs.
void ShapePickerScreen_Paint(int32_t width, int32_t height);

// A key while it is open: the arrows, Page Up / Page Down, Home / End move the selection; Return places the selected tile; Escape closes it. True: the key is the
// picker's (every key is while it is open)
bool ShapePickerScreen_Key(WPARAM key);

// A left press at (x, y), the editor's own pixels: on the scroll bar, an arrow moves a row, the track above or below the thumb a page, and the thumb is taken
// to be dragged (shape_picker.h ShapePicker_Bar); on a tile not selected, it is selected; on the selected one, it is placed (as Return)
void ShapePickerScreen_Click(int32_t x, int32_t y);

// The mouse moved to y (the editor's own pixels) while the left button is down: when the thumb is held, the rows follow it. True when it is held
bool ShapePickerScreen_Drag(int32_t y);

// The left button let go: the thumb, if held, is let go. True when it was held
bool ShapePickerScreen_Release();

// The mouse wheel: notches as Windows gives them (WHEEL_DELTA each; up is positive): a row a notch, up is back
void ShapePickerScreen_Wheel(int delta);

// The picker's fields on the state line (StateLine_Add), every frame: picker "open cols A rows B count N drawn D failed F first X selected <name>" (or
// "closed"), pickerboxes "<name>~<x> <y> <w> <h>|..." (the tiles on the screen, their pictures, in the WINDOW's pixels), pickertiles (those tiles' words,
// shape_picker.h ShapePicker_TileWords, '|' between), pickernotes (the folders' words), pickerscroll (the scroll bar's parts, "up~...|down~...|above~...|
// below~...|thumb~...", in the window's pixels), pickerdrag ("held" while the thumb is held, else "free"), pickershown (the tiles on the screen as the
// paint shows them, "shown S pictured P waiting W words X": with their drawing, still drawing, or words only), pickerleftout (the files left out as the
// picker opened, no geometry in them: "<name>|...") and pickerfolded (each copy folded into the tile it looks the same as: "<tile>~<copy>|...";
// shape_picker.h ShapePicker_Thin)
void ShapePickerScreen_StateFields();

// A picture of a tile's shape (shape_thumb.h's path under the game's graphics folder) read and decoded as the level's own pictures are (shape_picker_pictures.cpp).
// False with *why when it can't be. tileBytes: what this tile's pictures already hold (4 bytes a pixel), added to when this one is read; a picture that would take
// the tile past kShapePickerMostTileBytes is not read (one stranger's shape can name many large pictures).
uint64_t const kShapePickerMostTileBytes = 32ull * 1024 * 1024;   // two of RimLoader's largest (2048 x 2048); a stock picture is 128 x 128 (64 KB)
bool ShapePickerScreen_ReadPicture(host::ShapeThumbPicture const &picture, host::ShapeThumbTexture *out, std::string *why, uint64_t *tileBytes);

// THE PICKER'S ONE ALLOWANCE for its pictures (the game's RimLoader.h RimLoaderToolAllowance; Bishop 2's ruling on Drake's review of #293: one a visit, never one
// a picture): begun as the picker opens, ended as it closes, the open level's own count put back then. While none is held, no picture is read.
void ShapePickerPictures_Begin();
void ShapePickerPictures_End();
