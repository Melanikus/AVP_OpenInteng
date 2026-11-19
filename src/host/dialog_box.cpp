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
		}
	}

	DialogRect InDialog::ComboRow(DialogWidget const &combo, size_t row)
	{
		DialogRect r;

		r.x = combo.box.x;
		r.y = combo.box.y + combo.box.height * (int32_t)(row + 1);
		r.width = combo.box.width;
		r.height = combo.box.height;
		return r;
	}

	void InDialog::Close()
	{
		changeHook = nullptr;
		checkHook = nullptr;
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

		// an open list takes the press first: a row picks, and any other press closes it (a press on the combo's own box only closes it)
		for (size_t i = 0; i < widgets.size(); i++)
		{
			DialogWidget &combo = widgets[i];

			if (combo.type != DialogWidgetType::Combo || !combo.visible || !combo.listOpen)
			{
				continue;
			}

			combo.listOpen = false;

			for (size_t row = 0; row < combo.items.size(); row++)
			{
				if (Holds(ComboRow(combo, row), x, y))
				{
					bool const changed = combo.selected != (int32_t)row;

					combo.selected = (int32_t)row;
					combo.text = combo.items[row];

					if (changed && changeHook)
					{
						changeHook(widgets, i);
						CutPathsShown(&widgets);   // the hook may have given parts new words
					}

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

				w.listOpen = !w.items.empty();
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
