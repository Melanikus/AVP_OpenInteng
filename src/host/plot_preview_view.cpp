// plot_preview_view.cpp: THE PLOT'S PREVIEW's view (plot_preview_view.h; Drake's piece on Dietrich's skeleton).
// Written 2026-11-21 by Melanikus (Dietrich, the skeleton; Drake, the view) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT GOES, a frame at a time while Alien auto waypoint plots:
// 1. The worker says how many AI modules it has planned. When that is more than the one shown, the newest is taken (PlotPreview_Next): its modules' box (the
//    union of each module's own box), its planned boxes and links as the lines Waypoint mode draws (WaypointView_AllLines), and every other module hidden.
// 2. Every frame it is up, the level is drawn into the inset at the view's bottom right (LevelView_MainBox), from the owner's 45-degree corner of that box
//    (CameraOrbit_Place, 45 round and 45 up), far enough back that the whole box fits (PlotPreview_Distance), with the module's lines (WorldOverlay_Lines).
// 3. When the plot ends, the hiding is let go and the editor's own camera is given back: the next frame is the normal view.
#include "plot_preview_view.h"

#include <chrono>
#include <vector>

#include "autoplot_command.h"
#include "camera_orbit.h"
#include "track_view.h"
#include "waypoint_view.h"
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

	// Job k becomes the one shown: its box, its lines and its hiding. False (and nothing changed) when the job can't be read or none of its modules has a box:
	// the one shown before stays up
	bool TakeJob(size_t k)
	{
		int32_t head = -1;
		std::vector<int32_t> const *gameModules = nullptr;
		std::vector<host::WaypointRec> const *list = nullptr;
		int32_t mins[3], maxs[3];

		if (!AutoPlotCommand_PreviewJob(k, &head, &gameModules, &list) || !gameModules || !list || gameModules->empty() || !ModulesBox(*gameModules, mins, maxs))
		{
			return false;
		}

		for (int a = 0; a < 3; a++)
		{
			boxMins[a] = mins[a];
			boxMaxs[a] = maxs[a];
		}

		// the planned boxes and links as Waypoint mode draws an AI module's network (its colours: white boxes, the links in their kinds' colours)
		lines.clear();

		host::WaypointAllCounts const counts = host::WaypointView_AllLines({ list }, local_scale > 0.0 ? local_scale : 1.0, &lines);

		// only this AI module's modules drawn, no placed object (Waypoint modes 1 to 3 show an AI module this way)
		LevelView_HideForPreview(gameModules->data(), gameModules->size());
		framed = true;
		shown.head = head;
		shown.job = k;
		shown.boxes = counts.boxes;
		shown.links = counts.links;
		return true;
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

	// Nothing shown any more: the hiding let go (only if it was set), the job forgotten. The counts stay for the state line until a new run starts.
	// The world lines are left as they are: the screen's own owner sends its lines again on the next frame (Waypoint mode 0's every network, modes 1 to 3's
	// network), and editor_main takes the Waypoint lines away when the screen is left
	void LetGo()
	{
		if (framed)
		{
			LevelView_HideForPreview(nullptr, 0);
			framed = false;
		}

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

	if (host::PlotPreview_Next(&state, AutoPlotCommand_PreviewPlanned()))
	{
		TakeJob(state.shown);   // (when it can't be read, the module shown before stays up)
	}

	FlyCamera corner = camera;

	// nothing to show yet (the grid building, the first module still searched), or a view too small for an inset: the normal view
	if (!framed || inset.w <= 0 || inset.h <= 0 || !CornerCamera(inset, &corner))
	{
		shown.on = false;
		return camera;
	}

	// the whole level from the editor's own camera, as always, and the module over it in the inset (the owner 11-21: "Full map + inset on top")
	LevelView_Inset(true, corner, inset.x, inset.y, inset.w, inset.h);
	host::WorldOverlay_Lines(lines.empty() ? nullptr : lines.data(), lines.size());

	double const ms = MsSince(from);

	shown.on = true;
	shown.inset = inset;
	shown.redraws = state.redraws;
	shown.lines = lines.size();
	shown.drawMs += ms;
	AutoPlotCommand_PreviewDrew(ms);
	return camera;
}

PlotPreviewShown PlotPreviewView_Shown()
{
	return shown;
}
