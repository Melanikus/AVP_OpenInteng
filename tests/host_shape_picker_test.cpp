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
// RED FIRST: on a stand-in that lists nothing and lays out nothing, (1) to (4) fail; (5) on 4f0db774 (the words as they came); (6) by mutation, the lines
// cut every perLine letters as the paint did before: 3 of its 4 fail.
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
		ShapePickerLayout const g = ShapePicker_Layout(1024, 768);

		// width 1024 - 16 = 1008 over 104 a tile: 9; height 768 - 66 - 84 = 618 over 122: 5
		Check(g.columns == 9 && g.rows == 5 && g.stepX == 104 && g.stepY == 122, "a 1024 x 768 window: 9 columns by 5 rows (" + std::to_string(g.columns) + " by " +
			std::to_string(g.rows) + ")");
		Check(g.left == 48 && g.top == 70, "centred across: the first tile at " + std::to_string(g.left) + "," + std::to_string(g.top));

		ShapePickerLayout const tiny = ShapePicker_Layout(40, 40);

		Check(tiny.columns == 1 && tiny.rows == 1, "a window too small for one tile still shows 1 by 1");

		int32_t box[4] = {};

		Check(ShapePicker_TileBox(g, 0, 10, box) && box[0] == 48 + 104 && box[1] == 70 + 122 && box[2] == 96 && box[3] == 96,
			"tile 10 from the first: row 2, column 2, its 96-pixel picture (" + std::to_string(box[0]) + "," + std::to_string(box[1]) + ")");
		Check(!ShapePicker_TileBox(g, 9, 5, box) && !ShapePicker_TileBox(g, 0, 45, box) && ShapePicker_TileBox(g, 9, 53, box),
			"a tile before the first shown, or past the last row on the screen, has no box");

		Check(ShapePicker_TileAt(g, 0, 20, 48 + 104 + 5, 70 + 122 + 5) == 10, "a pixel in tile 10's picture is tile 10");
		Check(ShapePicker_TileAt(g, 0, 20, 48 + 104 + 5, 70 + 122 + 96 + 10) == 10, "a pixel in its name is tile 10 too");
		Check(ShapePicker_TileAt(g, 0, 20, 48 + 100, 75) == -1 && ShapePicker_TileAt(g, 0, 20, 48 + 5, 70 + 96 + 18 + 3) == -1, "a pixel between tiles is none");
		Check(ShapePicker_TileAt(g, 0, 8, 48 + 104 * 8 + 5, 75) == -1 && ShapePicker_TileAt(g, 9, 20, 48 + 5, 75) == 9, "past the last tile is none; the first row from 9 on is tile 9");
	}

	// ---- (3) THE KEYS AND THE WHEEL
	{
		ShapePickerLayout const g = ShapePicker_Layout(1024, 768);   // 9 by 5, 45 a page
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

	printf(checksFailed ? "host_shape_picker_test: %d FAILED\n" : "host_shape_picker_test: all passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
