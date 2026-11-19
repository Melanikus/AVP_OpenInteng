// movable_track_end.h: where movable.h's TRACK SECTION END adapter (movable_track_end.cpp) finds the track being edited. Written 2026-11-18 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Track Editing keeps the track in memory while it is edited (track_editor.h TrackEditor::working) and writes it to the level only at Stop Track Edit, so a section end is
// not in the file to be read or written. The screen that owns the editor (track_edit_screen.cpp) gives it here once; a test gives its own.
#pragma once

namespace host
{
	struct TrackEditor;   // track_editor.h

	// The editor whose held track the adapter reads and changes; null: none (every ref is then NoSuchThing)
	void MovableTrackEnd_UseEditor(TrackEditor *editor);
}
