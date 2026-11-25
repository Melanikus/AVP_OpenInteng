// See editor_bar.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (A6, the all-menus build). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".

#include "editor_bar.h"

#include <string.h>

#include "autoplot_switch.h"   // the owner's "hide auto pilot": the two Auto-plot items left out unless AutoPlot=1
#include "menu_paint.h"
#include "video_mode_screen.h"   // VideoModeScreen_Number: the Video screen Mode > Video shows

using host::kIntengScreenCount;
using host::kIntengScreens;

// The editor's own actions inteng's items now do (editor_bar.h)
static bool LeftOffTheBar(int32_t id)
{
	return id == 2 || id == 301;
}

// The screens with a second (pop-up) table, by inteng's table addresses: a screen's table, then its pop-up's
static const char *const kPopups[][2] =
{
	{ "0x5e2e38", "0x5e2c88" },   // Module Linking: its eight link commands
	// Object Placement (AvP): the menu object DAT_008f4cdc, FUN_004d68e0 page 0, registers the placement table 0x5e51b8 as the bar (method +0x10, 0x4d8249) and the Quick-create Lists table 0x5e7108 as the right-click
	// pop-up (method +0x14, 0x4d825b: PUSH 0x5e7108 before it); the other game's variant (0x5e5938) builds its pop-up in code (FUN_004f6a90, 0x4d8236)
	{ "0x5e51b8", "0x5e7108" },
};

size_t EditorBar_PopupScreen(size_t screen)
{
	if (screen >= kIntengScreenCount)
	{
		return kEditorNoPopup;
	}

	for (size_t p = 0; p < sizeof(kPopups) / sizeof(kPopups[0]); p++)
	{
		if (strcmp(kIntengScreens[screen].table, kPopups[p][0]) != 0)
		{
			continue;
		}

		for (size_t s = 0; s < kIntengScreenCount; s++)
		{
			if (strcmp(kIntengScreens[s].table, kPopups[p][1]) == 0)
			{
				return s;
			}
		}
	}

	return kEditorNoPopup;
}

// The page a code-built screen shows on both its bars (inteng's menu method +0x18, Waypoint mode's FUN_00583b40 and its kin choose it); 0 is the page the screen starts with
static int32_t shownPage = 0;

void EditorBar_SetPage(int32_t page)
{
	shownPage = page < 0 ? 0 : page;
}

// The page of the right-click pop-up alone (inteng's menu method +0x1c: the placed hierarchy's select FUN_004e9b20 calls it with 6, its unselect FUN_004e9b40 with 0, reassess/object-placement.md
// PH-1); 0: the screen's own pop-up. Object Placement and Track Editing are one menu object in inteng (DAT_008f4cdc, FUN_004d68e0), and the pages FUN_004d68e0 builds are kept with Track Editing's
// table here (0x5e6e98), so a pop-up page is looked for there. Added 2026-11-16 by Melanikus (Vasquez).
static int32_t shownPopupPage = 0;

void EditorBar_SetPopupPage(int32_t page)
{
	shownPopupPage = page < 0 ? 0 : page;
}

int32_t EditorBar_PopupPage()
{
	return shownPopupPage;
}

static bool PopupPageMenus(size_t screen, const host::IntengMenu **menus, size_t *count)
{
	if (shownPopupPage == 0 || screen >= kIntengScreenCount || strcmp(kIntengScreens[screen].table, "0x5e51b8") != 0)
	{
		return false;
	}

	for (size_t s = 0; s < kIntengScreenCount; s++)
	{
		if (strcmp(kIntengScreens[s].table, "0x5e6e98") != 0 || !kIntengScreens[s].pages)
		{
			continue;
		}

		for (size_t p = 0; p < kIntengScreens[s].pageCount; p++)
		{
			if (kIntengScreens[s].pages[p].page == shownPopupPage && kIntengScreens[s].pages[p].row == 2)
			{
				*menus = kIntengScreens[s].pages[p].menus;
				*count = kIntengScreens[s].pages[p].count;
				return *menus != nullptr && *count > 0;
			}
		}
	}

	return false;
}

// The screen as its bars show it now: on a page other than 0 each row is that page's menus (none when the page has none for that row)
static host::IntengScreen ScreenOnPage(const host::IntengScreen &screen)
{
	host::IntengScreen shown = screen;

	if (shownPage != 0 && screen.pages)
	{
		shown.menus = nullptr;
		shown.count = 0;
		shown.secondRow = nullptr;
		shown.secondRowCount = 0;

		for (size_t p = 0; p < screen.pageCount; p++)
		{
			if (screen.pages[p].page != shownPage)
			{
				continue;
			}

			if (screen.pages[p].row == 1)
			{
				shown.menus = screen.pages[p].menus;
				shown.count = screen.pages[p].count;
			}
			else if (screen.pages[p].row == 2)
			{
				shown.secondRow = screen.pages[p].menus;
				shown.secondRowCount = screen.pages[p].count;
			}
		}
	}

	return shown;
}

// The words of a cascade entry ("Add keyframe flag >"): kept here while the pop-up is open (one pop-up at a time)
static char popupGroupNames[ui::kMenuMostItems][40];

static bool PopupMenusOf(size_t screen, const host::IntengMenu **menus, size_t *count)
{
	if (PopupPageMenus(screen, menus, count))
	{
		return true;
	}

	size_t const popup = EditorBar_PopupScreen(screen);

	if (popup != kEditorNoPopup)
	{
		*menus = kIntengScreens[popup].menus;
		*count = kIntengScreens[popup].count;
		return *menus != nullptr && *count > 0;
	}

	if (screen >= kIntengScreenCount)
	{
		return false;
	}

	// a code-built screen: the second table of the page shown
	const host::IntengScreen shown = ScreenOnPage(kIntengScreens[screen]);

	*menus = shown.secondRow;
	*count = shown.secondRowCount;
	return *menus != nullptr && *count > 0;
}

// One inteng menu's items as rows of the room, from items[*used]: an item of level 1 opens a CASCADE (inteng's kind 1 with kind-2 records under it, BAR-10), its rows the level-2 items after it, put in
// room->subItems from *subUsed; a level-2 item is taken only so. *added counts the rows. False when the room, or a cascade, would hold more than it can.
// EVERY ROW AS INTENG DRAWS IT: its words alone, in the menu's one colour (FUN_004b6f70), so no tag goes after an item's name, also for an item we have not built (the owner, 2026-11-18:
// "I dont think the color gray is in inteng at all"; Bishop: "menus look exactly like inteng's"). Such an item still says why when it is clicked: EditorBar_Run puts IntengMenus_Why on the
// status line. (Until 2026-11-18 the row showed IntengMenus_TagWords after the name: " [wip]", " [Blender]", " [no AvP]", " [unfinished in inteng]".)
static bool AddIntengItems(EditorBarRoom *room, const host::IntengItem *items, size_t count, size_t most, size_t *used, size_t *subUsed, size_t *added)
{
	for (size_t i = 0; i < count; i++)
	{
		const host::IntengItem &item = items[i];

		if (item.level == 2)
		{
			continue;   // a cascade's row: taken with the level-1 item above it
		}

		if (host::AutoPlot_CommandHidden(item.command, host::AutoPlot_Shown()))
		{
			continue;   // hidden by the owner's ruling (autoplot_switch.h): not on the bar, not in the pop-up
		}

		if (*used >= most)
		{
			return false;
		}

		ui::MenuItem shown = { item.name, nullptr, item.action, nullptr, item.access };

		if (item.level == 1)
		{
			size_t const from = *subUsed;

			for (size_t k = i + 1; k < count && items[k].level == 2; k++)
			{
				if (*subUsed >= kEditorBarMostItems || *subUsed - from >= ui::kMenuMostSubItems)
				{
					return false;
				}

				ui::MenuItem const row = { items[k].name, nullptr, items[k].action, nullptr, items[k].access };

				room->subItems[(*subUsed)++] = row;
			}

			shown.subMenu = *subUsed > from;
			shown.subItems = *subUsed > from ? room->subItems + from : nullptr;
			shown.subCount = *subUsed - from;
		}

		room->items[(*used)++] = shown;
		(*added)++;
	}

	return true;
}

bool EditorBar_MakePopupLevel(size_t screen, int32_t group, EditorBarRoom *room)
{
	const host::IntengMenu *menus = nullptr;
	size_t count = 0;

	if (!room || !PopupMenusOf(screen, &menus, &count))
	{
		return false;
	}

	room->menuCount = 0;

	size_t used = 0;

	if (group >= 0)
	{
		if ((size_t)group >= count)
		{
			return false;
		}

		size_t subUsed = 0, added = 0;

		if (!AddIntengItems(room, menus[group].items, menus[group].count, ui::kMenuMostItems, &used, &subUsed, &added))
		{
			return false;
		}

		room->menus[0].title = menus[group].title;
		room->menus[0].titleAccess = menus[group].access;
	}
	else
	{
		size_t subUsed = 0;   // the cascades' rows in room->subItems

		for (size_t m = 0; m < count; m++)
		{
			if (used >= ui::kMenuMostItems)
			{
				return false;
			}

			if (menus[m].count == 1 && host::AutoPlot_CommandHidden(menus[m].items[0].command, host::AutoPlot_Shown()))
			{
				continue;   // hidden by the owner's ruling (autoplot_switch.h)
			}

			if (menus[m].count == 1)
			{
				const host::IntengItem &item = menus[m].items[0];
				ui::MenuItem const shown = { item.name, nullptr, item.action, nullptr, item.access };   // its words alone, as every row (AddIntengItems)

				room->items[used++] = shown;
			}
			else if (menus[m].count > 1)
			{
				// the words alone: the row's mark is inteng's glyph 2, drawn by the menus (menu_paint.cpp), or ">" when inteng's font was not read
				_snprintf_s(popupGroupNames[used], sizeof(popupGroupNames[used]), _TRUNCATE, "%s", menus[m].title);

				ui::MenuItem shown = { popupGroupNames[used], nullptr, kEditorPopupGroupBase + (int32_t)m, nullptr, menus[m].access };

				shown.subMenu = true;

				// THE SUB-MENU BESIDE THE POP-UP (Drake 2026-11-24, the owner on alpha.6: "place above/below etc is a sub menu, right now the main menu where you were
				// disappears and only the 'here, above...etc' show"): inteng's pop-up opens a sub-level to the side and keeps itself (FUN_004b8350), so a group of plain
				// rows is this row's CASCADE, shown beside the pop-up as the pointer comes on the row, the pop-up staying (ui/menu_bar.h's subItems, as the bar's own
				// cascades). Before, picking the row put its rows in the pop-up's place. A group with a sub-level of its own inside (a level-1 row: two levels deep,
				// which a cascade can't hold) still opens in the pop-up's place, as before: NAMED.
				size_t const from = subUsed;
				bool plain = true;

				for (size_t k = 0; k < menus[m].count && plain; k++)
				{
					host::IntengItem const &row = menus[m].items[k];

					if (host::AutoPlot_CommandHidden(row.command, host::AutoPlot_Shown()))
					{
						continue;   // hidden by the owner's ruling (autoplot_switch.h), as AddIntengItems leaves it out
					}

					plain = row.level == 0 && subUsed < kEditorBarMostItems && subUsed - from < ui::kMenuMostSubItems;

					if (plain)
					{
						ui::MenuItem const shownRow = { row.name, nullptr, row.action, nullptr, row.access };

						room->subItems[subUsed++] = shownRow;
					}
				}

				if (plain && subUsed > from)
				{
					shown.subItems = room->subItems + from;
					shown.subCount = subUsed - from;
				}
				else
				{
					subUsed = from;   // not a cascade: the row opens its group in the pop-up's place (PopupPicked)
				}

				room->items[used++] = shown;
			}
		}

		room->menus[0].title = "Pop-up";
	}

	room->menus[0].items = room->items;
	room->menus[0].count = used;
	room->menuCount = 1;
	return used > 0;
}

bool EditorBar_MakePopup(size_t screen, EditorBarRoom *room)
{
	return EditorBar_MakePopupLevel(screen, -1, room);
}

bool EditorBar_Make(size_t screen, EditorBarRoom *room)
{
	if (!room || screen >= kIntengScreenCount)
	{
		return false;
	}

	room->menuCount = 0;

	const host::IntengScreen inteng = ScreenOnPage(kIntengScreens[screen]);
	size_t const menuCount = inteng.count;

	if (menuCount > ui::kMenuMostMenus)
	{
		return false;
	}

	size_t used = 0, subUsed = 0;

	// inteng's own menus, each item with its words alone, and each cascade with its rows (BAR-10). A menu whose items are all hidden (autoplot_switch.h) is not on the bar
	size_t kept = 0;

	for (size_t m = 0; m < inteng.count; m++)
	{
		ui::Menu &menu = room->menus[kept];

		menu.title = inteng.menus[m].title;
		menu.titleAccess = inteng.menus[m].access;
		menu.items = room->items + used;
		menu.count = 0;

		if (!AddIntengItems(room, inteng.menus[m].items, inteng.menus[m].count, kEditorBarMostItems, &used, &subUsed, &menu.count))
		{
			return false;
		}

		kept += menu.count > 0 || inteng.menus[m].count == 0 ? 1 : 0;
	}

	// NO MENUS OF OURS ON THE BAR (ruling 10b, Bishop 2026-11-16: "remove everything our window shows that inteng never had"): the "View (ours)" menu and the hub's "Screens (ours)" menu are gone, and with them "Back to
	// Construction". The bar is inteng's own tables and nothing else. The actions of the editor's table (editor_actions.cpp) are still done by their keys (Ctrl+O, Ctrl+E, 7, 8, 9, B, Ctrl+B).

	(void)menuCount;
	room->menuCount = kept;
	return true;
}

bool EditorBar_MakeSecond(size_t screen, EditorBarRoom *room)
{
	if (!room || screen >= kIntengScreenCount)
	{
		return false;
	}

	room->menuCount = 0;

	const host::IntengScreen inteng = ScreenOnPage(kIntengScreens[screen]);

	if (!inteng.secondRow || inteng.secondRowCount == 0)
	{
		return true;
	}

	if (inteng.secondRowCount > ui::kMenuMostMenus)
	{
		return false;
	}

	size_t used = 0, subUsed = 0;

	size_t kept = 0;   // a menu whose items are all hidden (autoplot_switch.h) is not on the bar

	for (size_t m = 0; m < inteng.secondRowCount; m++)
	{
		ui::Menu &menu = room->menus[kept];

		menu.title = inteng.secondRow[m].title;
		menu.titleAccess = inteng.secondRow[m].access;
		menu.items = room->items + used;
		menu.count = 0;

		if (!AddIntengItems(room, inteng.secondRow[m].items, inteng.secondRow[m].count, kEditorBarMostItems, &used, &subUsed, &menu.count))
		{
			return false;
		}

		kept += menu.count > 0 || inteng.secondRow[m].count == 0 ? 1 : 0;
	}

	room->menuCount = kept;
	return true;
}

ui::PanelSizes EditorBar_Sizes(ui::PanelSizes sizes, bool second)
{
	if (second)
	{
		sizes.toolbarHeight = sizes.toolbarHeight > ui::kPanelMostSide / 2 ? ui::kPanelMostSide : sizes.toolbarHeight * 2;
	}

	return sizes;
}

EditorBarPlace EditorBar_Place(const ui::PanelLayout &layout, bool second)
{
	EditorBarPlace place;

	place.bar1 = layout.toolbar;
	place.bar2 = { layout.toolbar.x, layout.toolbar.y + layout.toolbar.height, layout.toolbar.width, 0 };

	if (second)
	{
		place.bar1.height = layout.toolbar.height / 2;
		place.bar2 = { layout.toolbar.x, layout.toolbar.y + place.bar1.height, layout.toolbar.width, layout.toolbar.height - place.bar1.height };
	}

	place.dropRoom1 = layout.view;
	place.dropRoom1.y = place.bar1.y + place.bar1.height;
	place.dropRoom1.height = layout.view.y + layout.view.height - place.dropRoom1.y > 0 ? layout.view.y + layout.view.height - place.dropRoom1.y : 0;

	place.dropRoom2 = layout.view;
	place.dropRoom2.y = place.bar2.y + place.bar2.height;
	place.dropRoom2.height = layout.view.y + layout.view.height - place.dropRoom2.y > 0 ? layout.view.y + layout.view.height - place.dropRoom2.y : 0;

	return place;
}

int32_t EditorBar_Page()
{
	return shownPage;
}

// ---- Titles that wrap ----

bool EditorBar_MakeRows(size_t screen, int32_t width, host::PaintLetterWidth widthOf, EditorBarRows *rows)
{
	if (!rows || !widthOf)
	{
		return false;
	}

	rows->rowCount = 0;

	for (size_t r = 0; r < kEditorBarMostRows; r++)
	{
		rows->menuCount[r] = 0;
	}

	if (!EditorBar_Make(screen, &rows->logical[0]) || !EditorBar_MakeSecond(screen, &rows->logical[1]))
	{
		return false;
	}

	int32_t const limit = width - kEditorBarEdge;

	// Only the BAR table (method +0x10) is drawn as bars. The second table (method +0x14, logical[1]) is inteng's right-click POP-UP: FUN_004b7c50 shows it on a right press (FUN_004b8350) and never as a bar.
	// It stays made in logical[1] for the pop-up and for the harness's item lookup.
	for (size_t l = 0; l < 1; l++)
	{
		EditorBarRoom const &room = rows->logical[l];

		if (room.menuCount == 0)
		{
			continue;
		}

		int32_t titles[ui::kMenuMostMenus];
		int32_t drops[ui::kMenuMostMenus];

		host::MenuPaint_Measure(room.menus, room.menuCount, widthOf, titles, drops);

		size_t row = 0;
		int32_t used = 0;
		bool fresh = true;   // the first menu of a logical row starts a physical row

		for (size_t m = 0; m < room.menuCount; m++)
		{
			if (fresh || (rows->menuCount[row] > 0 && used + titles[m] > limit))
			{
				if (rows->rowCount >= kEditorBarMostRows)
				{
					rows->rowCount = 0;
					return false;
				}

				row = rows->rowCount++;
				rows->menuCount[row] = 0;
				used = 0;
				fresh = false;
			}

			if (rows->menuCount[row] >= ui::kMenuMostMenus)
			{
				rows->rowCount = 0;
				return false;
			}

			rows->menus[row][rows->menuCount[row]++] = room.menus[m];
			used += titles[m];
		}
	}

	// A page of inteng's with no menu of its own on the bar (some of Track Editing's pages hold only the pop-up table) is one empty row: the editor's own menu that used to be on every page is gone
	if (rows->rowCount == 0)
	{
		rows->rowCount = 1;
		rows->menuCount[0] = 0;
	}

	return true;
}

ui::PanelSizes EditorBar_SizesRows(ui::PanelSizes sizes, size_t rows)
{
	if (rows > 1)
	{
		int64_t const height = (int64_t)sizes.toolbarHeight * (int64_t)(rows > kEditorBarMostRows ? kEditorBarMostRows : rows);

		sizes.toolbarHeight = height > ui::kPanelMostSide ? ui::kPanelMostSide : (int32_t)height;
	}

	return sizes;
}

EditorBarPlaceRows EditorBar_PlaceRows(const ui::PanelLayout &layout, size_t rows)
{
	EditorBarPlaceRows place = {};
	size_t const count = rows < 1 ? 1 : (rows > kEditorBarMostRows ? kEditorBarMostRows : rows);
	int32_t const each = layout.toolbar.height / (int32_t)count;

	for (size_t i = 0; i < kEditorBarMostRows; i++)
	{
		if (i < count)
		{
			int32_t const top = layout.toolbar.y + each * (int32_t)i;

			place.bar[i] = { layout.toolbar.x, top, layout.toolbar.width, i + 1 == count ? layout.toolbar.y + layout.toolbar.height - top : each };
		}
		else
		{
			place.bar[i] = { layout.toolbar.x, layout.toolbar.y + layout.toolbar.height, layout.toolbar.width, 0 };
		}

		place.dropRoom[i] = layout.view;
		place.dropRoom[i].y = place.bar[i].y + place.bar[i].height;
		place.dropRoom[i].height = layout.view.y + layout.view.height - place.dropRoom[i].y > 0 ? layout.view.y + layout.view.height - place.dropRoom[i].y : 0;
	}

	return place;
}

// Does every page of the screen wrap into few enough rows at that width? (and no title is wider than the room)
static bool ScreenFitsAt(size_t screen, host::PaintLetterWidth widthOf, int32_t width, int32_t *widestTitle)
{
	static EditorBarRows rows;
	int32_t const was = shownPage;
	bool fits = true;

	// page 0, then every page the screen registers
	size_t const pages = kIntengScreens[screen].pageCount;

	for (size_t p = 0; p <= pages && fits; p++)
	{
		EditorBar_SetPage(p == 0 ? 0 : kIntengScreens[screen].pages[p - 1].page);

		if (!EditorBar_MakeRows(screen, width, widthOf, &rows))
		{
			fits = false;
		}

		for (size_t l = 0; l < 1 && widestTitle; l++)
		{
			int32_t titles[ui::kMenuMostMenus];
			int32_t drops[ui::kMenuMostMenus];

			host::MenuPaint_Measure(rows.logical[l].menus, rows.logical[l].menuCount, widthOf, titles, drops);

			for (size_t m = 0; m < rows.logical[l].menuCount; m++)
			{
				*widestTitle = titles[m] > *widestTitle ? titles[m] : *widestTitle;
			}
		}
	}

	shownPage = was;
	return fits;
}

int32_t EditorBar_LeastWidth(host::PaintLetterWidth widthOf)
{
	int32_t widest = 0;
	int32_t least = 0;

	for (size_t s = 0; s < kIntengScreenCount; s++)
	{
		// the least width for this screen: wider windows never need more rows, so the answer is searched between 64 and 4096
		int32_t low = 64, high = 4096;

		if (!ScreenFitsAt(s, widthOf, high, &widest))
		{
			// not even the widest window gets this screen's bars into the rows allowed: its bars are too big for the editor, and the test says so
			return 4096;
		}

		while (low < high)
		{
			int32_t const middle = low + (high - low) / 2;

			if (ScreenFitsAt(s, widthOf, middle, nullptr))
			{
				high = middle;
			}
			else
			{
				low = middle + 1;
			}
		}

		least = high > least ? high : least;
	}

	// a title is not cut: the window is at least as wide as the widest title and the edge kept past it
	int32_t const forTitles = widest + kEditorBarEdge;

	return least > forTitles ? least : forTitles;
}

bool EditorBar_Run(int32_t action, EditorHostState *host)
{
	if (!host || LeftOffTheBar(action))
	{
		return false;
	}

	const EditorAction *const ours = EditorActions_Find(action);

	if (ours)
	{
		ours->run(host);
		return true;
	}

	if (action == kActionBackToHub)
	{
		host->screen = host::kIntengHub;
		host->statusWords = kIntengScreens[host::kIntengHub].name;
		return true;
	}

	if (action >= kActionScreenFirst && (uint32_t)(action - kActionScreenFirst) < kIntengScreenCount)
	{
		host->screen = (size_t)(action - kActionScreenFirst);
		host->statusWords = kIntengScreens[host->screen].name;
		return true;
	}

	const host::IntengItem *const item = host::IntengMenus_Find(action);

	if (!item)
	{
		return false;
	}

	switch (item->work)
	{
		case host::IntengWork::Quit:
			host->running = false;
			break;
		case host::IntengWork::ToScreen:
			if (item->toScreen >= 0 && (size_t)item->toScreen < kIntengScreenCount)
			{
				host->screen = (size_t)item->toScreen;
				host->statusWords = kIntengScreens[host->screen].name;
			}
			break;
		case host::IntengWork::ToScreenNoTextures:
			if (item->toScreen >= 0 && (size_t)item->toScreen < kIntengScreenCount)
			{
				host->screen = (size_t)item->toScreen;
				host->paintWhite = true;
				host->statusWords = kIntengScreens[host->screen].name;
			}
			break;
		case host::IntengWork::ToHub:
			// Light Sourcing's Exit does its write-back first (FUN_004aa910: Unhide all lights, FUN_004ad630, the level written)
			if (host->screen < kIntengScreenCount && (strcmp(kIntengScreens[host->screen].table, "0x5e1360") == 0 || strcmp(kIntengScreens[host->screen].table, "0x5e1750") == 0))
			{
				host->lightWork = kLightExit;
			}

			host->screen = host::kIntengHub;
			host->statusWords = kIntengScreens[host::kIntengHub].name;
			break;
		case host::IntengWork::ViewPolygon:
			host->viewMode = 1;
			break;
		case host::IntengWork::ViewPolyline:
			host->viewMode = 2;
			break;
		case host::IntengWork::ViewFilledPolyline:
			host->viewMode = 3;
			break;
		case host::IntengWork::LinkExit:
			host->linkOp = kLinkExit;
			host->screen = host::kIntengHub;
			host->statusWords = kIntengScreens[host::kIntengHub].name;
			break;
		case host::IntengWork::LinkExitNew:
			host->linkOp = kLinkExitNew;
			host->screen = host::kIntengHub;
			host->statusWords = kIntengScreens[host::kIntengHub].name;
			break;
		case host::IntengWork::LinkExitAll:
			host->linkOp = kLinkExitAll;
			host->screen = host::kIntengHub;
			host->statusWords = kIntengScreens[host::kIntengHub].name;
			break;
		case host::IntengWork::PlacementKeys:
			host->showPlacementKeys = true;
			break;
		case host::IntengWork::LinkHelp:
			host->showLinkHelp = true;
			break;
		case host::IntengWork::LinkWithAvp:
			host->linkOp = kLinkWithAvp;
			break;
		case host::IntengWork::WaypointCommand:
			host->waypointCommand = item->command;
			host->waypointAsked = true;
			break;
		case host::IntengWork::Keys:
			// Construction's Keys - Movement (0x2be): inteng shows the camera keys' text screen, FUN_005573c0 (TB-C6); the hub draws no status words, so words alone showed nothing
			if (item->command == 0x2be)
			{
				host->showMovementKeys = true;
				break;
			}

			host->statusWords = EditorActions_KeysWords();
			break;
		case host::IntengWork::DeleteObject:
			host->deleteSelectedObject = true;
			break;
		case host::IntengWork::CreateLikeSelected:
			host->createLikeSelected = true;
			break;


		case host::IntengWork::FindSmallModules:
			host->prompt = kPromptSmallLimit;
			break;
		case host::IntengWork::SetBaseModule:
			host->setBaseModule = true;
			break;
		case host::IntengWork::FindObject:
			host->prompt = kPromptFindSelect;
			break;
		case host::IntengWork::SaveCopy:
			host->saveCopy = true;
			break;
		case host::IntengWork::TextScale:
			host->textScaleStep = (action & 0xff) - 1;   // the owner's text sizes, 0x1ff02 .. 0x1ff05: steps 1 to 4 (text_scale.h)
			break;
		case host::IntengWork::PathCommand:
			host->pathCommand = item->command;
			break;
		case host::IntengWork::TrackEdit:
			host->trackCommand = item->command;
			break;
		case host::IntengWork::ExitMode:
			host->exitMode = true;

			// Light Sourcing's Exit does its write-back first (FUN_004aa910: Unhide all lights, FUN_004ad630), then the mode's Exit saves the copy and shows the hub
			if (host->screen < kIntengScreenCount && (strcmp(kIntengScreens[host->screen].table, "0x5e1360") == 0 || strcmp(kIntengScreens[host->screen].table, "0x5e1750") == 0))
			{
				host->lightWork = kLightExit;
			}

			break;
		case host::IntengWork::Strategy:
			host->strategyCommand = item->command;
			break;
		case host::IntengWork::ChangeToModule:
			host->changeType = 1;
			break;
		case host::IntengWork::ChangeToPlaced:
			host->changeType = 2;
			break;
		case host::IntengWork::AltLocation:
			// every item goes to AltLocationCommand; Assign Group Number (0x66) and Change shown location (0x69) open inteng's number window there (OP-36, OP-38)
			host->altLocation = item->command;
			break;
		case host::IntengWork::QuickCreate:
			host->quickCreate = item->command;
			break;
		case host::IntengWork::HierarchyCommand:
			host->hierarchyCommand = item->command;
			break;
		case host::IntengWork::AcousticCommand:
			host->acousticCommand = item->command;
			break;
		case host::IntengWork::PlacedHierarchy:
			host->placedHierarchyCommand = item->command;
			break;
		case host::IntengWork::VideoModeEnter:
			// Construction's Mode, Video (the owner's addition 11-22): the Video screen, found by its name (video_mode_screen.h)
			if (host::VideoModeScreen_Number() < kIntengScreenCount)
			{
				host->screen = host::VideoModeScreen_Number();
				host->statusWords = kIntengScreens[host->screen].name;
			}
			break;
		case host::IntengWork::VideoBrowse:
			host->videoBrowse = true;
			break;
		case host::IntengWork::TextureImport:
			host->textureImport = true;
			break;
		case host::IntengWork::ViewLighting:
			host->viewMode = 4;
			break;
		case host::IntengWork::PlacementPolyCounts:
			host->placementPolyCounts = true;
			break;
		case host::IntengWork::FixTearsSelected:
			host->fixTears = 1;
			break;
		case host::IntengWork::FixTearsEvery:
			host->fixTears = 2;
			break;
		case host::IntengWork::RecentrePolygon:
			host->recentrePolygon = true;
			break;
		case host::IntengWork::SplitScreenToggle:
			host->splitScreen = true;
			break;
		case host::IntengWork::MoveRelativeToModule:
			host->moveRelativeModule = true;
			break;
		case host::IntengWork::GravityToggle:
			host->gravityToggle = true;
			break;
		case host::IntengWork::SnapToVertex:
			host->snapToVertex = true;
			break;
		case host::IntengWork::SpecialCommand:
			host->specialCommand = item->command;
			break;
		case host::IntengWork::TextureMapCommand:
			host->textureCommand = item->command;
			break;
		case host::IntengWork::HideAllLights:
			host->lightWork = kLightHideAll;
			break;
		case host::IntengWork::UnhideAllLights:
			host->lightWork = kLightUnhideAll;
			break;
		case host::IntengWork::SelectLights:
			host->lightWork = kLightSelectLights;
			break;
		case host::IntengWork::LightHierarchy:
			host->lightWork = kLightHierarchy;
			break;
		case host::IntengWork::LightSequence:
			host->lightWork = kLightSequence;
			break;
		case host::IntengWork::SoftEdge:
			host->prompt = kPromptSoftEdge;
			break;
		case host::IntengWork::DoorsClose:
			host->lightWork = kLightDoorsClose;
			break;
		case host::IntengWork::DoorsOpen:
			host->lightWork = kLightDoorsOpen;
			break;
		case host::IntengWork::LightToggle:
			host->lightWork = kLightToggle;
			break;
		case host::IntengWork::LightSwitchAllOn:
			host->lightWork = kLightSwitchAllOn;
			break;
		case host::IntengWork::SaveIntensitiesGame:
			host->lightWork = kLightSaveGame;
			break;
		case host::IntengWork::SaveIntensitiesTools:
			host->lightWork = kLightSaveTools;
			break;
		case host::IntengWork::SelectObjects:
			host->lightWork = kLightSelectObjects;
			break;
		case host::IntengWork::LightHelp:
			host->lightWork = kLightHelp;
			break;
		case host::IntengWork::ConstructionKeys:
			host->showConstructionKeys = true;
			break;
		case host::IntengWork::CopyTrack:
			host->copyTrack = true;
			break;
		case host::IntengWork::PasteTrackAbsolute:
			host->pasteTrackAbsolute = true;
			break;
		case host::IntengWork::PasteTrackRelative:
			host->pasteTrackRelative = true;
			break;
		case host::IntengWork::MoveRelToPoly:
			host->moveRelToPoly = true;
			break;
		case host::IntengWork::MoveCentreTop:
			host->moveCentre = 1;
			break;
		case host::IntengWork::MoveCentreBottom:
			host->moveCentre = 2;
			break;
		case host::IntengWork::ModuleOverlaps:
			host->moduleOverlaps = true;
			break;
		case host::IntengWork::RecalculateNormals:
			host->recalculateNormals = true;
			break;
		case host::IntengWork::Screenshot:
			host->takeScreenshot = true;
			break;
		case host::IntengWork::WriteAsc:
			host->prompt = kPromptAsc;
			break;
		case host::IntengWork::WriteAscDoorsClosed:
			host->prompt = kPromptAscDoors;
			break;
		case host::IntengWork::WriteC:
			host->prompt = kPromptWriteC;   // write_c_file.h: nothing happens unless exactly one object is selected
			break;
		case host::IntengWork::LinkSetCurrent:
			host->linkOp = kLinkSetCurrent;
			break;
		case host::IntengWork::LinkNormal:
			host->linkOp = kLinkNormal;
			break;
		case host::IntengWork::LinkBranch:
			host->linkOp = kLinkBranch;
			break;
		case host::IntengWork::LinkDelete:
			host->linkOp = kLinkDelete;
			break;
		case host::IntengWork::LinkClose:
			host->linkOp = kLinkClose;
			break;
		case host::IntengWork::ReverseLinksPicked:
			host->linkOp = kLinkReverseModule;
			break;
		case host::IntengWork::ReverseLinksAll:
			host->linkOp = kLinkReverseAll;
			break;
		case host::IntengWork::DeleteLinksPicked:
			host->linkOp = kLinkDeleteLinks;
			break;
		case host::IntengWork::DeleteLinksAll:
			host->prompt = kPromptDeleteAllLinks;
			break;
		case host::IntengWork::AutoLinkModule:
			host->linkOp = kLinkAutoModule;
			break;
		case host::IntengWork::AutoLinkEnvironment:
			host->linkOp = kLinkAutoEnvironment;
			break;
		case host::IntengWork::AutoLinkModuleNew:
			host->linkOp = kLinkAutoModuleNew;
			break;
		case host::IntengWork::AutoLinkEnvironmentNew:
			host->linkOp = kLinkAutoEnvironmentNew;
			break;
		case host::IntengWork::AutoLinkLoadFile:
			host->linkOp = kLinkLoadFile;
			break;
		case host::IntengWork::SetVisibility:
			host->prompt = kPromptVisibility;
			break;
		case host::IntengWork::LinkOpen:
			host->linkOp = kLinkOpen;
			break;
		case host::IntengWork::MixedView:
			host->linkOp = kLinkMixed;
			break;
		case host::IntengWork::ShowAllModules:
			host->linkOp = kLinkShowAll;
			break;
		case host::IntengWork::ViewDistanceToggle:
			host->linkOp = kLinkViewDistance;
			break;
		case host::IntengWork::OnlyLinkedToggle:
			host->linkOp = kLinkOnlyLinked;
			break;
		case host::IntengWork::SelectByPolygons:
			host->optionToggle = 1;
			break;
		case host::IntengWork::PreferBmp:
			host->optionToggle = 2;
			break;
		case host::IntengWork::PolygonViewDistance:
			host->optionToggle = 3;
			break;
		case host::IntengWork::LinkView:
			host->linkOp = kLinkView;
			break;
		case host::IntengWork::LinkHide:
			host->linkOp = kLinkHide;
			break;
		case host::IntengWork::ShowPolyCounts:
			host->linkOp = kLinkPolyCounts;
			break;
		case host::IntengWork::SelectWithCentres:
			host->linkOp = kLinkCentres;
			break;
		case host::IntengWork::HideObjects:
			host->linkOp = kLinkHideObjects;
			break;
		case host::IntengWork::UnhideLastObject:
			host->linkOp = kLinkUnhideLast;
			break;
		case host::IntengWork::UnhideAllObjects:
			host->linkOp = kLinkUnhideAll;
			break;
		case host::IntengWork::Find:
			host->prompt = kPromptFind;
			break;
		case host::IntengWork::MoveRelative:
			host->prompt = kPromptMove;
			break;
		case host::IntengWork::FindNotDone:
			host->findNotDone = true;
			break;
		case host::IntengWork::RecalcEntryPoints:
			host->recalcEntryPoints = true;
			break;
		case host::IntengWork::AdjacentAdd:
			host->adjacentOp = kAdjAdd;
			break;
		case host::IntengWork::AdjacentDelete:
			host->adjacentOp = kAdjDelete;
			break;
		case host::IntengWork::AdjacentSetCurrent:
			host->adjacentOp = kAdjSetCurrent;
			break;
		case host::IntengWork::AdjacentExit:
			host->adjacentOp = kAdjExit;
			host->screen = host::kIntengHub;
			host->statusWords = kIntengScreens[host::kIntengHub].name;
			break;
		case host::IntengWork::AdjacentViewDistance:
			host->adjacentOp = kAdjViewDistance;
			break;
		case host::IntengWork::AdjacentShowAll:
			host->adjacentOp = kAdjShowAll;
			break;
		case host::IntengWork::DeleteModuleData:
			host->deleteModuleData = true;
			break;
		case host::IntengWork::MergeObjects:
			host->mergeObjects = true;
			break;
		case host::IntengWork::Rescale:
			host->prompt = kPromptRescale;
			break;
		case host::IntengWork::ChooserStart:
			host->chooserOp = kChooserStart;
			break;
		case host::IntengWork::ChooserStartCreate:
			host->chooserOp = kChooserStartCreate;
			break;
		case host::IntengWork::ChooserWith:
			host->chooserOp = kChooserWith;
			break;
		case host::IntengWork::ChooserWithout:
			host->chooserOp = kChooserWithout;
			break;
		default:
			// An item with nothing built behind it: it says why it does nothing yet (its row shows its name alone, as inteng's does)
			host->statusWords = host::IntengMenus_Why(item->tag);
			break;
	}

	return true;
}

int32_t EditorBar_WidestTitles(host::PaintLetterWidth widthOf)
{
	static EditorBarRoom room;
	int64_t widest = 0;

	for (size_t s = 0; s < kIntengScreenCount; s++)
	{
		int32_t titles[ui::kMenuMostMenus];
		int32_t drops[ui::kMenuMostMenus];

		if (!EditorBar_Make(s, &room))
		{
			continue;
		}

		host::MenuPaint_Measure(room.menus, room.menuCount, widthOf, titles, drops);

		int64_t sum = 0;

		for (size_t m = 0; m < room.menuCount; m++)
		{
			sum += titles[m];
		}

		widest = sum > widest ? sum : widest;

		// (the second table is the right-click pop-up, not a bar: it takes no width of the window)
	}

	return widest > ui::kPanelMostSide ? ui::kPanelMostSide : (int32_t)widest;
}

void EditorBar_CutOff(const ui::MenuBar &bar, const ui::Menu *menus, size_t count, char *words, size_t size)
{
	if (!words || size == 0)
	{
		return;
	}

	words[0] = '\0';

	if (!menus)
	{
		return;
	}

	std::string said;

	for (size_t m = 0; m < count && m < ui::kMenuMostMenus; m++)
	{
		if (bar.TitleBox(m).width <= 0 && menus[m].title)
		{
			said += said.empty() ? "Menus past the window's edge: " : ", ";
			said += menus[m].title;
		}
	}

	size_t const length = said.size() < size - 1 ? said.size() : size - 1;

	memcpy(words, said.data(), length);
	words[length] = '\0';
}
