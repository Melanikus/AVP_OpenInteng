// autoplot_preview.cpp: THE PLOT'S PREVIEW's rectangle and bookkeeping (autoplot_preview.h).
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_preview.h"

#include <math.h>

namespace host
{
	static bool previewOn = true;

	bool PlotPreview_On()
	{
		return previewOn;
	}

	void PlotPreview_SetOn(bool on)
	{
		previewOn = on;
	}

	PlotPreviewRect PlotPreview_Inset(PlotPreviewRect const &view)
	{
		PlotPreviewRect r;
		int32_t const w = (int32_t)floor(view.w * kPlotPreviewShare + 0.5), h = (int32_t)floor(view.h * kPlotPreviewShare + 0.5);

		r.x = view.x + view.w;
		r.y = view.y + view.h;

		if (w < kPlotPreviewLeast || h < kPlotPreviewLeast)
		{
			return r;   // (0 x 0 at the corner: too small to show anything)
		}

		r.w = w;
		r.h = h;
		r.x -= w;
		r.y -= h;
		return r;
	}

	bool PlotPreview_Next(PlotPreviewState *state, size_t jobsPlanned)
	{
		if (!state || jobsPlanned == 0 || (state->shown != (size_t)-1 && jobsPlanned - 1 <= state->shown))
		{
			return false;   // nothing planned yet, or the newest is the one shown
		}

		state->shown = jobsPlanned - 1;
		state->redraws++;
		return true;
	}

	double PlotPreview_Distance(int32_t const mins[3], int32_t const maxs[3], double focal, int32_t viewW, int32_t viewH)
	{
		if (!mins || !maxs || !(focal > 0.0) || viewW <= 0 || viewH <= 0 || maxs[0] < mins[0] || maxs[1] < mins[1] || maxs[2] < mins[2])
		{
			return 0.0;
		}

		double const dx = (double)maxs[0] - mins[0], dy = (double)maxs[1] - mins[1], dz = (double)maxs[2] - mins[2];
		double const round = 0.5 * sqrt(dx * dx + dy * dy + dz * dz) * kPlotPreviewMargin;
		double const angle = atan(0.5 * (double)(viewW < viewH ? viewW : viewH) / focal);   // the narrower half angle

		return round / sin(angle);
	}

	void PlotPreview_End(PlotPreviewState *state)
	{
		if (state)
		{
			state->shown = (size_t)-1;
		}
	}
}
