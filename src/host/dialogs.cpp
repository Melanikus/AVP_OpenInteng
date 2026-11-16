// See dialogs.h. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "dialogs.h"

#include "panel_pass.h"

namespace host
{
	namespace
	{
		InDialog box;
		std::function<void(int32_t, std::vector<std::string> const &)> boxDone;     // message, confirm, text
		std::function<void(int32_t, std::vector<DialogWidget> const &)> formDone;   // form
		ListPicker list;
		std::function<void(int32_t)> listDone;
		std::vector<size_t> listOrder;   // the rows as the dialog shows them (sorted): the place each had in the rows given
		DialogRect screen = { 0, 0, 640, 480 };

		int32_t TextWidth(std::string const &text)
		{
			int32_t w = 0;

			for (size_t c = 0; c < text.size(); c++)
			{
				w += PanelPass_SmallWidth((unsigned char)text[c]);
			}

			return w;
		}

		std::string Word(std::string const &label, DialogRect const &r)
		{
			std::string word;

			for (char c : label)
			{
				word += c == ' ' ? '_' : (c == '"' || c == '|' ? '?' : c);
			}

			return word + " " + std::to_string(r.x) + " " + std::to_string(r.y) + " " + std::to_string(r.width) + " " + std::to_string(r.height);
		}

		void Answered(int32_t answer)
		{
			std::vector<std::string> texts;
			std::vector<DialogWidget> widgets = box.Widgets();
			bool const form = box.Kind() == DialogKind::Form;

			for (DialogField const &f : box.Fields())
			{
				texts.push_back(f.text);
			}

			auto const done = boxDone;
			auto const formThen = formDone;

			box.Close();
			boxDone = nullptr;
			formDone = nullptr;

			if (form)
			{
				if (formThen)
				{
					formThen(answer, widgets);
				}
			}
			else if (done)
			{
				done(answer, texts);
			}
		}

		void ListAnswered(int32_t shownPlace)
		{
			auto const done = listDone;
			int32_t given = -1;

			if (shownPlace >= 0 && (size_t)shownPlace < listOrder.size())
			{
				given = (int32_t)listOrder[(size_t)shownPlace];
			}

			list.Close();
			listDone = nullptr;

			if (done)
			{
				done(given);
			}
		}
	}

	void Dialogs_Message(std::string const &text, std::function<void()> const &then)
	{
		box.OpenMessage(text, screen, TextWidth);
		formDone = nullptr;
		boxDone = [then](int32_t, std::vector<std::string> const &) { if (then) { then(); } };
	}

	void Dialogs_Confirm(std::string const &text, std::function<void(bool)> const &then)
	{
		box.OpenConfirm(text, screen, TextWidth);
		formDone = nullptr;
		boxDone = [then](int32_t answer, std::vector<std::string> const &) { then(answer == kDialogOk); };
	}

	void Dialogs_Text(std::string const &text, std::vector<DialogField> const &fields, std::function<void(int32_t, std::vector<std::string> const &)> const &then)
	{
		box.OpenText(text, fields, screen, TextWidth);
		formDone = nullptr;
		boxDone = then;
	}

	void Dialogs_Form(DialogFormSpec const &spec, std::function<void(int32_t, std::vector<DialogWidget> const &)> const &then)
	{
		box.OpenForm(spec, screen, TextWidth);
		boxDone = nullptr;
		formDone = then;
	}

	// FUN_00475bf0 0x476403-0x476670: the rows are put in an ascending list by FUN_0053ed50 (a byte-by-byte comparison), each after the ones that are not above it
	void Dialogs_List(std::string const &title, std::vector<std::string> const &rows, std::function<void(int32_t)> const &then)
	{
		listOrder.clear();

		for (size_t i = 0; i < rows.size(); i++)
		{
			size_t at = listOrder.size();

			while (at > 0 && rows[listOrder[at - 1]].compare(rows[i]) > 0)
			{
				at--;
			}

			listOrder.insert(listOrder.begin() + (std::ptrdiff_t)at, i);
		}

		std::vector<std::string> shown;

		for (size_t place : listOrder)
		{
			shown.push_back(rows[place]);
		}

		list.Open(title, shown, screen.width, screen.height, TextWidth, screen.x, screen.y);
		listDone = then;
	}

	void Dialogs_DismissList()
	{
		if (list.IsOpen())
		{
			list.Close();
			listDone = nullptr;
		}
	}

	bool Dialogs_IsOpen()
	{
		return box.IsOpen() || list.IsOpen();
	}

	void Dialogs_SetScreen(int32_t x, int32_t y, int32_t width, int32_t height)
	{
		screen.x = x;
		screen.y = y;
		screen.width = width > 0 ? width : 640;
		screen.height = height > 0 ? height : 480;
	}

	bool Dialogs_Press(int32_t x, int32_t y)
	{
		int32_t answer = 0;

		if (box.IsOpen())
		{
			if (box.Press(x, y, &answer))
			{
				Answered(answer);
			}

			return true;
		}

		if (list.IsOpen())
		{
			int32_t choice = -1;

			if (list.Press(x, y, &choice) == ListPickerHit::Choice && choice >= 0)
			{
				ListAnswered(choice);
			}

			return true;
		}

		return false;
	}

	bool Dialogs_Key(bool isChar, uint32_t key)
	{
		// the virtual keys the dialogs read (windows.h values, so this file needs no windows.h)
		uint32_t const kBack = 0x08, kTab = 0x09, kReturn = 0x0D, kEscape = 0x1B, kPrior = 0x21, kNext = 0x22, kEnd = 0x23, kHome = 0x24, kLeft = 0x25, kUp = 0x26, kRight = 0x27, kDown = 0x28;

		if (box.IsOpen())
		{
			if (isChar)
			{
				if (key >= 32 && key <= 126)
				{
					box.Type((char)key);
				}
			}
			else if (key == kReturn)
			{
				Answered(kDialogOk);
			}
			else if (key == kEscape)
			{
				Answered(kDialogEscape);
			}
			else if (key == kBack)
			{
				box.Backspace();
			}
			else if (key == kTab || key == kDown)
			{
				box.NextField();
			}

			return true;
		}

		if (list.IsOpen())
		{
			if (!isChar)
			{
				if (key == kReturn)
				{
					ListAnswered(list.Highlighted());
				}
				else if (key == kEscape)
				{
					ListAnswered(-1);
				}
				else if (key == kUp || key == kDown || key == kLeft || key == kRight || key == kPrior || key == kNext || key == kHome || key == kEnd)
				{
					list.Move(key == kUp ? ListPicker::Key::Up : key == kDown ? ListPicker::Key::Down : key == kLeft ? ListPicker::Key::Left : key == kRight ? ListPicker::Key::Right :
						key == kPrior ? ListPicker::Key::PageUp : key == kNext ? ListPicker::Key::PageDown : key == kHome ? ListPicker::Key::Home : ListPicker::Key::End);
				}
			}

			return true;
		}

		return false;
	}

	void Dialogs_Draw(std::vector<PaintBox> *boxes, std::vector<PaintText> *lines, uint32_t grayColour, uint32_t litColour)
	{
		auto const text = [&](std::string const &words, int32_t x, int32_t y)
		{
			PaintText t;

			t.text = words;
			t.x = x;
			t.y = y;
			lines->push_back(t);
		};

		auto const fill = [&](DialogRect const &r, uint32_t colour)
		{
			PaintBox b = { r.x, r.y, r.width, r.height, colour };

			boxes->push_back(b);
		};

		if (box.IsOpen())
		{
			fill(box.Window(), grayColour);

			if (box.Kind() == DialogKind::Form)
			{
				text(box.Title(), box.Window().x + 10, box.Window().y + 4);

				for (size_t i = 0; i < box.Widgets().size(); i++)
				{
					DialogWidget const &w = box.Widgets()[i];

					if (!w.visible)
					{
						continue;
					}

					if (w.type == DialogWidgetType::Combo)
					{
						fill(w.box, 0xFF202428u);
						text(w.text, w.box.x + 4, w.box.y + 1);

						DialogRect arrow = { w.box.x + w.box.width - 16, w.box.y + 1, 15, w.box.height > 2 ? w.box.height - 2 : 1 };

						fill(arrow, 0xFF30343Cu);
						text("v", arrow.x + 4, arrow.y);
					}
					else if (w.type == DialogWidgetType::Label)
					{
						// a label of more than one line (the words have \n in them) is drawn a line under a line
						size_t lineAt = 0;
						int32_t lineY = w.box.y + 1;

						while (lineAt <= w.text.size())
						{
							size_t const newline = w.text.find('\n', lineAt);

							text(w.text.substr(lineAt, newline == std::string::npos ? std::string::npos : newline - lineAt), w.box.x, lineY);
							lineY += 15;

							if (newline == std::string::npos)
							{
								break;
							}

							lineAt = newline + 1;
						}
					}
					else if (w.type == DialogWidgetType::Edit)
					{
						fill(w.box, i == box.Focus() ? 0xFF303844u : 0xFF202428u);
						text(w.text + (i == box.Focus() ? "_" : ""), w.box.x + 4, w.box.y + 1);
					}
					else if (w.type == DialogWidgetType::Check)
					{
						fill(w.box, 0xFF30343Cu);

						if (w.checked)
						{
							DialogRect inner = { w.box.x + 3, w.box.y + 3, w.box.width > 6 ? w.box.width - 6 : 1, w.box.height > 6 ? w.box.height - 6 : 1 };

							fill(inner, litColour);
						}
					}
					else
					{
						fill(w.box, 0xFF30343Cu);
						text(w.text, w.box.x + 4, w.box.y + 1);
					}
				}

				// an open combo's list, over everything else
				for (DialogWidget const &w : box.Widgets())
				{
					if (w.type != DialogWidgetType::Combo || !w.visible || !w.listOpen)
					{
						continue;
					}

					for (size_t row = 0; row < w.items.size(); row++)
					{
						DialogRect const r = InDialog::ComboRow(w, row);

						fill(r, (int32_t)row == w.selected ? litColour : 0xFF30343Cu);
						text(w.items[row], r.x + 4, r.y + 1);
					}
				}

				return;
			}

			for (size_t i = 0; i < box.Lines().size(); i++)
			{
				text(box.Lines()[i], box.TextArea().x, box.TextArea().y + (int32_t)i * kPanelPassLineHeight);
			}

			for (size_t i = 0; i < box.Fields().size(); i++)
			{
				DialogField const &f = box.Fields()[i];

				fill(f.edit, i == box.Focus() ? 0xFF303844u : 0xFF202428u);
				text(f.text + (i == box.Focus() ? "_" : ""), f.edit.x + 4, f.edit.y + 1);

				if (!f.label.empty())
				{
					text(f.label, f.labelBox.x, f.labelBox.y + 1);
				}
			}

			for (DialogButton const &b : box.Buttons())
			{
				fill(b.box, 0xFF30343Cu);
				text(b.label, b.box.x + 4, b.box.y + 1);
			}

			return;
		}

		if (list.IsOpen())
		{
			ListPickerBox const w = list.Window();
			DialogRect window = { w.x, w.y, w.width, w.height };

			fill(window, grayColour);
			text(list.Title(), w.x + 10, w.y + 4);

			for (ListPickerCell const &cell : list.Cells())
			{
				DialogRect b = { cell.box.x, cell.box.y, cell.box.width, cell.box.height };

				fill(b, cell.choice == list.Highlighted() ? litColour : 0xFF30343Cu);
				text(list.Choice(cell.choice), cell.box.x + 4, cell.box.y + 1);
			}

			if (list.Pages() > 1)
			{
				ListPickerBox const pb = list.PrevButton(), nb = list.NextButton();
				DialogRect p = { pb.x, pb.y, pb.width, pb.height }, n = { nb.x, nb.y, nb.width, nb.height };

				fill(p, 0xFF30343Cu);
				fill(n, 0xFF30343Cu);
				text("Prev", pb.x + 4, pb.y + 1);
				text("Next", nb.x + 4, nb.y + 1);
			}
		}
	}

	DialogsState Dialogs_State()
	{
		DialogsState s;

		if (box.IsOpen())
		{
			if (box.Kind() == DialogKind::Form)
			{
				s.title = box.Title();
				s.kind = "form";

				size_t edits = 0;

				for (DialogWidget const &w : box.Widgets())
				{
					if (!w.visible)
					{
						continue;
					}

					if (w.type == DialogWidgetType::Combo)
					{
						s.combos += (s.combos.empty() ? "" : "|") + Word(w.text.empty() ? std::string("-") : w.text, w.box);
						s.text += (edits++ ? "|" : "") + w.text;

						if (w.listOpen)
						{
							for (size_t row = 0; row < w.items.size(); row++)
							{
								s.comborows += (s.comborows.empty() ? "" : "|") + Word(w.items[row], InDialog::ComboRow(w, row));
							}
						}
					}
					else if (w.type == DialogWidgetType::Button)
					{
						s.buttons += (s.buttons.empty() ? "" : "|") + Word(w.text, w.box);
					}
					else if (w.type == DialogWidgetType::Check)
					{
						s.checks += (s.checks.empty() ? "" : "|") + Word(std::string(w.checked ? "on" : "off"), w.box);
					}
					else if (w.type == DialogWidgetType::Edit)
					{
						s.fields += (s.fields.empty() ? "" : "|") + Word("field", w.box);
						s.text += (edits++ ? "|" : "") + w.text;
					}
				}
			}
			else
			{
				s.title = box.Text();
				s.kind = box.Kind() == DialogKind::Message ? "message" : box.Kind() == DialogKind::Confirm ? "confirm" : "text";

				for (DialogButton const &b : box.Buttons())
				{
					s.buttons += (s.buttons.empty() ? "" : "|") + Word(b.label, b.box);
				}

				for (size_t i = 0; i < box.Fields().size(); i++)
				{
					s.text += (i ? "|" : "") + box.Fields()[i].text;
				}
			}

			for (char &c : s.title)
			{
				c = (c == '"' || c == '\n') ? ' ' : c;
			}

			for (char &c : s.text)
			{
				c = c == '"' ? '?' : c;
			}
		}
		else if (list.IsOpen())
		{
			s.title = list.Title();
			s.kind = "list";

			for (int32_t row = 0; row < list.Count(); row++)
			{
				s.rows += (row ? "|" : "") + list.Choice(row);
			}

			std::vector<ListPickerCell> const cells = list.Cells();

			for (ListPickerCell const &cell : cells)
			{
				for (char letter : list.Choice(cell.choice))
				{
					s.cells += (letter == '"' || letter == '|' || letter == '@') ? '?' : letter;
				}

				s.cells += "@" + std::to_string(cell.box.x) + "," + std::to_string(cell.box.y) + "," + std::to_string(cell.box.width) + "," + std::to_string(cell.box.height) + "|";
			}

			if (!cells.empty())
			{
				s.row1 = std::to_string(cells[0].box.x) + " " + std::to_string(cells[0].box.y) + " " + std::to_string(cells[0].box.width) + " 20";
			}

			if (list.Pages() > 1)
			{
				DialogRect p = { list.PrevButton().x, list.PrevButton().y, list.PrevButton().width, list.PrevButton().height };
				DialogRect n = { list.NextButton().x, list.NextButton().y, list.NextButton().width, list.NextButton().height };

				s.buttons = Word("Prev", p) + "|" + Word("Next", n);
			}
		}

		return s;
	}
}
