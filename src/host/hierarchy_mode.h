// hierarchy_mode.h: the Hierarchy mode's commands as the editor runs them: what each asks, what it checks, what it does to the open level. P11, Hierarchy slice 6.
// Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT IS. inteng's Hierarchy mode (state 0xc; FUN_004889d0 is its command switch) keeps a small state (the current hierarchy, sequence and shape set collection, the
// last platform speed and angle, the platform switch) and runs a command at a time: some ask a question first (a chooser, a typed name or number, a yes). This module holds
// that state and the answers; the editor (editor_main.cpp) shows the question and hands the typed answer back. All the data work is hierarchy_edit.h's (the level is rebuilt
// and the game's loader reads it before anything changes), and every rule is the model's (hierarchy_collections.h, sequence_speed.h, hierarchy_name.h).
//
// THE COMMANDS DONE (inteng's numbers, hierarchy-menu-pairs.md; each read in p11-shape-handlers.md sections 59 to 63):
//   0x03 Select Hierarchy, 0x04 Select Sequence, 0x49 Copy Hierarchy, 0x52 Select Collection, 0x53 Create Collection, 0x54 Delete Collection, 0x55 Add to Collection,
//   0x56 Remove from Collection, 0x57 Rename Collection, 0x59 Collection Voice Index, 0x4a Set speed, 0x4b Set angle, 0x4c Toggle on/off (the platform switch), 0x48 Save changes to
//   sequence times (the second switch), 0x68-0x6a Sequence Type, 0x6f-0x72 No Interpolation / Half Frame Rate.
// NAMED DEVIATIONS (carded in unfinished-in-inteng.md): inteng's choosers are lists you click; here the list is shown and the choice is typed (its number, or part of its name);
// inteng draws the selected hierarchy in a view of its own (the model, the sequence playing), which the editor does not; a hierarchy name is 1 to 255 plain letters.
// ASSUMPTION: the sequences of a hierarchy are the OBANSEQC chunks of its own OBANSEQS (beside its name), where the game's build_time_list reads them and all 23 stock models
// keep them; inteng's own sequence list (the hierarchy object's +0x38 array, FUN_0047d7xx) is built from the same list as far as read, not proven.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "hierarchy_edit.h"

class OpenLevelFile;

namespace host
{
	enum class HierarchyAsk
	{
		None,
		SelectHierarchy,
		SelectSequence,
		TurretSequence,   // Add 'turret delta' (0x7): "Select Turret Sequence"
		SelectCollection,
		CopyName,
		CreateName,
		RenameName,
		DeleteYes,
		VoiceIndex,
		Speed,
		Angle,
		AddSet,
		RemoveSet,
		SelectSingleSet,
		FemaleYes,
		OpenModel,
		SoundIndex,       // New sound (0x4d): the index
		SoundEditPick,    // Edit sound (0x4e): which sound (its fields are then one window: HierarchyStep::soundForm)
		SoundDeletePick,  // Delete sound (0x4f): which sound
		SoundApplyPick,   // Apply frame sound (0x50): which sound
		ChangeObjectList, // Change object for section (0x5e): "from a list?" yes or no
		ChangeObjectPick, // ... which object (the chooser)
		ChangeObjectName, // ... the typed object name
		CopyFrom,         // Copy between hierarchies (0x5b): the hierarchy to copy the frame sounds from
		CopyTo,           // ... and the one to copy them to
		DummySize,        // Dummy Object Size (0x5d): the size in mm
		Help,             // Help (0x38): the two pages of inteng's help
		SelectDelta,      // Add delta (0x9): which delta sequence plays over the main one
		AttachObject,     // Attach shape to bones (0x62): which object
		AttachHierarchy,  // ... to which hierarchy
		AttachRedo,       // ... "Do you want to redetermine the vertex connections anyway?"
	};

	struct HierarchyMode
	{
		std::string hierarchy;               // the current hierarchy's name; empty: none
		bool sequenceSelected = false;
		// inteng's delta frame of each sequence (its +0x10, which FUN_00478930 reads for Add delta's list and the text), as it was when the hierarchy was opened (FUN_0048b550 builds it from the
		// keyframe flags). Of the four delta types only "middle" / "current" write it (FUN_004808e0 -3, FUN_00480950); "not a delta" and "first frame" change the file's flags but leave it as it was
		// (reassess/hierarchy-deltas-vasquez.md HD-13: inteng's own quirk, kept), so Add delta lists what was there until the hierarchy is opened again. Key: number * 65536 + sub-number; -1 not a delta.
		std::map<int64_t, int32_t> deltaFrameHeld;
		bool turretChosen = false;           // Add 'turret delta' (0x7) was answered: the view starts that sequence on the sections under "dum turret" (turretSequence is its place in the list); the caller clears it
		size_t turretSequence = 0;
		size_t sequence = 0;                 // the current sequence's place in the hierarchy's list
		bool collectionSelected = false;
		int32_t collection = 0;              // the current collection's number
		model::PlatformState platform = { 3000, 0 };   // the last speed (the exe's data: 3000) and last angle (0)
		bool platformOn = false;             // Toggle on/off (DAT_00637118)
		std::vector<int32_t> applied;        // the shape sets applied to the view (inteng's view +0x28): a collection's members, one set (Select Single Set), or none
		std::vector<std::string> models;     // the model files the editor found (avp_huds, names without .rif): the caller fills it when the mode is entered
		std::string modelName;               // the model the mode works on ("" = the open level); set by the caller after it opened the file
		std::string modelRequested;          // set by a typed Open model answer: the caller opens avp_huds\<name>.rif and then runs Select Hierarchy again
		std::string section;                 // the section the view has selected, by its object name ("" = none): Change object for section works on it; the view sets it
		std::string copyFrom;                // Copy between hierarchies: the first answer
		bool addDeltaRequested = false;      // set by a typed Select Delta Sequence answer: the view adds that delta (hierarchy_view.h), then the caller clears this
		int32_t addDeltaNumber = 0;
		int32_t addDeltaSub = 0;
		bool vertexMode = false;             // V (0x63, DAT_00637130 "Vertex Selection Mode"): clicks in the view take the points of the attached object's shape; the view owns the click, the mode the switch
		std::vector<int32_t> vertices;       // the points taken so far (numbers in the attached object's shape); Shift + V (0x61) puts them on the selected bone and clears this
		bool deltaTimeRequested = false;     // set by Shift + / Shift - (0xf / 0x10): the view gives its delta controller deltaMs, then the caller clears this
		uint32_t playersMade = 0;            // counts inteng's new players (FUN_0048b550 -> FUN_00477450): each one starts with "Play commands affect all sequences" off (+0x88 = 0, 0x477603)
		bool deltaSelected = false;          // inteng's delta object (view +0x8) exists: a delta sequence was chosen with Add delta (0x9) and not taken away with Remove delta (0xa)
		size_t deltaPlace = 0;               // ... its place in the hierarchy's sequence list
		int32_t deltaMs = 1000;              // ... its time in milliseconds: its OBASEQTM number when above 0, else 1000; Shift + and Shift - (0xf / 0x10) change it
		bool boundsRequested = false;        // set by Calculate Bounding Box (0x46): the caller plays every sequence with the game's pose, writes the boxes (HierarchyEdit_WriteBoundingBox) and clears this
		bool removeDeltasRequested = false;  // set by Remove delta (0xa): the view takes its deltas away, then the caller clears this
		std::string attachObject;            // Attach shape to bones: the object chosen so far
		std::string attachHierarchy;         // ... and the hierarchy
		int32_t position = 0;                // where the view stands in the current sequence, 0 to 65535 (16.16 of its length): the keyframe flag and frame sound commands act on the frame that holds it; the view sets it
		double levelScale = 1.0;             // the file's ENVSDSCL scale (1 when it has none: every model): New sound's start ranges use it
		SoundInfo soundBeingEdited = { 0, std::string(), 0, 0, 0, 0, 0 };   // Edit sound's answers so far
		int32_t sequenceMs = 1000;           // the current sequence's time in milliseconds: its OBASEQTM number when above 0, else 1000 (FUN_0047d6f0); the time keys change it
		bool adaptiveDegradation = false;    // Adaptive Degradation (0x45, DAT_006370f4): each frame the view picks the model's detail level by the distance (FUN_00479b80); off at the start (0x4887da)
		bool dummyObjects = false;           // Dummy Objects (0x5c, DAT_006370f8): the cube and axes at the model's place are drawn when on (0x47f329); off at the start
		int32_t dummyObjectSize = 40000;     // Dummy Object Size (0x5d, DAT_005d4e24): inteng's unit (1e-5 m); 40000 to start with, shown in mm as size / 100
		int viewKind = 0;                    // Alien (0x3c) = 1 or Marine (0x3d) = 2: the view's projection scale (+0x34, +0x38) is 0xa0 or 0x140; 0 = not chosen yet
		int helpPage = 0;                    // Help: the page shown
		bool saveTimesOn = false;            // "Save changes to sequence times" (0x48, DAT_00637120: a switch, off at the start); the platform write needs it on too
		HierarchyAsk ask = HierarchyAsk::None;         // the question the editor is showing
		std::vector<std::string> list;       // the lines of the chooser the question shows
		std::vector<int32_t> listValues;     // what each line stands for (a place in a list, or a collection or shape set number)
	};

	struct HierarchyStep
	{
		bool prompt = false;                 // true: show `title` (and the mode's list) and wait for a typed answer, then HierarchyMode_Typed
		std::string title;
		std::string answer;                  // else: what to say on the status line
		bool messageBox = false;             // the answer is inteng's MESSAGE BOX (FUN_00474030 + FUN_0057c800(100, 100): the words and Ok), not the status line
		std::string defaultText;             // for a prompt: what is typed already
		bool confirm = false;                // the prompt is inteng's YES / NO box (FUN_00474a40, title the words): Yes answers "yes", No "no" (Delete Collection)
		// messageBox with prompt: the box (answer, Ok) comes first and then the question ("That name is already used", then the same text dialog again: FUN_0048af00 / FUN_0047a880)
		bool textDialog = false;             // the prompt is inteng's TEXT DIALOG (FUN_00474170: the words, one edit holding defaultText, Ok / Cancel, run by FUN_0057c800(100, 100)), not a typed line
		bool femaleForm = false;             // the prompt is Set Properties' window (FUN_0048d6a0: "Shape set properties", the label "Female" and its check box, Ok / Cancel); femaleChecked how the box starts
		bool femaleChecked = false;
		bool soundForm = false;              // open the Sound Edit window for mode->soundBeingEdited (FUN_004ebd60: edobtype's edit_sound), then HierarchyMode_SoundForm; with messageBox, after the box's Ok
	};

	// The number an inteng edit holds (the edit's method +0x2c, FUN_0057d620, which FUN_00474340 calls): a "-" first, then digits only, else 0; an empty edit is 0. The sum wraps as the exe's
	// 32-bit one does.
	int32_t HierarchyMode_EditNumber(std::string const &text);

	// The Sound Edit window's answers for mode->soundBeingEdited, in its order: the wav name, inner range (m), outer range (m), pitch (semitones), volume (%). FUN_004ebd60 gives edobtype the
	// sound's inner and outer x 0.00001, pitch / 128, volume x 100 / 127, and takes them back as ftol(m x 100000), ftol(semitones x 128), ftol((% + 50/127) x 0.01 x 127); the DLL's own limits
	// (DDV: inner 0..1000, outer 0..2000, pitch -48..48, volume 0..100) refuse an answer outside them, the window then shown again. The flag boxes are off for a hierarchy's sound (FUN_004ebc80's
	// third number is 0, so the mask FUN_004ebd60 passes is 0) and its flag bit 0 is cleared after the edit (FUN_004816f0 0x48171b). The ranges are inteng's unit: the file's x 100 x levelScale.
	HierarchyStep HierarchyMode_SoundForm(OpenLevelFile *file, HierarchyMode *mode, std::vector<std::string> const &values);

	// What the Sound Edit window shows first for mode.soundBeingEdited, in the same order (the volume as a whole percent, the others as MFC shows a float: "%.6g")
	std::vector<std::string> HierarchyMode_SoundFormShown(HierarchyMode const &mode);

	// The command's first step. An unknown command number is not this module's: answer is empty and prompt false.
	HierarchyStep HierarchyMode_Command(OpenLevelFile *file, HierarchyMode *mode, int32_t command);

	// The typed answer to the question that is open (mode->ask); closes it, or asks again (prompt true) when inteng asks again.
	HierarchyStep HierarchyMode_Typed(OpenLevelFile *file, HierarchyMode *mode, std::string const &typed);

	// The editor closes the question without an answer (Esc).
	void HierarchyMode_Cancel(HierarchyMode *mode);

	// A new level has been opened, or the mode is left: nothing is selected, the platform switch is off.
	void HierarchyMode_Reset(HierarchyMode *mode);

	// Whether a command number is one this module does
	bool HierarchyMode_Does(int32_t command);
}
