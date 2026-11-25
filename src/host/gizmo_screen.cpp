// gizmo_screen.cpp: see gizmo_screen.h. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <windows.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "editor_shared.h"
#include "gizmo.h"
#include "gizmo_screen.h"
#include "level_file.h"
#include "level_objects.h"
#include "level_view.h"
#include "menu_bar.h"
#include "movable.h"
#include "movable_live.h"   // Frost's P2 kinds drawn where the drag has them (agreed with Drake 11-19)
#include "pick_game.h"
#include "selection.h"
#include "state_line.h"
#include "text_scale.h"
#include "world_overlay.h"

namespace
{
	bool onGizmoScreen = false;   // the last frame was a screen the gizmo shows on (Object Placement, or one with a source)
	bool local = false;           // G: the object's own axes, else the world's

	// the screens' own sources (GizmoScreen_UseRefSource), by the screen's name: a function's own static, so a registration made while the program starts finds it made
	struct SourceRow
	{
		std::string screen;
		GizmoRefSource source;
		bool first;   // the source before the selection
	};

	std::vector<SourceRow> &Sources()
	{
		static std::vector<SourceRow> rows;

		return rows;
	}

	GizmoRefSource SourceOf(char const *screen)
	{
		if (!screen)
		{
			return nullptr;
		}

		for (SourceRow const &row : Sources())
		{
			if (row.screen == screen)
			{
				return row.source;
			}
		}

		return nullptr;
	}

	bool SourceFirst(char const *screen)
	{
		for (SourceRow const &row : Sources())
		{
			if (screen && row.screen == screen)
			{
				return row.first;
			}
		}

		return false;
	}

	bool GizmoScreen(char const *screen)
	{
		return screen != nullptr && (strcmp(screen, "Object Placement") == 0 || SourceOf(screen) != nullptr);
	}

	// what the last frame drew: the drags and the hits use exactly that
	bool shown = false;
	host::PickCamera camera = {};
	host::GizmoFrame frame;
	host::MovableRef ref;
	host::MovablePose pose;
	int32_t matrix[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };
	bool canRotate = false;
	host::GizmoHandle hover = host::GizmoHandle::None;
	int32_t pointerX = -1, pointerY = -1;
	bool pointerMoved = false;           // the pointer moved since the last frame (GizmoScreen_Pointer)
	host::PickCamera hoverCamera = {};   // the camera the lit handle was found with

	// the pose read last, kept while the level and the selection are the same (the read walks the level's objects)
	OpenLevelFile *readOpen = nullptr;
	unsigned readGeneration = 0;
	host::MovableRef readRef;         // the selection's ref the read was made for
	host::MovableRef readScreenRef;   // and the screen's
	host::MovableRef readUsed;        // the one that read
	bool readDone = false;
	host::MovablePose readPose;

	// the drag
	bool dragging = false;
	bool flying = false;          // the live copy shows the object as it is dragged (LevelView_FlyBegin)
	int32_t viewX = 0, viewY = 0;   // the 3D view's top left in the editor's own pixels: 0 0, or the split screen's bottom right quarter (GizmoScreen_ViewAt)
	host::GizmoHandle handle = host::GizmoHandle::None;
	host::PickCamera dragCamera = {};
	host::GizmoFrame dragFrame;
	host::MovableRef dragRef;
	host::MovablePose start, now;   // in the FILE's units, as movable.h has them
	int32_t startWorld[3] = { 0, 0, 0 }, nowWorld[3] = { 0, 0, 0 };   // the same places in the WORLD's (the file's times local_scale), as the camera and the live copy have them
	int32_t startMatrix[9] = {};
	double startAlong = 0.0;
	double startOnRing[3] = { 0.0, 0.0, 0.0 };
	double amount = 0.0;          // mm moved, or degrees turned, so far
	bool snapped = false;

	// one step of undo: the pose before the last drag that was written
	bool undoReady = false;
	host::MovableRef undoRef;
	host::MovablePose undoPose;

	std::string words;            // the status line's words (statusWords copies them)
	std::string last = "none";    // gizmolast: the last drag written

	// The level's scale (local_scale: the global scale times its ENVSDSCL): the world's units are the file's times it, as the fly's live copy and the camera have them
	double Scale()
	{
		return local_scale > 0.0 ? local_scale : 1.0;
	}

	void ToWorld(int32_t const file[3], int32_t world[3])
	{
		for (int i = 0; i < 3; i++)
		{
			double const v = (double)file[i] * Scale();

			world[i] = (int32_t)floor((v > (double)host::kMovableMostPlace * 4.0 ? (double)host::kMovableMostPlace * 4.0 : (v < -(double)host::kMovableMostPlace * 4.0 ? -(double)host::kMovableMostPlace * 4.0 : v)) + 0.5);
		}
	}

	void ToFile(int32_t const world[3], int32_t file[3])
	{
		for (int i = 0; i < 3; i++)
		{
			file[i] = (int32_t)floor((double)world[i] / Scale() + 0.5);
		}
	}

	bool SameRef(host::MovableRef const &a, host::MovableRef const &b)
	{
		return a.kind == b.kind && memcmp(a.id, b.id, sizeof(a.id)) == 0 && a.number == b.number && a.module == b.module;
	}

	// the same camera: its place, its turn, its projection and its view
	bool SameCamera(host::PickCamera const &a, host::PickCamera const &b)
	{
		return memcmp(a.place, b.place, sizeof(a.place)) == 0 && memcmp(a.turn, b.turn, sizeof(a.turn)) == 0 && a.xScale == b.xScale && a.yScale == b.yScale &&
		       a.viewWidth == b.viewWidth && a.viewHeight == b.viewHeight;
	}

	void Say(std::string const &text)
	{
		words = text;
		statusWords = words.c_str();
	}

	std::string Amount()
	{
		char text[96];

		if (host::Gizmo_IsTurn(handle))
		{
			_snprintf_s(text, sizeof(text), _TRUNCATE, "turned %.0f degrees about %c", amount, "xyz"[host::Gizmo_AxisOf(handle)]);
		}
		else
		{
			_snprintf_s(text, sizeof(text), _TRUNCATE, "moved %.0f mm along %c", amount, "xyz"[host::Gizmo_AxisOf(handle)]);
		}

		return text;
	}

	// A placed object's real display block put at the pose the level now holds (the file's units in, the world's on the block): LevelView_PutObjectAt
	void PutTheRealObject(host::MovableRef const &which, host::MovablePose const &written)
	{
		if (which.kind == host::MovableKind::PlacedObject)
		{
			int32_t world[3];

			ToWorld(written.place, world);
			LevelView_PutObjectAt(which.id, world, written.turn);
		}
	}

	void GiveUpTheDrag()
	{
		if (flying)
		{
			LevelView_FlyEnd();   // the real object shows again, as the level's own bytes have it
		}

		flying = false;
		dragging = false;
		host::MovableLive_Clear();   // Frost's P2 kinds: drawn as the level holds them again (movable_live.h)
	}

	// Whether the gizmo may show at all on this frame. Not while a pick waits for its press (Scroll Lock's centre, Move relative's, Strategy's collectors: "Select targets for switch",
	// Copy Strategy's objects, a lift's positions, a mission's objectives): inteng has no gizmo, so that press is the pick's, never a drag's or the centre ball's (acceptance-op-chain-door:
	// the switch's target click landed on the gizmo, a placement started and the door was never picked)
	bool Usable(char const *screen)
	{
		return GizmoScreen(screen) && !PlacementFlyHoldsTheCopy() && !ThreeViewsOn() && !ADialogIsOpen() && promptFor == kPromptNone &&
			!AMenuIsOpen() && !popupBar.IsOpen() && zoomStage == 0 && !rotationPickWaiting && !moveRelWaiting && !copyCollecting;
	}

	// The window's pixel (for the harness) of a world place, the editor's own pixel scaled as the window has it (text_scale.h); false when it is not ahead of the camera
	bool WindowPixel(double const point[3], int32_t *x, int32_t *y)
	{
		int32_t world[3];
		double px = 0.0, py = 0.0, ahead = 0.0;

		for (int i = 0; i < 3; i++)
		{
			world[i] = (int32_t)floor(point[i] + 0.5);
		}

		if (!PickRay_Project(camera, world, &px, &py, &ahead) || !(ahead > 0.0))
		{
			return false;
		}

		*x = host::TextScale_ToWindow((int32_t)floor(px) + viewX);
		*y = host::TextScale_ToWindow((int32_t)floor(py) + viewY);
		return true;
	}

	// A pixel the hit test gives this handle for (an arrow's middle, a point of its ring), as "name x y" in the window's pixels; empty when none is found
	std::string GrabPoint(host::GizmoHandle wanted)
	{
		int const axis = host::Gizmo_AxisOf(wanted);

		if (wanted == host::GizmoHandle::Centre)
		{
			int32_t wx = 0, wy = 0;

			return WindowPixel(frame.centre, &wx, &wy) ? std::string(host::Gizmo_HandleName(wanted)) + " " + std::to_string(wx) + " " + std::to_string(wy) : std::string();
		}

		for (int k = 0; k < host::kGizmoRingSegments; k++)
		{
			double point[3];

			if (host::Gizmo_IsTurn(wanted))
			{
				double u[3], v[3];
				double const *const a = frame.axis[axis];
				double const helper[3] = { fabs(a[0]) < 0.9 ? 1.0 : 0.0, fabs(a[0]) < 0.9 ? 0.0 : 1.0, 0.0 };

				u[0] = a[1] * helper[2] - a[2] * helper[1];
				u[1] = a[2] * helper[0] - a[0] * helper[2];
				u[2] = a[0] * helper[1] - a[1] * helper[0];

				double const lu = sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);

				u[0] /= lu;
				u[1] /= lu;
				u[2] /= lu;
				v[0] = a[1] * u[2] - a[2] * u[1];
				v[1] = a[2] * u[0] - a[0] * u[2];
				v[2] = a[0] * u[1] - a[1] * u[0];

				double const t = 2.0 * 3.14159265358979323846 * ((double)k + 0.5) / (double)host::kGizmoRingSegments;

				for (int i = 0; i < 3; i++)
				{
					point[i] = frame.centre[i] + frame.radius * (u[i] * cos(t) + v[i] * sin(t));
				}
			}
			else
			{
				double const along = 0.5 + 0.4 * (double)(k % 2 == 0 ? k / 2 : -(k + 1) / 2) / (double)host::kGizmoRingSegments;   // the middle, then either side of it

				for (int i = 0; i < 3; i++)
				{
					point[i] = frame.centre[i] + frame.axis[axis][i] * frame.length * along;
				}
			}

			int32_t wx = 0, wy = 0, ox = 0, oy = 0, world[3];
			double px = 0.0, py = 0.0, ahead = 0.0;

			for (int i = 0; i < 3; i++)
			{
				world[i] = (int32_t)floor(point[i] + 0.5);
			}

			if (!WindowPixel(point, &wx, &wy) || !PickRay_Project(camera, world, &px, &py, &ahead))
			{
				continue;
			}

			ox = (int32_t)floor(px);
			oy = (int32_t)floor(py);

			if (host::Gizmo_Hit(camera, frame, canRotate, ox, oy) == wanted)
			{
				return std::string(host::Gizmo_HandleName(wanted)) + " " + std::to_string(wx) + " " + std::to_string(wy);
			}
		}

		return std::string();
	}
}

void GizmoScreen_UseRefSource(char const *screen, GizmoRefSource source, bool sourceFirst)
{
	if (!screen || !source)
	{
		return;
	}

	for (SourceRow &row : Sources())
	{
		if (row.screen == screen)
		{
			row.source = source;
			row.first = sourceFirst;
			return;
		}
	}

	Sources().push_back(SourceRow{ screen, source, sourceFirst });
}

void GizmoScreen_Frame(int32_t viewWidth, int32_t viewHeight, char const *screen)
{
	static std::vector<host::TrackLine> const none;

	onGizmoScreen = GizmoScreen(screen);

	if (!Usable(screen))
	{
		if (dragging)
		{
			GiveUpTheDrag();
		}

		shown = false;
		hover = host::GizmoHandle::None;
		host::WorldOverlay_Extra(none);
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	GizmoRefSource const source = SourceOf(screen);
	host::MovableRef const fromSelection = dragging ? dragRef : host::Movable_FromSelection(host::Selection_Get());
	host::MovableRef const fromScreen = !dragging && source ? source() : host::MovableRef();
	host::MovableRef selected = fromSelection;

	shown = false;

	if (open && dragging)
	{
		pose = now;
		shown = true;
	}
	else if (open && (fromSelection.kind != host::MovableKind::None || fromScreen.kind != host::MovableKind::None))
	{
		// the selection's placed object when it reads, else the screen's own thing (Movable_GetFirst); read again only when the level or either ref changed
		// (a pose the editor holds in memory, not the level, is read every frame: Movable_PoseInLevel)
		if (open != readOpen || open->Generation() != readGeneration || !SameRef(fromSelection, readRef) || !SameRef(fromScreen, readScreenRef) ||
			(readDone && !host::Movable_PoseInLevel(readUsed.kind)))
		{
			readOpen = open;
			readGeneration = open->Generation();
			readRef = fromSelection;
			readScreenRef = fromScreen;
			readDone = (SourceFirst(screen) ? host::Movable_GetFirst(open, fromScreen, fromSelection, &readUsed, &readPose) :
				host::Movable_GetFirst(open, fromSelection, fromScreen, &readUsed, &readPose)) == host::MovableResult::Done;
		}

		selected = readUsed;
		pose = readPose;
		shown = readDone;
	}

	if (shown)
	{
		ref = selected;
		canRotate = host::Movable_CanRotate(ref.kind);
		LevelView_MatrixOfTurn(pose.turn, matrix);
		int32_t worldPlace[3];

		if (dragging)
		{
			memcpy(worldPlace, nowWorld, sizeof(worldPlace));
		}
		else
		{
			ToWorld(pose.place, worldPlace);
		}

		shown = PickGame_Camera(viewWidth, viewHeight, &camera) && host::Gizmo_Frame(camera, worldPlace, matrix, local, &frame);
	}

	if (!shown)
	{
		hover = host::GizmoHandle::None;
		host::WorldOverlay_Extra(none);
		return;
	}

	// THE LIT HANDLE (Drake 2026-11-25, the owner: "when camera moves the gizmo flickers alot"): it is found from the POINTER's move. Found again every frame from the
	// pointer's last place, the handles sliding under a still pointer while the camera moved (keys held, the mouse resting where it had just clicked the object) lit up and
	// went out one after another. So it is found again when the pointer moved, or with the camera it was found with; the camera moved under a still pointer: none, until
	// the pointer moves. (A press finds its own handle where it is pressed: GizmoScreen_Press.)
	if (dragging)
	{
		hover = handle;
	}
	else if (pointerX < 0)
	{
		hover = host::GizmoHandle::None;
	}
	else if (pointerMoved || SameCamera(camera, hoverCamera))
	{
		hover = host::Gizmo_Hit(camera, frame, canRotate, pointerX, pointerY);
		hoverCamera = camera;
	}
	else
	{
		hover = host::GizmoHandle::None;
	}

	pointerMoved = false;

	std::vector<host::TrackLine> lines;

	host::Gizmo_Lines(camera, frame, canRotate, hover, &lines);
	host::WorldOverlay_Extra(lines);
}

bool GizmoScreen_HoldsTheCopy()
{
	return dragging && flying;
}

void GizmoScreen_ViewAt(int32_t x, int32_t y)
{
	viewX = x;
	viewY = y;
}

bool GizmoScreen_CentreAt(int32_t x, int32_t y)
{
	return shown && !dragging && host::Gizmo_Hit(camera, frame, canRotate, x - viewX, y - viewY) == host::GizmoHandle::Centre;
}

bool GizmoScreen_Press(int32_t x, int32_t y)
{
	if (!shown || dragging)
	{
		return false;
	}

	x -= viewX;   // from the 3D view's top left (the split screen's bottom right quarter), as the frame's camera counts its pixels
	y -= viewY;

	host::GizmoHandle const hit = host::Gizmo_Hit(camera, frame, canRotate, x, y);
	int const axis = host::Gizmo_AxisOf(hit);

	if (axis < 0)
	{
		return false;
	}

	bool const started = host::Gizmo_IsTurn(hit) ? host::Gizmo_OnRingPlane(camera, frame, axis, x, y, startOnRing) : host::Gizmo_AlongAxis(camera, frame, axis, x, y, &startAlong);

	if (!started)
	{
		return false;   // the mouse's line meets that handle at no usable place (along the arrow, in the ring's plane): the press goes on as a pick
	}

	handle = hit;
	dragCamera = camera;
	dragFrame = frame;
	dragRef = ref;
	start = now = pose;
	ToWorld(start.place, startWorld);
	memcpy(nowWorld, startWorld, sizeof(nowWorld));
	memcpy(startMatrix, matrix, sizeof(startMatrix));
	amount = 0.0;
	snapped = false;
	dragging = true;
	flying = ref.kind == host::MovableKind::PlacedObject && LevelView_FlyBegin(ref.id);
	Say(std::string("Gizmo: dragging the ") + host::Movable_KindName(ref.kind) + "'s " + host::Gizmo_HandleName(handle) + " (let go to keep it, Esc puts it back)");
	return true;
}

bool GizmoScreen_Pointer(int32_t x, int32_t y, bool shift)
{
	x -= viewX;   // as Press
	y -= viewY;
	pointerX = x;
	pointerY = y;
	pointerMoved = true;   // the lit handle is found again on the next frame (GizmoScreen_Frame)

	if (!dragging)
	{
		return false;
	}

	int const axis = host::Gizmo_AxisOf(handle);

	if (host::Gizmo_IsTurn(handle))
	{
		double onRing[3];

		if (!host::Gizmo_OnRingPlane(dragCamera, dragFrame, axis, x, y, onRing))
		{
			return true;   // nowhere on the ring's plane: the turn stays as it is
		}

		double angle = host::Gizmo_AngleAbout(dragFrame.axis[axis], startOnRing, onRing);

		snapped = shift;

		if (shift)
		{
			angle = host::Gizmo_Snap(angle, host::kGizmoSnapRadians);
		}

		int32_t turned[9];

		host::Gizmo_Turned(startMatrix, dragFrame.axis[axis], angle, turned);
		LevelView_TurnOfMatrix(turned, now.turn);
		amount = angle * 180.0 / 3.14159265358979323846;
	}
	else
	{
		double along = 0.0;

		if (!host::Gizmo_AlongAxis(dragCamera, dragFrame, axis, x, y, &along))
		{
			return true;
		}

		host::Gizmo_Moved(startWorld, dragFrame.axis[axis], along - startAlong, nowWorld);
		ToFile(nowWorld, now.place);
		amount = along - startAlong;
	}

	if (flying)
	{
		LevelView_FlyPut(nowWorld, now.turn);
	}
	else
	{
		host::MovableLive_Show(dragRef, now);   // Frost's P2 kinds: their icon (or a placed hierarchy's model) drawn where the drag has it (movable_live.h)
	}

	Say(std::string("Gizmo: the ") + host::Movable_KindName(dragRef.kind) + " " + Amount() + (snapped ? " (15-degree steps)" : "") + " (let go to keep it, Esc puts it back)");
	return true;
}

bool GizmoScreen_Release()
{
	if (!dragging)
	{
		return false;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::MovableResult const result = host::Movable_Set(open, dragRef, now);

	GiveUpTheDrag();   // the live copy away
	readOpen = nullptr;   // read again

	if (result == host::MovableResult::Done)
	{
		undoReady = true;
		undoRef = dragRef;
		undoPose = start;
		PutTheRealObject(dragRef, now);   // the real block where the level now has it (Vasquez 11-18: the level's bytes alone never move it)
		last = std::string(host::Gizmo_HandleName(handle)) + " " + std::to_string((long long)floor(amount + 0.5)) + (snapped ? " snapped" : "");
		Say(std::string("Gizmo: the ") + host::Movable_KindName(dragRef.kind) + " " + Amount() + " (Ctrl+Z takes it back)");
	}
	else
	{
		Say(std::string("Gizmo: not moved: ") + host::Movable_ResultWords(result));
	}

	return true;
}

bool GizmoScreen_Key(unsigned key, bool shift, bool ctrl)
{
	(void)shift;

	if (key == VK_ESCAPE && dragging)
	{
		GiveUpTheDrag();
		Say("Gizmo: the drag was given up; the object is where it was");
		return true;
	}

	if (!onGizmoScreen || dragging)
	{
		return false;
	}

	if (key == 'G' && !ctrl)
	{
		local = !local;
		Say(local ? "Gizmo axes: the object's own (G: the world's)" : "Gizmo axes: the world's (G: the object's own)");
		return true;
	}

	if (key == 'Z' && ctrl && undoReady)
	{
		host::MovableResult const result = host::Movable_Set(LevelObjects_OpenForEdit(), undoRef, undoPose);

		undoReady = false;
		readOpen = nullptr;
		if (result == host::MovableResult::Done)
		{
			PutTheRealObject(undoRef, undoPose);
		}

		last = result == host::MovableResult::Done ? "undone" : last;
		Say(result == host::MovableResult::Done ? "Gizmo: the last drag was taken back" : std::string("Gizmo: the last drag was not taken back: ") + host::Movable_ResultWords(result));
		return true;
	}

	return false;
}

char const *GizmoScreen_LiveCopyWords()
{
	static std::string words;
	int32_t live[3] = { 0, 0, 0 };
	int32_t radius = 0;
	bool drawn = false;

	words = LevelView_FlyCopyState(live, &radius, &drawn) ? std::to_string(live[0]) + " " + std::to_string(live[1]) + " " + std::to_string(live[2]) + " drawn " + (drawn ? "1" : "0") :
		std::string();
	return words.c_str();
}

void GizmoScreen_StateLine()
{
	StateLine_Add("gizmo", !shown ? "off" : (dragging ? std::string("dragging ") + host::Gizmo_HandleName(handle) : std::string(host::Gizmo_HandleName(hover))));
	StateLine_Add("gizmospace", local ? "local" : "world");
	StateLine_Add("gizmolast", last);

	// the pose the gizmo shows, as the level holds it (read back after a drag is written): the place, and the turn x y z w in ten-thousandths
	char pose10k[160] = "";

	if (shown)
	{
		_snprintf_s(pose10k, sizeof(pose10k), _TRUNCATE, "%d %d %d %d %d %d %d", (int)pose.place[0], (int)pose.place[1], (int)pose.place[2], (int)floor(pose.turn[0] * 10000.0f + 0.5f),
			(int)floor(pose.turn[1] * 10000.0f + 0.5f), (int)floor(pose.turn[2] * 10000.0f + 0.5f), (int)floor(pose.turn[3] * 10000.0f + 0.5f));
	}

	StateLine_Add("gizmopose", pose10k);

	// where the placed object's real display block is drawn (the world's units): after a written drag, its new place
	int32_t drawn[3];

	StateLine_Add("gizmodrawn", shown && ref.kind == host::MovableKind::PlacedObject && LevelView_ObjectBlockAt(ref.id, drawn) ? std::to_string(drawn[0]) + " " +
		std::to_string(drawn[1]) + " " + std::to_string(drawn[2]) : std::string());

	// DURING a drag (the owner 11-19: "the object doesn't move with it"): where the live copy that stands for the object is, and whether the game draws it; empty when no drag is
	// going; "none: <why>" when the object got no live copy (LevelView_FlyRefused), so the state line says why it is not drawn moving
	StateLine_Add("gizmolive", !dragging ? std::string() : (flying ? std::string(GizmoScreen_LiveCopyWords()) : std::string("none: ") + LevelView_FlyRefused()));

	std::string points;

	if (shown && !dragging)
	{
		host::GizmoHandle const handles[7] = { host::GizmoHandle::Centre, host::GizmoHandle::MoveX, host::GizmoHandle::MoveY, host::GizmoHandle::MoveZ, host::GizmoHandle::TurnX,
			host::GizmoHandle::TurnY, host::GizmoHandle::TurnZ };

		for (int h = 0; h < (canRotate ? 7 : 4); h++)
		{
			std::string const one = GrabPoint(handles[h]);

			if (!one.empty())
			{
				points += (points.empty() ? "" : "|") + one;
			}
		}
	}

	StateLine_Add("gizmopoints", points);
}
