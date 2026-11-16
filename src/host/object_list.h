// object_list.h: the object list, in the list panel: one row for each object of the open level, which the mapper moves through with
// the keyboard or a click. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It's the "object list" among the fixed panels (R-INT-6). This is its model, window-free and tested: the rows' names, the scroll list
// that says which row is chosen and which show (src/ui/scroll_list.h), and what each shown row draws (panel_paint.h's lines).
//
// EVERYTHING FROM A LEVEL IS HOSTILE.
// - The names come from the level file as the game loaded it (level_objects.h reads them). Each is read only to its first 0 and to at
//   most kObjectNameMost bytes, through AvPx's OnlineScreens_SafeName, so a name with no end can't be read past, and every byte the font
//   has no letter for becomes a '?'. That safe copy is the only one kept; the level's own bytes are never shown, measured or used as a
//   format.
// - The count is the level's: a level can have thousands of objects, so the names live in storage sized to the count when the list is
//   loaded, up to kObjectListMost. A level with more is cut there, and the list says so (Cut). Nothing has a fixed size a level could
//   overrun, and the scroll list's own bound decides which rows show.
// - Loading again, for another level or the same one again, starts the list again: no row chosen but the first, and the first row shown.
//   A level with no objects shows an empty list, with none chosen.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "panel_layout.h"
#include "panel_paint.h"
#include "scroll_list.h"
#include "ui_event.h"

namespace host
{
	// The most bytes of an object's name that are read: the game's own limit for an object's name, 63 (Vasquez's name survey)
	size_t const kObjectNameMost = 63;

	// The most objects the list holds. The largest stock level has a few thousand; more than this is a damaged or hostile file.
	size_t const kObjectListMost = 1u << 20;

	// The bytes of an object's ID: the same object in the list and in a pick in the level's view is known by these 8 bytes, never by its
	// place, as the two read the level in different orders (selection.h)
	size_t const kObjectIdBytes = 8;

	// One object of the level, as the level gives it: a pointer to its name's own bytes, and its ID
	struct LevelObject
	{
		const char *name;
		uint8_t id[kObjectIdBytes];
	};

	class ObjectList
	{
	public:
		ObjectList();

		// Loads the level's objects: count of them, their names read as the rules say, their IDs kept as they are. A null name is shown
		// as an empty row, and no list at all as an empty list. The list starts again.
		void Load(const LevelObject *objects, size_t count);

		// A row's ID, or null for a row that isn't there
		const uint8_t *Id(size_t row) const;

		// The first row with this ID, or -1 when no row has it
		int32_t FindById(const uint8_t *id) const;

		// How many rows there are, and whether the level had more than the list holds
		size_t Count() const { return rows.size(); }
		bool Cut() const { return cut; }

		// A row's name as it's shown: safe, up to kObjectNameMost letters. An empty text for a row that isn't there.
		const std::string &Name(size_t row) const;

		// Where the list goes this frame, and how tall one row is: the scroll list then knows how many rows show at once
		void LayOut(const ui::Box &box, int32_t rowHeight);

		// A key for the list: Up, Down, Page Up, Page Down, Home and End. Gives back whether another row is chosen.
		bool Handle(const ui::Event &event);

		// A press of the mouse at a place in the window: on a row, that row is chosen, and onRow says so, even when that row was chosen
		// already (the editor then selects its object, which may not be what's selected now). Gives back whether the press was in the
		// list's box (taken by the list even when it's on no row).
		bool Press(int32_t x, int32_t y, bool *onRow);

		// The chosen row, or -1 when there are none; the first row shown, and how many show
		int32_t Chosen() const { return list.Chosen(); }
		int32_t First() const { return list.First(); }
		int32_t Shown() const;

		// Chooses a row from outside, such as a pick in the level's view. A row that isn't there changes nothing: while there are rows,
		// one is always chosen (scroll_list.h), and "nothing picked" is the editor's selection's to say, not the list's.
		void Choose(int32_t row);

		// What the list shows this frame: a band behind the chosen row, and each shown row's name, cut to its row. Gives back how many
		// lines and boxes it wrote, never more than the room given.
		void Plan(PaintLetterWidth widthOf, int32_t lineHeight, uint32_t chosenColour, PaintText *lines, size_t mostLines, size_t *lineCount,
			PaintBox *boxes, size_t mostBoxes, size_t *boxCount) const;

	private:
		struct Row
		{
			std::string name;
			uint8_t id[kObjectIdBytes];
		};

		std::vector<Row> rows;
		bool cut;
		ui::ScrollList list;
		ui::Box box;
		int32_t rowHeight;
	};
}
