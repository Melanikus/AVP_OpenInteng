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
	};

	struct ShapePickerList
	{
		std::vector<ShapePickerTile> tiles;   // in the order of their names in small letters
		std::vector<std::string> notes;       // words about the folders: a refused one, names past the cap
	};

	// The tiles, by the rules above. levelFiles: the names of the files the level's outside shapes came from (the Shape Chooser's, without ".rif"), any capitals.
	ShapePickerList ShapePicker_List(std::vector<ShapePickerFolder> const &folders, std::vector<std::string> const &levelFiles);

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
