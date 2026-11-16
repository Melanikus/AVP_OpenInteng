// See panel_layout.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "panel_layout.h"

namespace ui
{
	// A size from outside, brought between 0 and kPanelMostSide
	static int64_t Side(int64_t size)
	{
		return size < 0 ? 0 : (size > kPanelMostSide ? kPanelMostSide : size);
	}

	bool BoxHolds(const Box &box, int32_t x, int32_t y)
	{
		int64_t const left = box.x;
		int64_t const top = box.y;

		return x >= left && (int64_t)x < left + box.width && y >= top && (int64_t)y < top + box.height;
	}

	static int64_t Least(int64_t a, int64_t b)
	{
		return a < b ? a : b;
	}

	// The sums in LayOutPanels never give a side below 0 or a place past kPanelMostSide, so each number fits in 32 bits as it is
	static Box MakeBox(int64_t x, int64_t y, int64_t width, int64_t height)
	{
		Box const box = { (int32_t)x, (int32_t)y, (int32_t)width, (int32_t)height };
		return box;
	}

	PanelLayout LayOutPanels(int32_t windowWidth, int32_t windowHeight, const PanelSizes &sizes)
	{
		int64_t const width = Side(windowWidth);
		int64_t const height = Side(windowHeight);

		int64_t const askedToolbar = Side(sizes.toolbarHeight);
		int64_t const askedStatus = Side(sizes.statusHeight);
		int64_t const askedList = Side(sizes.listWidth);
		int64_t const askedFields = Side(sizes.fieldsWidth);
		int64_t const leastList = Least(Side(sizes.leastListWidth), askedList);
		int64_t const leastFields = Least(Side(sizes.leastFieldsWidth), askedFields);

		// The heights: the toolbar first, then the status line, then the middle row gets what's left
		int64_t const toolbar = Least(askedToolbar, height);
		int64_t const status = Least(askedStatus, height - toolbar);
		int64_t const middle = height - toolbar - status;

		// The widths, giving way in order: the view, the list to its least, the field panel to its least, the list, the field panel
		int64_t list = askedList;
		int64_t fields = askedFields;

		if (list + fields > width)
		{
			int64_t over = list + fields - width;
			int64_t const listGives = Least(over, list - leastList);

			list -= listGives;
			over -= listGives;

			int64_t const fieldsGive = Least(over, fields - leastFields);

			fields -= fieldsGive;
			over -= fieldsGive;

			int64_t const listGoes = Least(over, list);

			list -= listGoes;
			over -= listGoes;

			fields -= Least(over, fields);
		}

		int64_t const view = width - list - fields;

		PanelLayout layout;

		layout.toolbar = MakeBox(0, 0, width, toolbar);
		layout.list = MakeBox(0, toolbar, list, middle);
		layout.view = MakeBox(list, toolbar, view, middle);
		layout.fields = MakeBox(list + view, toolbar, fields, middle);
		layout.status = MakeBox(0, toolbar + middle, width, status);
		layout.roomy = toolbar == askedToolbar && status == askedStatus && list == askedList && fields == askedFields && view > 0 && middle > 0;
		return layout;
	}
}
