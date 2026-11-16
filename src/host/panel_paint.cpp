// See panel_paint.h. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "panel_paint.h"

#include <string.h>

#include "online_screens.h"

namespace host
{
	std::string PanelPaint_SafeText(const std::string &text)
	{
		char safe[kPaintMostLetters + 1];
		OnlineScreens_SafeName(text.data(), text.size(), safe, sizeof(safe));
		return std::string(safe);
	}

	bool PanelPaint_FitText(const std::string &text, const ui::Box &box, PaintLetterWidth widthOf, int32_t lineHeight, PaintText *out)
	{
		if (!out || !widthOf)
		{
			return false;
		}

		// The room inside the margin, and the line's height as counted (1 to kPaintTallestLine), in 64 bits
		int64_t const room = (int64_t)box.width - 2 * kPaintMarginAcross;
		int64_t const height = (int64_t)box.height - 2 * kPaintMarginDown;
		int64_t const line = lineHeight < 1 ? 1 : lineHeight > kPaintTallestLine ? kPaintTallestLine : lineHeight;

		// No room, or a box so near the end of the numbers that its text's place wouldn't fit one (the layout never gives one)
		if (room <= 0 || height < line || (int64_t)box.x + kPaintMarginAcross > INT32_MAX || (int64_t)box.y + kPaintMarginDown > INT32_MAX)
		{
			return false;
		}

		// Made safe, then cut. The safe text is at most kPaintMostLetters, so it always fits here with its 0. The room is at most a 32-bit
		// side, so it fits an int, and the game's cutting adds at most 1000 a letter for at most 255 letters, so its sum can't overflow.
		std::string const safe = PanelPaint_SafeText(text);
		char letters[kPaintMostLetters + 1];
		memcpy(letters, safe.data(), safe.size());
		letters[safe.size()] = '\0';
		OnlineScreens_FitToWidth(letters, (int)room, widthOf);

		if (letters[0] == '\0')
		{
			return false;
		}

		out->text = letters;
		out->x = (int32_t)((int64_t)box.x + kPaintMarginAcross);
		out->y = (int32_t)((int64_t)box.y + kPaintMarginDown);
		return true;
	}

	bool PanelPaint_CutBox(const PaintBox &box, int32_t width, int32_t height, PaintBox *out)
	{
		if (!out)
		{
			return false;
		}

		// Its edges, held to the window's, in 64 bits. A box, or a window, of no size or of a size below 0 ends up with no room between its
		// edges, so the test after this refuses it with every other box that has no part inside.
		int64_t const left = box.x < 0 ? 0 : (int64_t)box.x;
		int64_t const top = box.y < 0 ? 0 : (int64_t)box.y;
		int64_t const rightEnd = (int64_t)box.x + box.width;
		int64_t const bottomEnd = (int64_t)box.y + box.height;
		int64_t const right = rightEnd > width ? (int64_t)width : rightEnd;
		int64_t const bottom = bottomEnd > height ? (int64_t)height : bottomEnd;

		if (right <= left || bottom <= top)
		{
			return false;
		}

		out->x = (int32_t)left;
		out->y = (int32_t)top;
		out->width = (int32_t)(right - left);
		out->height = (int32_t)(bottom - top);
		out->colour = box.colour;
		return true;
	}

	size_t PanelPaint_Plan(const ui::PanelLayout &layout, const PanelTexts &texts, PaintLetterWidth widthOf, int32_t lineHeight,
		PaintText *items, size_t mostItems)
	{
		if (!items)
		{
			return 0;
		}

		struct Line { const ui::Box *box; const std::string *text; };
		std::string const tooSmall = kPaintTooSmall;
		Line const lines[4] =
		{
			{ &layout.toolbar, &texts.toolbar },
			{ &layout.list, &texts.list },
			{ &layout.fields, &texts.fields },
			{ &layout.status, layout.roomy ? &texts.status : &tooSmall },
		};

		size_t count = 0;

		for (size_t i = 0; i < 4 && count < mostItems; i++)
		{
			if (PanelPaint_FitText(*lines[i].text, *lines[i].box, widthOf, lineHeight, &items[count]))
			{
				count++;
			}
		}

		return count;
	}
}
