// movable_track_end.cpp: movable.h's adapter for a TRACK SECTION END in Track Editing: the place the object takes at the end of a section, and that end's turn, read and
// set on the track the editor holds (track_editor.h TrackEdit_SectionEnd / TrackEdit_SetSectionEnd, which put it as Finish End Pos Edit does: TrackData_SetEndTurn, then
// TrackData_FinishEnd). The track stays in memory, as every Track Editing change does; Stop Track Edit writes it, and the game's loader checks it then. Written 2026-11-18 by
// Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHICH ONE: ref.id is the object whose track is edited (it must be the editor's), ref.number the section. Only a section's END is moved: a section's start is the end of the
// one before it, and the first section's start is the object's own place (the placed object adapter's).
// The editor comes from movable_track_end.h (track_edit_screen.cpp gives it). With none, or no track held, or another object's, every ref is NoSuchThing.

#include "movable_track_end.h"

#include "movable.h"

#include <string.h>

#include "track_editor.h"

namespace host
{
	namespace
	{
		TrackEditor *heldEditor = nullptr;

		// NothingOpen with no level; NoSuchThing unless the editor holds this object's track and it has that section
		MovableResult Check(OpenLevelFile *file, MovableRef const &ref)
		{
			if (!file)
			{
				return MovableResult::NothingOpen;
			}

			if (!heldEditor || !heldEditor->working || ref.number < 0 || (size_t)ref.number >= heldEditor->working->track.sections.size() ||
				memcmp(heldEditor->id, ref.id, sizeof(heldEditor->id)) != 0)
			{
				return MovableResult::NoSuchThing;
			}

			return MovableResult::Done;
		}
	}

	void MovableTrackEnd_UseEditor(TrackEditor *editor)
	{
		heldEditor = editor;
	}

	MovableResult MovableTrackEnd_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose)
	{
		MovableResult const checked = Check(file, ref);

		if (checked != MovableResult::Done)
		{
			return checked;
		}

		MovablePose read;

		if (!TrackEdit_SectionEnd(*heldEditor, (size_t)ref.number, read.place, read.turn))
		{
			return MovableResult::NoSuchThing;
		}

		*pose = read;
		return MovableResult::Done;
	}

	MovableResult MovableTrackEnd_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose)
	{
		MovableResult const checked = Check(file, ref);

		if (checked != MovableResult::Done)
		{
			return checked;
		}

		return TrackEdit_SetSectionEnd(heldEditor, (size_t)ref.number, pose.place, pose.turn) ? MovableResult::Done : MovableResult::WriterRefused;
	}
}
