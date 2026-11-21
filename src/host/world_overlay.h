// world_overlay.h: the 3D view's world lines from more than one owner at once (Hicks's plan section 3; P1, for the mouse gizmo). Written 2026-11-18 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The engine holds ONE list of world lines (level_view.h LevelView_WorldLines: each call replaces the whole list, count 0 takes it away). The screens that draw lines (the
// special objects' boxes, Track Editing, the entry points, Strategy's boxes, the waypoints) each send theirs when they change; the gizmo's change every frame the pointer moves.
// So the owners send through WorldOverlay_Lines (the same call, the same meaning), the gizmo through WorldOverlay_Extra, and the engine is always given the extra lines FIRST and
// then the owner's last list: the engine draws a fixed most (level_view.h kLevelViewWorldLinesMax), so the gizmo is never the part cut off.
//
// THE WAYPOINT DISPLAY (Vasquez 2026-11-19, the owner's "see ... all alien paths and volumes across all modules"): every AI module's boxes and links at once is ~235k lines
// on v2d, and owners send their list every frame. A list the same as the one sent last is therefore NOT copied, joined or sent again (the engine keeps it); with the owner's
// own change number it is not even read.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "track_view.h"

namespace host
{
	// An owner's lines, replacing whatever owner sent last (count 0 or null: none), sent to the engine at once with the extra lines in front. The same lines as the
	// last call's (count and every byte) are not sent again
	void WorldOverlay_Lines(TrackLine const *lines, size_t count);

	// The same, with the owner's own change number: when it is not 0, is the number the last call was given, and the count is the same, the lines are taken as unchanged
	// without being read (an owner that remakes a big list only when its data changes). 0 is no number. A call without a number forgets the last one
	void WorldOverlay_Lines(TrackLine const *lines, size_t count, uint64_t generation);

	// The extra lines (the gizmo's) for now; sent again only when they are not the ones sent last
	void WorldOverlay_Extra(std::vector<TrackLine> const &lines);

	// EDIT ACROSS MODULES (Vasquez 2026-11-19; a named difference: inteng shows one AI module's network): the BACKGROUND lines, Waypoint modes 1 to 3's every OTHER network,
	// dimmed. Kept apart from the owner's list, so the current network's edits do not remake or resend them for nothing, with the same change-number rule as
	// WorldOverlay_Lines (0 = no number: compared byte for byte). The engine is given the gizmo's lines, then the background, then the owner's: it draws in that order with
	// no depth writes, so the bright current network is drawn over a dim line where they meet. When the whole is more than the engine takes (level_view.h
	// kLevelViewWorldLinesMax) the background's tail is the part cut. (nullptr, 0, 0) takes it away: its owner does that when it is not shown.
	void WorldOverlay_Background(TrackLine const *lines, size_t count, uint64_t generation);

	// A colour for the background: its red, green and blue each to 35% of themselves, the alpha kept (the engine draws these lines without blending), so inteng's colours
	// keep their meaning (a white box, a green link, an aliens-only lane's yellow), darker
	uint32_t WorldOverlay_Dim(uint32_t argb);

	// Every line's colour dimmed (WorldOverlay_Dim); the places untouched
	void WorldOverlay_DimLines(std::vector<TrackLine> *lines);
}

// The engine's own (level_view.cpp): what WorldOverlay hands the combined list to
void LevelView_WorldLines(const host::TrackLine *lines, size_t count);
