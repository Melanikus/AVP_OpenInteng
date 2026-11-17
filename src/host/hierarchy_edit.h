// hierarchy_edit.h: the data side of inteng's Hierarchy-mode commands that add, take away, rename or resize a chunk: Copy Hierarchy (0x49), the shape set collection
// commands Create / Rename / Delete / Voice Index (0x53, 0x57, 0x54, 0x59) and a sequence's Platform speed / angle (0x4a, 0x4b). P11, Hierarchy slice 5. Written 2026-11-15 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT WORKS (as module_data_edit.h and object_edit.h): the open level is written out as the next save would write it (OpenLevelFile::BuildLevel), the chunk is added, cut
// or made again in those bytes, the sizes of the chunks around it are made again, and OpenLevelFile::ReplaceLevel reads the result with the game's own loader and swaps it in
// only if the loader takes it. On any refusal nothing has changed. After a success every Chunk * the editor held is gone (the level is the new one).
//
// WHERE THE CHUNKS ARE (avpx 3dc/win95/hierarchy_chunk.cpp, animation_objects.cpp, avp/win95/projload.cpp:741-800):
// - a HIERARCHY is a child of the level's own REBINFF2 chunk named OBJCHIER, with its name in an OBHIERNM child and exactly one section (an OBJCHIER child);
// - the shape set COLLECTIONS (HSETCOLL) are children of REBINFF2 itself, one list for the whole file, not one for each hierarchy;
// - a SEQUENCE is an OBANSEQC child of the OBANSEQS child of the hierarchy's OWN chunk (beside its OBHIERNM; avp/win95/projload.cpp build_time_list reads it there, and all 23
//   stock models keep it there); its platform speed and angle are its OBASEQSP child (12 bytes). No stock model has one yet: the first write makes it.
//
// THE RULES: the rules of each command are the model's (model::CheckCopyName, CreateCollection, RenameCollection, DeleteCollection, SetCollectionVoiceIndex, SetPlatformSpeed,
// SetPlatformAngle, ApplyPlatformState); this file finds the chunks and writes the bytes. What the level's bytes hold is hostile: every size is checked as the bytes are walked.
//
// NAMED POINTS (deviations of the editor, carded in unfinished-in-inteng.md): a hierarchy name is 1 to 255 bytes of plain text (32 to 126; inteng's prompt length and
// letters are not read); a new HSETCOLL / OBASEQSP chunk is put at the END of its parent, as the game's own constructor does.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "hierarchy_collections.h"
#include "sequence_speed.h"

class OpenLevelFile;   // src/io/level_file.h

namespace host
{
	enum class HierarchyEditResult
	{
		Done,
		BadCall,            // no level open, or a missing argument
		NoHierarchy,        // no hierarchy has that name
		NoSequence,         // the hierarchy has no such sequence (or no sequence list)
		NoCollection,       // no collection has that number
		Refused,            // the model's rule refused the command: *why says how
		BadLevelBytes,      // a chunk of the level could not be read (a collection that does not read, a size that does not fit)
		CantBuild,          // the level could not be put together from its bytes
		LoaderRefused,      // the game's own loader would not take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *HierarchyEditWords(HierarchyEditResult result);

	// The name of every hierarchy of the open level that has one, in file order
	HierarchyEditResult HierarchyEdit_Names(OpenLevelFile *file, std::vector<std::string> *names);

	// Copy Hierarchy: the hierarchy called `from` is copied, named `newName`, and put at the end of the level. why (when not null) is the model's words on Refused.
	HierarchyEditResult HierarchyEdit_Copy(OpenLevelFile *file, std::string const &from, std::string const &newName, char const **why);

	// The sequences of the named hierarchy (its OBANSEQS list, in file order): the header's frame count, number and sub-number (OBASEQHD: 3 numbers; 0s when the chunk has no
	// header), and whether it has an OBASEQSP chunk
	struct SequenceInfo
	{
		int32_t frames;
		int32_t number;
		int32_t subNumber;
		bool hasPlatform;
		std::string name;   // the sequence's own NAME, stored in its OBASEQHD after the four numbers (payload from byte 16, ended by a 0: "PredatorHUD_Come", "AlienStand_boom_fall_back"); inteng's choosers list these (FUN_0048c640 / FUN_0048bc10 take the string at +0xc of the sequence object)
	};

	HierarchyEditResult HierarchyEdit_Sequences(OpenLevelFile *file, std::string const &hierarchy, std::vector<SequenceInfo> *sequences);

	// The shape set collections, in file order
	HierarchyEditResult HierarchyEdit_Collections(OpenLevelFile *file, std::vector<model::ShapeSetCollection> *list);

	// Create: *number (when not null) gets the new collection's number
	HierarchyEditResult HierarchyEdit_CreateCollection(OpenLevelFile *file, std::string const &name, int32_t *number, char const **why);
	HierarchyEditResult HierarchyEdit_RenameCollection(OpenLevelFile *file, int32_t number, std::string const &newName, char const **why);
	HierarchyEditResult HierarchyEdit_DeleteCollection(OpenLevelFile *file, int32_t number, char const **why);
	HierarchyEditResult HierarchyEdit_SetCollectionVoiceIndex(OpenLevelFile *file, int32_t number, int32_t voiceIndex, char const **why);

	// The level's alternate shape sets (OBHALTSH, children of the level chunk like the collections), in file order: each one's number and name
	HierarchyEditResult HierarchyEdit_AlternateSets(OpenLevelFile *file, std::vector<model::AlternateSetInfo> *sets);

	// An alternate shape set's flag word (OBHALTSH: after the number, the name, the pairs of names; the game's bit 1 is "Female"), by the set's number. Change: the new word is
	// (old | setMask) & ~clearMask; *flags (when not null) is the word the set holds now.
	HierarchyEditResult HierarchyEdit_ReadShapeSetFlags(OpenLevelFile *file, int32_t setNumber, uint32_t *flags);
	HierarchyEditResult HierarchyEdit_ChangeShapeSetFlags(OpenLevelFile *file, int32_t setNumber, uint32_t setMask, uint32_t clearMask, uint32_t *flags);

	// Add to / Remove from collection (0x55, 0x56): the rules are model::AddShapeSet / RemoveShapeSet (an offered set only; the first match leaves)
	HierarchyEditResult HierarchyEdit_AddShapeSet(OpenLevelFile *file, int32_t collectionNumber, int32_t setNumber, char const **why);
	HierarchyEditResult HierarchyEdit_RemoveShapeSet(OpenLevelFile *file, int32_t collectionNumber, int32_t setNumber, char const **why);

	// A sequence's platform speed and angle: the hierarchy called `hierarchy`, its sequence number `sequence` (0 is the first OBANSEQC), the state's last speed and angle
	// written as FUN_0047db10 does (the chunk made when there is none, its spare left). *written (when not null) is the chunk's numbers now.
	HierarchyEditResult HierarchyEdit_SetPlatform(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, model::PlatformState const &state,
		model::SequenceSpeed *written);

	// A sequence's flag word (its OBASEQFL chunk: Upper 1, Lower 2, No Interpolation 0x10, Half Frame Rate 0x20 in the game's names): the new flags are (old | setMask) &
	// ~clearMask, the old being 0 when the sequence has no chunk, which is then made (inteng's FUN_0047dc20 / FUN_0047dc50 and FUN_00407e10). *flags is the word it holds now.
	HierarchyEditResult HierarchyEdit_ChangeSequenceFlags(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, uint32_t setMask, uint32_t clearMask, uint32_t *flags);

	// The sequence's flag word, or false when it has no OBASEQFL chunk (0 then)
	HierarchyEditResult HierarchyEdit_ReadSequenceFlags(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, uint32_t *flags, bool *present);

	// The sequence's speed and angle, or false when it has no OBASEQSP chunk (*out all 0 then)
	HierarchyEditResult HierarchyEdit_ReadPlatform(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, model::SequenceSpeed *out, bool *present);

	// ---- Frame flags, frame sounds and the sound list (Hierarchy mode: Add / Remove keyframe flag 0x15-0x36, Apply / Clear frame sound 0x50 / 0x51, New / Edit / Delete sound
	// 0x4d / 0x4e / 0x4f; inteng's FUN_004789a0, FUN_00478a30, FUN_00479de0, FUN_00479e90, FUN_00481440, FUN_004816f0, FUN_00481620, FUN_004ebc80, FUN_004ec1b0)
	//
	// A frame's flag word: bits 0-23 the keyframe flags (0 Blocking, 1 Cause Damage, 2 Can Be Blocked, 3 Invincible, 4 Left Step, 5 Right Step, 6 Hit Floor Thud, 7 Hit Floor
	// Sound, 8 Metal Chink, 9 Jump Now, 10 Combat Roll Left, 11 Combat Roll Right, 12 Swipe Sound, 13 Start Pushing, 14 Stop Pushing, 15 Start Moving, 16 Stop Moving), bits 24-30
	// the frame's sound index (0 = none; a sound's number from the sound list), bit 31 the delta frame flag. Inteng changes only the frames of the hierarchy's ROOT section (its
	// root object's frame list): the one frame of the chosen sequence whose time range holds the current position (a frame runs from its own start time to the next frame's start;
	// the last to 65536). The frames are the root section's OBANALLS (stock models) or its OBANSEQS > OBANSEQC > OBASEQFR chunks.
	struct FrameInfo
	{
		int32_t at;       // the frame's start time, 0 to 65535
		uint32_t flags;   // the frame's flag word
	};

	// The root section's frames of the sequence with that number and sub-number, in file order
	HierarchyEditResult HierarchyEdit_RootFrames(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, std::vector<FrameInfo> *frames);

	// The frame that holds position: the new flags are (old | setMask) & ~clearMask, then the sound index is put in bits 24-30 when sound is 0 to 127 (-1 = leave it; a number
	// outside -1 to 127 is refused). *frame (when not null) is the frame's place in the list. NoSequence when the sequence or a frame holding position is missing.
	HierarchyEditResult HierarchyEdit_ChangeFrameAt(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, int32_t position, uint32_t setMask,
		uint32_t clearMask, int32_t sound, size_t *frame);

	// Ctrl+R (0x39, FUN_00479590 -> FUN_0047f720 -> FUN_00480eb0): every frame of the ROOT section's sequence turned half round the vertical, as inteng turns the frames' chunks: the
	// orientation's floats (x, y, z, w) become (-z, -w, x, y) and the transform's x and z are negated
	HierarchyEditResult HierarchyEdit_TurnRootFrames(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber);

	// Ctrl+F's part for one sequence (FUN_004795d0 0x479656-0x4796b2): every frame of the ROOT section's sequence has `by` taken off its transform's y (the sequence's lowest point at its
	// start, in the file's unit), so the sequence stands on the floor
	HierarchyEditResult HierarchyEdit_LowerRootFrames(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, int32_t by);

	// One sound of the hierarchy's sound list (an INDSOUND chunk of the file; the game plays the sound a frame names by its index)
	struct SoundInfo
	{
		int32_t index;      // 1 to 127
		std::string wav;    // the sound's file name ("Not Set" for a new sound)
		int32_t inner;      // inner range, in the file's millimetres
		int32_t outer;      // outer range
		int32_t volume;     // 0 to 127
		int32_t pitch;      // in 1/1536ths of an octave
		uint32_t flags;     // bit 0 loops
	};

	HierarchyEditResult HierarchyEdit_Sounds(OpenLevelFile *file, std::vector<SoundInfo> *sounds);

	// New sound: index 1 to 127, not used; made with inteng's start values (inner 500000 and outer 4000000 of its own unit, which are the file's divided by 100 x levelScale; volume
	// 127, pitch 0, no loop, the name "Not Set"), at the end of the file. why gets the rule's words on Refused.
	HierarchyEditResult HierarchyEdit_NewSound(OpenLevelFile *file, int32_t index, double levelScale, char const **why);

	// Edit sound: the sound's name, ranges, volume and pitch are set, and its loop bit is then cleared (inteng's FUN_004816f0 clears it after every edit)
	HierarchyEditResult HierarchyEdit_EditSound(OpenLevelFile *file, int32_t index, std::string const &wav, int32_t inner, int32_t outer, int32_t volume, int32_t pitch, char const **why);

	// Delete sound: the sound's chunk goes; the frames that name it are left as they are (inteng does not look at them)
	HierarchyEditResult HierarchyEdit_DeleteSound(OpenLevelFile *file, int32_t index, char const **why);

	// A sequence's time in milliseconds (its OBASEQTM chunk, 4 bytes; inteng's sequence object starts at 1000 and takes the chunk's number when it is above 0: FUN_0047d6f0
	// 0x47d982-0x47d99c). present = false when there is no chunk.
	HierarchyEditResult HierarchyEdit_ReadSequenceTime(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, int32_t *milliseconds, bool *present);

	// "Save changes to sequence times" (FUN_0047da50): the time is put in the sequence's OBASEQTM chunk, which is made (at the end of the sequence) when there is none. A time
	// below 1 is refused.
	HierarchyEditResult HierarchyEdit_SetSequenceTime(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, int32_t milliseconds);

	// ---- Change object for section (0x5e, FUN_0048df60): a section of a hierarchy is its OBJHIERD chunk (the section's object NAME: extra numbers, then the name); the command puts
	// another object's name in it (the chooser lists the level's objects bar the light kind "L?#..."; a name another section of the hierarchy already has is refused: "That object is
	// already used by this hierarchy"; the level is marked changed).
	// The object names of the hierarchy's sections, in tree order (the section's chunk first, then its children's)
	HierarchyEditResult HierarchyEdit_SectionObjects(OpenLevelFile *file, std::string const &hierarchy, std::vector<std::string> *names);

	// The level's objects' names (its RBOBJECT chunks, in order)
	HierarchyEditResult HierarchyEdit_ObjectNames(OpenLevelFile *file, std::vector<std::string> *names);

	// The section whose object is oldName now uses the object newName. why gets the words on Refused. NAMED POINT: any name is taken (inteng's chooser only offers the level's objects; its typed-name branch is not seen to check, and stock sections such as "Root" or "dum flash" name no object: the game then finds none for them).
	HierarchyEditResult HierarchyEdit_ChangeSectionObject(OpenLevelFile *file, std::string const &hierarchy, std::string const &oldName, std::string const &newName, char const **why);

	// Copy between hierarchies (0x5b, FUN_0048d8b0 / FUN_0048dc30): for every sequence of `from` that `to` also has (the same number and sub-number), the sounds the root section's frames
	// of the first name are put on the frames of the second that start at the same time (only frames that name a sound: sound 0 changes nothing). A pair whose frame counts differ is not
	// copied and is named in *errors ("<number>.<sub>: the first has N frames, the second M"); *copied counts the sequences done. Refused when the two names are the same.
	HierarchyEditResult HierarchyEdit_CopyFrameSounds(OpenLevelFile *file, std::string const &from, std::string const &to, size_t *copied, std::vector<std::string> *errors);

	// ---- Delta sequences (Hierarchy menu "Not a delta sequence" 0x11, "Deltas relative to first frame" 0x12, "... middle frame" 0x13, "... current frame" 0x14; inteng's FUN_0047eee0 with
	// FUN_004808e0 / FUN_00480950). A delta sequence is an ordinary sequence in which ONE frame of each section carries the frame flag 0x80000000: the game's loader (projload.cpp) turns every
	// frame of such a sequence into a change from that frame (quaternion times the inverse, offset minus its offset). The command clears the flag from the frames of the sequence in EVERY
	// section of the hierarchy (inteng recurses over the section tree), then sets it on one frame of each: the first, the middle (frame count / 2) or the frame whose time range holds
	// `position` (the last frame when none does).
	enum class DeltaMode
	{
		None,       // 0x11: no frame is the reference (the sequence is not a delta)
		First,      // 0x12
		Middle,     // 0x13: see the quirk below
		Current,    // 0x14
	};

	// *sections gets the number of sections that had frames for the sequence. NoSequence when no section has them.
	HierarchyEditResult HierarchyEdit_SetDelta(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, DeltaMode mode, int32_t position, size_t *sections);

	// The sequence's reference frame in the root section: its place in the frame list, or -1 when no frame carries the flag (the sequence is not a delta)
	HierarchyEditResult HierarchyEdit_DeltaFrame(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, int32_t *frame);

	// ---- Calculate Bounding Box (0x46, FUN_004796f0): a HIERBBOX chunk (24 bytes: the box's minimum x y z then maximum x y z, whole numbers) in each sequence's OBANSEQC (the box of the whole
	// hierarchy over that sequence's frames) and one in the hierarchy's own chunk (over all its sequences); made when missing. inteng works the boxes out by playing every sequence through its
	// keyframes with its own pose (hierarchy_player.h does that with the game's: HierarchyPlayer::Box); this writes them.
	// sequence = the place of the sequence in the hierarchy's list, or -1 for the hierarchy's own chunk.
	HierarchyEditResult HierarchyEdit_WriteBoundingBox(OpenLevelFile *file, std::string const &hierarchy, int sequence, int32_t const minimum[3], int32_t const maximum[3]);

	// The box a sequence (sequence >= 0) or the hierarchy (-1) holds, or present = false
	HierarchyEditResult HierarchyEdit_ReadBoundingBox(OpenLevelFile *file, std::string const &hierarchy, int sequence, int32_t minimum[3], int32_t maximum[3], bool *present);

	// ---- Attach shape to bones (Hierarchy menu 0x62, inteng's FUN_0048e610 -> FUN_0041f4d0 / FUN_0041edb0 / FUN_0041f2b0 / FUN_0041f8c0). Every point of an object's shape is connected to the ONE
	// section ("bone") of a hierarchy nearest to it in the hierarchy's REFERENCE pose, the one-frame sequence numbered (10000, 0) of every section. The result is a SHPBONHD chunk at the end of
	// the object's shape chunk: a SHPBONIN (the reference sequence's number and sub-number, the shape's point count, the hierarchy's name) and one SHPBONE (holding a SHPBONDT: the bone's name,
	// then one 5-number record per point: the point's number, three numbers (inteng leaves them unset; zeros here) and the weight 1.0 as a float) for every bone that got points. The game
	// does not read these chunks (avpx has no class for them) and no stock file holds one: what is READ and what is ASSUMED is in AvP-docs\engine\inteng\hierarchy-data-model.md.
	struct BoneAttachment
	{
		bool present = false;
		int32_t number = 0, subNumber = 0, points = 0;
		std::vector<std::pair<std::string, std::vector<int32_t>>> bones;   // each bone's name and the points it holds, in the file's order
		std::vector<std::vector<std::array<int32_t, 3>>> offsets;         // each of those points' place relative to its bone (the record's +4..+0xc: FUN_0041eae0), in the same order
	};

	// The attachment of the object's shape to the hierarchy (present = false when there is none)
	HierarchyEditResult HierarchyEdit_ReadBoneAttachment(OpenLevelFile *file, std::string const &objectName, std::string const &hierarchy, BoneAttachment *out);

	// Attaches (again). Refused, with *why "The shape is already attached to the hierarchy", when the shape holds an attachment to it of the same point count and redetermine is false (inteng asks
	// "Do you want to redetermine the vertex connections anyway?"); Refused with "The hierarchy doesn't have the reference sequence" when a section lacks a one-frame sequence (number, sub).
	// *attachedPoints (when not null) gets the number of points that were connected.
	HierarchyEditResult HierarchyEdit_AttachToBones(OpenLevelFile *file, std::string const &objectName, std::string const &hierarchy, int32_t number, int32_t subNumber, bool redetermine, char const **why,
		size_t *attachedPoints);

	// The points of the object's shape where the attachment sees them: turned and moved by the object's own turn and place (inteng's FUN_0041f4d0 does this before it picks the nearest bone), three whole
	// numbers each, in the hierarchy's space. The Hierarchy view's vertex selection (V) shows them at the model's place.
	HierarchyEditResult HierarchyEdit_SkinPoints(OpenLevelFile *file, std::string const &objectName, std::vector<int32_t> *points);

	// The polygons of that shape (its SHPPOLYS), four point numbers each (the fourth -1 for a triangle; a number of no point ends a polygon, one of fewer than three is left out): what the
	// vertex selection's click must be on (FUN_00485290: FUN_00559720 gives the object under the pointer by its polygons; vertex_pick.h). Vasquez 2026-11-17.
	HierarchyEditResult HierarchyEdit_SkinFaces(OpenLevelFile *file, std::string const &objectName, std::vector<int32_t> *faces);

	// Shift + V (Add selected vertices to selected bone, inteng's FUN_0047acf0 -> FUN_0041f8c0): each point is taken out of the bone that holds it and put at the end of that bone's points (the bone is made
	// when the shape's attachment has none of that name). Refused, with *why, when the shape is not attached to the hierarchy; BadCall for a point the shape does not have or a bone the hierarchy has no section of.
	// *moved gets how many points changed bone (one already in that bone stays where it is and is not counted).
	HierarchyEditResult HierarchyEdit_MovePointsToBone(OpenLevelFile *file, std::string const &objectName, std::string const &hierarchy, std::string const &bone, std::vector<int32_t> const &points, char const **why, size_t *moved);

	// A sequence's number and sub-number changed everywhere the hierarchy holds them (its OBASEQHD, and the OBANALLS / OBASEQHD of every section): used to give a one-frame sequence the reference
	// numbers (10000, 0) that Attach shape to bones needs. *changed gets how many places were changed. Refused when the hierarchy already has the new numbers.
	HierarchyEditResult HierarchyEdit_RenumberSequence(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, int32_t newNumber, int32_t newSubNumber, size_t *changed);

	// The detail-level distances of the model (HIDEGDIS, a child of the file's root: a count of 10 and then 10 whole numbers, in mm; the game's projload.cpp:1590 wants exactly 10). *present is false when the
	// model has none; the distances are then the game's own default, level * 10000 (hierarchy_chunk.cpp:589).
	HierarchyEditResult HierarchyEdit_ReadDegradation(OpenLevelFile *file, int32_t distances[10], bool *present);

	// Writes them (the chunk is made at the end of the root when missing). Level 0 is the closest-up shape: a distance never goes below the one of a lower level (inteng's FUN_00479a80 keeps them in order).
	HierarchyEditResult HierarchyEdit_WriteDegradation(OpenLevelFile *file, int32_t const distances[10]);
}
