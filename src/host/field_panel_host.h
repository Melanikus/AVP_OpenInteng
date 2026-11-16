// field_panel_host.h: the field panel in the editor's window: it shows the fields of whatever is selected (an object, its strategy, a
// generator, a light...), with the old editor's labels and limits, drawn in the panels' fields place. Written 2026-11-14 by Melanikus
// (Dietrich) for AVP_OpenInteng (the two-dev sprint: the front end of every inspect row). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHO GIVES IT WHAT. The back end (Drake's adapters: an object's header, its AVPSTRAT strategy, and the rest, one adapter per chunk kind)
// turns what's selected into a record: a title, a list of ui::FieldDescriptor (field_descriptor.h) and one ui::FieldValue each
// (field_panel.h), built from the field model's tables (src/model/field_model.h). The host shows that record. It knows nothing of chunks.
//
// THE RULES.
// - A record is checked whole when it's shown (ui::CheckFieldList, through ui::FieldPanel::Load). A list the check refuses shows
//   nothing, and Show says why: a refused record is a mistake in an adapter, never something to show half of.
// - The host keeps its own copy of the descriptor list and of the values, so an adapter may build them on the fly. The words, choices,
//   units and checks a descriptor POINTS TO are not copied: they must stay where they are while the record is shown. The field model's
//   own tables do (they're static), and nothing else should be pointed to.
// - INSPECT IS READ-ONLY. Show takes a record as read-only whatever its descriptors say: every field is shown, can take focus to be read
//   and copied, and is never changed. The edit path (a checked change, through the field model's refusals, written back through F7's
//   MarkChanged) comes as its own call, with its own tests.
// - The title is the editor's own words or the level's (a name): it's read only through the safe text (AvPx's OnlineScreens_SafeName),
//   to its first 0 and at most kFieldPanelHostTitleMost bytes, so a name from a hostile level can't be read past or shown as anything.
// - Drawn as one line a field, "label: value", the title first, each cut to the box inside the margins every panel's text keeps
//   (panel_paint.h): a box holds as many lines as fit between its top and bottom margins. A record with more fields than fit shows the
//   ones that fit, from the first, and says how many more there are on its last line.
// - A field's value is never cut while it can be shown whole: a line too wide cuts its label with the game's "..", and keeps the "!"
//   mark of a flagged value and ": value" whole. Only a value too wide even then is cut, with "..", so the cut is seen.
// - At most kFieldPanelHostMostFields fields in a record; a longer one is refused as a whole.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "field_descriptor.h"
#include "field_panel.h"
#include "panel_layout.h"
#include "panel_paint.h"

namespace host
{
	// The most fields one record shows, and the most bytes of its title that are read
	size_t const kFieldPanelHostMostFields = 64;
	size_t const kFieldPanelHostTitleMost = 63;

	class FieldPanelHost
	{
	public:
		FieldPanelHost();

		// Shows a record, read-only: its title, and count fields from list with count values. Gives back false, and shows nothing, for
		// no list where count isn't 0, more than kFieldPanelHostMostFields fields, or a list the panel's check refuses (problem, when
		// not null, says which field and why). A record of no fields shows its title alone. With editable true (the edit path, S3d), each
		// field is shown as its descriptor says: one the record opened (record_edit.h) can be changed, and the rest stay read-only. A record
		// made without record_edit.h has every field read-only, so editable changes nothing for it.
		bool Show(const char *title, const ui::FieldDescriptor *list, size_t count, const ui::FieldValue *values, ui::FieldListProblem *problem,
			bool editable = false);

		// Whether the keyboard is in the panel (the editor says so): the field with focus is then marked with ">" in front of its label, so a
		// mapper can see where typing goes. Typing is the editor's business; this only draws the mark. (Added 2026-11-15, S3d.)
		void SetTyping(bool typing) { this->typing = typing; }
		bool Typing() const { return typing; }

		// The field with focus, or -1 (none shown, or a record with no fields)
		int32_t FocusedField() const { return showing ? panel.Focused() : -1; }

		// Shows nothing: nothing is selected
		void Clear();

		// Whether a record is shown, its title as shown, and the panel that holds its fields (for reading them: the host owns it)
		bool Showing() const { return showing; }
		const std::string &Title() const { return title; }
		const ui::FieldPanel &Panel() const { return panel; }

		// Where it's drawn this frame (the whole box, margins included), and how tall one line is (without its margins)
		void LayOut(const ui::Box &box, int32_t lineHeight);

		// How tall a box has to be to show the whole record: one line height for the title and one for each field, and the margins above and
		// below. 0 when nothing is shown or a line has no height; never more than an int holds. The editor gives the host this much of the
		// list's column, or what's there (host_frame.h, HostFrame_ShareTheColumn).
		int32_t HeightFor(int32_t lineHeight) const;

		// What it draws this frame: the title, then "label: value" for each field that fits, each fitted to its line with its label cut
		// first, and "... and N more" on the last line when some don't fit. Gives back how many lines it wrote, never more than mostLines.
		void Plan(PaintLetterWidth widthOf, PaintText *lines, size_t mostLines, size_t *lineCount) const;

		// The field a place in the window is on, or -1 for none (the title, a place past the last field shown, or outside the box)
		int32_t FieldAt(int32_t x, int32_t y) const;

		// An event for the field with focus, and a click on a field (FieldAt's answer): Tab and Shift+Tab, and a click, move focus, so the
		// mapper can read each field's help line on the status line. Anything that would change a field is refused, with the panel's own
		// words (ui::kPanelReadOnly) on its status line, and nothing changes. With no record shown, they mean nothing. In a record shown
		// editable, a field the record opened takes typing as the ui kit's panel does, and gives back Kept when a value is kept: the editor
		// then applies it (record_edit.h, ApplyKeptStrategyField).
		ui::PanelAnswer Handle(const ui::Event &event);
		ui::PanelAnswer ClickField(size_t field);

	private:
		FieldPanelHost(const FieldPanelHost &) = delete;
		FieldPanelHost &operator=(const FieldPanelHost &) = delete;

		bool showing;
		bool typing;       // the keyboard is in the panel: the focused field is marked
		std::string title;
		std::vector<ui::FieldDescriptor> descriptors;   // the host's own copy of the record's list, read-only
		ui::FieldPanel panel;
		ui::Box box;
		int32_t lineHeight;
	};
}
