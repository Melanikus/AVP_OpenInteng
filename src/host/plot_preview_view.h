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

// each frame, from editor_main just after the level was drawn: IN STEP WITH THE PLOT (autoplot_handoff.h), the copy this frame showed has been drawn, so a worker
// waiting on it goes on now rather than at the next frame's start (the owner's lockstep then costs about a frame less a module)
void PlotPreviewView_Drawn();

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
	// THE MAIN VIEW'S LIVE MODULES (the owner 11-21: "I want to see the modules updating on the map as they are build"): how many AI modules of this plot are on the main
	// view (each one's planned boxes and links, as it was planned), and the main view's lines for them
	size_t landed = 0;
	size_t mapLines = 0;
};

PlotPreviewShown PlotPreviewView_Shown();

// THE INSET'S FRAME (the owner 11-21: "the slightest border so we can see it's a 'pop up'"): while the inset is up, a frame round it in inteng's own dialog frame,
// white and one pixel wide (two on a window over 1920 x 1200), inside the inset's box, over the level and under the menu bar and the dialogs. Gone with the inset.
// From editor_main's panels (AddThePanels), the window's size in pixels
void PlotPreviewView_DrawFrame(int windowWidth, int windowHeight);
