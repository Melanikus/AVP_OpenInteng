// track_editor.h: inteng's Track Editing screen as a state machine over the open level: Edit Track (0x36) and the commands of its pages 2 to 5 (0x514..0x52b).
// Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// No window: the editor (editor_main.cpp) owns the three views' cursor, the prompts and the playing; this decides what each command does to the level and
// to the screen's state. The sequence of every command is inteng's switch in FUN_004d9af0 (dump-all-3.txt 4586-4720, listing-all-3.txt 0x4dcabd):
//
//   state 0  not editing     -> Edit Track (0x36) loads the track and shows page 2, state 2 (0x4dd05e..0x4dd074)
//   state 2  track loaded    page 2: 0x516 0x519 0x51a 0x518 0x514 0x51b 0x527 0x524 0x525 0x528 0x529 0x52a 0x52b 0x51f 0x520
//   state 3  end-position edit page 3: 0x51d 0x518 0x521 0x522 0x519 0x51a 0x517
//   state 4  playing         page 4: 0x515
//   state 5  pivot-point edit page 5: 0x523 0x51e
//   and in states 2, 3 and 4 the static table's Stop Track Edit (0x51c)
//
// THE TRACK IS EDITED IN MEMORY (TrackEditor::working) and written to the level when Stop Track Edit lets it go, as inteng's FUN_004eff90 / FUN_004f1300 do; Edit Track loads it re-based on the object (FUN_004f0490)
// and Stop gives the object the track's pose at its timer start (FUN_004f0870). The game's loader checks what is written; if it refuses, the editing goes on.
//
// THE END MARKER: inteng flies the object itself with the movement keys (F) and turns it with the turn keys; Finish End Pos Edit (FUN_004f0cd0) takes its place and, when the section has no spin, its matrix as the end.
// Here the marker is carried by the camera (the movement and turn keys: the editor passes the turn since the marker was last taken as markerTurn), drawn as a cross with its three axes, and taken by Finish End Pos Edit.
// NAMED DEVIATIONS (track-edit-facts.md): the marker is a cross with axes, not the object drawn live; Track Properties, the sound and Section time are the toolkit's dialogs (inteng's own windows); the sounds are not
// played while the track plays; < and > (a spin section's angle) are not built: unfinished in inteng.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "track_edit.h"
#include "track_sections.h"

class OpenLevelFile;

namespace host
{
	enum class TrackState
	{
		Off = 0,
		Loaded = 2,
		EndEdit = 3,
		Playing = 4,
		PivotEdit = 5
	};

	enum class TrackPrompt
	{
		None,
		SectionTime,    // Properties > Section (0x51b): "Time for section"
		TrackFlags,     // Properties > Track (0x527)
		Sound,          // Sound / Start Sound / End Sound > Edit (0x524 0x528 0x52a), the sound's index in soundIndex
		DeleteTrack     // Delete > Track (0x520): "Delete this track?"
	};

	struct TrackEditor
	{
		TrackState state = TrackState::Off;
		uint8_t id[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };   // the object whose track is edited
		size_t current = 0;                           // the current section, whose markers are shown (FUN_004f0da0)
		// THE TRACK IN MEMORY while it is edited, as inteng keeps it (FUN_004eff90 loads it into the track editor's own sections, FUN_004f1300 writes the chunks only when the track is let go: Stop Track Edit): the
		// level's bytes are not touched by an edit, only by Stop Track Edit (the game's loader checks the result then). Null when no track is held.
		std::shared_ptr<TrackLoaded> working;
		// THE END MARKER OBJECT while it flies (F): how far the keys have turned it since the section's end turn was last taken, in degrees about the vertical axis, pitch, roll (the editor sets these every frame from the camera's
		// turn, which carries the marker as the flying object is carried); the next Finish End Pos Edit (or the end edit being left: FUN_004f0cd0) makes it the section's end turn. markerAboutPivot: P.
		double markerTurn[3] = { 0.0, 0.0, 0.0 };
		bool markerAboutPivot = false;
		// THE TURN THE MARKER IS TURNED FROM (markerTurn is counted from it): the current section's end turn, unless Jump To Start gave the marker a start's turn (markerBaseSet: inteng's 0x521 / 0x522
		// copy the start marker's matrix into the end marker object as well as its place, 0x4dcda4 / 0x4dcdf5; T-3a). Let go when the marker's turn is taken or another section's end is edited.
		float markerBase[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		bool markerBaseSet = false;
		size_t markerBaseSection = 0;   // the section whose end edit the jump was made in: markerBase counts only while that section is current
	};

	// The turn the end marker is turned from now (markerBase after a Jump To Start, else the current section's end turn), x y z w; false when no track is held or the section is not there
	bool TrackEdit_MarkerBase(TrackEditor const &editor, float out[4]);

	// THE MOUSE GIZMO's section end (movable.h, movable_track_end.cpp; the owner's addition, 11-18): the place the OBJECT takes at the END of section `section` of the track held,
	// as its end marker shows it (TrackData_MarkerAt with the editor's place function), and that end's turn (x y z w). false when no track is held or there is no such section.
	bool TrackEdit_SectionEnd(TrackEditor const &editor, size_t section, int32_t place[3], float quat[4]);

	// The end of section `section` put at a place and a turn, as Finish End Pos Edit puts it (FUN_004f0cd0): the turn first when it is another (TrackData_SetEndTurn: the object
	// keeps its places at the end of this section and the start of the next), then the place when it is another (TrackData_FinishEnd: the next section starts there). The track is
	// kept in memory, as every edit is (Stop Track Edit writes it). Once a turn is taken, a Jump To Start's turn of that section is let go, as Finish End lets it go. false, with
	// nothing changed, when no track is held, there is no such section, or the track's own checks refuse the place or the turn.
	bool TrackEdit_SetSectionEnd(TrackEditor *editor, size_t section, int32_t const place[3], float const quat[4]);

	// What a command asks the editor to do besides what it already did to the level and the state
	struct TrackAction
	{
		std::string words;            // for the status line
		bool changed = false;         // the level's bytes changed
		bool leave = false;           // back to Object Placement (Stop Track Edit, or the track was deleted)
		bool markerTurnTaken = false; // the marker's turn became the section's end turn: the editor counts the marker's turn from now
		bool startPlay = false;       // play *playData with the game's code
		bool stopPlay = false;
		bool cursorSet = false;       // the 3D cursor is to become cursor
		int32_t cursor[3] = { 0, 0, 0 };
		TrackPrompt prompt = TrackPrompt::None;
		int soundIndex = 0;
		std::string promptDefault;    // what the prompt starts with
		TrackSound sound;             // a sound's Edit: the sound as the track holds it (the file's units), for edobtype's Sound Edit window (FUN_004f2bf0)
		bool soundNew = false;        // the track had no such sound: inteng made one (FUN_004f2a60), its ranges in inteng's own units (500000 / 1000000)
		TrackData playData;
		std::vector<TrackSound> playSounds;   // the track's sounds, played with it (the game's sound code)
	};

	// The function that says where the OBJECT is at an end of a section (the editor passes the game's own: track_play_game.h). With none, the pivot is the marker.
	void TrackEdit_UseObjectPlace(TrackObjectPlaceFn placeFn);

	// The function that gives the pose of a track at its timer start as the object's file place and turn (the editor passes the game's: track_play_game.h TrackPlay_PoseAtStartAsFile): Stop Track Edit
	// puts it into the object's record, as inteng's FUN_004eff90 does when it lets the track go (0x4effba: FUN_004f0870)
	typedef bool (*TrackPoseFn)(TrackData const &track, int32_t place[3], float quat[4]);
	void TrackEdit_UsePose(TrackPoseFn poseFn);

	// The function that turns a section's start turn by yaw, pitch and roll degrees (the editor passes the game's: track_play_game.h TrackPlay_TurnedBy)
	typedef bool (*TrackTurnFn)(float const startQuat[4], double yaw, double pitch, double roll, float outQuat[4]);
	void TrackEdit_UseTurn(TrackTurnFn turnFn);

	// The track as it is being edited now, as a clip of the chunks (the whole OBJTRAK2 chunk and the sounds), for what draws it; false when this object is not the one being edited
	bool TrackEdit_WorkingClip(TrackEditor const &editor, uint8_t const *objectId, TrackClip *clip);

	// The inteng state the command is on a page of, or false for a command that is not one of Track Editing's
	bool TrackEdit_IsTrackCommand(int32_t command);

	// Edit Track (0x36): the selected object's track is loaded and the editor is in state 2 at section 0. *action has the cursor and words. False (with words) when it cannot.
	bool TrackEdit_Enter(OpenLevelFile *file, uint8_t const *objectId, TrackEditor *editor, TrackAction *action, bool followTheObject = true);

	// A command of Track Editing's menus. cursor is the 3D cursor now (the end place of Finish End Pos Edit; minus the object offset of a pivot edit).
	void TrackEdit_Command(OpenLevelFile *file, TrackEditor *editor, int32_t command, int32_t const cursor[3], TrackAction *action);

	// The answers of the prompts the commands asked for
	void TrackEdit_AnswerTime(OpenLevelFile *file, TrackEditor *editor, std::string const &typed, TrackAction *action);
	void TrackEdit_AnswerFlags(OpenLevelFile *file, TrackEditor *editor, std::string const &typed, TrackAction *action);
	void TrackEdit_AnswerSound(OpenLevelFile *file, TrackEditor *editor, int soundIndex, std::string const &typed, TrackAction *action);

	// The sound back from the Sound Edit window (FUN_004f2bf0, after OK or Cancel alike): it becomes the track's sound of that index (0 the track's own, 1 start, 2 end), made if the track had none,
	// whatever its numbers (inteng takes the window's floats through __ftol and checks none). Its name is bound as the file's: 1 to kTrackSoundNameMost letters, no zero inside.
	void TrackEdit_SetSound(OpenLevelFile *file, TrackEditor *editor, int soundIndex, TrackSound const &sound, TrackAction *action);
	void TrackEdit_AnswerDelete(OpenLevelFile *file, TrackEditor *editor, std::string const &typed, TrackAction *action);

	// The play has reached the end of a track that does not loop (the game's code set playing to 0): back to state 2 as Stop (0x515) does
	void TrackEdit_PlayEnded(TrackEditor *editor);

	// SHIFT+T, "Updating track positions" (inteng's FUN_004efd50, object-placement.md OP-83), after its message box: a track being edited is let go first as Stop Track Edit lets it go (inteng's first
	// FUN_004eff90(block, 0) writes it), then every object of the level that holds a track, in the list's order, has it loaded as Edit Track loads it (re-based onto where the object is now:
	// FUN_004eff90(block, 0) -> FUN_004f0490) and let go as Stop Track Edit does (FUN_004f1300 writes it, FUN_004f0870 gives the object the track's start pose). An object with no track is passed
	// over. INTENG BUG TB-T2, FIXED (Bishop's ruling 11-17): inteng's last FUN_004eff90(0, 0) lets the edited track go but leaves Track Editing on, with no track (its next command reads freed
	// sections); here the editing is over (*editor Off) and action->leave says so. action->changed when any bytes changed; the words say how many tracks, and any that could not be written.
	void TrackEdit_UpdatePositions(OpenLevelFile *file, TrackEditor *editor, TrackAction *action);
}
