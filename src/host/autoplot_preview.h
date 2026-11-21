// autoplot_preview.h: THE PLOT'S PREVIEW (the owner 11-21, for alpha.3: "Can we draw the module being linked at the bottom right/bottom 40% of the screen space with
// aspet ratio so people see the flashing modules being built :)", "Like we have with Waypoint"). While Alien auto waypoint plots, the Waypoint mode's own module
// view of the newest module planned, with its boxes, in an inset at the view's bottom right; a new module drawn once each time a job finishes; gone when the plot
// ends. Pure maths and bookkeeping here (no drawing): the inset's rectangle, and which module it shows when.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace host
{
	// a rectangle of the view in pixels: its left and top, its width and height
	struct PlotPreviewRect
	{
		int32_t x = 0, y = 0, w = 0, h = 0;
	};

	double const kPlotPreviewShare = 0.4;      // the inset takes this share of the view's width AND of its height, so it keeps the view's own aspect
	int32_t const kPlotPreviewLeast = 32;      // px: an inset narrower or lower than this is not drawn (a view too small to hold one)

	// the inset: at the view's bottom right, kPlotPreviewShare of its width and of its height (rounded to the pixel), the view's aspect kept; 0 x 0 at the view's
	// corner when either side would be under kPlotPreviewLeast
	PlotPreviewRect PlotPreview_Inset(PlotPreviewRect const &view);

	// which planned module the inset shows: the newest, once a module. shown: the job shown (none: (size_t)-1); redraws: how many times the inset was drawn anew
	struct PlotPreviewState
	{
		size_t shown = (size_t)-1;
		size_t redraws = 0;
	};

	// a frame of the plot: jobsPlanned jobs are planned (the worker's count, jobs 0 .. jobsPlanned - 1). True, the inset to be drawn anew with job jobsPlanned - 1
	// and a redraw counted, when that is newer than the one shown; false otherwise (the same module as last frame: nothing to redraw)
	bool PlotPreview_Next(PlotPreviewState *state, size_t jobsPlanned);

	double const kPlotPreviewMargin = 1.1;   // the module framed this much bigger than its box's round (a little ground round it)

	// how far from a module's box (mins, maxs: millimetres) the inset's camera stands, at the owner's 45-degree corner (camera_orbit.h), so the whole box is in view:
	// the round about the box (half its diagonal, times kPlotPreviewMargin) over the sine of the view's narrower half angle (the inset keeps the view's own angle:
	// its aspect is the view's). focal: the view's focal length in pixels for its width (LevelView_FocalLength); 0 when the box or the view makes no sense
	double PlotPreview_Distance(int32_t const mins[3], int32_t const maxs[3], double focal, int32_t viewW, int32_t viewH);

	// THE SWITCH ([Editor] PlotPreview in the ini, read at start: on unless 0; Frost's timing with and without)
	bool PlotPreview_On();
	void PlotPreview_SetOn(bool on);

	// the plot ended (or was stopped): nothing shown, the count kept for the words
	void PlotPreview_End(PlotPreviewState *state);
}
