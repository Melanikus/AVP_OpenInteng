// plot_preview_state.cpp: THE PLOT'S PREVIEW's state line (plot_preview_state.h; Vasquez's piece).
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng; filled 2026-11-21 by Melanikus (Vasquez). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include "plot_preview_state.h"

#include <math.h>
#include <stddef.h>

#include <string>

#include "plot_preview_view.h"
#include "state_line.h"

// a whole number as words (the state line is text)
static std::string Number(long long n)
{
	return std::to_string(n);
}

std::string PlotPreviewState_Words(PlotPreviewShown const &shown)
{
	// the job and the drawing time end both forms of the field ("none" while no job has been shown)
	std::string const job = shown.job == (size_t)-1 ? std::string("none") : Number((long long)shown.job);
	double const ms = shown.drawMs > 0.0 ? floor(shown.drawMs + 0.5) : 0.0;
	std::string const counts = "job " + job + " redraws " + Number((long long)shown.redraws) + " ms " + Number((long long)ms);

	if (shown.on)
	{
		// showing: where the inset is (the window's pixels), which module, and the counts
		return "on left " + Number(shown.inset.x) + " top " + Number(shown.inset.y) + " width " + Number(shown.inset.w) + " height " + Number(shown.inset.h) +
			" head " + Number(shown.head) + " " + counts + (shown.over ? " main 1" : " main 0");   // main: the inset drawn OVER the whole main view (the owner's "Full map + inset on top")
	}

	if (shown.redraws == 0)
	{
		return "off";   // no plot has shown anything yet
	}

	return "off " + counts;   // the plot ended: its count kept, so the end can be checked
}

void PlotPreviewState_Line()
{
	StateLine_Add("plotpreview", PlotPreviewState_Words(PlotPreviewView_Shown()));
}
