// world_overlay.h: the 3D view's world lines from more than one owner at once (Hicks's plan section 3; P1, for the mouse gizmo). Written 2026-11-18 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The engine holds ONE list of world lines (level_view.h LevelView_WorldLines: each call replaces the whole list, count 0 takes it away). The screens that draw lines (the
// special objects' boxes, Track Editing, the entry points, Strategy's boxes, the waypoints) each send theirs when they change; the gizmo's change every frame the pointer moves.
// So the owners send through WorldOverlay_Lines (the same call, the same meaning), the gizmo through WorldOverlay_Extra, and the engine is always given the extra lines FIRST and
// then the owner's last list: LevelView_WorldLines draws at most 4096, so the gizmo is never the part cut off.

#pragma once

#include <stddef.h>

#include <vector>

#include "track_view.h"

namespace host
{
	// An owner's lines, replacing whatever owner sent last (count 0 or null: none), sent to the engine at once with the extra lines in front
	void WorldOverlay_Lines(TrackLine const *lines, size_t count);

	// The extra lines (the gizmo's) for now; sent again only when they are not the ones sent last
	void WorldOverlay_Extra(std::vector<TrackLine> const &lines);
}

// The engine's own (level_view.cpp): what WorldOverlay hands the combined list to
void LevelView_WorldLines(const host::TrackLine *lines, size_t count);
