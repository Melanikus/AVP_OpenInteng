// See menu_bar.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "menu_bar.h"

#include "text_field.h"   // IsFontLetter: the font's letters, one rule for the whole kit

namespace ui
{
	// How many bytes a text has before its zero, reading no more than most + 1 of them: most + 1 means it's longer than most. It stops
	// at the zero, so it never reads past the end of a text that's shorter.
	static size_t BoundedLength(const char *text, size_t most)
	{
		size_t length = 0;

		while (length <= most && text[length] != 0)
		{
			length++;
		}

		return length;
	}

	// Whether a text of the editor's own is there, and is 1 to most font letters
	static bool IsWords(const char *text, size_t most)
	{
		if (!text)
		{
			return false;
		}

		size_t const length = BoundedLength(text, most);

		if (length == 0 || length > most)
		{
			return false;
		}

		for (size_t i = 0; i < length; i++)
		{
			if (!IsFontLetter((unsigned char)text[i]))
			{
				return false;
			}
		}

		return true;
	}

	// A sum done in 64 bits, brought back inside what a box's place can hold
	static int32_t Clamp32(int64_t value)
	{
		return value < INT32_MIN ? INT32_MIN : (value > INT32_MAX ? INT32_MAX : (int32_t)value);
	}

	static int64_t NotBelowZero(int64_t value)
	{
		return value < 0 ? 0 : value;
	}

	MenuBar::MenuBar() : menus(0), count(0), open(-1), lit(-1), first(0), picked(0), refused(0), openedByPress(false), itemHeight(0)
	{
		Box const none = { 0, 0, 0, 0 };

		bar = none;
		room = none;

		for (size_t m = 0; m < kMenuMostMenus; m++)
		{
			titleWidths[m] = 0;
			dropWidths[m] = 0;

			for (size_t i = 0; i < kMenuMostItems; i++)
			{
				enabled[m][i] = true;
			}
		}
	}

	bool MenuBar::Load(const Menu *newMenus, size_t newCount)
	{
		menus = 0;
		count = 0;
		open = -1;
		openedByPress = false;
		lit = -1;
		first = 0;
		picked = 0;
		refused = 0;

		for (size_t m = 0; m < kMenuMostMenus; m++)
		{
			for (size_t i = 0; i < kMenuMostItems; i++)
			{
				enabled[m][i] = true;
				checked[m][i] = false;
			}
		}

		if (!newMenus || newCount == 0 || newCount > kMenuMostMenus)
		{
			return false;
		}

		for (size_t m = 0; m < newCount; m++)
		{
			const Menu &menu = newMenus[m];

			if (!IsWords(menu.title, kMenuNameMost) || menu.count > kMenuMostItems || (menu.count > 0 && !menu.items))
			{
				return false;
			}

			for (size_t i = 0; i < menu.count; i++)
			{
				const MenuItem &item = menu.items[i];

				if (!IsWords(item.name, kMenuNameMost) || (item.keys && !IsWords(item.keys, kMenuKeysMost)) || item.action <= 0)
				{
					return false;
				}

				// A tag, when there is one, is plain words too: no tag is null, never empty (Added 2026-11-15 by Melanikus (Frost), A6.)
				if (item.tag && !IsWords(item.tag, kMenuTagMost))
				{
					return false;
				}

				// Each action once in the whole bar: against every item before this one, in this menu and the ones before it
				for (size_t earlierMenu = 0; earlierMenu <= m; earlierMenu++)
				{
					size_t const earlierCount = earlierMenu < m ? newMenus[earlierMenu].count : i;

					for (size_t j = 0; j < earlierCount; j++)
					{
						if (newMenus[earlierMenu].items[j].action == item.action)
						{
							return false;
						}
					}
				}
			}
		}

		menus = newMenus;
		count = newCount;
		return true;
	}

	void MenuBar::SetEnabled(int32_t action, bool on)
	{
		for (size_t m = 0; m < count; m++)
		{
			for (size_t i = 0; i < menus[m].count; i++)
			{
				if (menus[m].items[i].action == action)
				{
					enabled[m][i] = on;
				}
			}
		}

		// The lit item can't be one that's off: it moves on to the next that's on, or to none
		if (open >= 0 && lit >= 0 && !enabled[open][lit])
		{
			lit = NextEnabled(lit, 1);
			KeepLitShown();
		}
	}

	void MenuBar::SetChecked(int32_t action, bool on)
	{
		for (size_t m = 0; m < count; m++)
		{
			for (size_t i = 0; i < menus[m].count; i++)
			{
				if (menus[m].items[i].action == action)
				{
					checked[m][i] = on;
				}
			}
		}
	}

	bool MenuBar::IsChecked(size_t menu, size_t item) const
	{
		return menu < count && item < menus[menu].count && checked[menu][item];
	}

	bool MenuBar::IsEnabled(size_t menu, size_t item) const
	{
		return menu < count && item < menus[menu].count && enabled[menu][item];
	}

	// The next item that's on in the open menu, from one place going one way, round the end; or -1 when none is. From -1 it starts at
	// the first item going forward, or the last going back.
	int MenuBar::NextEnabled(int from, int step) const
	{
		int const items = (int)menus[open].count;

		if (items == 0)
		{
			return -1;
		}

		if (from < 0)
		{
			from = step > 0 ? -1 : items;
		}

		for (int k = 1; k <= items; k++)
		{
			int const place = ((from + step * k) % items + items) % items;

			if (enabled[open][place])
			{
				return place;
			}
		}

		return -1;
	}

	void MenuBar::OpenMenuAt(int menu)
	{
		open = menu;
		lit = -1;
		first = 0;
		lit = NextEnabled(-1, 1);
		KeepLitShown();
	}

	MenuAnswer MenuBar::Release(int32_t x, int32_t y)
	{
		// One release ends one press: whatever it does, the gesture is over
		bool const fromTitle = openedByPress;

		openedByPress = false;
		picked = 0;
		refused = 0;

		if (!menus || !fromTitle || open < 0)
		{
			return MenuAnswer::Nothing;
		}

		Box const drop = DropBox();

		if (!BoxHolds(drop, x, y))
		{
			return MenuAnswer::Nothing;
		}

		// Which item the release is on, as for a press: whole rows of itemHeight from the drop-down's top, from the first shown
		size_t const item = first + (size_t)(((int64_t)y - drop.y) / itemHeight);

		if (item >= menus[open].count)
		{
			return MenuAnswer::Nothing;
		}

		// An item that's off is never picked: the menu stays open, and the editor is told which, to say why on the status line
		if (!enabled[open][item])
		{
			refused = menus[open].items[item].action;
			return MenuAnswer::Refused;
		}

		picked = menus[open].items[item].action;
		Close();
		return MenuAnswer::Picked;
	}

	MenuAnswer MenuBar::Close()
	{
		openedByPress = false;
		open = -1;
		lit = -1;
		first = 0;
		return MenuAnswer::Closed;
	}

	size_t MenuBar::ShownItems() const
	{
		if (open < 0 || itemHeight <= 0 || room.width <= 0 || room.height <= 0 || dropWidths[open] <= 0)
		{
			return 0;
		}

		size_t const fit = (size_t)(room.height / itemHeight);
		size_t const items = menus[open].count;

		return fit < items ? fit : items;
	}

	// The drop-down scrolls so the lit item is among those shown
	void MenuBar::KeepLitShown()
	{
		size_t const shown = ShownItems();
		size_t const items = open >= 0 ? menus[open].count : 0;

		if (shown == 0)
		{
			first = 0;
			return;
		}

		if (lit >= 0 && (size_t)lit < first)
		{
			first = (size_t)lit;
		}

		if (lit >= 0 && (size_t)lit >= first + shown)
		{
			first = (size_t)lit - shown + 1;
		}

		if (first + shown > items)
		{
			first = items - shown;
		}
	}

	MenuAnswer MenuBar::Handle(const Event &event)
	{
		picked = 0;
		refused = 0;
		openedByPress = false;   // a key ends a press's gesture: the release after it picks nothing

		if (!menus)
		{
			return MenuAnswer::Nothing;
		}

		if (open < 0)
		{
			if (event.key == Key::Menu)
			{
				OpenMenuAt(0);
				return MenuAnswer::Opened;
			}

			return MenuAnswer::Nothing;
		}

		int const items = (int)menus[open].count;

		switch (event.key)
		{
			case Key::Menu:
			case Key::Escape:
				return Close();

			case Key::Left:
			case Key::Right:
			{
				if (count < 2)
				{
					return MenuAnswer::Nothing;
				}

				int const step = event.key == Key::Left ? -1 : 1;

				OpenMenuAt((open + step + (int)count) % (int)count);
				return MenuAnswer::Moved;
			}

			case Key::Up:
			case Key::Down:
			case Key::Home:
			case Key::End:
			{
				int to = -1;

				if (event.key == Key::Up || event.key == Key::Down)
				{
					to = NextEnabled(lit, event.key == Key::Up ? -1 : 1);
				}
				else
				{
					to = event.key == Key::Home ? NextEnabled(-1, 1) : NextEnabled(items, -1);
				}

				if (to < 0 || to == lit)
				{
					return MenuAnswer::Nothing;
				}

				lit = to;
				KeepLitShown();
				return MenuAnswer::Moved;
			}

			case Key::Enter:
			case Key::Space:
			case Key::Letter:
			{
				bool const picks = event.key != Key::Letter || (event.letter == ' ' && !event.ctrl);

				if (!picks || lit < 0 || !enabled[open][lit])
				{
					return MenuAnswer::Nothing;
				}

				picked = menus[open].items[lit].action;
				Close();
				return MenuAnswer::Picked;
			}

			default:
				return MenuAnswer::Nothing;
		}
	}

	void MenuBar::LayOut(const Box &newBar, const Box &newRoom, const int32_t *newTitleWidths, const int32_t *newDropWidths, int32_t newItemHeight)
	{
		bar = newBar;
		room = newRoom;
		bar.width = (int32_t)NotBelowZero(bar.width);
		bar.height = (int32_t)NotBelowZero(bar.height);
		room.width = (int32_t)NotBelowZero(room.width);
		room.height = (int32_t)NotBelowZero(room.height);
		itemHeight = (int32_t)NotBelowZero(newItemHeight);

		for (size_t m = 0; m < kMenuMostMenus; m++)
		{
			titleWidths[m] = m < count && newTitleWidths ? (int32_t)NotBelowZero(newTitleWidths[m]) : 0;
			dropWidths[m] = m < count && newDropWidths ? (int32_t)NotBelowZero(newDropWidths[m]) : 0;
		}

		KeepLitShown();
	}

	// The titles go left to right along the bar, each as wide as the view measured it, cut where the bar ends
	Box MenuBar::TitleBox(size_t menu) const
	{
		Box box = { 0, 0, 0, 0 };

		if (menu >= count)
		{
			return box;
		}

		int64_t left = bar.x;

		for (size_t m = 0; m < menu; m++)
		{
			left += titleWidths[m];
		}

		int64_t const barRight = (int64_t)bar.x + bar.width;
		int64_t const right = left + titleWidths[menu] < barRight ? left + titleWidths[menu] : barRight;

		box.x = Clamp32(left < barRight ? left : barRight);
		box.y = bar.y;
		box.width = Clamp32(NotBelowZero(right - left));
		box.height = bar.height;
		return box;
	}

	// Under its title, moved left to stay inside the room, as wide as the view measured it but no wider than the room, and as tall as
	// the whole items that fit
	Box MenuBar::DropBox() const
	{
		Box box = { 0, 0, 0, 0 };
		size_t const shown = ShownItems();

		if (shown == 0)
		{
			return box;
		}

		int64_t const width = dropWidths[open] < room.width ? dropWidths[open] : room.width;
		int64_t const roomLeft = room.x;
		int64_t const roomRight = roomLeft + room.width;
		int64_t left = TitleBox((size_t)open).x;

		if (left + width > roomRight)
		{
			left = roomRight - width;
		}

		if (left < roomLeft)
		{
			left = roomLeft;
		}

		box.x = Clamp32(left);
		box.y = room.y;
		box.width = Clamp32(width);
		box.height = Clamp32((int64_t)shown * itemHeight);
		return box;
	}

	MenuAnswer MenuBar::Press(int32_t x, int32_t y)
	{
		picked = 0;
		refused = 0;
		openedByPress = false;

		if (!menus)
		{
			return MenuAnswer::Nothing;
		}

		for (size_t m = 0; m < count; m++)
		{
			if (BoxHolds(TitleBox(m), x, y))
			{
				if (open == (int)m)
				{
					return Close();
				}

				bool const wasOpen = open >= 0;

				OpenMenuAt((int)m);
				openedByPress = true;
				return wasOpen ? MenuAnswer::Moved : MenuAnswer::Opened;
			}
		}

		if (open < 0)
		{
			return MenuAnswer::Nothing;
		}

		Box const drop = DropBox();

		if (!BoxHolds(drop, x, y))
		{
			return Close();
		}

		// Which item the press is on: whole rows of itemHeight from the drop-down's top, from the first shown
		size_t const item = first + (size_t)(((int64_t)y - drop.y) / itemHeight);

		if (item >= menus[open].count)
		{
			return MenuAnswer::Nothing;
		}

		// An item that's off is never picked: the menu stays open, and the editor is told which, to say why on the status line
		if (!enabled[open][item])
		{
			refused = menus[open].items[item].action;
			return MenuAnswer::Refused;
		}

		picked = menus[open].items[item].action;
		Close();
		return MenuAnswer::Picked;
	}
}
