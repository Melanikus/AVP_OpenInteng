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

		// THE PARTS THAT REACH PAST THE WINDOW (Drake 11-22; the owner, of the Save dialog: "Menus are drawing on top of each other"): each visible part of a form,
		// measured on the rectangle it is drawn in, against the window's own box; a label's words as Dialogs_Draw draws them (from its box's x + 1 and y - 1, a line
		// under a line, each as wide as the font in use draws its letters). An open combo's list is not a part: a drop-down list opens over the window and past
		// it, as inteng's do (anything painted over its rows is overlist); but each row in view is one line, so its words must end inside the row ("row <n>: right
		// <pixels>", n from 1 in the list). "none" when every part is inside
		std::string PastTheWindow(DialogRect const &window, std::vector<DialogWidget> const &widgets)
		{
			bool const inteng = PanelPass_HasMenuFont();
			int32_t const fontHeight = inteng ? PanelPass_MenuFontHeight() : kPanelPassLineHeight;
			std::string past;
			size_t labels = 0, combos = 0, fields = 0, checks = 0;

			// one line of words, as wide as the font in use draws its letters
			auto const wide = [inteng](std::string const &words)
			{
				int32_t width = 0;

				for (char const letter : words)
				{
					width += inteng ? PanelPass_MenuWidth((unsigned char)letter) : PanelPass_SmallWidth((unsigned char)letter);
				}

				return width;
			};

			auto const measure = [&](std::string const &part, int32_t x, int32_t y, int32_t width, int32_t height)
			{
				int64_t const reach[4] = { (int64_t)window.x - x, (int64_t)window.y - y, ((int64_t)x + width) - ((int64_t)window.x + window.width),
					((int64_t)y + height) - ((int64_t)window.y + window.height) };
				char const *const sides[4] = { "left", "top", "right", "bottom" };
				std::string words;

				for (int k = 0; k < 4; k++)
				{
					if (reach[k] > 0)
					{
						words += " " + std::string(sides[k]) + " " + std::to_string((long long)reach[k]);
					}
				}

				if (!words.empty())
				{
					past += (past.empty() ? "" : "|") + part + ":" + words;
				}
			};

			for (DialogWidget const &w : widgets)
			{
				if (!w.visible)
				{
					continue;
				}

				if (w.type == DialogWidgetType::Label)
				{
					int32_t widest = 0, lines = 0, width = 0;

					for (size_t at = 0; at <= w.text.size(); at++)
					{
						if (at == w.text.size() || w.text[at] == '\n')
						{
							widest = width > widest ? width : widest;
							width = 0;
							lines++;
							continue;
						}

						width += inteng ? PanelPass_MenuWidth((unsigned char)w.text[at]) : PanelPass_SmallWidth((unsigned char)w.text[at]);
					}

					measure("label " + std::to_string((unsigned long long)++labels), w.box.x + 1, w.box.y - 1, widest, lines * fontHeight);
				}
				else if (w.type == DialogWidgetType::Combo)
				{
					measure("combo " + std::to_string((unsigned long long)++combos), w.box.x, w.box.y, w.box.width, w.box.height);

					// its open list's rows in view: each row's words drawn from its x + 1, ending inside the row
					for (size_t row = 0; w.listOpen && row < w.items.size(); row++)
					{
						DialogRect const r = InDialog::ComboRow(w, row);
						int64_t const over = r.width > 0 ? (int64_t)1 + wide(w.items[row]) - r.width : 0;

						if (over > 0)
						{
							past += (past.empty() ? "" : "|") + std::string("row ") + std::to_string((unsigned long long)row + 1) + ": right " + std::to_string((long long)over);
						}
					}
				}
				else if (w.type == DialogWidgetType::Button)
				{
					measure(Word("button " + w.text, w.box).substr(0, 7 + w.text.size()), w.box.x, w.box.y, w.box.width, w.box.height);
				}
				else if (w.type == DialogWidgetType::Edit)
				{
					measure("field " + std::to_string((unsigned long long)++fields), w.box.x, w.box.y, w.box.width, w.box.height);
				}
				else if (w.type == DialogWidgetType::Check)
				{
					measure("check " + std::to_string((unsigned long long)++checks), w.box.x, w.box.y, w.box.width, w.box.height);
				}
			}

			return past.empty() ? std::string("none") : past;
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

	int32_t Dialogs_ScreenWidth()
	{
		return screen.width;
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

	bool Dialogs_Wheel(int32_t delta)
	{
		// only a combo's open list takes the wheel (dialog_box.h, the list that scrolls); with a dialog open the wheel goes nowhere else (the dialogs are modal)
		box.Wheel(delta);
		return box.IsOpen() || list.IsOpen();
	}

	bool Dialogs_Key(bool isChar, uint32_t key)
	{
		// the virtual keys the dialogs read (windows.h values, so this file needs no windows.h)
		uint32_t const kBack = 0x08, kTab = 0x09, kReturn = 0x0D, kEscape = 0x1B, kPrior = 0x21, kNext = 0x22, kEnd = 0x23, kHome = 0x24, kLeft = 0x25, kUp = 0x26, kRight = 0x27, kDown = 0x28;

		if (box.IsOpen())
		{
			// A COMBO'S OPEN LIST takes its keys first (dialog_box.h, the list that scrolls): the arrows, Page Up and Down, Home and End move the row the keys are on; Return picks it and
			// Escape closes the list, and neither leaves the dialog
			if (!isChar && box.ListOpen())
			{
				bool const taken = key == kReturn ? box.ListReturn() : key == kEscape ? box.ListEscape() :
					key == kUp ? box.ListKey(DialogListKey::Up) : key == kDown ? box.ListKey(DialogListKey::Down) : key == kPrior ? box.ListKey(DialogListKey::PageUp) :
					key == kNext ? box.ListKey(DialogListKey::PageDown) : key == kHome ? box.ListKey(DialogListKey::Home) : key == kEnd ? box.ListKey(DialogListKey::End) : false;

				if (taken)
				{
					return true;
				}
			}

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
		bool onTop = false;   // the open drop-down list's boxes and words: the panel pass's top layer, over every word of the window (PanelPaint_LayerOf)

		auto const text = [&](std::string const &words, int32_t x, int32_t y)
		{
			PaintText t;

			t.text = words;
			t.x = x;
			t.y = y;
			t.colour = kDialogWhite;
			t.menuFont = inteng;
			t.top = onTop;
			lines->push_back(t);
		};

		auto const fill = [&](DialogRect const &r, uint32_t colour)
		{
			if (r.width <= 0 || r.height <= 0)
			{
				return;
			}

			PaintBox b = { r.x, r.y, r.width, r.height, colour };

			b.top = onTop;
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
					else if (w.type == DialogWidgetType::Group)
					{
						// the frame's four edges (BS_GROUPBOX, whose top edge runs through the middle of its words' line), then the words on black over the top edge
						int32_t const top = w.box.y + fontHeight / 2;
						DialogRect const edges[4] = { { w.box.x, top, w.box.width, 1 }, { w.box.x, w.box.y + w.box.height - 1, w.box.width, 1 }, { w.box.x, top, 1, w.box.y + w.box.height - top },
						                              { w.box.x + w.box.width - 1, top, 1, w.box.y + w.box.height - top } };

						for (DialogRect const &edge : edges)
						{
							fill(edge, kDialogWhite);
						}

						if (!w.text.empty())
						{
							DialogRect const behind = { w.box.x + 6, w.box.y, (int32_t)w.text.size() * (fontHeight / 2) + 4, fontHeight };

							fill(behind, kDialogBlack);
							text(w.text, w.box.x + 8, w.box.y - 1);
						}
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

				// an open combo's list, over everything else: its boxes AND its words after every word of the window (the owner's screenshot 11-18: the window's labels showed through it)
				onTop = true;

				for (DialogWidget const &w : box.Widgets())
				{
					if (w.type != DialogWidgetType::Combo || !w.visible || !w.listOpen)
					{
						continue;
					}

					// the rows in view (a long list scrolls: dialog_box.h), the one the keys are on red, as the picked one was
					for (size_t row = 0; row < w.items.size(); row++)
					{
						DialogRect const r = InDialog::ComboRow(w, row);

						if (r.width <= 0)
						{
							continue;
						}

						framed(r, kDialogWhite, (int32_t)row == w.listLit ? kDialogRed : kDialogBlue);
						text(w.items[row], r.x + 1, r.y - 1);
					}

					// ITS SCROLL BAR, of the toolkit's own parts: the column a field's black framed white, the thumb a button's blue framed white, and at its ends two buttons, each with
					// its arrow drawn as Windows draws a scroll bar's arrows, a small white triangle (inteng's font may have no "^"; the box's own arrow keeps its "v")
					DialogScrollBar const bar = InDialog::ComboScrollBar(w);

					if (bar.column.width > 0)
					{
						framed(bar.column, kDialogWhite, kDialogBlack);
						framed(bar.thumb, kDialogWhite, kDialogBlue);

						for (int32_t end = 0; end < 2; end++)
						{
							DialogRect const arrow = end == 0 ? bar.up : bar.down;

							framed(arrow, kDialogWhite, kDialogBlue);

							for (int32_t step = 0; step < 4; step++)   // 7, 5, 3 and 1 pixels wide, its point up for the top arrow and down for the bottom one
							{
								int32_t const across = 7 - 2 * step;
								int32_t const y = arrow.y + arrow.height / 2 + (end == 0 ? 1 - step : step - 2);

								fill({ arrow.x + (arrow.width - across) / 2, y, across, 1 }, kDialogWhite);
							}
						}
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

	// The harness's dialogoverlist (Vasquez 11-18, the owner's screenshot): the words of the window the panel pass paints OVER its open drop-down list, in the pass's own order (PanelPaint_LayerOf:
	// a word is over a box when its layer is the box's or a later one; every item of a dialog is in the overlay, editor_panels.cpp). The list's own words are left out. "" when none, or no list is open.
	static std::string WordsOverTheOpenList()
	{
		struct Row
		{
			DialogRect r;
			std::string words;
		};

		std::vector<Row> rows;

		for (DialogWidget const &w : box.Widgets())
		{
			for (size_t row = 0; w.type == DialogWidgetType::Combo && w.visible && w.listOpen && row < w.items.size(); row++)
			{
				if (InDialog::ComboRow(w, row).width > 0)   // the rows in view only: a row scrolled out of it isn't drawn
				{
					rows.push_back({ InDialog::ComboRow(w, row), w.items[row] });
				}
			}
		}

		if (rows.empty())
		{
			return std::string();
		}

		std::vector<PaintBox> drawnBoxes;
		std::vector<PaintText> drawnLines;

		Dialogs_Draw(&drawnBoxes, &drawnLines, 0, 0);

		int32_t const fontHeight = PanelPass_HasMenuFont() ? PanelPass_MenuFontHeight() : kPanelPassLineHeight;
		std::string over;

		for (PaintText line : drawnLines)
		{
			line.overlay = true;

			bool own = false;

			for (Row const &one : rows)
			{
				own = own || (line.text == one.words && line.x == one.r.x + 1 && line.y == one.r.y - 1);
			}

			int32_t const width = (int32_t)line.text.size() * (fontHeight / 2);
			bool covers = false;

			for (Row const &one : rows)
			{
				bool const meets = line.x < one.r.x + one.r.width && one.r.x < line.x + width && line.y < one.r.y + one.r.height && one.r.y < line.y + fontHeight;

				for (PaintBox b : drawnBoxes)
				{
					b.overlay = true;

					if (meets && b.x == one.r.x && b.y == one.r.y && b.width == one.r.width && b.height == one.r.height && PanelPaint_LayerOf(line) >= PanelPaint_LayerOf(b))
					{
						covers = true;
					}
				}
			}

			if (!own && covers && !line.text.empty())
			{
				over += (over.empty() ? "" : "|") + line.text;
			}
		}

		return over;
	}

	bool Dialogs_FormWidgetBox(size_t index, DialogRect *out)
	{
		if (!out || !box.IsOpen() || box.Kind() != DialogKind::Form || index >= box.Widgets().size())
		{
			return false;
		}

		*out = box.Widgets()[index].box;
		return true;
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
				s.overlist = WordsOverTheOpenList();
				s.past = PastTheWindow(box.Window(), box.Widgets());
				s.window = Word("w", box.Window()).substr(2);

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

						// an editable combo also takes letters as a field does (its box left of the arrow): form-field counts it, in the form's order (Vasquez 11-16)
						if (w.editable)
						{
							DialogRect typedPart = w.box;

							typedPart.width = w.box.width > 16 ? w.box.width - 16 : w.box.width;
							s.fields += (s.fields.empty() ? "" : "|") + Word("field", typedPart);
						}

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
