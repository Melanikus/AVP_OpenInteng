// shape_picker.h: THE PLACEMENT PICKER, its pure part: which tiles it shows, where each one is on the screen, what a key, a click or the wheel does, and its
// words for the state line. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT IS (the owner's wishlist item 8, 2026-11-23: "placeables ... will show in a AxB column little 3d thumbnails to pick and place"; "The little 3d view
// picker shows that OR the shape_rifs, it's meant to be used when we're placing objects"; his answers: "Grid replaces it", "With textures now"; Bishop 2's spec
// v1 with Vasquez's changes a-h, AvP-docs director/alpha6-wishlist.md item 8): Object Placement > Create > With shape > Choose opens a grid of every shape file
// the editor's lookup order finds (shape_search.h: ShapeFolder=, <game>\Tools\shape_rifs, <game>\shape_rifs), in place of inteng's one-at-a-time Shape Chooser
// (Replace shape keeps that one: inteng's own, 1:1). Each tile is the shape drawn small and textured, its name under it. A click selects a tile and a second
// click on it places it (Enter does too), the way Quick-create does: the level's own shape BY ITS NUMBER when the level holds that file's shape, else the file
// brought in (object_make.h). A file that can't be drawn or placed shows its name and why: none is ever left out without a word.
//
// THE LIST (ShapePicker_List), as the lookup order would answer for each name:
//   - the folders in their order; a folder that isn't there is passed over; a folder that is REFUSED stops the list there, its words in the notes, and no
//     folder after it is shown (Quick-create's lookup stops at it too, so a tile from a later folder could never be placed: shape_search.h, FAIL CLOSED);
//   - in a folder, the names at its top ending in ".rif" (any capitals), at most kShapePickerMostFilesAFolder of them (the rest said in one note);
//   - a name is shown once, from the FIRST folder that holds it (any capitals): the file Quick-create would take, so the picture is what gets placed;
//   - a name Quick-create can't take (not plain: shape_import.h ShapeImport_NameIsPlain) is still shown, and can't be placed;
//   - "in this level": the level holds an outside shape of that file (the Shape Chooser's names, shape_chooser.h).
// THE GRID (ShapePicker_Layout): the space under the bars and over the status line and its words (as the texture chooser's, texture_map_screen.cpp), filled
// with tiles of a fixed size, A columns by B rows (as many as fit, at least 1 by 1); more tiles than fit scroll by rows.
// Pure: no file, no Windows, no drawing.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "shape_search.h"

namespace host
{
	size_t const kShapePickerMostFilesAFolder = 4096;   // the stock package holds 322 and Option C makes 343 (measured 2026-11-23); a folder may hold anything

	// One folder of the order as the host found it
	struct ShapePickerFolder
	{
		ShapeSearchFolder folder;
		bool missing = false;             // not there: passed over without a word
		std::string refused;              // why it can't be looked in, "" when it can (ShapeSearch_FolderWhyNot)
		std::vector<std::string> names;   // the names at its top ending in ".rif", as the folder holds them, at most kShapePickerMostFilesAFolder
		size_t more = 0;                  // names past that cap, not listed
	};

	struct ShapePickerTile
	{
		std::string name;       // the file's name without ".rif", as the folder holds it ("pulam")
		std::string fileName;   // with it ("pulam.rif")
		std::string where;      // the folder's label (shape_search.h: never a folder of the user's)
		std::string path;       // the whole path: the folder, '\', fileName
		bool plain = false;     // a name Quick-create can take; one that isn't can't be placed
		bool inLevel = false;   // the level holds an outside shape of this file: placed by its number
		bool levelOnly = false; // a tile of the LEVEL's own shape, not of a folder's file (ShapePicker_AddLevelShapes): drawn and placed by levelNumber
		int32_t levelNumber = -1;   // that shape's number (SHPHEAD1's, the Shape Chooser's)
		std::vector<std::string> copies;   // the files folded into this tile: they look the same (ShapePicker_Thin), in the order of their names
	};

	struct ShapePickerList
	{
		std::vector<ShapePickerTile> tiles;   // in the order of their names in small letters
		std::vector<std::string> notes;       // words about the folders: a refused one, names past the cap
	};

	// The tiles, by the rules above. levelFiles: the names of the files the level's outside shapes came from (the Shape Chooser's, without ".rif"), any capitals.
	ShapePickerList ShapePicker_List(std::vector<ShapePickerFolder> const &folders, std::vector<std::string> const &levelFiles);

	// THE OPENING'S LOOK AT EACH FOLDER TILE'S FILE (the owner, 2026-11-23, through Bishop 2: "No picture means no geometry? if yes don't show!", and copies that
	// look the same folded into one tile). The screen reads each tile drawn from its own file as the picker opens, with the read the drawing uses (shape_import.h
	// ShapeImport_Read, then shape_thumb.h ShapeThumb_Read), at most kShapePickerMostLookMs in all: what it learned of tile i is looks[i].
	struct ShapePickerLook
	{
		bool noGeometry = false;    // the file holds no geometry to draw (ShapeThumb_Read's noGeometry): left out of the grid
		std::string fingerprint;    // what is drawn, as a SHA-256 of the drawing's model (its points, polygons, texture places and pictures' paths); empty when
		                            // it doesn't fold: not read (in this level, or past the time), broken, too big, or a fragment file (only its first part drawn)
	};

	unsigned const kShapePickerMostLookMs = 3000;   // the opening's look at every file, at most (the owner's 352 files take about 70 ms; a stranger's folder of
	                                                // 4096 files of 4 MB would take minutes: past this, the rest keep their tiles and say why when drawn)

	struct ShapePickerThinned
	{
		std::vector<std::string> leftOut;   // the names left out (no geometry), in order
		std::vector<std::string> folded;    // "<kept>~<copy>" for each copy folded into a tile, in order
	};

	// NO GEOMETRY, NO TILE; COPIES FOLDED: each tile whose look says no geometry left out of the grid (a broken or too big file keeps its tile, with why); of the
	// tiles whose fingerprints are the same, the first by name kept (the plain one, when the group holds it: "cratese" sorts before "cratese_fcra") with the
	// others' names in its copies, and the others taken out. A copy that truly looks different has another fingerprint and keeps its own tile. looks shorter
	// than the tiles leaves the rest as they are. Notes, one line each when any: how many were left out, how many copies were folded, and how many files
	// weren't looked at in time (pastTime)
	void ShapePicker_Thin(ShapePickerList *list, std::vector<ShapePickerLook> const &looks, size_t pastTime, ShapePickerThinned *out);

	// THE LEVEL'S OWN SHAPES (Bishop 2's decision, 2026-11-23, on the owner's "items from shape_rifs + level" and his first words for the picker, "shows that OR the
	// shape_rifs"): inteng's Choose listed every outside shape of the level, and the grid did not, so a shape the level alone holds (no folder has its file) could no
	// longer be made by Choose, nor one PART of a fragment file. Every shape of the level is added as a tile "in this level", drawn and placed BY ITS NUMBER (the
	// Quick-create way): a file the level holds once, by the file's name, unless a folder's tile already has that name (that tile is the same shape, and is placed
	// by number already); each part of a fragment file (a file the level holds more than once), by its own name, gameName ("Game@offbits"; inteng's list says
	// "Shape 'offbits'" for every part), beside the file's own "N parts" tile when a folder holds it. At most kShapePickerMostLevelShapes (the rest in one note).
	// Then every tile is in the order of its name in small letters again.
	struct ShapePickerLevelShape
	{
		std::string file;       // the outside file's name, without ".rif" (shape_chooser.h ShapeChoice::name)
		std::string gameName;   // "object@file" (ShapeChoice::gameName), empty when the level gives none
		int32_t number = -1;    // the shape's own number
	};

	size_t const kShapePickerMostLevelShapes = 4096;

	void ShapePicker_AddLevelShapes(ShapePickerList *list, std::vector<ShapePickerLevelShape> const &shapes);

	// The base of the name of an object made from a tile (object_make.h nameBase: "<base><n>"): a part's object name ("Game" of "Game@offbits"), else the tile's name
	std::string ShapePicker_NameBase(ShapePickerTile const &tile);

	// ----- the grid -----
	int32_t const kShapePickerPicture = 96;      // a tile's picture, square, in pixels (the thumbnail is drawn at this size)
	int32_t const kShapePickerNameHeight = 18;   // the line under it with the name (inteng's 16-pixel font)
	int32_t const kShapePickerGap = 8;           // between tiles
	int32_t const kShapePickerTop = 66;          // under the two bars (as the texture chooser's)
	int32_t const kShapePickerBottomRoom = 84;   // over the status line and its words (the texture chooser's 36 + 48)

	struct ShapePickerLayout
	{
		int32_t left = 0, top = 0;           // the first tile's top-left
		int32_t columns = 1, rows = 1;       // A by B
		int32_t stepX = 0, stepY = 0;        // from one tile to the next
	};

	// The grid for a window of that size (client pixels): as many columns and rows as fit, at least 1 by 1, centred across
	ShapePickerLayout ShapePicker_Layout(int32_t windowWidth, int32_t windowHeight);

	// A tile's PICTURE box on the screen (x, y, w, h; the name goes under it), for tile number `tile` when the grid shows from tile `first` on (a multiple of
	// the columns). False when that tile isn't on the screen now.
	bool ShapePicker_TileBox(ShapePickerLayout const &layout, size_t first, size_t tile, int32_t box[4]);

	// The tile under a pixel (its picture or its name), or -1 for none (between tiles, past the last of `count`)
	int ShapePicker_TileAt(ShapePickerLayout const &layout, size_t first, size_t count, int32_t x, int32_t y);

	// ----- what is shown and selected -----
	struct ShapePickerView
	{
		size_t first = 0;      // the first tile on the screen: always a multiple of the columns
		size_t selected = 0;   // the selected tile (when there is any)
	};

	enum class ShapePickerKey
	{
		Left, Right, Up, Down, PageUp, PageDown, Home, End
	};

	// The view after a key: the selection moves (it stops at the ends, never wraps), and the rows shown follow it so it stays on the screen
	ShapePickerView ShapePicker_Key(ShapePickerView view, ShapePickerLayout const &layout, size_t count, ShapePickerKey key);

	// The view after the wheel: `rows` rows on (down, positive) or back; the selection stays as it is (it may leave the screen, as a list's does)
	ShapePickerView ShapePicker_Scroll(ShapePickerView view, ShapePickerLayout const &layout, size_t count, int rows);

	// The view with the selection on screen (after the list or the window changed): first moved the least that shows it, and kept a multiple of the columns
	ShapePickerView ShapePicker_Show(ShapePickerView view, ShapePickerLayout const &layout, size_t count);

	// ----- THE SCROLL BAR (the owner, 2026-11-23: "Ok I want a dragable scroll box on the top right, inteng folk do not mouse scroll") -----
	// Down the right side of the grid from its top, in the look of the dialog lists' bar (dialog_box.cpp ComboScrollBar, dialogs.cpp: the column black framed
	// white, the thumb blue framed white, an arrow button at each end), always there. Its thumb is as long as the share of the rows in view (at least 8 pixels,
	// all of the track when every tile is in view) and as far down the track as the first row in view is down the rows past one page. A press on an arrow moves
	// a row; on the column above or below the thumb, a page; the thumb is DRAGGED (new: the dialog lists' thumb isn't). The selection stays as it is, as the
	// wheel leaves it. The grid leaves the bar's room on the right (ShapePicker_Layout), so no tile is ever under it.
	int32_t const kShapePickerBarWidth = 16;     // as wide as a dialog list's bar (dialog_box.h kDialogScrollBarWidth)
	int32_t const kShapePickerLeastThumb = 8;    // the shortest thumb, as a dialog list's

	struct ShapePickerBar
	{
		int32_t column[4] = {};   // x, y, w, h of the whole bar, its arrows included
		int32_t up[4] = {};       // the top arrow
		int32_t down[4] = {};     // the bottom arrow
		int32_t thumb[4] = {};    // the box dragged
		int32_t above[4] = {};    // the track above the thumb (a page back), 0 high when the thumb is at the top
		int32_t below[4] = {};    // the track below it (a page on), 0 high when it is at the foot
	};

	ShapePickerBar ShapePicker_Bar(ShapePickerLayout const &layout, ShapePickerView const &view, size_t count, int32_t windowWidth);

	enum class ShapePickerBarPart
	{
		None, Up, Down, Above, Below, Thumb
	};

	// The bar's part under a pixel (None: not on the bar)
	ShapePickerBarPart ShapePicker_BarPartAt(ShapePickerBar const &bar, int32_t x, int32_t y);

	// The view after a press on a part: Up / Down a row, Above / Below a page (the rows in view), Thumb and None nothing (a drag starts on the thumb)
	ShapePickerView ShapePicker_BarPress(ShapePickerView view, ShapePickerLayout const &layout, size_t count, ShapePickerBarPart part);

	// The view with the thumb dragged so its top is at thumbTop (the pointer's y, less where on the thumb it was taken): the first row in view is the nearest
	// to that share of the track, from the first row to the last page's; past either end it stops there
	ShapePickerView ShapePicker_ThumbTo(ShapePickerView view, ShapePickerLayout const &layout, size_t count, ShapePickerBar const &bar, int32_t thumbTop);

	// The bar's boxes for the state line, as pickerboxes are: "up~x y w h|down~...|above~...|below~...|thumb~...", a part 0 high left out (own pixels; the
	// screen gives them in the window's)
	std::string ShapePicker_BarWords(ShapePickerBar const &bar);

	// ----- a tile's drawing, as the host's thumbnail work reports it, and its words -----
	enum class ShapePickerDrawing
	{
		Waiting,    // not drawn yet: its tile says "drawing..."
		Drawn,      // its picture is ready
		Failed      // it can't be drawn: why says why
	};

	struct ShapePickerState
	{
		ShapePickerDrawing drawing = ShapePickerDrawing::Waiting;
		std::string why;               // Failed: why it can't be drawn
		size_t parts = 1;              // the shapes the file holds: more than one is a fragment file, shown as its first part, "N parts"
		size_t textures = 0;           // the pictures its polygons use
		size_t texturesFound = 0;      // of them, those that were read and decoded
		std::string textureWhy;        // the first that wasn't: "<its name>: <why>"
	};

	// Why a tile can't be PLACED, "" when it can: a name that isn't plain; a fragment file the level doesn't hold (the import takes one shape a file); a file
	// that can't be drawn (it can't be read as a shape, so the import couldn't either)
	std::string ShapePicker_WhyNotPlaced(ShapePickerTile const &tile, ShapePickerState const &state);

	// A tile's words for the state line's pickertiles: "<name>: <where>[; in this level][; <n> parts[: the level's first is placed]][; textures <k> of <n>]
	// [; texture <name>: <why>][; can't be drawn: <why>][; can't be placed: <why>]" (Waiting: "; drawing"). The name made safe by ShapePicker_SafeName, every
	// other part by ShapePicker_SafeWords. An "in this level" tile's parts are the level's shapes of that file: the level's first is the one drawn and placed.
	std::string ShapePicker_TileWords(ShapePickerTile const &tile, ShapePickerState const &state);

	// HOSTILE WORDS MADE SAFE TO SHOW (Frost's review of 4f0db774, SHOULD 1): a file's name, a texture's name and a why come from a folder or a file anyone made.
	// The editor's safe text rule (panel_paint.h PanelPaint_SafeText: the space to the tilde, every other byte a '?', at most 255 letters), and '|' and '~' a
	// '?' as well: they part the state line's tiles and boxes (pickertiles, pickerboxes)
	size_t const kShapePickerMostWordLetters = 255;
	std::string ShapePicker_SafeWords(std::string const &text);

	// A NAME made safe (a file's, a texture's): as ShapePicker_SafeWords, and ';' a '?' too, since a tile's own words are parted by "; " (Frost's nit on ec8a4d0e:
	// a file named "x; in this level.rif" would read like the flag)
	std::string ShapePicker_SafeName(std::string const &name);

	// A tile's words (a shape that can't be drawn says why in its tile) in lines of at most perLine letters, broken at spaces, a word longer than a line cut
	// to it; at most mostLines lines (the rest left out). The picker's first acceptance run showed "can't be dra" / "wn" when they were cut every perLine letters.
	std::vector<std::string> ShapePicker_TileLines(std::string const &words, size_t perLine, size_t mostLines);

	// The state line's picker field while it is open: "open cols <a> rows <b> count <n> drawn <d> failed <f> first <i> selected <name>" ("selected none" with no
	// tile). The name made safe by ShapePicker_SafeName as pickerboxes' and pickertiles' are (Drake's nit on df479439): a shape folder takes files from anywhere,
	// and a file's name may hold '~', ';' or a byte past 0x7E.
	std::string ShapePicker_OpenWords(ShapePickerLayout const &layout, ShapePickerView const &view, std::vector<ShapePickerTile> const &tiles, size_t drawn,
		size_t failed);
}
