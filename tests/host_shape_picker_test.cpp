// The test for the placement picker's pure part (src/host/shape_picker.h). Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code". No file is touched: the folders are given as the host would have found them.
//   (1) THE LIST: one name from the FIRST folder holding it (any capitals), so the tile is the file Quick-create takes; a missing folder passed over; a REFUSED
//       folder stops the list there, its words in the notes, nothing after it shown; names that don't end in ".rif" left out; past the cap said in a note;
//       a name that isn't plain shown and marked; "in this level" by the level's file names (any capitals); sorted by name in small letters
//   (2) THE GRID: A by B to fit the window (at least 1 by 1, centred across); a tile's box, on and off the screen; the tile under a pixel (its picture, its name,
//       a gap, past the last)
//   (3) THE KEYS AND THE WHEEL: the selection stops at the ends, never wraps; the rows shown follow it; the wheel stops at the first and the last page
//   (4) THE WORDS: drawing, drawn with textures, a texture that wasn't read, can't be drawn, a fragment file (placed only from the level), in this level
//   (5) HOSTILE WORDS MADE SAFE (Frost's review of 4f0db774, SHOULD 1): a byte outside the space to the tilde, and the state line's '|' and '~', each a '?';
//       at most 255 letters; in the tile's words, its name and every why from a file
//   (6) A TILE'S WORDS IN LINES: broken at spaces, a longer word cut, at most the lines asked for
//   (7) THE STATE LINE'S PICKER FIELD: the selected name made safe; "selected none" with no tile (red by mutation, the raw name: 1 of 2 fails)
//   (8) THE SCROLL BAR (the owner 11-23): down the right side from the grid's top, an arrow at each end, the thumb the share in view; an arrow a row, the track a page,
//       the thumb dragged (to the foot the last page, to the top the first row); the selection as it was; its words. Red: no bar before it (the cut 8275930d)
//   (9) THE LEVEL'S OWN SHAPES: every shape of the level a tile (a fragment file's parts by their own names), none twice, placeable by number, a part's object
//       named from its object name. Red: no such tiles before it (b4521891)
//   (10) NO GEOMETRY, NO TILE; COPIES FOLDED (the owner 11-23): the empty file left out, copies that look the same folded under the first by name, a copy that
//       truly looks different kept; one note line each; a tile's words name its copies, made safe. Red: on a stand-in that leaves nothing out and folds nothing
//   (11) THE TURNING'S CLOCK: once round every 8 s from its start, in radians and in whole degrees
// RED FIRST: on a stand-in that lists nothing and lays out nothing, (1) to (4) fail; (5) on 4f0db774 (the words as they came); (6) by mutation, the lines
// cut every perLine letters as the paint did before: 3 of its 4 fail.
#include <math.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "shape_picker.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static ShapePickerFolder Folder(char const *path, char const *label, std::vector<std::string> const &names)
{
	ShapePickerFolder f;
	f.folder.path = path;
	f.folder.label = label;
	f.names = names;
	return f;
}

static std::string Names(ShapePickerList const &list)
{
	std::string out;

	for (ShapePickerTile const &t : list.tiles)
	{
		out += (out.empty() ? "" : ",") + t.name + "@" + t.where;
	}

	return out;
}

int main()
{
	printf("host_shape_picker_test: the placement picker's list, grid, keys and words\n");

	// ---- (1) THE LIST
	{
		std::vector<ShapePickerFolder> folders = {
			Folder("C:\\Mine", "ShapeFolder=", { "Chair.RIF", "notes.txt" }),
			Folder("C:\\Game\\Tools\\shape_rifs", "the game's Tools\\shape_rifs", { "pulam.rif", "chair.rif", "medbox01.rif" }),
			Folder("C:\\Game\\shape_rifs", "the game's shape_rifs", { "PULAM.rif", "timdesk.rif", "bad name.rif", "nul.rif", ".rif" }),
		};
		ShapePickerList const list = ShapePicker_List(folders, { "TimDesk", "medbox01" });

		Check(Names(list) == "bad name@the game's shape_rifs,Chair@ShapeFolder=,medbox01@the game's Tools\\shape_rifs,nul@the game's shape_rifs,"
			"pulam@the game's Tools\\shape_rifs,timdesk@the game's shape_rifs",
			"each name once, from the first folder that holds it (Chair from ShapeFolder=, pulam from Tools, never shape_rifs' PULAM), sorted, a .txt and \".rif\" left out: " + Names(list));

		bool marks = list.tiles.size() == 6;

		for (ShapePickerTile const &t : list.tiles)
		{
			bool const wantPlain = t.name != "bad name" && t.name != "nul", wantLevel = t.name == "timdesk" || t.name == "medbox01";
			marks = marks && t.plain == wantPlain && t.inLevel == wantLevel;
		}

		Check(marks, "a name with a space and a device's name aren't plain; timdesk and medbox01 are in this level (the level's names in other capitals)");
		Check(list.tiles.size() == 6 && list.tiles[4].path == "C:\\Game\\Tools\\shape_rifs\\pulam.rif" && list.tiles[4].fileName == "pulam.rif",
			"a tile's path is its folder's and its file's own name: " + (list.tiles.size() == 6 ? list.tiles[4].path : std::string("-")));
		Check(list.notes.empty(), "no note when every folder was looked in and none holds too many");

		// a missing folder is passed over; a refused one stops the list there
		folders[0].missing = true;
		folders[1].refused = "it is a link or a junction";
		ShapePickerList const stopped = ShapePicker_List(folders, {});

		Check(stopped.tiles.empty() && stopped.notes.size() == 1 && stopped.notes[0].find("the game's Tools\\shape_rifs isn't used: it is a link or a junction") == 0,
			"FAIL CLOSED: a refused folder stops the list, nothing after it is shown, its words in the notes: " + (stopped.notes.empty() ? std::string("-") : stopped.notes[0]));

		folders[1].refused.clear();
		folders[1].more = 5;
		ShapePickerList const capped = ShapePicker_List(folders, {});

		Check(capped.tiles.size() == 6 && capped.notes.size() == 1 && capped.notes[0].find("holds 5 more .rif files than the 4096") != std::string::npos,
			"names past a folder's cap are said in a note, the ones listed still shown (" + std::to_string(capped.tiles.size()) + " tiles)");
	}

	// ---- (2) THE GRID
	{
		// THE BOX (Dietrich 2026-11-24; the owner: "make the object picker take 80% of width and height with 10% padding on top/left/right/bottom"): a 1024 x 768
		// window's box is 102..922 across (1024 / 10 = 102, rounded down) and 76..692 down; in it, width 820 - 16 - 24 (the scroll bar's room) = 780 over 104 a
		// tile: 7; height 616 - 4 - 48 (the words' room) = 564 over 122: 4
		ShapePickerLayout const g = ShapePicker_Layout(1024, 768);

		Check(g.box[0] == 102 && g.box[1] == 76 && g.box[2] == 820 && g.box[3] == 616, "(2) a 1024 x 768 window's box: 10% in from each side (" + std::to_string(g.box[0]) +
			"," + std::to_string(g.box[1]) + " " + std::to_string(g.box[2]) + " x " + std::to_string(g.box[3]) + ")");
		Check(g.columns == 7 && g.rows == 4 && g.stepX == 104 && g.stepY == 122, "(2) in it 7 columns by 4 rows (" + std::to_string(g.columns) + " by " +
			std::to_string(g.rows) + ")");
		Check(g.left == 140 && g.top == 80, "(2) centred across the box left of its scroll bar ((102 + 922 - 24 - 728 + 8) / 2): the first tile at " + std::to_string(g.left) +
			"," + std::to_string(g.top));
		Check(g.wordsX == 110 && g.wordsY == 646 && g.top + g.rows * g.stepY - 8 <= g.wordsY && g.wordsY + 3 * 15 <= g.box[1] + g.box[3],
			"(2) the lines of words inside the box's foot, under the grid: from " + std::to_string(g.wordsX) + "," + std::to_string(g.wordsY));

		ShapePickerLayout const big = ShapePicker_Layout(1280, 960);
		ShapePickerLayout const small = ShapePicker_Layout(640, 480);

		Check(big.columns == 9 && big.rows == 5 && big.box[0] == 128 && big.box[2] == 1024, "(2) a 1280 x 960 window: its box 1024 wide from 128, 9 by 5");
		Check(small.box[1] == 66 && small.box[1] + small.box[3] == 432, "(2) a 640 x 480 window: the box never starts above the bars (66, not 48), and ends at 90% (432)");

		ShapePickerLayout const tiny = ShapePicker_Layout(40, 40);

		Check(tiny.columns == 1 && tiny.rows == 1, "(2) a window too small for one tile still shows 1 by 1");

		int32_t box[4] = {};

		bool const tenBoxed = ShapePicker_TileBox(g, 0, 10, box);   // (called before its Check: MSVC builds the check's words first)

		Check(tenBoxed && box[0] == g.left + 3 * 104 && box[1] == 80 + 122 && box[2] == 96 && box[3] == 96,
			"(2) tile 10 from the first: row 2, column 4, its 96-pixel picture (" + std::to_string(box[0]) + "," + std::to_string(box[1]) + ")");
		Check(!ShapePicker_TileBox(g, 9, 5, box) && !ShapePicker_TileBox(g, 0, 28, box) && ShapePicker_TileBox(g, 7, 34, box),
			"(2) a tile before the first shown, or past the last row on the screen, has no box");

		Check(ShapePicker_TileAt(g, 0, 20, g.left + 3 * 104 + 5, 80 + 122 + 5) == 10, "(2) a pixel in tile 10's picture is tile 10");
		Check(ShapePicker_TileAt(g, 0, 20, g.left + 3 * 104 + 5, 80 + 122 + 96 + 10) == 10, "(2) a pixel in its name is tile 10 too");
		Check(ShapePicker_TileAt(g, 0, 20, g.left + 100, 85) == -1 && ShapePicker_TileAt(g, 0, 20, g.left + 5, 80 + 96 + 18 + 3) == -1, "(2) a pixel between tiles is none");
		Check(ShapePicker_TileAt(g, 0, 8, g.left + 104 + 5, 80 + 122 + 5) == -1 && ShapePicker_TileAt(g, 7, 20, g.left + 5, 85) == 7,
			"(2) past the last tile is none; the first row from 7 on is tile 7");
		Check(ShapePicker_TileAt(g, 0, 40, g.box[0] - 20, 85) == -1, "(2) the padding left of the box is no tile");
	}

	// ---- (3) THE KEYS AND THE WHEEL
	{
		ShapePickerLayout const g = ShapePicker_Layout(1280, 960);   // 9 by 5, 45 a page (its box 1024 x 768: the owner's 10% padding)
		ShapePickerView v;

		v = ShapePicker_Key(v, g, 100, ShapePickerKey::Left);
		Check(v.selected == 0 && v.first == 0, "Left at the first stays there (never wraps)");
		v = ShapePicker_Key(v, g, 100, ShapePickerKey::Down);
		v = ShapePicker_Key(v, g, 100, ShapePickerKey::Right);
		Check(v.selected == 10 && v.first == 0, "Down then Right: tile 10");

		for (int i = 0; i < 5; i++)
		{
			v = ShapePicker_Key(v, g, 100, ShapePickerKey::Down);
		}

		Check(v.selected == 55 && v.first == 18, "five rows Down: tile 55, the rows shown follow it (first " + std::to_string(v.first) + ")");
		v = ShapePicker_Key(v, g, 100, ShapePickerKey::End);
		Check(v.selected == 99 && v.first == 63, "End: the last, its page shown to the end, no more (first " + std::to_string(v.first) + ")");
		v = ShapePicker_Key(v, g, 100, ShapePickerKey::Down);
		Check(v.selected == 99, "Down on the last row stays");
		v = ShapePicker_Key(v, g, 100, ShapePickerKey::Home);
		Check(v.selected == 0 && v.first == 0, "Home: the first");
		v = ShapePicker_Key(v, g, 100, ShapePickerKey::PageDown);
		Check(v.selected == 45 && v.first == 9, "Page Down: a page on (" + std::to_string(v.selected) + ", first " + std::to_string(v.first) + ")");
		v = ShapePicker_Key(v, g, 100, ShapePickerKey::PageUp);
		Check(v.selected == 0, "Page Up: back");

		v = ShapePicker_Scroll(v, g, 100, 3);
		Check(v.first == 27 && v.selected == 0, "the wheel 3 rows down: first 27, the selection kept");
		v = ShapePicker_Scroll(v, g, 100, 50);
		Check(v.first == 63, "the wheel far down stops at the last page (first " + std::to_string(v.first) + ")");
		v = ShapePicker_Scroll(v, g, 100, -50);
		Check(v.first == 0, "and far up at the first");
		Check(ShapePicker_Key(ShapePickerView(), g, 0, ShapePickerKey::Down).selected == 0, "no tiles: nothing moves");

		ShapePickerView past;
		past.selected = 500;
		past.first = 900;
		past = ShapePicker_Show(past, g, 100);
		Check(past.selected == 99 && past.first == 63, "a view past a shorter list comes back onto it");
	}

	// ---- (4) THE WORDS
	{
		ShapePickerTile t;
		t.name = "pulam";
		t.where = "the game's Tools\\shape_rifs";
		t.plain = true;
		ShapePickerState s;

		Check(ShapePicker_TileWords(t, s) == "pulam: the game's Tools\\shape_rifs; drawing", "waiting: " + ShapePicker_TileWords(t, s));

		s.drawing = ShapePickerDrawing::Drawn;
		s.textures = 1;
		s.texturesFound = 1;
		Check(ShapePicker_TileWords(t, s) == "pulam: the game's Tools\\shape_rifs; textures 1 of 1" && ShapePicker_WhyNotPlaced(t, s).empty(),
			"drawn, textured, can be placed: " + ShapePicker_TileWords(t, s));

		s.textures = 2;
		s.textureWhy = "Common\\Nothere.rim: not found";
		Check(ShapePicker_TileWords(t, s) == "pulam: the game's Tools\\shape_rifs; textures 1 of 2; texture Common\\Nothere.rim: not found",
			"a texture that wasn't read is named with why: " + ShapePicker_TileWords(t, s));

		ShapePickerState failed;
		failed.drawing = ShapePickerDrawing::Failed;
		failed.why = "a chunk's size runs past its end";
		Check(ShapePicker_TileWords(t, failed) == "pulam: the game's Tools\\shape_rifs; can't be drawn: a chunk's size runs past its end; can't be placed: it can't be read as a shape",
			"can't be drawn, so can't be placed: " + ShapePicker_TileWords(t, failed));

		ShapePickerState parts;
		parts.drawing = ShapePickerDrawing::Drawn;
		parts.parts = 7;
		Check(ShapePicker_TileWords(t, parts).find("pulam: the game's Tools\\shape_rifs; 7 parts; can't be placed: the file holds 7 shapes") == 0 &&
			ShapePicker_WhyNotPlaced(t, parts).find("once the '@' import is in") != std::string::npos, "a fragment file the level doesn't hold: " + ShapePicker_TileWords(t, parts));

		t.inLevel = true;
		Check(ShapePicker_TileWords(t, parts) == "pulam: the game's Tools\\shape_rifs; in this level; 7 parts: the level's first is placed" &&
			ShapePicker_WhyNotPlaced(t, failed).empty(), "in this level: placed by its number, even a fragment file's (the level's first part, said) or one that can't be drawn: " +
			ShapePicker_TileWords(t, parts));

		t.inLevel = false;
		t.plain = false;
		Check(ShapePicker_WhyNotPlaced(t, s).find("Quick-create takes plain names only") != std::string::npos, "a name that isn't plain can't be placed");
	}

	// ---- (5) HOSTILE WORDS MADE SAFE
	{
		std::string const odd = std::string("a|b~c;d") + '\t' + '\n' + (char)0xE9 + "z";

		Check(ShapePicker_SafeWords(odd) == "a?b?c;d???z", "'|', '~', a tab, a line end and a byte past the tilde are each a '?'; ';' and the plain letters stay (" +
			ShapePicker_SafeWords(odd) + ")");
		Check(ShapePicker_SafeWords(std::string(400, 'x')).size() == kShapePickerMostWordLetters, "at most 255 letters");

		ShapePickerTile t;
		t.name = "pul|am";
		t.where = "the game's shape_rifs";
		t.plain = true;
		ShapePickerState s;
		s.drawing = ShapePickerDrawing::Drawn;
		s.textures = 1;
		s.textureWhy = std::string("Bad~name.rim") + '\r' + ": refused";

		std::string const words = ShapePicker_TileWords(t, s);
		Check(words == "pul?am: the game's shape_rifs; textures 0 of 1; texture Bad?name.rim?: refused" && words.find('|') == std::string::npos,
			"a tile's words carry no '|', '~' or line end from its file's name or a texture's: " + words);

		// a NAME's ';' is a '?' too (Frost's nit on ec8a4d0e: the tile's words are parted by "; "), a why's isn't (ours hold "; ")
		Check(ShapePicker_SafeName("x; in this level") == "x? in this level" && ShapePicker_SafeWords("a; b") == "a; b", "a name's ';' is a '?', a why's stays");

		t.name = "x; in this level";
		Check(ShapePicker_TileWords(t, s).find("x? in this level: the game's shape_rifs") == 0, "so a file named so can't read like the flag: " + ShapePicker_TileWords(t, s));
	}

	// ---- (6) A TILE'S WORDS IN LINES, broken at spaces (the first acceptance run, 11-23: "can't be dra" / "wn" in pulam_cut's tile)
	{
		auto joined = [](std::vector<std::string> const &lines)
		{
			std::string all;

			for (std::string const &line : lines)
			{
				all += (all.empty() ? "" : "/") + line;
			}

			return all;
		};

		std::vector<std::string> const cut = ShapePicker_TileLines("can't be drawn: not a rif file", 12, 5);
		Check(joined(cut) == "can't be/drawn: not a/rif file", "(6) broken at spaces, never inside a word: " + joined(cut));

		std::vector<std::string> const longWord = ShapePicker_TileLines("abcdefghijklmnopqrstuvwxyz is long", 12, 5);
		Check(joined(longWord) == "abcdefghijkl/mnopqrstuvwx/yz is long", "(6) a word longer than a line is cut to it: " + joined(longWord));

		std::vector<std::string> const most = ShapePicker_TileLines("one two three four five six seven", 5, 3);
		Check(joined(most) == "one/two/three", "(6) at most the lines asked for, the rest left out: " + joined(most));

		std::vector<std::string> const spaces = ShapePicker_TileLines("   ", 4, 5);
		std::vector<std::string> const none = ShapePicker_TileLines("", 4, 5);
		std::vector<std::string> const zero = ShapePicker_TileLines("ab cd", 0, 5);
		Check(spaces.empty() && none.empty() && joined(zero) == "a/b/c/d", "(6) only spaces or nothing: no line; a line of 0 letters taken as 1: " + joined(zero));
	}

	// ---- (7) THE STATE LINE'S PICKER FIELD: the selected tile's name made safe, as pickerboxes' and pickertiles' are (Drake's nit on df479439). A shape folder
	// takes files from anywhere, and a Windows file name may hold '~' (pickerboxes' own mark), ';' and bytes past 0x7E.
	{
		ShapePickerLayout const g = ShapePicker_Layout(1024, 768);
		ShapePickerView v;
		std::vector<ShapePickerTile> tiles(2);

		tiles[0].name = "plain";
		tiles[1].name = "a~b;c\xe9";
		v.selected = 1;

		std::string const words = ShapePicker_OpenWords(g, v, tiles, 1, 0);
		std::string const none = ShapePicker_OpenWords(g, ShapePickerView(), std::vector<ShapePickerTile>(), 0, 0);

		Check(words == "open cols " + std::to_string(g.columns) + " rows " + std::to_string(g.rows) + " count 2 drawn 1 failed 0 first 0 selected a?b?c?",
			"(7) the selected name made safe: " + words);
		Check(none.find(" count 0 ") != std::string::npos && none.size() > 14 && none.compare(none.size() - 14, 14, " selected none") == 0,
			"(7) no tile: selected none: " + none);
	}

	// ---- (8) THE SCROLL BAR (the owner 11-23: "a dragable scroll box on the top right"): its place, its thumb, a press on each part, the thumb dragged, its words.
	// A 1280 x 960 window (its box 1024 x 768, the owner's 10% padding): 9 x 5, 45 a page; 100 tiles take 12 rows, so the first row in view goes from 0 to 7
	{
		ShapePickerLayout const g = ShapePicker_Layout(1280, 960);
		ShapePickerView v;
		size_t const count = 100;
		ShapePickerBar const bar = ShapePicker_Bar(g, v, count, 1280);
		int32_t const height = 5 * 122 - 8, track = height - 32;

		// down the BOX's right side (1280 - 128 = 1152: its right edge), from the grid's top to its last row's foot; an arrow a square at each end; the thumb
		// 5 / 12 of the track, at its top
		Check(bar.column[0] == 1152 - 8 - 16 && bar.column[1] == g.top && bar.column[2] == 16 && bar.column[3] == height,
			"(8) the bar down the box's right side from the grid's top: " + ShapePicker_BarWords(bar));
		Check(bar.up[1] == g.top && bar.up[3] == 16 && bar.down[1] == g.top + height - 16 && bar.down[3] == 16, "(8) an arrow at each end");
		Check(bar.thumb[1] == g.top + 16 && bar.thumb[3] == track * 5 / 12 && bar.above[3] == 0 && bar.below[3] == track - track * 5 / 12,
			"(8) the thumb the share in view (" + std::to_string(bar.thumb[3]) + " of " + std::to_string(track) + "), at the top, all the track below it");
		Check(g.left + 9 * 104 - 8 <= bar.column[0] - 8, "(8) no tile under the bar: the grid ends a gap before it");

		// a press on each part (the selection stays as it is)
		v.selected = 3;
		ShapePickerView const down = ShapePicker_BarPress(v, g, count, ShapePicker_BarPartAt(bar, bar.down[0] + 8, bar.down[1] + 8));
		ShapePickerView const below = ShapePicker_BarPress(down, g, count, ShapePickerBarPart::Below);
		ShapePickerView const past = ShapePicker_BarPress(below, g, count, ShapePickerBarPart::Below);
		ShapePickerView const up = ShapePicker_BarPress(past, g, count, ShapePicker_BarPartAt(bar, bar.up[0] + 8, bar.up[1] + 8));
		ShapePickerView const above = ShapePicker_BarPress(up, g, count, ShapePickerBarPart::Above);

		Check(down.first == 9 && below.first == 54 && past.first == 63 && up.first == 54 && above.first == 9 && above.selected == 3,
			"(8) the bottom arrow a row (" + std::to_string(down.first) + "), the track a page (" + std::to_string(below.first) + "), and no further than the last page (" +
			std::to_string(past.first) + "); the top arrow a row back (" + std::to_string(up.first) + "), the track above a page back (" + std::to_string(above.first) +
			"); the selection as it was");

		ShapePickerBarPart const onThumb = ShapePicker_BarPartAt(bar, bar.thumb[0] + 8, bar.thumb[1] + 2);
		ShapePickerBarPart const offBar = ShapePicker_BarPartAt(bar, bar.column[0] - 1, bar.thumb[1] + 2);

		Check(onThumb == ShapePickerBarPart::Thumb && offBar == ShapePickerBarPart::None, "(8) the thumb found under the pointer; beside the bar, no part");

		// the thumb dragged: its top to the track's foot is the last page, past it too; to the top, the first row; half way, the middle row
		int32_t const room = track - bar.thumb[3];
		ShapePickerView const bottom = ShapePicker_ThumbTo(v, g, count, bar, bar.up[1] + 16 + room);
		ShapePickerView const farPast = ShapePicker_ThumbTo(v, g, count, bar, 10000);
		ShapePickerView const top = ShapePicker_ThumbTo(bottom, g, count, bar, -10000);
		ShapePickerView const middle = ShapePicker_ThumbTo(v, g, count, bar, bar.up[1] + 16 + room / 2);

		Check(bottom.first == 63 && farPast.first == 63 && top.first == 0 && (middle.first == 27 || middle.first == 36) && middle.selected == 3,
			"(8) the thumb dragged: to the foot the last page (" + std::to_string(bottom.first) + ", past it " + std::to_string(farPast.first) + "), to the top the first row (" +
			std::to_string(top.first) + "), half way the middle (" + std::to_string(middle.first) + ")");

		// at the last page the thumb is at the track's foot, all of it above
		ShapePickerBar const atEnd = ShapePicker_Bar(g, bottom, count, 1280);

		Check(atEnd.thumb[1] + atEnd.thumb[3] == atEnd.down[1] && atEnd.below[3] == 0 && atEnd.above[3] == room, "(8) at the last page: the thumb at the track's foot");

		// every tile in view: the thumb all of the track, nothing to drag to
		ShapePickerBar const few = ShapePicker_Bar(g, ShapePickerView(), 10, 1280);
		ShapePickerView const still = ShapePicker_ThumbTo(ShapePickerView(), g, 10, few, few.thumb[1] + 50);

		Check(few.thumb[3] == track && few.above[3] == 0 && few.below[3] == 0 && still.first == 0, "(8) every tile in view: the thumb the whole track, a drag moves nothing");

		// the words: the parts there are, by name; a window too narrow for a bar (20 pixels: the bar and its gap take 24) has none
		std::string const words = ShapePicker_BarWords(bar);
		ShapePickerBar const none = ShapePicker_Bar(ShapePicker_Layout(20, 768), ShapePickerView(), 100, 20);

		Check(words.find("up~") == 0 && words.find("|down~") != std::string::npos && words.find("|thumb~") != std::string::npos && words.find("|below~") != std::string::npos &&
			words.find("above~") == std::string::npos && none.column[2] == 0 && ShapePicker_BarWords(none).empty(), "(8) the words: " + words);
	}

	// ---- (9) THE LEVEL'S OWN SHAPES (Bishop 2's decision on the owner's "items from shape_rifs + level"): a folder holds pulam and offbits; the level holds pulam
	// once, Smrtam01 and Medbox01 (no folder has them), and offbits' parts Aerosol and Game (a fragment file: the level holds it twice)
	{
		std::vector<ShapePickerFolder> folders = { Folder("C:\\game\\shape_rifs", "the game's shape_rifs", { "pulam.rif", "offbits.rif" }) };
		ShapePickerList list = ShapePicker_List(folders, { "pulam", "offbits" });

		ShapePicker_AddLevelShapes(&list, { { "pulam", "pulam", 5 }, { "Smrtam01", "", 7 }, { "offbits", "Aerosol@offbits", 150 }, { "offbits", "Game@offbits", 152 },
			{ "Medbox01", "Medbox01", 9 }, { "broken", "", -1 } });

		std::string order;

		for (ShapePickerTile const &t : list.tiles)
		{
			order += (order.empty() ? "" : " ") + t.name + (t.levelOnly ? "#" + std::to_string(t.levelNumber) : std::string());
		}

		Check(order == "Aerosol@offbits#150 Game@offbits#152 Medbox01#9 offbits pulam Smrtam01#7",
			"(9) every shape of the level a tile, a fragment file's parts by their own names, none twice, a shape with no number left out, in name order: " + order);

		bool marked = true;

		for (ShapePickerTile const &t : list.tiles)
		{
			marked = marked && (!t.levelOnly || (t.inLevel && t.plain && t.where == "the level's own shape" && ShapePicker_WhyNotPlaced(t, ShapePickerState()).empty()));
		}

		Check(marked, "(9) each level tile 'in this level', placeable (by its number)");

		ShapePickerTile part, single;

		part.name = "Game@offbits";
		part.levelOnly = true;
		single.name = "Smrtam01";
		single.levelOnly = true;
		Check(ShapePicker_NameBase(part) == "Game" && ShapePicker_NameBase(single) == "Smrtam01", "(9) a part's object is named from its own name (Game), never with an '@'");

		// a fragment file the level gives no names for: told apart by number
		ShapePickerList plain = ShapePicker_List({}, {});

		ShapePicker_AddLevelShapes(&plain, { { "bits", "", 3 }, { "bits", "", 4 } });
		Check(plain.tiles.size() == 2 && plain.tiles[0].name == "bits#3" && plain.tiles[1].name == "bits#4", "(9) unnamed parts told apart by their numbers");
	}

	// ---- (11) THE TURNING'S CLOCK (the owner 11-23, "rotating in 3d"): once round every 8 s, from where it began
	{
		float const start = ShapePicker_TurnAt(0), quarter = ShapePicker_TurnAt(2000), half = ShapePicker_TurnAt(4000), round = ShapePicker_TurnAt(8000);
		float const later = ShapePicker_TurnAt(8000ull * 1000000 + 6000);   // a long sitting: the same as 6 s in
		int32_t const degrees[4] = { ShapePicker_TurnDegrees(quarter), ShapePicker_TurnDegrees(half), ShapePicker_TurnDegrees(later), ShapePicker_TurnDegrees(-1.5707963f) };

		Check(start == 0.0f && fabsf(quarter - 1.5707963f) < 1e-5f && fabsf(half - 3.1415927f) < 1e-5f && round == 0.0f && fabsf(later - 4.712389f) < 1e-4f,
			"(11) 0 at the start, a quarter at 2 s, half at 4 s, round again at 8 s; a long sitting the same as its part of 8 s");
		Check(degrees[0] == 90 && degrees[1] == 180 && degrees[2] == 270 && degrees[3] == 270, "(11) in whole degrees, 0 to 359: " + std::to_string(degrees[0]) + " " +
			std::to_string(degrees[1]) + " " + std::to_string(degrees[2]) + " " + std::to_string(degrees[3]));
	}

	// ---- (10) NO GEOMETRY, NO TILE; COPIES FOLDED (the owner 11-23, through Bishop 2: "No picture means no geometry? if yes don't show!", and copies with the
	// same points and textures folded into one tile under the plain name). Looks given as the screen would have learned them at the opening
	{
		ShapePickerList list = ShapePicker_List({ Folder("C:\\Game\\shape_rifs", "the game's shape_rifs", { "broken.rif", "cratese.rif", "cratese_fcra.rif",
			"cratese_fgcr.rif", "empty.rif", "notlooked.rif", "zz|same.rif" }) }, {});
		std::vector<ShapePickerLook> looks(7);
		size_t const notesBefore = list.notes.size();
		ShapePickerThinned thinned;

		// broken: no look (it keeps its tile, with why when drawn); cratese and cratese_fcra one look; cratese_fgcr truly another, and "zz|same" looks as it does;
		// empty holds no geometry; notlooked wasn't looked at (past the time)
		looks[1].fingerprint = looks[2].fingerprint = "A";
		looks[3].fingerprint = looks[6].fingerprint = "B";
		looks[4].noGeometry = true;

		ShapePicker_Thin(&list, looks, 2, &thinned);

		std::string kept;

		for (ShapePickerTile const &t : list.tiles)
		{
			kept += (kept.empty() ? "" : " ") + t.name;
		}

		Check(kept == "broken cratese cratese_fgcr notlooked", "(10) left out the empty one, the copies folded, the rest kept in order (" + kept + ")");
		Check(thinned.leftOut == std::vector<std::string>{ "empty" }, "(10) the left-out names: empty");
		Check(thinned.folded == std::vector<std::string>{ "cratese~cratese_fcra", "cratese_fgcr~zz|same" }, "(10) the folded: each copy under the first by name, the "
			"plain one when the group holds it (cratese before cratese_fcra); a copy that truly looks different (cratese_fgcr) keeps its own tile");
		Check(list.tiles.size() == 4 && list.tiles[1].copies == std::vector<std::string>{ "cratese_fcra" } && list.tiles[2].copies == std::vector<std::string>{ "zz|same" } &&
			list.tiles[0].copies.empty(), "(10) each kept tile holds its copies' names");

		std::string notes;

		for (size_t i = notesBefore; i < list.notes.size(); i++)
		{
			notes += list.notes[i] + " / ";
		}

		Check(list.notes.size() == notesBefore + 3 && notes.find("1 file left out") != std::string::npos && notes.find("2 copies folded") != std::string::npos &&
			notes.find("2 files weren't looked at") != std::string::npos, "(10) one note line each: left out, folded, not looked at in time (" + notes + ")");

		ShapePickerTile const fgcr = list.tiles.size() > 2 ? list.tiles[2] : ShapePickerTile();
		std::string const words = ShapePicker_TileWords(fgcr, ShapePickerState());
		Check(words.find("looks the same as zz?same") != std::string::npos && words.find('|') == std::string::npos, "(10) a tile's words name its copies, made safe (" +
			words + ")");

		// a look list shorter than the tiles: the rest as they are; and nothing to do: no note, nothing out
		ShapePickerList shorter = ShapePicker_List({ Folder("C:\\G", "here", { "a.rif", "b.rif", "c.rif" }) }, {});
		ShapePickerLook none;
		none.noGeometry = true;
		ShapePicker_Thin(&shorter, { none }, 0, &thinned);
		Check(shorter.tiles.size() == 2 && shorter.tiles[0].name == "b" && thinned.leftOut == std::vector<std::string>{ "a" }, "(10) a shorter look list: only its tiles");

		size_t const quietBefore = shorter.notes.size();
		ShapePicker_Thin(&shorter, std::vector<ShapePickerLook>(2), 0, &thinned);
		Check(shorter.tiles.size() == 2 && shorter.notes.size() == quietBefore && thinned.leftOut.empty() && thinned.folded.empty(), "(10) nothing to do: no change, "
			"no note");
	}

	printf(checksFailed ? "host_shape_picker_test: %d FAILED\n" : "host_shape_picker_test: all passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
