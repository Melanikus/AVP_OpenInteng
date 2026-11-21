// waypoint_gizmo.cpp: see waypoint_gizmo.h. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The includes are gizmo_screen.cpp's (editor_shared.h needs them), and the waypoint ones.

#include <windows.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "editor_shared.h"
#include "gizmo.h"
#include "level_file.h"
#include "level_objects.h"
#include "menu_bar.h"
#include "pick_game.h"
#include "state_line.h"
#include "text_scale.h"
#include "waypoint_box_gizmo.h"
#include "waypoint_gizmo.h"
#include "waypoint_mode.h"
#include "waypoint_net_level.h"
#include "world_overlay.h"

namespace
{
	// what the last frame drew: the hits and the drags use exactly that
	bool shown = false;
	host::PickCamera camera = {};
	host::GizmoFrame frame;
	host::WaypointRec box = {};
	int32_t shownIndex = -1, shownModule = -1;
	bool aim = false;             // the arrows move the aim point (G switches: a click near a box's centre is inteng's pick, so the centre ball takes no click)
	host::WaypointBoxHandle hover = host::WaypointBoxHandle::None;
	int32_t pointerX = -1, pointerY = -1;

	// a press on the centre ball, until its release (a click switches what the arrows move)
	bool ballHeld = false;
	int32_t ballX = 0, ballY = 0;
	int32_t const kBallClickPixels = 6;

	// the drag
	bool dragging = false;
	bool dragAim = false;
	host::WaypointBoxHandle handle = host::WaypointBoxHandle::None;
	host::PickCamera dragCamera = {};
	host::GizmoFrame dragFrame;
	host::WaypointRec start = {}, now = {};
	int32_t dragIndex = -1, dragModule = -1;
	double startAlong = 0.0;

	// one step of undo: the box before the last written edit, and after it (taken back only while the box is still as it was written)
	bool undoReady = false;
	int32_t undoIndex = -1, undoModule = -1;
	host::WaypointRec undoBefore = {}, undoAfter = {};

	std::string words;            // the status line's words (statusWords copies them)
	std::string last = "none";    // boxgizmolast: the last edit written

	double Scale()
	{
		return local_scale > 0.0 ? local_scale : 1.0;
	}

	void Say(std::string const &text)
	{
		words = text;
		statusWords = words.c_str();
	}

	bool SameShape(host::WaypointRec const &a, host::WaypointRec const &b)
	{
		return memcmp(a.min, b.min, sizeof(a.min)) == 0 && memcmp(a.max, b.max, sizeof(a.max)) == 0 && memcmp(a.centre, b.centre, sizeof(a.centre)) == 0 && a.flags == b.flags;
	}

	std::string Three(int32_t const v[3])
	{
		return std::to_string((long long)v[0]) + " " + std::to_string((long long)v[1]) + " " + std::to_string((long long)v[2]);
	}

	// Mode 1, the alien list, a current waypoint, and none of the modal states Drake's gizmo hides in (gizmo_screen.cpp Usable)
	bool OnIt()
	{
		int32_t const current = waypointMode.CurrentWaypoint();

		return OnWaypointScreen() && waypointMode.Mode() == 1 && waypointMode.AlienList() && current >= 0 && (size_t)current < waypointMode.List().size() &&
			!PlacementFlyHoldsTheCopy() && !ThreeViewsOn() && !ADialogIsOpen() && promptFor == kPromptNone && !AMenuIsOpen() && !popupBar.IsOpen() && zoomStage == 0 &&
			!rotationPickWaiting && !moveRelWaiting;
	}

	void GiveUp()
	{
		if (dragging)
		{
			waypointMode.MouseShowBox(dragIndex, start);   // the box as it was
		}

		dragging = false;
	}

	// The edit's words, from the box before and after it
	std::string WhatWasDone(host::WaypointBoxHandle h, bool onAim, host::WaypointRec const &before, host::WaypointRec const &after)
	{
		int const face = host::WaypointBoxGizmo_FaceOf(h), axis = host::WaypointBoxGizmo_ArrowAxis(h);
		char const xyz[3] = { 'x', 'y', 'z' };

		if (face >= 0)
		{
			int const a = face / 2;
			bool const high = face % 2 != 0;
			int32_t const was = high ? before.max[a] : before.min[a], is = high ? after.max[a] : after.min[a];

			return std::string("its ") + (high ? "highest " : "lowest ") + xyz[a] + " moved " + std::to_string((long long)is - was) + " to " + std::to_string((long long)is) +
				" (aim point " + Three(after.centre) + ")";
		}

		if (onAim)
		{
			return "its aim point moved to " + Three(after.centre) + ", inside its box";
		}

		return std::string("the box moved ") + std::to_string((long long)after.min[axis < 0 ? 0 : axis] - before.min[axis < 0 ? 0 : axis]) + " along " + xyz[axis < 0 ? 0 : axis] +
			" (lowest " + Three(after.min) + ", highest " + Three(after.max) + ")";
	}

	// Written: what was done and what the checker now finds in the AI module (a report, never a refusal)
	void Write(int32_t index, host::WaypointRec const &before, host::WaypointRec const &after, std::string const &what, bool isUndo)
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		host::WaypointOutcome const outcome = waypointMode.MouseSetBox(open, index, after, what);

		if (!outcome.levelChanged)
		{
			Say(outcome.say);
			return;
		}

		LoadTheObjectListAgain();

		if (isUndo)
		{
			undoReady = false;
		}
		else
		{
			undoReady = true;
			undoIndex = index;
			undoModule = waypointMode.CurrentModule();
			undoBefore = before;
			undoAfter = after;
		}

		Say(outcome.say + "; " + host::WaypointNets_ReportModule(open, waypointMode.CurrentModule()) + (isUndo ? "" : " (Ctrl+Z takes it back)"));
	}

	// The window's pixel (for the harness) of a world place; false when it is not ahead of the camera
	bool WindowPixel(double const point[3], int32_t *x, int32_t *y)
	{
		int32_t world[3];
		double px = 0.0, py = 0.0, ahead = 0.0;

		for (int i = 0; i < 3; i++)
		{
			world[i] = (int32_t)floor(point[i] + 0.5);
		}

		if (!host::PickRay_Project(camera, world, &px, &py, &ahead) || !(ahead > 0.0))
		{
			return false;
		}

		*x = host::TextScale_ToWindow((int32_t)floor(px));
		*y = host::TextScale_ToWindow((int32_t)floor(py));
		return true;
	}

	// A pixel the hit test gives this handle for, as "name x y" in the window's pixels; empty when none is found
	std::string GrabPoint(host::WaypointBoxHandle wanted)
	{
		int const face = host::WaypointBoxGizmo_FaceOf(wanted), axis = host::WaypointBoxGizmo_ArrowAxis(wanted);
		std::vector<std::vector<double> > tries;

		if (wanted == host::WaypointBoxHandle::Ball)
		{
			tries.push_back({ frame.centre[0], frame.centre[1], frame.centre[2] });
		}
		else if (face >= 0)
		{
			double point[3];

			host::WaypointBoxGizmo_FacePoint(box, Scale(), face, point);
			tries.push_back({ point[0], point[1], point[2] });
		}
		else if (axis >= 0)
		{
			for (int k = 0; k < 24; k++)
			{
				double const along = 0.5 + 0.4 * (double)(k % 2 == 0 ? k / 2 : -(k + 1) / 2) / 24.0;   // the arrow's middle, then either side of it

				tries.push_back({ frame.centre[0] + frame.axis[axis][0] * frame.length * along, frame.centre[1] + frame.axis[axis][1] * frame.length * along,
					frame.centre[2] + frame.axis[axis][2] * frame.length * along });
			}
		}

		for (std::vector<double> const &p : tries)
		{
			double const point[3] = { p[0], p[1], p[2] };
			int32_t world[3], wx = 0, wy = 0;
			double px = 0.0, py = 0.0, ahead = 0.0;

			for (int i = 0; i < 3; i++)
			{
				world[i] = (int32_t)floor(point[i] + 0.5);
			}

			// only a pixel inside the view: a handle off it can't be pressed (a face of a big box often lies off the view)
			if (!WindowPixel(point, &wx, &wy) || !host::PickRay_Project(camera, world, &px, &py, &ahead) || px < 0.0 || py < 0.0 || px >= (double)camera.viewWidth ||
				py >= (double)camera.viewHeight)
			{
				continue;
			}

			if (host::WaypointBoxGizmo_Hit(camera, frame, box, Scale(), floor(px), floor(py)) == wanted)
			{
				return std::string(host::WaypointBoxGizmo_HandleName(wanted)) + " " + std::to_string((long long)wx) + " " + std::to_string((long long)wy);
			}
		}

		return std::string();
	}
}

void WaypointGizmo_Frame(int32_t viewWidth, int32_t viewHeight)
{
	if (!OnIt())
	{
		GiveUp();
		ballHeld = false;
		shown = false;
		hover = host::WaypointBoxHandle::None;
		return;   // Drake's frame, before this one, has set the overlay's lines
	}

	int32_t const index = waypointMode.CurrentWaypoint(), module = waypointMode.CurrentModule();

	if (!dragging && (index != shownIndex || module != shownModule))
	{
		aim = false;   // another box: the arrows move the box
	}

	shownIndex = index;
	shownModule = module;
	box = dragging ? now : waypointMode.List()[(size_t)index];

	double centre[3];

	if (aim)
	{
		host::WaypointBoxGizmo_Aim(box, Scale(), centre);
	}
	else
	{
		host::WaypointBoxGizmo_Middle(box, Scale(), centre);
	}

	int32_t const place[3] = { (int32_t)floor(centre[0] + 0.5), (int32_t)floor(centre[1] + 0.5), (int32_t)floor(centre[2] + 0.5) };

	shown = PickGame_Camera(viewWidth, viewHeight, &camera) && host::Gizmo_Frame(camera, place, nullptr, false, &frame);

	if (!shown)
	{
		hover = host::WaypointBoxHandle::None;
		return;
	}

	hover = dragging ? handle : (ballHeld ? host::WaypointBoxHandle::Ball : (pointerX >= 0 ? host::WaypointBoxGizmo_Hit(camera, frame, box, Scale(), pointerX, pointerY) :
		host::WaypointBoxHandle::None));

	std::vector<host::TrackLine> lines;

	host::WaypointBoxGizmo_Lines(camera, frame, box, Scale(), hover, &lines);
	host::WorldOverlay_Extra(lines);
}

bool WaypointGizmo_Press(int32_t x, int32_t y)
{
	if (!shown || dragging || ballHeld)
	{
		return false;
	}

	host::WaypointBoxHandle const hit = host::WaypointBoxGizmo_Hit(camera, frame, box, Scale(), x, y);

	if (hit == host::WaypointBoxHandle::None)
	{
		return false;
	}

	// the centre ball sits on the box's middle, where inteng's pick takes a box (FUN_00584ee0: the box nearest the click): its click is left to the pick, and G switches the
	// arrows between the box and its aim point (Frost 11-19: acceptance-waypoint-mouse-delete's click on the current box was taken by the ball)
	if (hit == host::WaypointBoxHandle::Ball)
	{
		return false;
	}

	int const face = host::WaypointBoxGizmo_FaceOf(hit);
	int const axis = face >= 0 ? face / 2 : host::WaypointBoxGizmo_ArrowAxis(hit);
	host::GizmoFrame along = frame;

	// a face is dragged along its axis through its own middle
	if (face >= 0)
	{
		host::WaypointBoxGizmo_FacePoint(box, Scale(), face, along.centre);
	}

	if (axis < 0 || !host::Gizmo_AlongAxis(camera, along, axis, x, y, &startAlong))
	{
		return false;   // the mouse's line meets that axis at no usable place (nearly along it): the press goes on as a pick
	}

	handle = hit;
	dragCamera = camera;
	dragFrame = along;
	dragAim = aim && face < 0;
	start = now = box;
	dragIndex = shownIndex;
	dragModule = shownModule;
	dragging = true;
	Say("Waypoint " + std::to_string((long long)dragIndex) + ": dragging " + (dragAim ? "its aim point's " : "its ") + host::WaypointBoxGizmo_HandleName(handle) +
		" (let go to keep it, Esc puts it back)");
	return true;
}

bool WaypointGizmo_Pointer(int32_t x, int32_t y, bool shift)
{
	(void)shift;
	pointerX = x;
	pointerY = y;

	if (!dragging)
	{
		return ballHeld;
	}

	int const face = host::WaypointBoxGizmo_FaceOf(handle);
	int const axis = face >= 0 ? face / 2 : host::WaypointBoxGizmo_ArrowAxis(handle);
	double along = 0.0;

	if (!host::Gizmo_AlongAxis(dragCamera, dragFrame, axis, x, y, &along))
	{
		return true;   // nowhere on the axis: the box stays as it is
	}

	int32_t const by = host::WaypointBox_FileStep(along - startAlong, Scale());
	host::WaypointRec made;
	bool const usable = face >= 0 ? host::WaypointBox_FaceMoved(start, face, by, Scale(), &made) :
		(dragAim ? host::WaypointBox_AimMoved(start, axis, by, Scale(), &made) : host::WaypointBox_Moved(start, axis, by, Scale(), &made));

	if (usable)
	{
		now = made;
		waypointMode.MouseShowBox(dragIndex, now);   // the 3D view draws it as it goes (nothing written yet)
	}

	Say("Waypoint " + std::to_string((long long)dragIndex) + ": " + WhatWasDone(handle, dragAim, start, now) + " (let go to keep it, Esc puts it back)");
	return true;
}

bool WaypointGizmo_Release()
{
	if (ballHeld)
	{
		ballHeld = false;

		if (abs(pointerX - ballX) + abs(pointerY - ballY) <= kBallClickPixels || pointerX < 0)
		{
			aim = !aim;
			last = aim ? "aim" : "box";
			Say(aim ? "Waypoint " + std::to_string((long long)shownIndex) + ": the arrows move its aim point (G again: the box)" :
				"Waypoint " + std::to_string((long long)shownIndex) + ": the arrows move the box (G again: its aim point)");
		}

		return true;
	}

	if (!dragging)
	{
		return false;
	}

	dragging = false;
	waypointMode.MouseShowBox(dragIndex, start);   // the start back: MouseSetBox keeps the lists as they were to go back to, if the write is refused

	if (SameShape(start, now))
	{
		Say("Waypoint " + std::to_string((long long)dragIndex) + ": not changed");
		return true;
	}

	std::string const what = WhatWasDone(handle, dragAim, start, now);

	Write(dragIndex, start, now, what, false);
	last = std::string(dragAim ? "aim " : "") + host::WaypointBoxGizmo_HandleName(handle);
	return true;
}

bool WaypointGizmo_Key(unsigned key, bool shift, bool ctrl)
{
	(void)shift;

	if (key == VK_ESCAPE && dragging)
	{
		GiveUp();
		Say("Waypoint " + std::to_string((long long)dragIndex) + ": the drag was given up; the box is where it was");
		return true;
	}

	// G: the arrows switch between the box and its aim point (the Waypoint screen has no G of its own; Object Placement's G is Drake's gizmo's, the hub's the small-module step)
	if (key == 'G' && !ctrl && shown && !dragging && OnWaypointScreen() && waypointMode.Mode() == 1 && waypointMode.AlienList())
	{
		aim = !aim;
		last = aim ? "aim" : "box";
		Say(aim ? "Waypoint " + std::to_string((long long)shownIndex) + ": the arrows move its aim point (G again: the box)" :
			"Waypoint " + std::to_string((long long)shownIndex) + ": the arrows move the box (G again: its aim point)");
		return true;
	}

	if (key == 'Z' && ctrl && undoReady && OnWaypointScreen() && waypointMode.Mode() == 1 && waypointMode.AlienList() && waypointMode.CurrentModule() == undoModule && !dragging)
	{
		if (undoIndex < 0 || (size_t)undoIndex >= waypointMode.List().size() || !SameShape(waypointMode.List()[(size_t)undoIndex], undoAfter))
		{
			undoReady = false;
			Say("Waypoint " + std::to_string((long long)undoIndex) + ": the last edit was not taken back: the box has changed since");
			return true;
		}

		Write(undoIndex, undoAfter, undoBefore, "the last edit taken back", true);
		last = "undone";
		return true;
	}

	return false;
}

void WaypointGizmo_StateLine()
{
	StateLine_Add("boxgizmo", !shown ? "off" : (dragging ? std::string("dragging ") + (dragAim ? "aim " : "") + host::WaypointBoxGizmo_HandleName(handle) :
		std::string(aim ? "aim " : "box ") + host::WaypointBoxGizmo_HandleName(hover)));
	StateLine_Add("boxgizmolast", last);

	// the current waypoint's box as the editor holds it: its number, lowest, highest, aim point and flags (the file's units)
	int32_t const current = waypointMode.CurrentWaypoint();
	std::string held;

	if (OnWaypointScreen() && waypointMode.Mode() == 1 && current >= 0 && (size_t)current < waypointMode.List().size())
	{
		host::WaypointRec const &wp = waypointMode.List()[(size_t)current];
		char flags[16];

		_snprintf_s(flags, sizeof(flags), _TRUNCATE, "0x%08x", (unsigned)wp.flags);
		held = std::to_string((long long)current) + ": " + Three(wp.min) + " " + Three(wp.max) + " " + Three(wp.centre) + " " + flags;
	}

	StateLine_Add("waypointbox", held);

	std::string points;

	if (shown && !dragging)
	{
		host::WaypointBoxHandle const handles[10] = { host::WaypointBoxHandle::Ball, host::WaypointBoxHandle::MoveX, host::WaypointBoxHandle::MoveY, host::WaypointBoxHandle::MoveZ,
			host::WaypointBoxHandle::MinX, host::WaypointBoxHandle::MaxX, host::WaypointBoxHandle::MinY, host::WaypointBoxHandle::MaxY, host::WaypointBoxHandle::MinZ,
			host::WaypointBoxHandle::MaxZ };

		for (host::WaypointBoxHandle const h : handles)
		{
			std::string const one = GrabPoint(h);

			if (!one.empty())
			{
				points += (points.empty() ? "" : "|") + one;
			}
		}
	}

	StateLine_Add("boxgizmopoints", points);
}
