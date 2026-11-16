// See host_frame.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "host_frame.h"

HostPanel HostFrame_PanelAt(const ui::PanelLayout &layout, int32_t x, int32_t y)
{
	if (ui::BoxHolds(layout.toolbar, x, y))
	{
		return HostPanel::Toolbar;
	}

	if (ui::BoxHolds(layout.list, x, y))
	{
		return HostPanel::List;
	}

	if (ui::BoxHolds(layout.view, x, y))
	{
		return HostPanel::View;
	}

	if (ui::BoxHolds(layout.fields, x, y))
	{
		return HostPanel::Fields;
	}

	if (ui::BoxHolds(layout.status, x, y))
	{
		return HostPanel::Status;
	}

	return HostPanel::None;
}

ui::PanelSizes HostFrame_PanelSizes(int32_t windowWidth)
{
	int64_t const share = windowWidth > 0 ? (int64_t)windowWidth * kListSharePercent / 100 : 0;
	int32_t const list = share < kListMostWidth ? (int32_t)share : kListMostWidth;
	ui::PanelSizes const sizes = { ui::kPanelSizesAsBuilt.toolbarHeight, ui::kPanelSizesAsBuilt.statusHeight, list, list, 0, 0 };

	return sizes;
}

void HostFrame_ShareTheColumn(const ui::Box &column, int32_t wanted, ui::Box *listPart, ui::Box *fieldsPart)
{
	if (!listPart || !fieldsPart)
	{
		return;
	}

	int64_t const width = column.width > 0 ? column.width : 0;
	int64_t height = column.height > 0 ? column.height : 0;

	// The column ends at the far end of the numbers at most
	if ((int64_t)column.y + height > INT32_MAX)
	{
		height = INT32_MAX - (int64_t)column.y;
	}

	// The field panel takes what it wants, up to half the column; the list keeps the rest, which is the odd pixel too
	int64_t const half = height / 2;
	int64_t const fields = wanted > 0 ? (wanted < half ? wanted : half) : 0;
	int64_t const listHeight = height - fields;

	ui::Box const list = { column.x, column.y, (int32_t)width, (int32_t)listHeight };
	ui::Box const foot = { column.x, (int32_t)((int64_t)column.y + listHeight), (int32_t)width, (int32_t)fields };

	*listPart = list;
	*fieldsPart = foot;
}

// One side of HostFrame_WindowToPanels: a place across a window side of from pixels, into a picture side of to pixels
static int32_t Stretched(int32_t place, int32_t from, int32_t to)
{
	if (from <= 0 || to <= 0)
	{
		return place;
	}

	if (place < 0)
	{
		return -1;
	}

	// The middle of the pixel, (place + 1/2), times to / from, in whole numbers: (2 place + 1) to / (2 from). Every number here is at
	// most about 2^32 times 2^31, inside 64 bits, and the answer is at most about 2^31 times to / from, held to an int's range below.
	int64_t const stretched = ((2 * (int64_t)place + 1) * to) / (2 * (int64_t)from);

	return stretched > INT32_MAX ? INT32_MAX : (int32_t)stretched;
}

void HostFrame_WindowToPanels(int32_t x, int32_t y, int32_t windowWidth, int32_t windowHeight, int32_t panelsWidth, int32_t panelsHeight,
	int32_t *panelsX, int32_t *panelsY)
{
	*panelsX = Stretched(x, windowWidth, panelsWidth);
	*panelsY = Stretched(y, windowHeight, panelsHeight);
}
