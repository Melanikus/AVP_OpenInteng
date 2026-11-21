// plot_preview_state.h: THE PLOT'S PREVIEW's state line (part B, Vasquez's piece; the skeleton by Dietrich 11-21): each frame, the harness's "plotpreview" field from
// what the preview showed (plot_preview_view.h PlotPreviewView_Shown), for the hidden acceptance.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng; the field's words by Melanikus (Vasquez) 11-21. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// THE FIELD'S WORDS, each number after its own name, so a scenario's expect-number finds it ("left_", "redraws_"):
//   while the preview shows:  on left 614 top 461 width 410 height 307 head 235 job 2 redraws 3 ms 41 main 1
//     main: 1 when the last frame drew the inset OVER the whole main view (PlotPreviewShown.over: the owner's "Full map + inset on top"; Frost 11-21), else 0
//     left, top, width, height: the inset, in the window's pixels (the view's bottom right, 40% of each side: autoplot_preview.h PlotPreview_Inset)
//     head: the AI module the inset shows (its head module's number); job: the job it belongs to, counted from 0; redraws: how many times the inset was drawn
//     anew in this plot (once a module); ms: the time spent drawing it so far, rounded to the millisecond
//   before any plot:          off
//   after a plot:             off job 2 redraws 3 ms 120    (the last plot's count, kept so a scenario can read it once the plot has ended)
#pragma once

#include <string>

struct PlotPreviewShown;

// The field's words for what the preview showed (above). Pure: no state line, no drawing; the unit test calls it directly
std::string PlotPreviewState_Words(PlotPreviewShown const &shown);

// Each frame, after the preview's frame (editor_main): StateLine_Add("plotpreview", the words for PlotPreviewView_Shown())
void PlotPreviewState_Line();
