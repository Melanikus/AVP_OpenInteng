// plot_preview_view.h: THE PLOT'S PREVIEW's view (part B, Drake's piece; the skeleton by Dietrich 11-21): each frame while Alien auto waypoint plots, the main view boxed
// in the view's bottom right (autoplot_preview.h PlotPreview_Inset), its camera at the owner's 45-degree corner of the newest planned module (PlotPreview_Distance), only
// that module shown (level_view.h LevelView_HideForPreview), its boxes drawn (waypoint_view.h WaypointView_AllLines); the normal view back when the plot ends.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "autoplot_preview.h"
#include "level_view.h"

// each frame, from editor_main just before the level is drawn (the view's rectangle in window pixels): the camera to draw the main view with, which is always
// camera (the owner 11-21: "Full map + inset on top"; the inset goes to level_view.h LevelView_Inset)
FlyCamera PlotPreviewView_Frame(FlyCamera const &camera, int viewLeft, int viewTop, int viewWidth, int viewHeight);

// what this frame showed, for the state line (plot_preview_state.h): on, the inset (window px), the module's head, the job, the redraws, the drawing time so far (ms);
// and the shown module's planned network as drawn (Drake): its boxes, its links, the world lines sent
struct PlotPreviewShown
{
	bool on = false;
	host::PlotPreviewRect inset;
	int32_t head = -1;
	size_t job = (size_t)-1;
	size_t redraws = 0;
	double drawMs = 0.0;
	size_t boxes = 0;
	size_t links = 0;
	size_t lines = 0;
	bool over = false;   // the last frame drew the inset OVER the whole main view (level_view.h LevelView_InsetDrawn; the owner's "Full map + inset on top")
};

PlotPreviewShown PlotPreviewView_Shown();
