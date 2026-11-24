// plot_preview_view.cpp: THE PLOT'S PREVIEW's view (plot_preview_view.h; Drake's piece on Dietrich's skeleton).
// Written 2026-11-21 by Melanikus (Dietrich, the skeleton; Drake, the view) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT GOES, a frame at a time while Alien auto waypoint plots (IN STEP WITH THE PLOT, autoplot_handoff.h; the owner 11-21: "Show every module in order"):
// 1. The worker hands over a copy of the AI module it is on: as its search begins, then its ways as they are found, then its last copy with its volumes, and it
//    waits until that last copy has been drawn. Each frame the window takes the newest copy (AutoPlotHandOff::Newest) and keeps its own pointer to it.
// 2. A copy of a new module: its modules' box (the union of each module's own box) and every other module hidden. Any new copy: its lines, the search's ways (in
//    the colours Waypoint mode gives an alien-only link and a plain link) and its planned boxes and links as Waypoint mode draws them (WaypointView_AllLines).
// 3. Every frame it is up, the level is drawn into the inset at the view's bottom right (LevelView_MainBox), from the owner's 45-degree corner of that box
//    (CameraOrbit_Place, 45 round and 45 up), far enough back that the whole box fits (PlotPreview_Distance), with the copy's lines (WorldOverlay_Lines).
// 4. On the next frame, once the inset was drawn (LevelView_InsetDrawn), the window says so (Drawn): a worker waiting on that copy goes on. While the inset is up
//    the window is WATCHING; when it isn't (the plot ended, the preview off, another screen), it is not, and the worker never waits for it.
// 5. When the plot ends, the hiding is let go and the editor's own camera is given back: the next frame is the normal view.
#include "plot_preview_view.h"

#include <chrono>
#include <memory>
#include <vector>

#include "autoplot_command.h"
#include "autoplot_handoff.h"
#include "camera_orbit.h"
#include "track_view.h"
#include "waypoint_view.h"
#include "panel_pass.h"     // PanelPass_AddToFrame: the inset's frame over the level, under the bar and the dialogs
#include "world_overlay.h"

// Two of editor_main.cpp's own (editor_shared.h has them too, but it needs the ui folder on the include path, which this file's project entry does not give)
extern double local_scale;   // the open level's scale: the file's numbers times it are the world's
bool OnWaypointScreen();     // the Waypoint screen is up

namespace
{
	host::PlotPreviewState state;          // which planned job the inset shows (autoplot_preview.h)
	PlotPreviewShown shown;                // what the last frame showed (PlotPreviewView_Shown)
	std::vector<host::TrackLine> lines;    // the shown module's planned boxes and links, in the world's units
	int32_t boxMins[3] = { 0, 0, 0 };      // the shown module's box in the world (the union of its modules' boxes)
	int32_t boxMaxs[3] = { 0, 0, 0 };
	bool framed = false;                   // a module is up: its box, its lines and its hiding are set
	bool wasBusy = false;                  // a run was going last frame (a new run starts the counts again)
	// THE COPY SHOWN (autoplot_handoff.h WHO OWNS WHAT): the window's own pointer to the worker's constant copy, kept while it is drawn from; and the number of the
	// copy set into the last frame's inset (0: none), said Drawn once that frame was drawn
	std::shared_ptr<host::AutoPlotShown const> showing;
	uint64_t setNumber = 0;
	// THE MAIN VIEW'S LIVE MODULES (the owner 11-21: "I want to see the modules updating on the map as they are build"): each module's planned boxes and links, added
	// to the main view's lines as its last copy comes (the plot's own copy: nothing is written into the level until the plot ends, as the worker still reads it); the
	// number of the last copy added, so none is added twice
	std::vector<host::TrackLine> mapLines;
	uint64_t landedUpTo = 0;
	// THE RUN the window shows (AutoPlotHandOff::Run): when the hand-over starts another, everything of the last run goes (Drake 2026-11-23, the owner's "are we
	// clearing the previous level when loading the new one?"). Not by "no plot" frames alone: All stock maps plots level after level with no such frame between,
	// and the copy numbers count from 1 again, so the window kept derelict's last copy into genshd1's plot and never landed genshd1's modules
	uint64_t viewRun = 0;

	// The box round all of these modules (the game's module numbers): the smallest box that holds each one's own box. False when none of them has a box
	bool ModulesBox(std::vector<int32_t> const &modules, int32_t mins[3], int32_t maxs[3])
	{
		bool any = false;

		for (int32_t const module : modules)
		{
			int32_t lo[3], hi[3];

			if (!LevelView_ModuleBox(module, lo, hi))
			{
				continue;   // (a module with no shape adds nothing)
			}

			for (int a = 0; a < 3; a++)
			{
				mins[a] = any && mins[a] < lo[a] ? mins[a] : lo[a];
				maxs[a] = any && maxs[a] > hi[a] ? maxs[a] : hi[a];
			}

			any = true;
		}

		return any;
	}

	// A copy becomes the one shown (the window holds it from here). A new module: its box and its hiding, and a redraw counted (PlotPreview_Next: once a module).
	// Every copy: its lines. A new module none of whose modules has a box keeps the module shown before up, with its lines (the words still name the new one)
	void TakeCopy(std::shared_ptr<host::AutoPlotShown const> const &copy)
	{
		bool const newModule = !showing || showing->job != copy->job || !framed;

		showing = copy;

		if (newModule)
		{
			int32_t mins[3], maxs[3];

			if (copy->gameModules.empty() || !ModulesBox(copy->gameModules, mins, maxs))
			{
				return;
			}

			for (int a = 0; a < 3; a++)
			{
				boxMins[a] = mins[a];
				boxMaxs[a] = maxs[a];
			}

			// only this AI module's modules drawn, no placed object (Waypoint modes 1 to 3 show an AI module this way)
			LevelView_HideForPreview(copy->gameModules.data(), copy->gameModules.size());
			framed = true;
			host::PlotPreview_Next(&state, copy->job + 1);
		}

		lines.clear();

		// the search's ways found so far, point to point, in the colours Waypoint mode's every-network view gives an alien-only link and a plain link
		for (host::AutoPlotShownWay const &way : copy->ways)
		{
			for (size_t p = 3; p + 2 < way.points.size(); p += 3)
			{
				host::TrackLine line;

				for (int a = 0; a < 3; a++)
				{
					line.from[a] = way.points[p - 3 + (size_t)a];
					line.to[a] = way.points[p + (size_t)a];
				}

				line.colour = way.alien ? host::kWaypointLinkHighlighted : host::kWaypointLinkPlain;
				lines.push_back(line);
			}
		}

		// the planned boxes and links as Waypoint mode draws an AI module's network (its colours: white boxes, the links in their kinds' colours)
		host::WaypointAllCounts const counts = host::WaypointView_AllLines({ &copy->volumes }, local_scale > 0.0 ? local_scale : 1.0, &lines);

		shown.head = copy->head;
		shown.job = copy->job;
		shown.boxes = counts.boxes;
		shown.links = counts.links;

		// a module's last copy with boxes in it: onto the main view, once
		if (copy->finished && copy->number > landedUpTo && !copy->volumes.empty())
		{
			// (in Waypoint mode 0's colours, as its every-network view draws them after the plot: a one-way link's closed half in its link's own colour, never red, which is
			// inteng's PICK there; the owner's picture of 11-21 showed the plot's links red during the plot and yellow after it)
			host::WaypointView_AllLines({ &copy->volumes }, local_scale > 0.0 ? local_scale : 1.0, &mapLines, false);
			landedUpTo = copy->number;
			shown.landed++;
			shown.mapLines = mapLines.size();
		}
	}

	// The camera at the owner's corner of the shown box (45 degrees round, 45 up), far enough back that the whole box fits in the inset. False when the box gives
	// no place (CameraOrbit_Place refuses it)
	bool CornerCamera(host::PlotPreviewRect const &inset, FlyCamera *out)
	{
		double const distance = host::PlotPreview_Distance(boxMins, boxMaxs, LevelView_FocalLength(inset.w), inset.w, inset.h);
		host::OrbitCamera orbit = {};
		double used = 0.0;

		// (a distance of 0, a box PlotPreview_Distance can't measure, is the orbit's own default: 1 to 4 metres off)
		if (!host::CameraOrbit_Place(boxMins, boxMaxs, host::kOrbitDefaultAround, host::kOrbitDefaultUp, distance > 0.0 ? distance : 0.0, &orbit, &used))
		{
			return false;
		}

		out->x = orbit.place[0];
		out->y = orbit.place[1];
		out->z = orbit.place[2];
		out->turn = orbit.turn;
		out->look = orbit.look;
		out->roll = 0;
		FlyCamera_Hold(out);
		return true;
	}

	// Nothing shown any more: the hiding let go (only if it was set), the job forgotten, the inset's lines taken away. The counts stay for the state line until a
	// new run starts. The main view's lines are left as they are: the screen's own owner sends its lines again on the next frame (Waypoint mode 0's every network,
	// read from the level, which now holds the plot's networks; modes 1 to 3's network), and editor_main takes the Waypoint lines away when the screen is left
	void LetGo()
	{
		if (framed)
		{
			LevelView_HideForPreview(nullptr, 0);
			framed = false;
		}

		AutoPlotCommand_HandOff().Watching(false);   // (nobody watches: a worker waiting on its copy goes on)
		host::WorldOverlay_InsetLines(nullptr, 0);
		showing.reset();
		setNumber = 0;
		mapLines.clear();
		landedUpTo = 0;
		host::PlotPreview_End(&state);
		shown.on = false;
		shown.inset = host::PlotPreviewRect();
	}

	double MsSince(std::chrono::steady_clock::time_point from)
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
	}
}

FlyCamera PlotPreviewView_Frame(FlyCamera const &camera, int viewLeft, int viewTop, int viewWidth, int viewHeight)
{
	std::chrono::steady_clock::time_point const from = std::chrono::steady_clock::now();
	bool const busy = AutoPlotCommand_Busy();

	shown.over = LevelView_InsetDrawn();   // (the last frame's: the state line is read before this frame is drawn)

	host::AutoPlotHandOff &handOff = AutoPlotCommand_HandOff();

	// IN STEP: the frame the copy was set into has been drawn, so a worker waiting on it goes on (autoplot_handoff.h). Said even when the level view drew no inset
	// that frame (shown.over false): a picture that can't be drawn never holds the plot up
	if (setNumber != 0)
	{
		handOff.Drawn(setNumber);
	}

	setNumber = 0;

	// THE INSET'S DRAWING TIME (Frost 11-21: "preview drawing 0 ms" in every run, the bookkeeping below being all that was timed): the last frame's inset, made in
	// level_view (LevelView_InsetMs, the processor's part), counted once, on the frame after it. The frame a plot ends on is the one not counted
	if (shown.over)
	{
		double const insetMs = LevelView_InsetMs();

		shown.drawMs += insetMs;
		AutoPlotCommand_PreviewDrew(insetMs);
	}
	LevelView_Inset(false, camera, 0, 0, 0, 0);   // no inset unless this frame shows one (below)

	if (busy && !wasBusy)
	{
		// a new run: its own counts (the last run's stayed for the state line till now)
		state = host::PlotPreviewState();
		shown = PlotPreviewShown();
	}

	wasBusy = busy;

	// A NEW RUN BY THE HAND-OVER'S OWN COUNT (viewRun above): the last run's copy, its framing and its modules on the map let go, and its counts, whether or not a
	// frame with no plot came between
	uint64_t const run = handOff.Run();

	if (run != viewRun)
	{
		viewRun = run;
		LetGo();
		state = host::PlotPreviewState();
		shown = PlotPreviewShown();
	}

	// DECIDED (Drake 11-21): shown only on the Waypoint screen, where the plot goes on (AutoPlotCommand_Frame runs there only: on another screen the run waits,
	// and that screen's own view is drawn, with its own box)
	if (!busy || !host::PlotPreview_On() || !OnWaypointScreen())
	{
		LetGo();
		return camera;
	}

	host::PlotPreviewRect view;

	view.x = viewLeft;
	view.y = viewTop;
	view.w = viewWidth;
	view.h = viewHeight;

	host::PlotPreviewRect const inset = host::PlotPreview_Inset(view);

	// the newest copy the worker handed over (the window's own from here)
	std::shared_ptr<host::AutoPlotShown const> const newest = handOff.Newest();

	if (newest && (!showing || newest->number != showing->number))
	{
		TakeCopy(newest);
	}

	FlyCamera corner = camera;

	// nothing to show yet (the grid building, no copy yet, a first module with no box), or a view too small for an inset: the normal view, and nobody watching
	if (!framed || inset.w <= 0 || inset.h <= 0 || !CornerCamera(inset, &corner))
	{
		handOff.Watching(false);
		shown.on = false;
		return camera;
	}

	// the whole level from the editor's own camera, as always, with the plot's modules so far on it (the main view's lines: while the preview is up, Waypoint mode 0
	// sends none of its own, editor_main), and the module being planned over it in the inset, with its own lines (the owner 11-21: "Full map + inset on top")
	LevelView_Inset(true, corner, inset.x, inset.y, inset.w, inset.h);
	host::WorldOverlay_Lines(mapLines.empty() ? nullptr : mapLines.data(), mapLines.size());
	host::WorldOverlay_InsetLines(lines.empty() ? nullptr : lines.data(), lines.size());

	// WATCHING: the inset is drawn this frame with the copy shown (its words on the status line), said Drawn on the next frame
	handOff.Watching(true);
	setNumber = showing ? showing->number : 0;

	double const ms = MsSince(from);

	shown.on = true;
	shown.inset = inset;
	shown.redraws = state.redraws;
	shown.lines = lines.size();
	shown.drawMs += ms;
	AutoPlotCommand_PreviewDrew(ms);
	return camera;
}

void PlotPreviewView_Drawn()
{
	// (the next frame's start says it too, when this was not called: Drawn is said once for a copy, either way)
	if (setNumber != 0)
	{
		AutoPlotCommand_HandOff().Drawn(setNumber);
		setNumber = 0;
	}
}

PlotPreviewShown PlotPreviewView_Shown()
{
	return shown;
}

void PlotPreviewView_DrawFrame(int windowWidth, int windowHeight)
{
	host::PlotPreviewRect const &r = shown.inset;

	if (!shown.on || r.w <= 4 || r.h <= 4)
	{
		return;
	}

	// inteng's dialog frame (reassess/dialog-look-drake.md DL-2: a 1-pixel WHITE frame along the window's four edges, kDialogWhite), inside the inset's box; two
	// pixels on a big window, so it still reads as a frame
	uint32_t const kInsetFrameColour = 0xFFFFFFFFu;   // inteng's dialog frame white (dialogs.cpp's kDialogWhite)
	int32_t const t = windowWidth > 1920 || windowHeight > 1200 ? 2 : 1;
	host::PaintBox const edges[4] = {
		{ r.x, r.y, r.w, t, kInsetFrameColour },                   // top
		{ r.x, r.y + r.h - t, r.w, t, kInsetFrameColour },         // bottom
		{ r.x, r.y + t, t, r.h - 2 * t, kInsetFrameColour },       // left
		{ r.x + r.w - t, r.y + t, t, r.h - 2 * t, kInsetFrameColour },   // right
	};
	PanelExtras const extras = { edges, 4, nullptr, 0 };

	PanelPass_AddToFrame(windowWidth, windowHeight, extras);
}
