// host_frame.h: where things are in the editor's window, from the fixed panels' layout (src/ui/panel_layout.h). Written 2026-11-14 by
// Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Which panel a click lands in, so the click goes to that panel and no other, worked out as plain sums so a test can try every place.
// A box holds a point as ui::BoxHolds says (panel_layout.h), and as the layout never puts two boxes on the same pixel, no point is in
// two panels.
// (The level's view has a child window of its own over its box, so it needs no viewport from here: the Direct3D viewport this file
// used to work out went with it, Apone's review W2, 2026-11-14.)

#pragma once

#include <stdint.h>

#include "panel_layout.h"

enum class HostPanel
{
	None,      // outside every panel: past the window's edge, or in the room a small window leaves
	Toolbar,
	List,
	View,
	Fields,
	Status
};

// The panel a point of the window is in
HostPanel HostFrame_PanelAt(const ui::PanelLayout &layout, int32_t x, int32_t y);

// THE PANELS' SIZES FOR A WINDOW. The level's view is the main part of the window, at least three quarters of its area at every size from
// 640 by 480 up (owner-intent.md, Melanikus, 2026-11-14: "The level fills the window"; his first build showed the view as a narrow
// strip between two empty grey columns). So the list is a narrow column, kListSharePercent of the window's width and at most
// kListMostWidth, and the field panel takes no room at all while it has nothing to show, which is so until the field panel is in the
// editor. The list's column stays even when the level has no objects: the menus drop down into it, as the panels can't draw over the
// view's child window. The toolbar and the status line keep the heights they were built with. A width below 0 counts as 0.
// (Added 2026-11-14 by Melanikus (Dietrich), F3.)
int32_t const kListSharePercent = 15;
int32_t const kListMostWidth = 200;
ui::PanelSizes HostFrame_PanelSizes(int32_t windowWidth);

// THE LIST'S COLUMN, SHARED WITH THE FIELD PANEL. The field panel has no column of its own (above), so while it shows a record it takes
// the foot of the list's column: the height its record asks for (wanted, from the host's HeightFor), but never more than half the column,
// so the list always keeps the upper half at least. Both parts are the column's width, and every row of pixels of the column is in one of
// them: the list above, the field panel below. A wanted height of 0 or less leaves the whole column to the list, with the field panel's
// box empty at its foot. A side below 0 counts as 0, and a column at the far end of the numbers is cut there, so no place wraps round.
// Every sum is done in 64 bits. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
void HostFrame_ShareTheColumn(const ui::Box &column, int32_t wanted, ui::Box *listPart, ui::Box *fieldsPart);

// THE CLICK'S PLACE ON THE PANELS. A click comes in the window's own pixels. The panels are drawn into a picture of their own (the
// panels' swap chain), which Windows stretches over the whole window when the two sizes differ: for a frame after the window is resized,
// or with any scaling between them. So a click is moved into the picture's pixels before anything tests it, by the same stretch: the
// middle of the window's pixel, times the picture's size over the window's, rounded down. When the sizes agree, as the keeper makes
// them, every place stays where it is. A place left of or above the window stays outside the picture (-1), and with no size on either
// side the place is given as it is. Every sum is done in 64 bits. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
void HostFrame_WindowToPanels(int32_t x, int32_t y, int32_t windowWidth, int32_t windowHeight, int32_t panelsWidth, int32_t panelsHeight,
	int32_t *panelsX, int32_t *panelsY);
