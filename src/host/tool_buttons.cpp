// See tool_buttons.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (A8). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include "editor_actions.h"
#include "inteng_menus.h"
#include "tool_buttons.h"

namespace host
{
	namespace
	{
		bool Add(ToolButton *out, size_t most, size_t *count, const char *label, int32_t action, bool lit, bool off)
		{
			if (*count >= most)
			{
				return false;
			}

			ToolButton &button = out[(*count)++];

			memset(&button, 0, sizeof(button));
			_snprintf_s(button.label, sizeof(button.label), _TRUNCATE, "%s", label);
			button.action = action;
			button.lit = lit;
			button.off = off;
			return true;
		}

		// The action of the first item of the screen that does this work, or 0
		int32_t ItemDoing(size_t screen, IntengWork work)
		{
			if (screen >= kIntengScreenCount)
			{
				return 0;
			}

			for (size_t m = 0; m < kIntengScreens[screen].count; m++)
			{
				for (size_t i = 0; i < kIntengScreens[screen].menus[m].count; i++)
				{
					if (kIntengScreens[screen].menus[m].items[i].work == work && kIntengScreens[screen].menus[m].items[i].tag == IntengTag::None)
					{
						return kIntengScreens[screen].menus[m].items[i].action;
					}
				}
			}

			return 0;
		}

		int32_t TextWidth(const char *text, PaintLetterWidth widthOf)
		{
			int64_t width = 0;

			for (const char *c = text; *c; c++)
			{
				width += widthOf((unsigned char)*c);
			}

			return width > 100000 ? 100000 : (int32_t)width;
		}
	}

	size_t ToolButtons_Tools(const ViewToggles &now, bool outlinesCanBeShown, ToolButton *out, size_t most)
	{
		size_t count = 0;

		if (!out)
		{
			return 0;
		}

		Add(out, most, &count, "Polygon", kActionViewPolygon, now.drawMode == DrawMode::Polygon, false);
		Add(out, most, &count, "Polyline", kActionViewPolyline, now.drawMode == DrawMode::Polyline, !outlinesCanBeShown);
		Add(out, most, &count, "Filled", kActionViewFilledPolyline, now.drawMode == DrawMode::FilledPolyline, !outlinesCanBeShown);
		Add(out, most, &count, "Wire tex.", kActionViewWireTextured, now.drawMode == DrawMode::WireTextured, false);
		Add(out, most, &count, "Gouraud", kActionViewGouraud, now.gouraud == Gouraud::On, false);
		Add(out, most, &count, "Textures", kActionViewTextures, now.textured, false);
		Add(out, most, &count, "Headlight", kActionViewHeadlight, now.headlight, false);
		return count;
	}

	size_t ToolButtons_Panel(size_t screen, bool objectSelected, ToolButton *out, size_t most)
	{
		size_t count = 0;

		if (!out)
		{
			return 0;
		}

		Add(out, most, &count, "Open", kActionOpenLevel, false, false);
		Add(out, most, &count, "Save copy", kActionSaveCopy, false, false);

		int32_t const add = ItemDoing(screen, IntengWork::CreateLikeSelected);
		int32_t const remove = ItemDoing(screen, IntengWork::DeleteObject);

		if (add)
		{
			Add(out, most, &count, "Add", add, false, !objectSelected);
		}

		if (remove)
		{
			Add(out, most, &count, "Delete", remove, false, !objectSelected);
		}

		return count;
	}

	size_t ToolButtons_LayOut(const ToolButton *buttons, size_t count, const ui::Box &room, int32_t gap, int32_t height, PaintLetterWidth widthOf,
		ui::Box *boxes)
	{
		size_t placed = 0;
		int64_t left = room.x;
		int64_t const right = (int64_t)room.x + (room.width > 0 ? room.width : 0);

		if (!buttons || !boxes || !widthOf || height <= 0 || room.height < height)
		{
			return 0;
		}

		for (size_t i = 0; i < count; i++)
		{
			int64_t const width = (int64_t)TextWidth(buttons[i].label, widthOf) + 2 * kPaintMarginAcross;

			if (left + width > right)
			{
				break;
			}

			boxes[placed] = ui::Box{ (int32_t)left, room.y, (int32_t)width, height };
			placed++;
			left += width + (gap > 0 ? gap : 0);
		}

		return placed;
	}

	int ToolButtons_At(const ui::Box *boxes, size_t placed, int32_t x, int32_t y)
	{
		for (size_t i = 0; i < placed && boxes; i++)
		{
			if (ui::BoxHolds(boxes[i], x, y))
			{
				return (int)i;
			}
		}

		return -1;
	}
}
