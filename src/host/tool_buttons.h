// tool_buttons.h: A8, the buttons (owner-intent item 8, Melanikus: "Both"): a TOOL ROW across the top of the view and ACTION BUTTONS at the foot
// of the left panels. Each button does exactly what its menu item does (it holds that item's action number, which the editor runs through the same
// door), lit while its state is on, and off when the item is off. Window-free, so a test can drive it. Written 2026-11-15 by Melanikus (Dietrich)
// for AVP_OpenInteng (A8). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHICH BUTTONS. "A button for a tool that isn't built yet isn't shown" (owner-intent 8), so only what exists:
// - the tool row: the draw modes (Polygon, Polyline, Filled polyline, Wireframe textured), Gouraud, Textures and Headlight. Exactly one draw mode is lit, and
//   Gouraud, Textures and Headlight while they are on; they follow every change, from a key, a menu or a button. Select, move, place object,
//   light and sound tools come when those tools are built;
// - the action buttons: Open and Save copy, and on a screen that has them, Add (a copy of the selected object) and Delete (the selected object),
//   which are inteng's own items of that screen, off while nothing is selected.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "panel_layout.h"
#include "panel_paint.h"
#include "view_toggles.h"

namespace host
{
	size_t const kToolButtonMost = 16;
	size_t const kToolLabelMost = 16;

	struct ToolButton
	{
		char label[kToolLabelMost];
		int32_t action;   // the menu item's number: what a press runs
		bool lit;         // its state is on (the draw mode in use, Gouraud on, the headlight on)
		bool off;         // the item is off: shown, and a press does nothing
	};

	// The tool row for the toggles as they are now. outlinesCanBeShown is whether Polyline and Filled polyline can be shown (the game's lines).
	// Gives back how many it wrote, never more than most.
	size_t ToolButtons_Tools(const ViewToggles &now, bool outlinesCanBeShown, ToolButton *out, size_t most);

	// The action buttons for screen (a place in kIntengScreens), with objectSelected saying whether an object is selected
	size_t ToolButtons_Panel(size_t screen, bool objectSelected, ToolButton *out, size_t most);

	// Lays the buttons left to right inside room, each as wide as its label and margins, with gap between and height tall, from the room's left and
	// top. A button that doesn't fit whole isn't placed, and none after it. Gives back how many were placed, boxes' first that many.
	size_t ToolButtons_LayOut(const ToolButton *buttons, size_t count, const ui::Box &room, int32_t gap, int32_t height, PaintLetterWidth widthOf,
		ui::Box *boxes);

	// Which placed button holds the point, or -1
	int ToolButtons_At(const ui::Box *boxes, size_t placed, int32_t x, int32_t y);
}
