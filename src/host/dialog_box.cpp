// See dialog_box.h. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "dialog_box.h"

#include "screen_names.h"

namespace host
{
	namespace
	{
		size_t const kMostFieldLetters = 10;   // FUN_00474350 builds its fields with FUN_0057de30, an edit of up to 10 letters

		// NO FULL PATH ON THE SCREEN (Drake 2026-11-18, Bishop's privacy item for alpha.1: Sound Edit's title showed "C:\Users\<name>\..."): the words a dialog SHOWS, its title, its
		// text, its labels, buttons, check boxes and group boxes, have every full path cut to its file's or folder's name (screen_names.h). An edit's contents and a combo's rows are
		// left as they are: they are what the user types or picks, and what the dialog gives back.
		void CutPathsShown(std::vector<DialogWidget> *widgets)
		{
			for (DialogWidget &w : *widgets)
			{
				if (w.type == DialogWidgetType::Label || w.type == DialogWidgetType::Button || w.type == DialogWidgetType::Check || w.type == DialogWidgetType::Group)
				{
					w.text = ScreenNames_Cut(w.text);
				}
			}
		}

		bool Holds(DialogRect const &r, int32_t x, int32_t y)
		{
			return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
		}

		DialogRect Placed(DialogRect const &window, int32_t x, int32_t y, int32_t width, int32_t height)
		{
			DialogRect r;

			r.x = window.x + x;
			r.y = window.y + y;
			r.width = width;
			r.height = height;
			return r;
		}

		// The text broken at spaces (and at the new lines in it) so that no line is wider than `width`; a word wider than the area goes on a line of its own
		std::vector<std::string> Broken(std::string const &text, int32_t width, std::function<int32_t(std::string const &)> const &textWidth)
		{
			std::vector<std::string> lines;
			std::string line;
			std::string word;

			auto const flushWord = [&]()
			{
				if (word.empty())
				{
					return;
				}

				std::string const joined = line.empty() ? word : line + " " + word;

				if (!line.empty() && textWidth(joined) > width)
				{
					lines.push_back(line);
					line = word;
				}
				else
				{
					line = joined;
				}

				word.clear();
			};

			for (char const letter : text)
			{
				if (letter == '\n')
				{
					flushWord();
					lines.push_back(line);
					line.clear();
				}
				else if (letter == ' ')
				{
					flushWord();
				}
				else
				{
					word += letter;
				}
			}

			flushWord();

			if (!line.empty() || lines.empty())
			{
				lines.push_back(line);
			}

			return lines;
		}
	}

	void InDialog::Layout(DialogRect screen, int32_t width, int32_t height, std::function<int32_t(std::string const &)> const &textWidth)
	{
		// FUN_0057c800 0x57c80e-0x57c82d: the window in the middle of the screen
		window.width = width;
		window.height = height;
		window.x = screen.x + (screen.width - width) / 2;
		window.y = screen.y + (screen.height - height) / 2;
		textArea = Placed(window, 0x12, 0x12, 0x172, kind == DialogKind::Text ? 0x50 : 100);
		lines = Broken(text, textArea.width, textWidth);
		open = true;
		focus = 0;
	}

	void InDialog::OpenMessage(std::string const &message, DialogRect screen, std::function<int32_t(std::string const &)> const &textWidth)
	{
		kind = DialogKind::Message;
		text = ScreenNames_Cut(message);
		buttons.clear();
		fields.clear();
		Layout(screen, 400, 200, textWidth);

		DialogButton ok;

		ok.label = "Ok";
		ok.box = Placed(window, 200, 0x96, 0x32, 0x10);
		ok.answer = kDialogOk;
		buttons.push_back(ok);
	}

	void InDialog::OpenConfirm(std::string const &message, DialogRect screen, std::function<int32_t(std::string const &)> const &textWidth)
	{
		kind = DialogKind::Confirm;
		text = ScreenNames_Cut(message);
		buttons.clear();
		fields.clear();
		Layout(screen, 400, 200, textWidth);

		DialogButton yes, no;

		yes.label = "Yes";
		yes.box = Placed(window, 100, 0x96, 0x32, 0x10);
		yes.answer = kDialogOk;
		no.label = "No";
		no.box = Placed(window, 200, 0x96, 0x32, 0x10);
		no.answer = kDialogNo;
		buttons.push_back(yes);
		buttons.push_back(no);
	}

	void InDialog::OpenText(std::string const &message, std::vector<DialogField> const &given, DialogRect screen, std::function<int32_t(std::string const &)> const &textWidth)
	{
		kind = DialogKind::Text;
		text = ScreenNames_Cut(message);
		buttons.clear();
		fields.clear();

		// DECIDED (Bishop's Q9c, 11-17; the Strategy box batch): ONE field with no label is inteng's number / text box FUN_00474170 (whole number), FUN_004747f0 (decimal) or FUN_00474bc0 (a
		// name): 400 x 200 (0x47419c, 0x47481c, 0x474bec), the text at 0x12, 0x12 in 0x172 x 0x28 (0x4741d6), the field at 0x1e, 100, 300 x 0x12 (0x47420c), Ok at 100, 0x96 and Cancel at
		// 200, 0x96, 0x32 x 0x10 (0x474248 / 0x474287). Fields with labels, or more than one, are FUN_00474350's box: 400 x 250 (0x47437c), the text 0x50 high, each field 0x96 wide with its
		// label at 0xbe (0x4743fd / 0x474438), the next at 0x7c. (Was: every text box 400 x 250.)
		if (given.size() == 1 && given[0].label.empty())
		{
			Layout(screen, 400, 200, textWidth);
			textArea = Placed(window, 0x12, 0x12, 0x172, 0x28);

			DialogField f = given[0];

			f.label = ScreenNames_Cut(f.label);   // shown words (the edit's own text is the user's)
			f.edit = Placed(window, 0x1e, 100, 300, 0x12);
			f.labelBox = Placed(window, 0x1e + 300, 100, 0, 0x12);
			fields.push_back(f);

			DialogButton ok, cancel;

			ok.label = "Ok";
			ok.box = Placed(window, 100, 0x96, 0x32, 0x10);
			ok.answer = kDialogOk;
			cancel.label = "Cancel";
			cancel.box = Placed(window, 200, 0x96, 0x32, 0x10);
			cancel.answer = kDialogEscape;
			buttons.push_back(ok);
			buttons.push_back(cancel);
			return;
		}

		Layout(screen, 400, 0xfa, textWidth);

		int32_t const rows[3] = { 100, 0x7c, 0x94 };

		for (size_t i = 0; i < given.size() && i < 3; i++)
		{
			DialogField f = given[i];

			f.label = ScreenNames_Cut(f.label);
			f.edit = Placed(window, 0x1e, rows[i], 0x96, 0x12);
			f.labelBox = Placed(window, 0xbe, rows[i], 0x96, 0x12);
			fields.push_back(f);
		}

		DialogButton ok, cancel;

		ok.label = "Ok";
		ok.box = Placed(window, 100, 0xbe, 0x32, 0x10);
		ok.answer = kDialogOk;
		cancel.label = "Cancel";
		cancel.box = Placed(window, 200, 0xbe, 0x32, 0x10);
		cancel.answer = kDialogEscape;
		buttons.push_back(ok);
		buttons.push_back(cancel);
	}

	void InDialog::OpenForm(DialogFormSpec const &spec, DialogRect screen, std::function<int32_t(std::string const &)> const &textWidth)
	{
		kind = DialogKind::Form;
		text.clear();
		title = ScreenNames_Cut(spec.title);
		buttons.clear();
		fields.clear();
		widgets = spec.widgets;
		CutPathsShown(&widgets);
		changeHook = spec.onChange;
		checkHook = spec.onCheck;
		Layout(screen, spec.width, spec.height, textWidth);
		lines.clear();
		focus = widgets.size();

		for (size_t i = 0; i < widgets.size(); i++)
		{
			DialogRect const given = widgets[i].box;

			widgets[i].box = Placed(window, given.x, given.y, given.width, given.height);

			if (widgets[i].type == DialogWidgetType::Edit && focus == widgets.size())
			{
				focus = i;
			}

			// a combo's list never runs past the screen's foot, whatever its own rows (the list that scrolls, dialog_box.h)
			if (widgets[i].type == DialogWidgetType::Combo && widgets[i].box.height > 0)
			{
				int32_t const below = screen.y + screen.height - (widgets[i].box.y + widgets[i].box.height);

				widgets[i].listFit = below / widgets[i].box.height > 1 ? below / widgets[i].box.height : 1;
			}
		}
	}

	int32_t DialogBox_ComboListRows(int32_t resourceHeight)
	{
		// (see dialog_box.h: the resource's height in pixels, less the closed box's 21 and the list's frame, in whole rows of 13)
		int32_t const rows = (resourceHeight * 13 / 8 - 21 - 2) / 13;

		return rows < 1 ? 1 : rows;
	}

	size_t InDialog::ComboRowsShown(DialogWidget const &combo)
	{
		size_t shown = combo.items.size();

		shown = combo.listRows > 0 && (size_t)combo.listRows < shown ? (size_t)combo.listRows : shown;
		shown = combo.listFit > 0 && (size_t)combo.listFit < shown ? (size_t)combo.listFit : shown;
		return shown;
	}

	bool InDialog::ComboScrolls(DialogWidget const &combo)
	{
		return ComboRowsShown(combo) < combo.items.size();
	}

	namespace
	{
		// The first row in view: what the list holds, kept so the rows in view are always whole (no blank rows under the last one)
		size_t TopRow(DialogWidget const &combo)
		{
			size_t const shown = InDialog::ComboRowsShown(combo);
			size_t const most = combo.items.size() - shown;

			return combo.listTop < most ? combo.listTop : most;
		}

		// The list scrolled so a row is in view (it moves as little as it can, as Windows' list boxes do)
		void Reveal(DialogWidget *combo, size_t row)
		{
			size_t const shown = InDialog::ComboRowsShown(*combo);
			size_t top = TopRow(*combo);

			if (shown == 0)
			{
				return;
			}

			top = row < top ? row : top;
			top = row >= top + shown ? row - shown + 1 : top;
			combo->listTop = top;
		}

		// The list scrolled by a number of rows (less than 0 toward its top), kept inside the list
		void ScrollRows(DialogWidget *combo, int64_t rows)
		{
			int64_t const most = (int64_t)(combo->items.size() - InDialog::ComboRowsShown(*combo));
			int64_t top = (int64_t)TopRow(*combo) + rows;

			top = top < 0 ? 0 : top;
			top = top > most ? most : top;
			combo->listTop = (size_t)top;
		}
	}

	DialogRect InDialog::ComboRow(DialogWidget const &combo, size_t row)
	{
		DialogRect r;
		size_t const top = TopRow(combo);

		if (row < top || row >= top + ComboRowsShown(combo))
		{
			return r;   // out of view: 0 wide, not drawn, not pressed
		}

		r.x = combo.box.x;
		r.y = combo.box.y + combo.box.height * (int32_t)(row - top + 1);
		r.width = combo.box.width - (ComboScrolls(combo) ? kDialogScrollBarWidth : 0);
		r.height = combo.box.height;
		return r;
	}

	DialogScrollBar InDialog::ComboScrollBar(DialogWidget const &combo)
	{
		DialogScrollBar bar;

		if (!ComboScrolls(combo))
		{
			return bar;
		}

		int32_t const rowHeight = combo.box.height;
		int32_t const shown = (int32_t)ComboRowsShown(combo);
		int64_t const all = (int64_t)combo.items.size();

		// the column beside the rows in view, from the first row's top to the last one's foot; an arrow a row high at each end
		bar.column = { combo.box.x + combo.box.width - kDialogScrollBarWidth, combo.box.y + rowHeight, kDialogScrollBarWidth, rowHeight * shown };
		bar.up = { bar.column.x, bar.column.y, kDialogScrollBarWidth, rowHeight };
		bar.down = { bar.column.x, bar.column.y + bar.column.height - rowHeight, kDialogScrollBarWidth, rowHeight };

		// the thumb between them: as long as the share of the list in view (at least 8 pixels), as far down the track as the first row in view is down the list
		int32_t const trackTop = bar.up.y + bar.up.height, track = bar.down.y - trackTop;

		if (track >= 8)
		{
			int32_t length = (int32_t)((int64_t)track * shown / all);
			int64_t const most = all - shown;

			length = length < 8 ? 8 : length;
			bar.thumb = { bar.column.x, trackTop + (int32_t)((int64_t)(track - length) * (int64_t)TopRow(combo) / (most > 0 ? most : 1)), kDialogScrollBarWidth, length };
		}

		return bar;
	}

	DialogWidget *InDialog::OpenList(size_t *index)
	{
		for (size_t i = 0; open && kind == DialogKind::Form && i < widgets.size(); i++)
		{
			if (widgets[i].type == DialogWidgetType::Combo && widgets[i].visible && widgets[i].listOpen)
			{
				*index = i;
				return &widgets[i];
			}
		}

		return nullptr;
	}

	bool InDialog::ListOpen() const
	{
		for (DialogWidget const &w : widgets)
		{
			if (open && kind == DialogKind::Form && w.type == DialogWidgetType::Combo && w.visible && w.listOpen)
			{
				return true;
			}
		}

		return false;
	}

	void InDialog::PickRow(size_t index, size_t row)
	{
		DialogWidget &combo = widgets[index];
		bool const changed = combo.selected != (int32_t)row;

		combo.listOpen = false;
		combo.selected = (int32_t)row;
		combo.text = combo.items[row];

		if (changed && changeHook)
		{
			changeHook(widgets, index);
			CutPathsShown(&widgets);   // the hook may have given parts new words
		}
	}

	bool InDialog::Wheel(int32_t delta)
	{
		size_t index = 0;
		DialogWidget *const combo = OpenList(&index);

		if (!combo)
		{
			wheelLeft = 0;
			return false;
		}

		// 3 rows a whole notch; a part of a notch is kept until the turn adds up to a row (more than 0 turns toward the top)
		int64_t const turned = (int64_t)wheelLeft + delta;
		int64_t const rows = turned * kDialogWheelRows / kDialogWheelNotch;

		wheelLeft = (int32_t)(turned - rows * kDialogWheelNotch / kDialogWheelRows);
		ScrollRows(combo, -rows);
		return true;
	}

	bool InDialog::ListKey(DialogListKey key)
	{
		size_t index = 0;
		DialogWidget *const combo = OpenList(&index);

		if (!combo || combo->items.empty())
		{
			return combo != nullptr;
		}

		int64_t const last = (int64_t)combo->items.size() - 1;
		int64_t const page = (int64_t)ComboRowsShown(*combo) > 1 ? (int64_t)ComboRowsShown(*combo) - 1 : 1;
		int64_t lit = combo->listLit;

		switch (key)
		{
			case DialogListKey::Up: lit = lit < 0 ? 0 : lit - 1; break;
			case DialogListKey::Down: lit = lit + 1; break;
			case DialogListKey::PageUp: lit = lit < 0 ? 0 : lit - page; break;
			case DialogListKey::PageDown: lit = lit < 0 ? page : lit + page; break;
			case DialogListKey::Home: lit = 0; break;
			case DialogListKey::End: lit = last; break;
		}

		lit = lit < 0 ? 0 : (lit > last ? last : lit);
		combo->listLit = (int32_t)lit;
		Reveal(combo, (size_t)lit);
		return true;
	}

	bool InDialog::ListReturn()
	{
		size_t index = 0;
		DialogWidget *const combo = OpenList(&index);

		if (!combo)
		{
			return false;
		}

		if (combo->listLit >= 0 && (size_t)combo->listLit < combo->items.size())
		{
			PickRow(index, (size_t)combo->listLit);
		}
		else
		{
			combo->listOpen = false;
		}

		return true;
	}

	bool InDialog::ListEscape()
	{
		size_t index = 0;
		DialogWidget *const combo = OpenList(&index);

		if (!combo)
		{
			return false;
		}

		combo->listOpen = false;
		return true;
	}

	void InDialog::Close()
	{
		changeHook = nullptr;
		checkHook = nullptr;
		wheelLeft = 0;
		open = false;
		buttons.clear();
		fields.clear();
		widgets.clear();
		lines.clear();
	}

	bool InDialog::Press(int32_t x, int32_t y, int32_t *answer)
	{
		if (!open)
		{
			return false;
		}

		for (DialogButton const &b : buttons)
		{
			if (Holds(b.box, x, y))
			{
				*answer = b.answer;
				return true;
			}
		}

		// an open list takes the press first: its scroll bar scrolls it and it stays open, a row in view picks, and any other press closes it (a press on the combo's own box only
		// closes it)
		for (size_t i = 0; i < widgets.size(); i++)
		{
			DialogWidget &combo = widgets[i];

			if (combo.type != DialogWidgetType::Combo || !combo.visible || !combo.listOpen)
			{
				continue;
			}

			// THE SCROLL BAR (the list that scrolls, dialog_box.h): an arrow scrolls a row, the bar above or below the thumb a page (the rows in view), the thumb itself nothing (it
			// isn't dragged); the list stays open
			DialogScrollBar const bar = ComboScrollBar(combo);

			if (bar.column.width > 0 && Holds(bar.column, x, y))
			{
				int64_t const page = (int64_t)ComboRowsShown(combo);

				if (Holds(bar.up, x, y))
				{
					ScrollRows(&combo, -1);
				}
				else if (Holds(bar.down, x, y))
				{
					ScrollRows(&combo, 1);
				}
				else if (bar.thumb.height > 0 && y < bar.thumb.y)
				{
					ScrollRows(&combo, -page);
				}
				else if (bar.thumb.height > 0 && y >= bar.thumb.y + bar.thumb.height)
				{
					ScrollRows(&combo, page);
				}

				return false;
			}

			combo.listOpen = false;

			for (size_t row = 0; row < combo.items.size(); row++)
			{
				if (Holds(ComboRow(combo, row), x, y))
				{
					PickRow(i, row);
					return false;
				}
			}

			if (Holds(combo.box, x, y))
			{
				return false;
			}
		}

		for (size_t i = 0; i < widgets.size(); i++)
		{
			DialogWidget &w = widgets[i];

			if (!w.visible || !w.enabled || !Holds(w.box, x, y))
			{
				continue;
			}

			if (w.type == DialogWidgetType::Combo)
			{
				// an editable combo's box (left of its arrow) takes the letters; its arrow, or a plain combo anywhere, opens the list
				if (w.editable && x < w.box.x + w.box.width - 16)
				{
					focus = i;
					continue;
				}

				// it opens scrolled so the picked row is its top row, or as near as the list's end lets it (Windows' drop-down: the list's top index set to the picked row,
				// LB_SETTOPINDEX in Wine's combo.c CBDropDown), the keys on that row
				w.listOpen = !w.items.empty();
				w.listLit = w.selected >= 0 && (size_t)w.selected < w.items.size() ? w.selected : -1;
				w.listTop = w.listLit >= 0 ? (size_t)w.listLit : 0;
				w.listTop = TopRow(w);
				wheelLeft = 0;
				continue;
			}

			if (w.type == DialogWidgetType::Button)
			{
				*answer = w.answer;
				return true;
			}

			if (w.type == DialogWidgetType::Edit)
			{
				focus = i;
			}
			else if (w.type == DialogWidgetType::Check)
			{
				if (w.group >= 0)
				{
					for (DialogWidget &other : widgets)
					{
						if (other.type == DialogWidgetType::Check && other.group == w.group)
						{
							other.checked = false;
						}
					}

					w.checked = true;
				}
				else
				{
					w.checked = !w.checked;
				}

				if (checkHook)
				{
					checkHook(widgets, i);
					CutPathsShown(&widgets);
				}
			}
		}

		for (size_t i = 0; i < fields.size(); i++)
		{
			if (Holds(fields[i].edit, x, y))
			{
				focus = i;
			}
		}

		return false;
	}

	void InDialog::Type(char letter)
	{
		if (open && kind == DialogKind::Form && focus < widgets.size() && widgets[focus].type == DialogWidgetType::Edit && (unsigned char)letter >= 32 && (unsigned char)letter <= 126 && widgets[focus].text.size() < widgets[focus].most)
		{
			widgets[focus].text += letter;
			return;
		}

		if (open && kind == DialogKind::Form && focus < widgets.size() && widgets[focus].type == DialogWidgetType::Combo && widgets[focus].editable && (unsigned char)letter >= 32 &&
			(unsigned char)letter <= 126 && widgets[focus].text.size() < widgets[focus].most)
		{
			widgets[focus].text += letter;
			widgets[focus].selected = -1;
			return;
		}

		if (open && kind == DialogKind::Text && focus < fields.size() && (unsigned char)letter >= 32 && (unsigned char)letter <= 126 && fields[focus].text.size() < kMostFieldLetters)
		{
			fields[focus].text += letter;
		}
	}

	void InDialog::Backspace()
	{
		if (open && kind == DialogKind::Form && focus < widgets.size() && (widgets[focus].type == DialogWidgetType::Edit || (widgets[focus].type == DialogWidgetType::Combo && widgets[focus].editable)) &&
			!widgets[focus].text.empty())
		{
			widgets[focus].text.pop_back();

			if (widgets[focus].type == DialogWidgetType::Combo)
			{
				widgets[focus].selected = -1;
			}

			return;
		}

		if (open && kind == DialogKind::Text && focus < fields.size() && !fields[focus].text.empty())
		{
			fields[focus].text.pop_back();
		}
	}

	void InDialog::NextField()
	{
		if (open && kind == DialogKind::Form)
		{
			for (size_t step = 1; step <= widgets.size(); step++)
			{
				size_t const at = (focus + step) % widgets.size();

				if (widgets[at].type == DialogWidgetType::Edit)
				{
					focus = at;
					return;
				}
			}

			return;
		}

		if (open && !fields.empty())
		{
			focus = (focus + 1) % fields.size();
		}
	}
}
