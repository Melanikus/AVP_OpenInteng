// See dialogs.h. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "dialogs.h"

#include "list_order.h"   // ListOrder: the rows' order, a stable sort (hostile counts)
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
		std::function<void(int32_t)> listRight;   // the list's right-click handler (inteng's list +0x6c, FUN_00476f70; FUN_00476d10 calls it with the row): the row's place in the rows given
		std::vector<size_t> listOrder;   // the rows as the dialog shows them (sorted): the place each had in the rows given
		DialogRect screen = { 0, 0, 640, 480 };

		// inteng's dialogs measure and draw their words in its own font (font16.fnt, FUN_0057adb0; dialog-look-drake.md DL-3) when the editor read it, else the game's small font
		int32_t TextWidth(std::string const &text)
		{
			bool const inteng = PanelPass_HasMenuFont();
			int32_t w = 0;

			for (size_t c = 0; c < text.size(); c++)
			{
				w += inteng ? PanelPass_MenuWidth((unsigned char)text[c]) : PanelPass_SmallWidth((unsigned char)text[c]);
			}

			return w;
		}

		// the toolkit's four colours (FUN_0057d170: the dialog's +0x30, +0x33, +0x36, +0x39; DL-1)
		uint32_t const kDialogWhite = 0xFFFFFFFFu;
		uint32_t const kDialogBlack = 0xFF000000u;
		uint32_t const kDialogBlue = 0xFF0000FFu;
		uint32_t const kDialogRed = 0xFFFF0000u;

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
			listRight = nullptr;

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

	// FUN_00475bf0 0x476403-0x476670: the rows are put in an ascending list by FUN_0053ed50 (a byte-by-byte comparison), each after the ones that are not above it. The same order,
	// made by a stable sort (list_order.h): one insertion at a time took a hostile level's 100,000 rows minutes
	void Dialogs_List(std::string const &title, std::vector<std::string> const &rows, std::function<void(int32_t)> const &then, std::function<void(int32_t)> const &onRight)
	{
		listRight = onRight;
		listOrder = ListOrder(rows);

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
			listRight = nullptr;
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

	bool Dialogs_RightPress(int32_t x, int32_t y)
	{
		if (box.IsOpen())
		{
			return true;   // a box takes the press and does nothing with the right button
		}

		if (list.IsOpen())
		{
			int32_t choice = -1;

			// a right press on a row: the list's handler with that row (FUN_00476d10 0x476f43-0x476f57), the dialog stays open
			if (listRight && list.Press(x, y, &choice) == ListPickerHit::Choice && choice >= 0 && (size_t)choice < listOrder.size())
			{
				auto const handler = listRight;

				handler((int32_t)listOrder[(size_t)choice]);
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

	// inteng's dialog toolkit's look (reassess/dialog-look-drake.md): the window BLACK with a WHITE 1-pixel frame (FUN_0057cee0); a title a RED band with a white frame across the
	// window's top, the font's height high (FUN_0057e330); a button a WHITE frame filled BLUE (FUN_0057d890); a text field framed WHITE when it has the focus, else BLUE, BLACK inside
	// (FUN_0057d970); words WHITE in inteng's font, letters only (FUN_0057be40 with its colour key); a check box a WHITE frame, its tick the font's glyph 3 (FUN_0057d6f0). Nothing is lit
	// when pointed at or pressed (DL-9). grayColour and litColour are no longer used: the look is inteng's whatever the editor's own panels are.
	void Dialogs_Draw(std::vector<PaintBox> *boxes, std::vector<PaintText> *lines, uint32_t, uint32_t)
	{
		bool const inteng = PanelPass_HasMenuFont();
		int32_t const fontHeight = inteng ? PanelPass_MenuFontHeight() : kPanelPassLineHeight;

		auto const text = [&](std::string const &words, int32_t x, int32_t y)
		{
			PaintText t;

			t.text = words;
			t.x = x;
			t.y = y;
			t.colour = kDialogWhite;
			t.menuFont = inteng;
			lines->push_back(t);
		};

		auto const fill = [&](DialogRect const &r, uint32_t colour)
		{
			if (r.width <= 0 || r.height <= 0)
			{
				return;
			}

			PaintBox b = { r.x, r.y, r.width, r.height, colour };

			boxes->push_back(b);
		};

		// a 1-pixel frame along the rectangle's edges (FUN_0057c360), and its inside filled (FUN_0057c4e0)
		auto const framed = [&](DialogRect const &r, uint32_t edge, uint32_t inside)
		{
			fill(r, edge);

			DialogRect const in = { r.x + 1, r.y + 1, r.width - 2, r.height - 2 };

			fill(in, inside);
		};

		auto const button = [&](DialogRect const &r, std::string const &words)
		{
			framed(r, kDialogWhite, kDialogBlue);
			text(words, r.x + 1, r.y - 1);
		};

		auto const field = [&](DialogRect const &r, std::string const &words, bool focus)
		{
			framed(r, focus ? kDialogWhite : kDialogBlue, kDialogBlack);
			text(words + (focus ? "_" : ""), r.x + 1, r.y - 1);   // the "_" is ours (DL-6: inteng's cursor not read)
		};

		auto const window = [&](DialogRect const &r, std::string const &title)
		{
			framed(r, kDialogWhite, kDialogBlack);

			if (!title.empty())
			{
				DialogRect const band = { r.x, r.y, r.width, fontHeight };

				framed(band, kDialogWhite, kDialogRed);
				text(title, r.x + 1, r.y);
			}
		};

		if (box.IsOpen())
		{
			if (box.Kind() == DialogKind::Form)
			{
				window(box.Window(), box.Title());

				for (size_t i = 0; i < box.Widgets().size(); i++)
				{
					DialogWidget const &w = box.Widgets()[i];

					if (!w.visible)
					{
						continue;
					}

					if (w.type == DialogWidgetType::Combo)
					{
						// not one of inteng's own controls (edobtype's MFC windows have them): drawn as a text field with its arrow
						field(w.box, w.text, false);

						DialogRect arrow = { w.box.x + w.box.width - 16, w.box.y, 16, w.box.height };

						button(arrow, "v");
					}
					else if (w.type == DialogWidgetType::Label)
					{
						// a label of more than one line (the words have \n in them) is drawn a line under a line
						size_t lineAt = 0;
						int32_t lineY = w.box.y - 1;

						while (lineAt <= w.text.size())
						{
							size_t const newline = w.text.find('\n', lineAt);

							text(w.text.substr(lineAt, newline == std::string::npos ? std::string::npos : newline - lineAt), w.box.x + 1, lineY);
							lineY += fontHeight;

							if (newline == std::string::npos)
							{
								break;
							}

							lineAt = newline + 1;
						}
					}
					else if (w.type == DialogWidgetType::Edit)
					{
						field(w.box, w.text, i == box.Focus());
					}
					else if (w.type == DialogWidgetType::Check)
					{
						framed(w.box, w.enabled ? kDialogWhite : kDialogBlue, kDialogBlack);

						if (w.checked)
						{
							text(inteng ? std::string(1, (char)3) : std::string("X"), w.box.x, w.box.y);   // glyph 3: an X across the box (DL-8)
						}
					}
					else
					{
						button(w.box, w.text);
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

						framed(r, kDialogWhite, (int32_t)row == w.selected ? kDialogRed : kDialogBlue);
						text(w.items[row], r.x + 1, r.y - 1);
					}
				}

				return;
			}

			window(box.Window(), std::string());

			for (size_t i = 0; i < box.Lines().size(); i++)
			{
				text(box.Lines()[i], box.TextArea().x, box.TextArea().y + (int32_t)i * fontHeight);
			}

			for (size_t i = 0; i < box.Fields().size(); i++)
			{
				DialogField const &f = box.Fields()[i];

				field(f.edit, f.text, i == box.Focus());

				if (!f.label.empty())
				{
					text(f.label, f.labelBox.x + 1, f.labelBox.y - 1);
				}
			}

			for (DialogButton const &b : box.Buttons())
			{
				button(b.box, b.label);
			}

			return;
		}

		if (list.IsOpen())
		{
			ListPickerBox const w = list.Window();
			DialogRect const all = { w.x, w.y, w.width, w.height };

			window(all, list.Title());

			for (ListPickerCell const &cell : list.Cells())
			{
				DialogRect const b = { cell.box.x, cell.box.y, cell.box.width, cell.box.height };

				// the list's keyboard highlight is ours (list_picker.h, NAMED): drawn only once a key moved it
				if (list.Keyed() && cell.choice == list.Highlighted())
				{
					framed(b, kDialogWhite, kDialogRed);
					text(list.Choice(cell.choice), b.x + 1, b.y - 1);
				}
				else
				{
					button(b, list.Choice(cell.choice));
				}
			}

			// the text field under the entries (FUN_0057d8e0 at (0x1e, height - 0x3c)): which control has the focus at the start is not read (DL-6), so it is drawn framed blue
			ListPickerBox const tf = list.TextField();

			if (tf.width > 0 && tf.height > 0)
			{
				field(DialogRect{ tf.x, tf.y, tf.width, tf.height }, std::string(), false);
			}

			if (list.Pages() > 1)
			{
				ListPickerBox const pb = list.PrevButton(), nb = list.NextButton();

				button(DialogRect{ pb.x, pb.y, pb.width, pb.height }, "Prev");
				button(DialogRect{ nb.x, nb.y, nb.width, nb.height }, "Next");
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
