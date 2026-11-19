// gizmo_sources.cpp: what the mouse gizmo moves on the screens whose current thing is not a placed object (P2 of Hicks's plan .claude/plans/streamed-twirling-spindle.md;
// the owner's addition, inteng had none). Each screen's current thing is given to the gizmo as a movable.h ref by GizmoScreen_UseRefSource (gizmo_screen.h), once at start-up;
// the gizmo reads and writes it through that kind's adapter (movable_<kind>.cpp). Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// The screens and what each gives (kind None while nothing of it is current, or while the screen's own wait holds it):
//   Object Placement, and its sounds screen    the current special object (specialMode: a click or Select Object makes one current and clears the selection, editor_main.cpp
//                                              PlacementFind), else the current placed hierarchy (placedHierarchyCurrent, its number)
//   Light Sourcing (both of its screens)       the selected light (the selection's lightIndex), not while L flies it or it is hidden
//   Track Editing                              the current section's end of the track held (trackEd), not while it plays or its pivot is edited
//   Strategy Data                              the current object, as a virtual object (the selection's ID: a virtual object made current is the selection, strategy_screen.cpp
//                                              SelectedObjectId); a placed object is the gizmo's own first try, and an ID no virtual object has gives no gizmo
// Waypoint's entry point marker is given by construction_modes_screen.cpp, whose state it is (the module the markers are of is that file's own).
// DECIDED P2-SOURCES-FILE: one file for the sources that read state editor_shared.h already shares, so no screen file and not editor_main.cpp (at its size limit) changes for them.

#include <windows.h>

#include <string.h>

#include <vector>

#include "editor_shared.h"
#include "gizmo_screen.h"
#include "level_objects.h"
#include "light_edit.h"
#include "movable.h"
#include "selection.h"
#include "special_mode.h"
#include "special_objects_edit.h"
#include "track_editor.h"

namespace
{
	host::MovableKind KindOf(host::SpecialKind kind)
	{
		switch (kind)
		{
			case host::SpecialKind::Generator: return host::MovableKind::AvpGenerator;
			case host::SpecialKind::BadGuy: return host::MovableKind::BadGuy;
			case host::SpecialKind::PlayerStart: return host::MovableKind::PlayerStart;
			case host::SpecialKind::Sound: return host::MovableKind::Sound;
			case host::SpecialKind::ParticleGenerator: return host::MovableKind::ParticleGenerator;
		}

		return host::MovableKind::None;
	}

	host::MovableRef PlacementSource()
	{
		host::MovableRef ref;

		// a question of the special object's (Find's list, Edit's fields) holds the screen
		if (specialMode.ask != host::SpecialAsk::None)
		{
			return ref;
		}

		if (specialMode.haveCurrent)
		{
			ref.kind = KindOf(specialMode.kind);
			ref.number = (int32_t)specialMode.number;
			return ref;
		}

		if (placedHierarchyCurrent >= 0)
		{
			ref.kind = host::MovableKind::PlacedHierarchy;   // by its number (an all-zero ID): movable_placed_hierarchy.cpp
			ref.number = placedHierarchyCurrent;
		}

		return ref;
	}

	host::MovableRef LightSource()
	{
		host::MovableRef ref;
		host::Selected const picked = host::Selection_Get();

		if (lightFlying || picked.kind != host::SelectedKind::Light || picked.lightIndex < 0)
		{
			return ref;
		}

		// a hidden light (Hide All, or I: the local flag LOFlag_Invisible 0x20, chunk_light.hpp; the STDLIGHT's local flags at +72) has no marker drawn, so no gizmo either
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		host::LightData light;
		uint32_t local = 0;

		if (!open || host::CopyLight(open, (size_t)picked.lightIndex, &light) != host::LightEditResult::Done || light.size() < 76)
		{
			return ref;
		}

		memcpy(&local, light.data() + 72, sizeof(local));

		if (local & 0x20)
		{
			return ref;
		}

		ref.kind = host::MovableKind::Light;
		ref.number = picked.lightIndex;
		return ref;
	}

	host::MovableRef TrackSource()
	{
		host::MovableRef ref;

		if (!trackEd.working || trackPlaying || trackEd.state == host::TrackState::Playing || trackEd.state == host::TrackState::PivotEdit)
		{
			return ref;
		}

		ref.kind = host::MovableKind::TrackSectionEnd;
		memcpy(ref.id, trackEd.id, sizeof(ref.id));
		ref.number = (int32_t)trackEd.current;
		return ref;
	}

	host::MovableRef StrategySource()
	{
		host::MovableRef ref;
		uint8_t id[8];

		if (!SelectedObjectId(id))
		{
			return ref;
		}

		ref.kind = host::MovableKind::VirtualObject;
		memcpy(ref.id, id, sizeof(ref.id));
		return ref;
	}

	// Given at start-up (gizmo_screen.h: its table is a function's own static, made at its first call)
	bool const sourcesGiven = (GizmoScreen_UseRefSource("Object Placement", &PlacementSource),
		GizmoScreen_UseRefSource("Placement mode 1 (sounds)", &PlacementSource),
		GizmoScreen_UseRefSource("Light Sourcing (with Doors)", &LightSource),
		GizmoScreen_UseRefSource("Light Sourcing (with Hierarchy)", &LightSource),
		GizmoScreen_UseRefSource("Track Editing", &TrackSource, true),   // before the selection: the tracked object stays selected while its track is edited
		GizmoScreen_UseRefSource("Strategy Data", &StrategySource), true);
}
