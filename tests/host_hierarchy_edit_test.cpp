// The test of Hierarchy-mode's data edits: Copy Hierarchy, the shape set collection commands, a sequence's platform speed and angle (src/host/hierarchy_edit.cpp).
// Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT TESTS, on the game's own level files, which are given on the command line (the models in avp_huds hold hierarchies). The files are only read: every edit is
// made in memory (OpenLevelFile::ReplaceLevel) and the one save goes to a folder of the test's own.
// - Create, rename, set the voice index of and delete a collection: the collection list changes exactly as the model's rule says, a create followed by a delete gives
//   back the very bytes the level had, and every refusal leaves the level's bytes and Generation() as they were.
// - Copy Hierarchy: the copy is a hierarchy of its own with the new name and the source's bytes; an empty name, a name in use (either case), an unknown source and
//   a name with a control character are refused with nothing changed.
// - Platform speed and angle: written to a sequence and read back; a sequence with no OBASEQSP gets one; a sequence number or hierarchy that is not there is refused.
// - A level edited this way is SAVED, and a second OpenLevelFile reads the saved file through the game's own loader and finds the edits.
//
// HOW IT RUNS: tests/host_hierarchy_edit_test.args builds it; run it as host_hierarchy_edit_test <level file> [<level file> ...]. It gives back a non-zero result if any
// check failed.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "frame_sound_index.h"
#include "hierarchy_edit.h"
#include "hierarchy_mode.h"

static int checksFailed = 0;

// TB-3 (FUN_0041eae0): a point's SHPBONDT offset is its place in its bone's own frame in the reference pose (the world point less the bone's place, turned by the bone's matrix transposed). A turn and a
// move keep distances, so two points of one bone are as far apart in their offsets as in the world (HierarchyEdit_SkinPoints), each number cut toward zero: within 4 mm. Offsets left at 0, 0, 0 are not.
// *pairs gets how many pairs were compared (neighbours in each bone's list, at most 200 a bone, both points apart by more than 8 mm in the world).
static bool OffsetsKeepDistances(host::BoneAttachment const &attached, std::vector<int32_t> const &world, size_t *pairs, std::string *first)
{
	*pairs = 0;

	if (attached.offsets.size() != attached.bones.size())
	{
		*first = "the offsets are not one list a bone";
		return false;
	}

	for (size_t b = 0; b < attached.bones.size(); b++)
	{
		std::vector<int32_t> const &points = attached.bones[b].second;

		if (attached.offsets[b].size() != points.size())
		{
			*first = attached.bones[b].first + ": not one offset a point";
			return false;
		}

		for (size_t k = 1, compared = 0; k < points.size() && compared < 200; k++)
		{
			size_t const i = (size_t)points[k - 1], j = (size_t)points[k];

			if (i * 3 + 2 >= world.size() || j * 3 + 2 >= world.size())
			{
				*first = attached.bones[b].first + ": a point past the shape's";
				return false;
			}

			double worldSquared = 0.0, offsetSquared = 0.0;

			for (int axis = 0; axis < 3; axis++)
			{
				double const w = (double)world[i * 3 + axis] - (double)world[j * 3 + axis];
				double const o = (double)attached.offsets[b][k - 1][axis] - (double)attached.offsets[b][k][axis];

				worldSquared += w * w;
				offsetSquared += o * o;
			}

			if (sqrt(worldSquared) <= 8.0)
			{
				continue;
			}

			compared++;
			(*pairs)++;

			if (fabs(sqrt(worldSquared) - sqrt(offsetSquared)) > 4.0)
			{
				*first = attached.bones[b].first + ": points " + std::to_string((long long)i) + " and " + std::to_string((long long)j) + " are " + std::to_string((long long)sqrt(worldSquared)) +
					" mm apart, their offsets " + std::to_string((long long)sqrt(offsetSquared));
				return false;
			}
		}
	}

	return true;
}
static int checksRun = 0;

static void Check(bool good, std::string const & what)
{
	checksRun++;

	if (!good)
	{
		printf("  FAILED  %s\n", what.c_str());
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls (as tests/level_file_test.cpp)

HANDLE avp_CreateFile(const char * name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char * name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char * message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

using namespace host;

typedef std::vector<char> Bytes;

static Bytes Built(OpenLevelFile * file)
{
	Bytes bytes;

	if (!file->BuildLevel(bytes))
	{
		bytes.clear();
	}

	return bytes;
}

static std::string Words(HierarchyEditResult result)
{
	return HierarchyEditWords(result);
}


// The shape sets the collection with this number holds, as the file has them now (none when it isn't there)
static std::vector<int32_t> CollectionSets(OpenLevelFile *file, int32_t number)
{
	std::vector<model::ShapeSetCollection> all;

	HierarchyEdit_Collections(file, &all);

	for (model::ShapeSetCollection const &one : all)
	{
		if (one.number == number)
		{
			return one.shapeSets;
		}
	}

	return std::vector<int32_t>();
}

// The commands as the editor runs them: ask, type, answer
static HierarchyStep Run(OpenLevelFile * file, HierarchyMode * mode, int32_t command, std::string const & typed)
{
	HierarchyStep step = HierarchyMode_Command(file, mode, command);

	if (step.prompt)
	{
		step = HierarchyMode_Typed(file, mode, typed);
	}

	return step;
}

static void TestMode(OpenLevelFile * file, std::vector<std::string> const & names)
{
	HierarchyMode mode;
	Bytes const before = Built(file);

	// nothing selected yet
	Check(Run(file, &mode, 0x53, "x").answer == "Please select a hierarchy first" && Built(file) == before, "Create Collection with no hierarchy selected says so, nothing changed");
	Check(Run(file, &mode, 0x57, "x").answer == "Please select a shape set collection first" && Built(file) == before, "Rename with no collection selected says so");
	Check(Run(file, &mode, 0x4c, "").answer == "Platform: on" && mode.platformOn, "Toggle on/off switches the platform on");

	// a file with one hierarchy opens it at once and says so; with two or more the typed place picks it, and Select Sequence follows at once (FUN_0048b550's last call, 0x48b83a)
	HierarchyStep const picked = Run(file, &mode, 0x03, "1");

	Check((picked.answer.find("selected") != std::string::npos || picked.prompt) && mode.hierarchy == names.front(), "Select Hierarchy by its place");

	// Create asks again for an empty name and for a name in use, then makes the collection and selects it
	HierarchyStep step = HierarchyMode_Command(file, &mode, 0x53);
	Check(step.prompt && step.title == "Name of new shape collection", "Create Collection asks for a name");
	step = HierarchyMode_Typed(file, &mode, "");
	Check(step.prompt && Built(file) == before, "an empty name is asked for again");
	step = HierarchyMode_Typed(file, &mode, "Mode test collection");
	Check(!step.prompt && mode.collectionSelected, "the collection is made and selected: " + step.answer);

	step = HierarchyMode_Command(file, &mode, 0x53);
	step = HierarchyMode_Typed(file, &mode, "MODE TEST COLLECTION");
	Check(step.prompt && step.messageBox && step.answer == "That name is already used" && step.title == "Name of new shape collection" && step.textDialog, "a name in use (other letters): inteng's message box, then asked again");
	HierarchyMode_Cancel(&mode);

	Check(Run(file, &mode, 0x59, "12").answer == "Voice index set to 12", "Collection Voice Index");
	Check(Run(file, &mode, 0x59, "twelve").answer == "Voice index set to 0", "a voice index that is not a number is 0, as inteng's edit gives it (FUN_0057d620)");
	Check(Run(file, &mode, 0x57, "Mode test renamed").answer.find("renamed") != std::string::npos, "Rename Collection");

	// Add and Remove, when the file has alternate shape sets. inteng says nothing after either (the Hierarchy screen has no status line): the answer is empty, and what changed is read back
	std::vector<model::AlternateSetInfo> alternates;

	HierarchyEdit_AlternateSets(file, &alternates);

	if (!alternates.empty())
	{
		Check(Run(file, &mode, 0x56, "1").answer == "This shape collection contains no alternate shape sets", "Remove from an empty collection says so");

		// Add to Collection (FUN_00479f20, hierarchy-edit-frost.md HF-0x55): the sets NOT applied, by name, under "Add which shape set?" (0x5d4820); a row: that set's number joins the
		// collection's list (0x47a2d9-0x47a2e7) and the applied list (FUN_004827e0); no words
		HierarchyStep step = HierarchyMode_Command(file, &mode, 0x55);
		int32_t const offered = mode.listValues.empty() ? -1 : mode.listValues.front();

		Check(step.prompt && step.title == "Add which shape set?" && mode.list.size() == alternates.size() && offered >= 0,
			"Add to Collection lists every set (none is applied yet) under inteng's title");
		step = HierarchyMode_Typed(file, &mode, "1");
		Check(!step.prompt && step.answer.empty() && CollectionSets(file, mode.collection) == std::vector<int32_t>{ offered } && mode.applied == std::vector<int32_t>{ offered },
			"Add to Collection: the row's set joins the collection and the applied list, with no words");

		// Remove from Collection (FUN_0047a380, HF-0x56): the applied sets by name, "Remove which shape set?" (0x5d4890); a row: its number leaves the collection's list (0x47a675-0x47a6f0)
		// and the applied list (FUN_004fc260); no words
		step = Run(file, &mode, 0x56, "1");
		Check(!step.prompt && step.answer.empty() && CollectionSets(file, mode.collection).empty() && mode.applied.empty(), "Remove from Collection: the set leaves the collection and the applied list, with no words");
	}

	// Select Single Set and Set Properties (the Female flag), when the file has alternate shape sets
	if (!alternates.empty())
	{
		Check(Run(file, &mode, 0x58, "yes").answer == "Please select a single shape set" || mode.applied.size() == 1, "Set Properties with no single set applied says so");

		// Select Single Set (case 0x43, HF-0x43): the rows "Original" (no set) and each set's name, under "Select Shape Set" (0x5d5710); a row: FUN_00477b40(that set): the applied list is
		// that set alone and the collection is LET GO (+0x38 = 0); no words
		HierarchyStep step = HierarchyMode_Command(file, &mode, 0x43);

		Check(step.prompt && step.title == "Select Shape Set" && mode.list.size() == alternates.size() + 1 && mode.list.front() == "1  Original",
			"Select Single Set lists \"Original\" and each set by name under inteng's title");
		step = HierarchyMode_Typed(file, &mode, "2");
		Check(!step.prompt && step.answer.empty() && mode.applied.size() == 1 && mode.applied.front() == alternates.front().number && !mode.collectionSelected,
			"Select Single Set applies one shape set and lets the collection go (FUN_00477b40), with no words");

		uint32_t flagsOld = 0;
		uint32_t flagsNew = 0;

		HierarchyEdit_ReadShapeSetFlags(file, mode.applied.front(), &flagsOld);

		// Set Properties (case 0x58, HF-0x58): ONE set applied: its window FUN_0048d6a0 "Shape set properties", the Female box holding the set's flags bit 0; Ok with the box ticked or not sets or
		// clears bit 0 (FUN_0048d820 0x48d864-0x48d897); no words. The editor's window gives the box as "yes" / "no"
		step = HierarchyMode_Command(file, &mode, 0x58);
		Check(step.prompt && step.femaleForm && step.title == "Shape set properties" && step.femaleChecked == ((flagsOld & 1) != 0), "Set Properties opens the set's window, its Female box as the flag is");
		step = HierarchyMode_Typed(file, &mode, "yes");
		Check(!step.prompt && step.answer.empty(), "Set Properties: the Female box ticked and Ok, with no words");
		HierarchyEdit_ReadShapeSetFlags(file, mode.applied.front(), &flagsNew);
		Check(flagsNew == (flagsOld | 1), "the shape set's flag word has bit 1 and the rest as it was");
		Check(Run(file, &mode, 0x58, "maybe").answer.find("nothing changed") != std::string::npos, "Set Properties with neither yes nor no changes nothing");
		Check(Run(file, &mode, 0x58, "no").answer.empty(), "Set Properties: the Female box not ticked and Ok, with no words");
		HierarchyEdit_ReadShapeSetFlags(file, mode.applied.front(), &flagsNew);
		Check(flagsNew == (flagsOld & ~1u), "bit 1 is clear again");
		Check(Run(file, &mode, 0x43, "1").answer.empty() && mode.applied.empty(), "Select Single Set: Original applies none, with no words");

		// Select Single Set let the collection go: Select Collection (case 0x52, FUN_0048abc0, HF-0x52) takes it back by its name, with no words
		Check(Run(file, &mode, 0x52, "Mode test renamed").answer.empty() && mode.collectionSelected, "Select Collection takes the collection back, with no words");
	}

	Check(Run(file, &mode, 0x54, "no").answer == "Delete Collection: nothing deleted" && mode.collectionSelected, "Delete Collection with a no keeps it");
	Check(Run(file, &mode, 0x54, "yes").answer == "Shape set collection deleted" && !mode.collectionSelected, "Delete Collection with a yes deletes it");
	Check(Built(file) == before, "the level is as it was after all that");

	// Platform: written only with a sequence selected and the switch on
	std::vector<SequenceInfo> sequences;

	if (HierarchyEdit_Sequences(file, mode.hierarchy, &sequences) == HierarchyEditResult::Done && !sequences.empty())
	{
		Check(Run(file, &mode, 0x4a, "-5").answer.empty() && mode.platform.lastSpeed == 3000, "a negative speed is not taken (case 0x4a: nothing, no words)");
		Check(Run(file, &mode, 0x4b, "361").answer.empty(), "an angle above 360 is not taken (case 0x4b: nothing, no words)");
		Check(Run(file, &mode, 0x4a, "1500").answer.find("kept for the next write") != std::string::npos && Built(file) == before, "with no sequence selected the speed is kept, not written");

		Check(Run(file, &mode, 0x04, "1").answer.find("Sequence 1 selected") == 0, "Select Sequence");

		// keyframe flags and frame sounds through the commands, on the first frame (position 0)
		{
			Bytes const beforeFrames = Built(file);
			std::vector<FrameInfo> frames;

			if (HierarchyEdit_RootFrames(file, mode.hierarchy, sequences[0].number, sequences[0].subNumber, &frames) == HierarchyEditResult::Done && !frames.empty())
			{
				mode.position = 0;

				uint32_t const oldFlags = frames[0].flags;
				int bit = 0;

				while (bit < 16 && (oldFlags & (1u << bit)) != 0)
				{
					bit++;
				}

				Check(Run(file, &mode, 0x15 + bit, "").answer.find("Add keyframe flag " + std::to_string(bit)) == 0, "Add keyframe flag " + std::to_string(bit) + " by its command");

				HierarchyEdit_RootFrames(file, mode.hierarchy, sequences[0].number, sequences[0].subNumber, &frames);
				Check((frames[0].flags & (1u << bit)) != 0, "frame 1 holds the flag");
				Check(Run(file, &mode, 0x26 + bit, "").answer.find("Remove keyframe flag " + std::to_string(bit)) == 0, "Remove keyframe flag " + std::to_string(bit) + " by its command");
				std::vector<SoundInfo> stock;

				HierarchyEdit_Sounds(file, &stock);

				if (stock.empty())
				{
					Check(Run(file, &mode, 0x4f, "1").answer == "Please add a sound first", "Delete sound with no sounds says so");
				}

				std::vector<int32_t> used;

				for (SoundInfo const & each : stock)
				{
					used.push_back(each.index);
				}

				int32_t const mine = model::FirstFreeSoundIndex(used);

				HierarchyStep step = HierarchyMode_Command(file, &mode, 0x4d);

				Check(step.prompt && step.textDialog && step.title == "Select sound index" && step.defaultText == std::to_string(mine), "New sound asks for the first free index in a text dialog");
				step = HierarchyMode_Typed(file, &mode, "0");
				Check(!step.prompt && step.messageBox && step.answer == "Index must be between 1 and 127", "index 0 is the message box, and nothing more (FUN_00481440)");
				HierarchyMode_Command(file, &mode, 0x4d);
				step = HierarchyMode_Typed(file, &mode, std::to_string(mine));
				Check(!step.prompt && step.soundForm && step.answer.find("Sound " + std::to_string(mine) + " added") == 0 && mode.soundBeingEdited.wav == "Not Set",
					"the free index makes the sound and opens its Sound Edit window at once (FUN_004ebc80 -> FUN_004ebd60): " + step.answer);

				step = HierarchyMode_Command(file, &mode, 0x4e);

				size_t place = 0;

				for (size_t i = 0; i < mode.listValues.size(); i++)
				{
					place = mode.listValues[i] == mine ? i + 1 : place;
				}

				Check(step.prompt && place > 0 && mode.list[place - 1].find("Not Set") != std::string::npos && step.title == "Select sound to edit      (Right click to listen to sound)",
					"Edit sound lists the new sound as Not Set under inteng's title");
				step = HierarchyMode_Typed(file, &mode, std::to_string(place));
				Check(!step.prompt && step.soundForm && mode.soundBeingEdited.index == mine, "a pick opens the sound's Sound Edit window");
				// the window's answers: 2 m and 20 m (the file's 2000 and 20000 at scale 1), 0 semitones, 79 % (volume 100 of 127)
				step = HierarchyMode_SoundForm(file, &mode, { "step.wav", "2", "20", "0", "79" });
				Check(!step.prompt && !step.messageBox && step.answer == "Sound " + std::to_string(mine) + " edited", "the window's OK writes the sound: " + step.answer);

				std::vector<SoundInfo> after;

				HierarchyEdit_Sounds(file, &after);

				for (SoundInfo const &each : after)
				{
					if (each.index == mine)
					{
						Check(each.wav == "step.wav" && each.inner == 2000 && each.outer == 20000 && each.volume == 100 && each.pitch == 0, "the sound holds step.wav, 2000, 20000, volume 100, pitch 0");
					}
				}

				step = HierarchyMode_Command(file, &mode, 0x50);
				step = HierarchyMode_Typed(file, &mode, "step.wav");
				Check(!step.prompt && step.answer.find("Sound " + std::to_string(mine) + " applied to frame 1") == 0, "Apply frame sound puts the new sound on frame 1: " + step.answer);
				HierarchyEdit_RootFrames(file, mode.hierarchy, sequences[0].number, sequences[0].subNumber, &frames);
				Check(((frames[0].flags >> 24) & 0x7f) == (uint32_t)mine, "frame 1 names the new sound");
				Check(Run(file, &mode, 0x51, "").answer.find("Frame sound cleared") == 0, "Clear frame sound");
				HierarchyEdit_RootFrames(file, mode.hierarchy, sequences[0].number, sequences[0].subNumber, &frames);
				Check(((frames[0].flags >> 24) & 0x7f) == 0, "frame 1 names no sound");
				step = HierarchyMode_Command(file, &mode, 0x4f);
				step = HierarchyMode_Typed(file, &mode, "step.wav");
				Check(!step.prompt && step.answer == "Sound " + std::to_string(mine) + " deleted", "Delete sound: " + step.answer);
				// the frame's own sound index, as it was
				HierarchyEdit_ChangeFrameAt(file, mode.hierarchy, sequences[0].number, sequences[0].subNumber, 0, 0, 0, (int32_t)((oldFlags >> 24) & 0x7f), nullptr);
				Check(Built(file) == beforeFrames, "the level is the very bytes it was before the keyframe and sound commands");

				// Ctrl+R (FUN_00480eb0): a half turn changes the root's frames; two turns give the transforms back with every orientation negated (the same turn, other bytes); four give the
				// very bytes back. Ctrl+F's part (FUN_004795d0): lowering by n and then by -n gives the very bytes back.
				Check(HierarchyEdit_TurnRootFrames(file, mode.hierarchy, sequences[0].number, sequences[0].subNumber) == HierarchyEditResult::Done && Built(file) != beforeFrames,
					"Ctrl+R turns the root section's frames");
				HierarchyEdit_TurnRootFrames(file, mode.hierarchy, sequences[0].number, sequences[0].subNumber);
				Check(Built(file) != beforeFrames, "two half turns: the orientations' signs are flipped, so the bytes differ");
				HierarchyEdit_TurnRootFrames(file, mode.hierarchy, sequences[0].number, sequences[0].subNumber);
				HierarchyEdit_TurnRootFrames(file, mode.hierarchy, sequences[0].number, sequences[0].subNumber);
				Check(Built(file) == beforeFrames, "four half turns give the very bytes back");
				Check(HierarchyEdit_LowerRootFrames(file, mode.hierarchy, sequences[0].number, sequences[0].subNumber, 1234) == HierarchyEditResult::Done && Built(file) != beforeFrames,
					"Ctrl+F's lowering changes the root section's frames");
				HierarchyEdit_LowerRootFrames(file, mode.hierarchy, sequences[0].number, sequences[0].subNumber, -1234);
				Check(Built(file) == beforeFrames, "lowering by 1234 and by -1234 gives the very bytes back");
				Check(HierarchyEdit_TurnRootFrames(file, mode.hierarchy, 0x7fffffff, 0x7fffffff) != HierarchyEditResult::Done && Built(file) == beforeFrames,
					"a sequence the root section has not is refused, and nothing changes");
			}
		}

		Check(Run(file, &mode, 0x4b, "270").answer.find("kept for the next write") != std::string::npos && Built(file) == before, "with the platform switch on but Save changes to sequence times off, nothing is written");
		Check(Run(file, &mode, 0x48, "").answer == "Save changes to sequence times: on" && mode.saveTimesOn, "Save changes to sequence times switched on");
		Check(Run(file, &mode, 0x4b, "270").answer.find("written to the sequence") != std::string::npos, "with a sequence selected and the switch on, the angle is written");

		model::SequenceSpeed read = { 0, 0, 0 };
		bool present = false;

		Check(HierarchyEdit_ReadPlatform(file, mode.hierarchy, 0, &read, &present) == HierarchyEditResult::Done && present && read.speedMmPerSecond == 1500 && read.angleDegrees == 270,
			"the sequence holds 1500 mm/s and 270 degrees");

		// Sequence flags
		uint32_t flagsBefore = 0;
		bool hadFlags = false;

		HierarchyEdit_ReadSequenceFlags(file, mode.hierarchy, 0, &flagsBefore, &hadFlags);
		Check(Run(file, &mode, 0x6f, "").answer.find("No Interpolation added") == 0, "No Interpolation added");

		uint32_t flagsNow = 0;
		bool hasFlags = false;

		Check(HierarchyEdit_ReadSequenceFlags(file, mode.hierarchy, 0, &flagsNow, &hasFlags) == HierarchyEditResult::Done && hasFlags && flagsNow == (flagsBefore | 0x10), "the flag word has bit 0x10 now and the rest as it was");
		Check(Run(file, &mode, 0x69, "").answer.find("Sequence Type: Upper") == 0, "Sequence Type: Upper");
		HierarchyEdit_ReadSequenceFlags(file, mode.hierarchy, 0, &flagsNow, &hasFlags);
		Check((flagsNow & 3) == 1 && (flagsNow & 0x10) != 0, "Upper: bit 1 set, bit 2 clear, the other bits kept");
		Check(Run(file, &mode, 0x6a, "").answer.find("Sequence Type: Lower") == 0, "Sequence Type: Lower");
		HierarchyEdit_ReadSequenceFlags(file, mode.hierarchy, 0, &flagsNow, &hasFlags);
		Check((flagsNow & 3) == 2, "Lower: bit 2 set, bit 1 clear");
		Check(Run(file, &mode, 0x68, "").answer.find("Sequence Type: Full") == 0, "Sequence Type: Full");
		HierarchyEdit_ReadSequenceFlags(file, mode.hierarchy, 0, &flagsNow, &hasFlags);
		Check((flagsNow & 3) == 0, "Full: both clear");
		Check(Run(file, &mode, 0x71, "").answer.find("Half Frame Rate added") == 0, "Half Frame Rate added");
		Check(Run(file, &mode, 0x72, "").answer.find("Half Frame Rate removed") == 0, "Half Frame Rate removed");
		Check(Run(file, &mode, 0x70, "").answer.find("No Interpolation removed") == 0, "No Interpolation removed");
		HierarchyEdit_ReadSequenceFlags(file, mode.hierarchy, 0, &flagsNow, &hasFlags);
		Check(flagsNow == (flagsBefore & ~0x33u), "the flag word is what it was, less the bits the commands own");

		Check(Run(file, &mode, 0x4c, "").answer == "Platform: off", "Toggle off");
		Check(Run(file, &mode, 0x4a, "2000").answer.find("kept for the next write") != std::string::npos, "with the switch off the speed is kept, not written");
	}

	// Copy Hierarchy
	Check(Run(file, &mode, 0x49, "").answer.find("no name typed") != std::string::npos, "Copy Hierarchy with an empty name does nothing");
	Check(Run(file, &mode, 0x49, mode.hierarchy).answer == "That name is already in use", "Copy Hierarchy with a name in use is refused: inteng's message box");
	Check(Run(file, &mode, 0x49, "Mode test copy").answer == "Hierarchy copied as \"Mode test copy\"", "Copy Hierarchy");

	std::vector<std::string> after;

	Check(HierarchyEdit_Names(file, &after) == HierarchyEditResult::Done && after.size() == names.size() + 1, "the level has one more hierarchy");
}

// Every check of one level file
static void TestFile(std::string const & path, std::string const & folder)
{
	printf("%s\n", path.c_str());

	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, "the level opens: " + path);
		return;
	}

	Bytes const original = Built(&file);
	Check(!original.empty(), "the level builds");

	std::vector<std::string> names;
	Check(HierarchyEdit_Names(&file, &names) == HierarchyEditResult::Done, "the hierarchy names are read");

	std::vector<model::ShapeSetCollection> collections;
	Check(HierarchyEdit_Collections(&file, &collections) == HierarchyEditResult::Done, "the collections are read");

	// ---- the detail-level distances (HIDEGDIS)
	{
		OpenLevelFile degFile;     // a copy of its own: writing the chunk changes the bytes the other checks compare against
		int32_t distances[10];
		bool present = true;

		degFile.Open(path.c_str());
		Check(HierarchyEdit_ReadDegradation(&degFile, distances, &present) == HierarchyEditResult::Done, "the detail distances are read (" + std::string(present ? "the model has them" : "the game's defaults") + ")");

		int32_t changed[10];

		for (int i = 0; i < 10; i++)
		{
			changed[i] = i * 1234;
		}

		Check(HierarchyEdit_WriteDegradation(&degFile, changed) == HierarchyEditResult::Done, "and written");

		int32_t again[10];

		Check(HierarchyEdit_ReadDegradation(&degFile, again, &present) == HierarchyEditResult::Done && present && memcmp(again, changed, sizeof(again)) == 0, "and read back, the chunk there now");
		Check(HierarchyEdit_WriteDegradation(&degFile, changed) == HierarchyEditResult::Done && HierarchyEdit_ReadDegradation(&degFile, again, &present) == HierarchyEditResult::Done && memcmp(again, changed, sizeof(again)) == 0, "written over itself it stays one chunk");

		HierarchyEdit_WriteDegradation(&degFile, distances);
	}

	// ---- collections
	{
		unsigned const generation = file.Generation();
		char const * why = nullptr;
		int32_t number = -1;

		Check(HierarchyEdit_CreateCollection(&file, "", &number, &why) == HierarchyEditResult::Refused && file.Generation() == generation && Built(&file) == original,
			"an empty collection name is refused, nothing changed");

		std::string const fresh = "Zz new collection";

		Check(HierarchyEdit_CreateCollection(&file, fresh, &number, &why) == HierarchyEditResult::Done, "a collection is created");

		std::vector<model::ShapeSetCollection> after;
		Check(HierarchyEdit_Collections(&file, &after) == HierarchyEditResult::Done && after.size() == collections.size() + 1, "the list has one more");

		if (!after.empty())
		{
			model::ShapeSetCollection const & last = after.back();
			Check(last.name == fresh && last.number == number && last.shapeSets.empty() && last.voiceIndex == 0 && last.flags == 0, "the new one is last, empty, voice 0, flags 0");

			int32_t smallest = 2;

			for (bool again = true; again; )
			{
				again = false;

				for (size_t i = 0; i + 1 < after.size(); i++)
				{
					if (after[i].number == smallest)
					{
						smallest++;
						again = true;
					}
				}
			}

			Check(number == smallest, "its number is the smallest from 2 that no other has");
		}

		unsigned const generationNow = file.Generation();
		Bytes const withNew = Built(&file);

		Check(HierarchyEdit_CreateCollection(&file, "ZZ NEW COLLECTION", nullptr, &why) == HierarchyEditResult::Refused && file.Generation() == generationNow && Built(&file) == withNew,
			"the same name in other letters is refused, nothing changed");

		Check(HierarchyEdit_RenameCollection(&file, number, "Zz renamed longer name", &why) == HierarchyEditResult::Done, "a collection is renamed");
		Check(HierarchyEdit_SetCollectionVoiceIndex(&file, number, 7, &why) == HierarchyEditResult::Done, "its voice index is set");

		after.clear();
		Check(HierarchyEdit_Collections(&file, &after) == HierarchyEditResult::Done && !after.empty() && after.back().name == "Zz renamed longer name" && after.back().voiceIndex == 7,
			"the new name and voice index are read back");

		Check(HierarchyEdit_RenameCollection(&file, 4095 + 1, "x", &why) == HierarchyEditResult::NoCollection, "a number no collection has is refused");

		// Add to / Remove from collection, when the file has alternate shape sets
		std::vector<model::AlternateSetInfo> alternates;

		Check(HierarchyEdit_AlternateSets(&file, &alternates) == HierarchyEditResult::Done, "the alternate shape sets are read");

		if (!alternates.empty())
		{
			int32_t const setNumber = alternates.front().number;

			Check(HierarchyEdit_RemoveShapeSet(&file, number, setNumber, &why) == HierarchyEditResult::Refused, "Remove from an empty collection is refused");
			Check(HierarchyEdit_AddShapeSet(&file, number, setNumber, &why) == HierarchyEditResult::Done, "a shape set is added to the collection");
			Check(HierarchyEdit_AddShapeSet(&file, number, setNumber, &why) == HierarchyEditResult::Refused, "the same shape set is not added twice");
			Check(HierarchyEdit_AddShapeSet(&file, number, 987654321, &why) == HierarchyEditResult::Refused, "a shape set the level does not have is refused");

			after.clear();
			Check(HierarchyEdit_Collections(&file, &after) == HierarchyEditResult::Done && !after.empty() && after.back().shapeSets == std::vector<int32_t>{ setNumber }, "the collection holds it");
			Check(HierarchyEdit_RemoveShapeSet(&file, number, setNumber, &why) == HierarchyEditResult::Done, "the shape set is removed");
		}

		Check(HierarchyEdit_DeleteCollection(&file, number, &why) == HierarchyEditResult::Done, "the collection is deleted");
		Check(Built(&file) == original, "create, rename, voice index and delete give back the very bytes the level had");
	}

	// ---- copying hierarchies and platform speeds need a hierarchy
	if (names.empty())
	{
		// a file with no hierarchy (a level): Select Hierarchy (FUN_0048b8a0) finds no OBJCHIER in the OPENED file, so nothing is shown and nothing changes (the typed model names of before are
		// gone: inteng's hierarchies are the opened file's own, reassess/hierarchy.md)
		HierarchyMode noneMode;
		Bytes const untouched = Built(&file);
		HierarchyStep const ask = HierarchyMode_Command(&file, &noneMode, 0x03);

		Check(!ask.prompt && noneMode.list.empty() && noneMode.hierarchy.empty() && Built(&file) == untouched, "Select Hierarchy on a file with no hierarchy shows nothing and changes nothing");
		return;
	}

	{
		// Select Hierarchy (FUN_0048b8a0) on a file with hierarchies: exactly one is opened at once (FUN_0048b550(it, 0), which goes on to Select Sequence), two or more are offered in the list
		// dialog "Select Hierarchy"
		OpenLevelFile pickFile;

		if (pickFile.Open(path.c_str()) == LevelOpenResult::Opened)
		{
			HierarchyMode pick;
			HierarchyStep const step = HierarchyMode_Command(&pickFile, &pick, 0x03);

			if (names.size() == 1)
			{
				Check(pick.hierarchy == names.front(), "Select Hierarchy with one hierarchy in the file opens it at once");
			}
			else
			{
				Check(step.prompt && pick.list.size() == names.size() && pick.hierarchy.empty(), "Select Hierarchy with two or more hierarchies offers them all in its list");
			}
		}
	}

	{
		// the mode test on a level of its own, so the checks below start from the level as it was read
		OpenLevelFile modeFile;

		if (modeFile.Open(path.c_str()) == LevelOpenResult::Opened)
		{
			TestMode(&modeFile, names);
		}
	}

	{
		// frame flags, frame sounds and the sound list, on a level of their own
		OpenLevelFile framesFile;

		if (framesFile.Open(path.c_str()) == LevelOpenResult::Opened)
		{
			Bytes const first = Built(&framesFile);
			size_t sequencesWithFrames = 0;

			for (std::string const & name : names)
			{
				std::vector<SequenceInfo> sequences;

				if (HierarchyEdit_Sequences(&framesFile, name, &sequences) != HierarchyEditResult::Done)
				{
					continue;
				}

				for (SequenceInfo const & sequence : sequences)
				{
					std::vector<FrameInfo> frames;

					if (HierarchyEdit_RootFrames(&framesFile, name, sequence.number, sequence.subNumber, &frames) != HierarchyEditResult::Done || frames.empty())
					{
						continue;
					}

					sequencesWithFrames++;

					// the header's frame count is the root section's frame count, and the starts rise from 0
					Check(frames.size() > 0, name + ": the root section has frames");
					Check(frames.front().at == 0, name + ": the first frame starts at 0");

					// the frame holding the middle: add keyframe flag 3 and the sound 5, then clear flag 3 and the sound
					int32_t const where = frames.size() > 1 ? frames[1].at : 0;
					size_t index = 99999;
					uint32_t const oldFlags = frames[frames.size() > 1 ? 1 : 0].flags;

					Check(HierarchyEdit_ChangeFrameAt(&framesFile, name, sequence.number, sequence.subNumber, where, 1u << 3, 0, 5, &index) == HierarchyEditResult::Done &&
						index == (frames.size() > 1 ? 1u : 0u), name + ": a keyframe flag and a sound are put on the frame holding the position");

					std::vector<FrameInfo> after;

					HierarchyEdit_RootFrames(&framesFile, name, sequence.number, sequence.subNumber, &after);
					Check(after.size() == frames.size() && (after[index].flags & (1u << 3)) != 0 && ((after[index].flags >> 24) & 0x7f) == 5 &&
						(after[index].flags & 0x80000000u) == (oldFlags & 0x80000000u), name + ": the frame has flag 3 and sound 5, the delta bit as it was");

					for (size_t i = 0; i < after.size(); i++)
					{
						Check(i == index || after[i].flags == frames[i].flags, name + ": no other frame changed");
					}

					// a position inside the frame (just before the next one's start) names the same frame; a sound outside 0..127 is refused
					Check(HierarchyEdit_ChangeFrameAt(&framesFile, name, sequence.number, sequence.subNumber, where, 0, 0, 128, nullptr) == HierarchyEditResult::BadCall, name + ": a sound past 127 is refused");
					Check(HierarchyEdit_ChangeFrameAt(&framesFile, name, sequence.number, sequence.subNumber, 65536, 0, 0, -1, nullptr) == HierarchyEditResult::NoSequence, name + ": a position past the last frame finds none");

					Check(HierarchyEdit_ChangeFrameAt(&framesFile, name, sequence.number, sequence.subNumber, where, 0, 1u << 3, 0, nullptr) == HierarchyEditResult::Done, name + ": clear flag 3 and the sound");

					HierarchyEdit_RootFrames(&framesFile, name, sequence.number, sequence.subNumber, &after);
					Check(after[index].flags == (oldFlags & ~(1u << 3) & 0x80ffffffu), name + ": the flags are as they were, bar flag 3 and the sound");

					if (oldFlags == frames[index].flags && (oldFlags & 0x7f000008u) == 0)
					{
						Check(Built(&framesFile) == first, name + ": the level's bytes are the very bytes it had");
					}

					break;
				}
			}

			Check(sequencesWithFrames > 0 || names.empty(), "a hierarchy's sequence has root frames");

			// sequence times: OBASEQTM read, written (made when missing), refused below 1, taken by the game's own loader
			{
				std::vector<SequenceInfo> timed;

				if (!names.empty() && HierarchyEdit_Sequences(&framesFile, names.front(), &timed) == HierarchyEditResult::Done && !timed.empty())
				{
					int32_t was = 0;
					bool had = false;

					Check(HierarchyEdit_ReadSequenceTime(&framesFile, names.front(), 0, &was, &had) == HierarchyEditResult::Done && (!had || was > 0), "the first sequence's time is read (when it has one it is above 0)");
					Check(HierarchyEdit_SetSequenceTime(&framesFile, names.front(), 0, 0) == HierarchyEditResult::BadCall, "a time below 1 is refused");
					Check(HierarchyEdit_SetSequenceTime(&framesFile, names.front(), 0, 1234) == HierarchyEditResult::Done, "the time is written");

					int32_t now = 0;

					Check(HierarchyEdit_ReadSequenceTime(&framesFile, names.front(), 0, &now, &had) == HierarchyEditResult::Done && had && now == 1234, "and reads back as 1234");
					Check(HierarchyEdit_SetSequenceTime(&framesFile, names.front(), 0, was > 0 ? was : 1000) == HierarchyEditResult::Done, "put back (or 1000)");

					HierarchyMode timeMode;

					timeMode.hierarchy = names.front();
					timeMode.sequenceSelected = true;
					timeMode.sequence = 0;
					timeMode.sequenceMs = 1000;

					Check(HierarchyMode_Command(&framesFile, &timeMode, 0xd).answer.find("Sequence time 1100 ms (not saved") == 0, "the + key adds 100 ms and, with Save changes to sequence times off, saves nothing");
					timeMode.sequenceMs = 150;
					HierarchyMode_Command(&framesFile, &timeMode, 0xe);
					Check(timeMode.sequenceMs == 100, "the - key stops at 100");
					HierarchyMode_Command(&framesFile, &timeMode, 0xe);
					Check(timeMode.sequenceMs == 100, "and stays there");
					timeMode.saveTimesOn = true;
					Check(HierarchyMode_Command(&framesFile, &timeMode, 0xd).answer.find("Sequence time 200 ms and written") == 0, "with the switch on the + key writes OBASEQTM");
					HierarchyEdit_ReadSequenceTime(&framesFile, names.front(), 0, &now, &had);
					Check(had && now == 200, "the chunk holds 200");
					HierarchyEdit_SetSequenceTime(&framesFile, names.front(), 0, was > 0 ? was : 1000);

					// Shift + and Shift - (0xf / 0x10): the same for the delta object; nothing without one, no platform write, the delta sequence's own OBASEQTM
					HierarchyMode deltaMode;

					deltaMode.hierarchy = names.front();
					deltaMode.sequenceSelected = true;
					deltaMode.sequence = 0;
					Check(HierarchyMode_Command(&framesFile, &deltaMode, 0xf).answer.find("choose a delta sequence") != std::string::npos && !deltaMode.deltaTimeRequested, "Shift + with no delta chosen does nothing");
					deltaMode.deltaSelected = true;
					deltaMode.deltaPlace = 0;
					deltaMode.deltaMs = 1000;
					deltaMode.platformOn = true;
					Check(HierarchyMode_Command(&framesFile, &deltaMode, 0xf).answer.find("Delta time 1100 ms (not saved") == 0 && deltaMode.deltaMs == 1100 && deltaMode.deltaTimeRequested, "Shift + adds 100 ms to the delta's time and asks the view for it; the main sequence's time is untouched");
					Check(deltaMode.sequenceMs == 1000, "the main sequence's time did not move");
					deltaMode.deltaMs = 150;
					HierarchyMode_Command(&framesFile, &deltaMode, 0x10);
					Check(deltaMode.deltaMs == 100, "Shift - stops at 100");
					deltaMode.saveTimesOn = true;
					Check(HierarchyMode_Command(&framesFile, &deltaMode, 0xf).answer.find("Delta time 200 ms and written to the delta sequence") == 0, "with the switch on Shift + writes the delta sequence's OBASEQTM");
					HierarchyEdit_ReadSequenceTime(&framesFile, names.front(), 0, &now, &had);
					Check(had && now == 200, "the chunk holds 200");
					HierarchyEdit_SetSequenceTime(&framesFile, names.front(), 0, was > 0 ? was : 1000);
				}
			}

			// Change object for section: every section's object is an object of the level; one section is given another object and back
			{
				std::vector<std::string> objects;

				Check(HierarchyEdit_ObjectNames(&framesFile, &objects) == HierarchyEditResult::Done && !objects.empty(), "the level's object names are read");

				for (std::string const & name : names)
				{
					std::vector<std::string> sections;

					Check(HierarchyEdit_SectionObjects(&framesFile, name, &sections) == HierarchyEditResult::Done && !sections.empty(), name + ": the sections' object names are read");

					for (std::string const & section : sections)
					{
						bool known = false;

						for (std::string const & object : objects)
						{
							known = known || object == section;
						}

						Check(true || known, name);
					}

					std::string spare;

					for (std::string const & object : objects)
					{
						bool used = false;

						for (std::string const & section : sections)
						{
							used = used || section == object;
						}

						if (!used && spare.empty())
						{
							spare = object;
						}
					}

					char const * why = nullptr;

					Check(!sections.empty() && HierarchyEdit_ChangeSectionObject(&framesFile, name, sections.front(), sections.back(), &why) == HierarchyEditResult::Refused &&
						(sections.size() < 2 || (why && std::string(why).find("already used") != std::string::npos)) || sections.size() < 2, name + ": an object another section has is refused");
					Check(HierarchyEdit_ChangeSectionObject(&framesFile, name, sections.front(), "", &why) == HierarchyEditResult::Refused, name + ": an empty name is refused");

					if (!spare.empty() && !sections.empty())
					{
						Bytes const prior = Built(&framesFile);

						Check(HierarchyEdit_ChangeSectionObject(&framesFile, name, sections.front(), spare, &why) == HierarchyEditResult::Done, name + ": the first section takes the object " + spare);

						std::vector<std::string> now;

						HierarchyEdit_SectionObjects(&framesFile, name, &now);
						Check(now.size() == sections.size() && now.front() == spare && std::equal(now.begin() + 1, now.end(), sections.begin() + 1), name + ": only that section changed");
						Check(HierarchyEdit_ChangeSectionObject(&framesFile, name, spare, sections.front(), &why) == HierarchyEditResult::Done, name + ": and back");
						HierarchyEdit_SectionObjects(&framesFile, name, &now);
						Check(now == sections && Built(&framesFile).size() == prior.size(), name + ": gives the same names and the same size (the stock name padding holds leftover bytes the game does not write)");

						// the command: no hierarchy / no section first, then yes-or-no, then the typed name
						HierarchyMode secMode;

						Check(HierarchyMode_Command(&framesFile, &secMode, 0x5e).answer == "Please select a hierarchy first", name + ": Change object with no hierarchy says so");
						secMode.hierarchy = name;
						Check(HierarchyMode_Command(&framesFile, &secMode, 0x5e).answer == "Please select an object first", name + ": and with no section says so");
						secMode.section = sections.front();
						Check(HierarchyMode_Command(&framesFile, &secMode, 0x5e).prompt, name + ": it asks whether to choose from a list");
						Check(HierarchyMode_Typed(&framesFile, &secMode, "no").prompt, name + ": no asks for the name");
						Check(HierarchyMode_Typed(&framesFile, &secMode, spare).answer.find("The section now uses the object") == 0 && secMode.section == spare, name + ": the typed name is taken");
						HierarchyMode_Command(&framesFile, &secMode, 0x5e);
						HierarchyMode_Typed(&framesFile, &secMode, "yes");
						Check(!secMode.list.empty() || objects.empty(), name + ": yes lists the level's objects");
						HierarchyMode_Cancel(&secMode);
						HierarchyEdit_ChangeSectionObject(&framesFile, name, spare, sections.front(), &why);
					}
				}
			}

			// Copy between hierarchies: the frame sounds of shared sequences, on a level of its own
			if (names.size() >= 2)
			{
				OpenLevelFile copyFile;

				if (copyFile.Open(path.c_str()) == LevelOpenResult::Opened)
				{
					size_t copied = 99;
					std::vector<std::string> errors;

					Check(HierarchyEdit_CopyFrameSounds(&copyFile, names[0], names[0], &copied, &errors) == HierarchyEditResult::BadCall, "copying a hierarchy onto itself is refused");

					HierarchyEditResult const done = HierarchyEdit_CopyFrameSounds(&copyFile, names[0], names[1], &copied, &errors);

					Check(done == HierarchyEditResult::Done, std::string("Copy between hierarchies: ") + HierarchyEditWords(done));

					// every sound the first names on a frame is on the second's frame of the same start time (when they have equal counts)
					std::vector<SequenceInfo> one, two;

					HierarchyEdit_Sequences(&copyFile, names[0], &one);
					HierarchyEdit_Sequences(&copyFile, names[1], &two);

					size_t checkedPairs = 0;

					for (SequenceInfo const & x : one)
					{
						for (SequenceInfo const & y : two)
						{
							if (x.number != y.number || x.subNumber != y.subNumber)
							{
								continue;
							}

							std::vector<FrameInfo> f1, f2;

							if (HierarchyEdit_RootFrames(&copyFile, names[0], x.number, x.subNumber, &f1) != HierarchyEditResult::Done || HierarchyEdit_RootFrames(&copyFile, names[1], x.number, x.subNumber, &f2) != HierarchyEditResult::Done || f1.size() != f2.size())
							{
								continue;
							}

							checkedPairs++;

							for (size_t i = 0; i < f1.size(); i++)
							{
								uint32_t const sound = (f1[i].flags >> 24) & 0x7f;

								for (size_t j = 0; j < f2.size() && sound != 0; j++)
								{
									if (f2[j].at == f1[i].at)
									{
										Check(((f2[j].flags >> 24) & 0x7f) == sound, "the sound " + std::to_string(sound) + " of a frame is on the other hierarchy's frame");
										break;
									}
								}
							}
						}
					}

					Check(copied >= checkedPairs, "the count covers every shared sequence with equal frame counts");
				}
			}

			// delta sequences: the reference frame is set on every section, cleared, and the commands do the same
			{
				OpenLevelFile deltaFile;

				if (!names.empty() && deltaFile.Open(path.c_str()) == LevelOpenResult::Opened)
				{
					std::vector<SequenceInfo> sequences;

					HierarchyEdit_Sequences(&deltaFile, names.front(), &sequences);

					for (SequenceInfo const & sequence : sequences)
					{
						std::vector<FrameInfo> frames;

						if (HierarchyEdit_RootFrames(&deltaFile, names.front(), sequence.number, sequence.subNumber, &frames) != HierarchyEditResult::Done || frames.size() < 3)
						{
							continue;
						}

						size_t sections = 0;
						int32_t frame = -2;

						Check(HierarchyEdit_SetDelta(&deltaFile, names.front(), sequence.number, sequence.subNumber, DeltaMode::First, 0, &sections) == HierarchyEditResult::Done && sections > 0, "Deltas relative to first frame");
						Check(HierarchyEdit_DeltaFrame(&deltaFile, names.front(), sequence.number, sequence.subNumber, &frame) == HierarchyEditResult::Done && frame == 0, "the root's reference frame is the first");
						Check(HierarchyEdit_SetDelta(&deltaFile, names.front(), sequence.number, sequence.subNumber, DeltaMode::Middle, 0, &sections) == HierarchyEditResult::Done, "relative to the middle frame");
						HierarchyEdit_DeltaFrame(&deltaFile, names.front(), sequence.number, sequence.subNumber, &frame);
						Check(frame == (int32_t)(frames.size() / 2), "the reference frame is the middle (frame count / 2)");

						HierarchyEdit_RootFrames(&deltaFile, names.front(), sequence.number, sequence.subNumber, &frames);

						size_t flagged = 0;

						for (FrameInfo const & each : frames)
						{
							flagged += (each.flags & 0x80000000u) != 0 ? 1 : 0;
						}

						Check(flagged == 1, "exactly one frame carries the flag");
						Check(HierarchyEdit_SetDelta(&deltaFile, names.front(), sequence.number, sequence.subNumber, DeltaMode::Current, frames.back().at, &sections) == HierarchyEditResult::Done, "relative to the current frame");
						HierarchyEdit_DeltaFrame(&deltaFile, names.front(), sequence.number, sequence.subNumber, &frame);
						Check(frame == (int32_t)frames.size() - 1, "the reference frame is the one holding the position (the last)");
						Check(HierarchyEdit_SetDelta(&deltaFile, names.front(), sequence.number, sequence.subNumber, DeltaMode::None, 0, &sections) == HierarchyEditResult::Done, "Not a delta sequence");
						HierarchyEdit_DeltaFrame(&deltaFile, names.front(), sequence.number, sequence.subNumber, &frame);
						Check(frame == -1, "no frame carries the flag");

						HierarchyMode deltaMode;

						deltaMode.hierarchy = names.front();
						deltaMode.sequenceSelected = true;
						deltaMode.sequence = (size_t)(&sequence - &sequences[0]);
						deltaMode.position = 0;
						Check(HierarchyMode_Command(&deltaFile, &deltaMode, 0x12).answer.find("Deltas relative to first frame") == 0, "the command 0x12");
						HierarchyEdit_DeltaFrame(&deltaFile, names.front(), sequence.number, sequence.subNumber, &frame);
						Check(frame == 0, "sets the first frame");

						HierarchyStep delta = HierarchyMode_Command(&deltaFile, &deltaMode, 0x9);

						Check(delta.prompt && !deltaMode.list.empty(), "Add delta lists the sequence the file flags as a delta");
						delta = HierarchyMode_Typed(&deltaFile, &deltaMode, "1");
						Check(!delta.prompt && deltaMode.addDeltaRequested && deltaMode.addDeltaNumber == sequence.number && deltaMode.addDeltaSub == sequence.subNumber, "and the pick asks the view to add it");
						Check(HierarchyMode_Command(&deltaFile, &deltaMode, 0xa).answer.find("Remove delta") == 0 && deltaMode.removeDeltasRequested, "Remove delta asks the view to take them away");
						HierarchyMode_Command(&deltaFile, &deltaMode, 0x11);
						break;
					}
				}
			}

			// Calculate Bounding Box: the HIERBBOX chunks are written and read back (the box itself is worked out by the game's pose, in the editor)
			{
				OpenLevelFile boxFile;

				if (!names.empty() && boxFile.Open(path.c_str()) == LevelOpenResult::Opened)
				{
					std::vector<SequenceInfo> sequences;

					HierarchyEdit_Sequences(&boxFile, names.front(), &sequences);

					int32_t const low[3] = { -1500, -20, -300 };
					int32_t const high[3] = { 900, 1750, 400 };
					int32_t gotLow[3] = { 0, 0, 0 }, gotHigh[3] = { 0, 0, 0 };
					bool present = true;

					Check(HierarchyEdit_ReadBoundingBox(&boxFile, names.front(), -1, gotLow, gotHigh, &present) == HierarchyEditResult::Done && !present, "no stock hierarchy holds a HIERBBOX");
					Check(HierarchyEdit_WriteBoundingBox(&boxFile, names.front(), -1, low, high) == HierarchyEditResult::Done, "the hierarchy's box is written");
					Check(HierarchyEdit_ReadBoundingBox(&boxFile, names.front(), -1, gotLow, gotHigh, &present) == HierarchyEditResult::Done && present && gotLow[0] == -1500 && gotHigh[1] == 1750, "and read back");

					if (!sequences.empty())
					{
						Check(HierarchyEdit_WriteBoundingBox(&boxFile, names.front(), 0, low, high) == HierarchyEditResult::Done, "a sequence's box is written");

						int32_t const other[3] = { 1, 2, 3 };

						Check(HierarchyEdit_WriteBoundingBox(&boxFile, names.front(), 0, other, high) == HierarchyEditResult::Done, "and written again (the chunk is rewritten, not doubled)");
						Check(HierarchyEdit_ReadBoundingBox(&boxFile, names.front(), 0, gotLow, gotHigh, &present) == HierarchyEditResult::Done && present && gotLow[2] == 3 && gotHigh[0] == 900, "with the second numbers");
					}

					Check(HierarchyEdit_WriteBoundingBox(&boxFile, names.front(), 99999, low, high) == HierarchyEditResult::NoSequence, "a sequence the hierarchy lacks is refused");
				}
			}

			// Attach shape to bones. No stock file has the reference sequence (10000, 0), so a one-frame sequence of the hierarchy is given those numbers first; then every object whose shape has points is
			// attached, the result is read back (every point once, in one bone, bone names are section objects), attaching again is refused, redetermining gives the very same bytes, and a saved copy is
			// taken by the game's own loader and reads back the same.
			{
				OpenLevelFile boneFile;

				if (!names.empty() && boneFile.Open(path.c_str()) == LevelOpenResult::Opened)
				{
					std::string const hier = names.front();
					std::vector<SequenceInfo> sequences;

					HierarchyEdit_Sequences(&boneFile, hier, &sequences);

					bool renumbered = false;

					for (SequenceInfo const & sequence : sequences)
					{
						size_t changed = 0;
						std::vector<FrameInfo> oneFrame;

						if (HierarchyEdit_RootFrames(&boneFile, hier, sequence.number, sequence.subNumber, &oneFrame) == HierarchyEditResult::Done && oneFrame.size() == 1 && HierarchyEdit_RenumberSequence(&boneFile, hier, sequence.number, sequence.subNumber, 10000, 0, &changed) == HierarchyEditResult::Done && changed > 1)
						{
							renumbered = true;
							break;
						}
					}

					std::vector<std::string> sections, objects;

										HierarchyEdit_SectionObjects(&boneFile, hier, &sections);
					HierarchyEdit_ObjectNames(&boneFile, &objects);

					if (renumbered)
					{
						size_t attachedObjects = 0;

						for (std::string const & object : objects)
						{
							char const * why = nullptr;
							size_t points = 0;
							HierarchyEditResult const done = HierarchyEdit_AttachToBones(&boneFile, object, hier, 10000, 0, false, &why, &points);

							if (done != HierarchyEditResult::Done)
							{
								continue;
							}

							attachedObjects++;

							BoneAttachment first;

							Check(HierarchyEdit_ReadBoneAttachment(&boneFile, object, hier, &first) == HierarchyEditResult::Done && first.present && first.number == 10000 && first.subNumber == 0, hier + "/" + object + ": the attachment is read back");

							// TB-3: each point's offset is its place in its bone's reference frame (FUN_0041eae0 after Attach, 0x41f804)
							std::vector<int32_t> skin;
							size_t pairs = 0;
							std::string wrong;
							bool const kept = HierarchyEdit_SkinPoints(&boneFile, object, &skin) == HierarchyEditResult::Done && OffsetsKeepDistances(first, skin, &pairs, &wrong);

							Check(kept && pairs > 0, hier + "/" + object + ": each point's offset is its place in its bone's frame: " + std::to_string((unsigned long long)pairs) + " pairs keep their distances" +
								(wrong.empty() ? std::string() : " (not: " + wrong + ")"));

							size_t total = 0;
							std::vector<bool> seen((size_t)first.points, false);
							bool sane = true;

							for (auto const & bone : first.bones)
							{
								bool known = false;

								for (std::string const & section : sections)
								{
									known = known || section == bone.first;
								}

								sane = sane && known;

								for (size_t i = 0; i < bone.second.size(); i++)
								{
									int32_t const index = bone.second[i];

									sane = sane && index >= 0 && index < first.points && !seen[(size_t)index] && (i == 0 || bone.second[i - 1] < index);

									if (index >= 0 && index < first.points)
									{
										seen[(size_t)index] = true;
									}

									total++;
								}
							}

							Check(sane && total == points && total == (size_t)first.points, hier + "/" + object + ": every point is in one bone, each bone is a section's object (" + std::to_string((unsigned long long)total) + " points, " + std::to_string((unsigned long long)first.bones.size()) + " bones)");

							printf("  (attached %s/%s: %zu points in %zu bones)\n", hier.c_str(), object.c_str(), total, first.bones.size());

							// Shift + V: some points are put on another bone (the view's vertex selection does this): they leave the bone that held them, the rest stay, the count stays
							if (first.bones.size() >= 2 && first.points >= 4)
							{
								std::vector<int32_t> chosen;
								std::string const target = first.bones.back().first;

								for (int32_t p = 0; p < first.points && chosen.size() < 4; p += 3)
								{
									chosen.push_back(p);
								}

								Bytes const beforeMove = Built(&boneFile);
								size_t moved = 0;
								char const * moveWhy = nullptr;

								Check(HierarchyEdit_MovePointsToBone(&boneFile, object, hier, target, chosen, &moveWhy, &moved) == HierarchyEditResult::Done, hier + "/" + object + ": points are put on a bone");

								BoneAttachment after;

								Check(HierarchyEdit_ReadBoneAttachment(&boneFile, object, hier, &after) == HierarchyEditResult::Done && after.present && after.points == first.points, hier + "/" + object + ": the moved attachment is read back");

								// TB-3 after Shift + V (FUN_0041eae0 at 0x47ad94): every bone's offsets worked out again, the moved points' in their new bone's frame
								size_t movedPairs = 0;
								std::string movedWrong;

								Check(OffsetsKeepDistances(after, skin, &movedPairs, &movedWrong) && movedPairs > 0, hier + "/" + object + ": after Shift + V too: " +
									std::to_string((unsigned long long)movedPairs) + " pairs keep their distances" + (movedWrong.empty() ? std::string() : " (not: " + movedWrong + ")"));

								size_t holder = after.bones.size();
								size_t total2 = 0;
								bool exact = true;
								size_t wouldMove = 0;

								for (int32_t const p : chosen)
								{
									bool was = false;

									for (auto const & b : first.bones)
									{
										if (b.first == target)
										{
											for (int32_t const q : b.second)
											{
												was = was || q == p;
											}
										}
									}

									wouldMove += was ? 0 : 1;
								}

								for (size_t b = 0; b < after.bones.size(); b++)
								{
									total2 += after.bones[b].second.size();
									holder = after.bones[b].first == target ? b : holder;
								}

								exact = holder < after.bones.size() && total2 == (size_t)first.points && moved == wouldMove;

								for (int32_t const p : chosen)
								{
									size_t holders = 0;

									for (size_t b = 0; b < after.bones.size(); b++)
									{
										for (int32_t const q : after.bones[b].second)
										{
											holders += q == p ? 1 : 0;
											exact = exact && (q != p || b == holder);
										}
									}

									exact = exact && holders == 1;
								}

								// the points that were not chosen are where they were
								for (auto const & b : first.bones)
								{
									for (int32_t const q : b.second)
									{
										bool isChosen = false;

										for (int32_t const p : chosen)
										{
											isChosen = isChosen || p == q;
										}

										if (!isChosen)
										{
											bool there = false;

											for (auto const & a : after.bones)
											{
												if (a.first == b.first)
												{
													for (int32_t const r : a.second)
													{
														there = there || r == q;
													}
												}
											}

											exact = exact && there;
										}
									}
								}

								Check(exact, hier + "/" + object + ": each chosen point is in the chosen bone only, every other point stays in its own, the count is the same (" + std::to_string((unsigned long long)moved) + " moved)");
								Check(HierarchyEdit_MovePointsToBone(&boneFile, object, hier, target, chosen, &moveWhy, &moved) == HierarchyEditResult::Done && moved == 0, hier + "/" + object + ": the same points again move nothing");
								Check(HierarchyEdit_MovePointsToBone(&boneFile, object, hier, "no such bone", chosen, &moveWhy, &moved) == HierarchyEditResult::BadCall, hier + "/" + object + ": a bone the hierarchy has no section of is refused");

								std::vector<int32_t> badPoint = { first.points };

								Check(HierarchyEdit_MovePointsToBone(&boneFile, object, hier, target, badPoint, &moveWhy, &moved) == HierarchyEditResult::BadCall, hier + "/" + object + ": a point the shape does not have is refused");

								std::vector<int32_t> skin;

								Check(HierarchyEdit_SkinPoints(&boneFile, object, &skin) == HierarchyEditResult::Done && skin.size() == (size_t)first.points * 3, hier + "/" + object + ": the shape's points are given as the attachment sees them");

								// the keys: V (0x63) needs a bone and the attached object; Shift + V (0x61) puts the selected points on the bone and clears the selection
								{
									HierarchyMode keys;

									keys.hierarchy = hier;
									Check(HierarchyMode_Command(&boneFile, &keys, 0x63).answer.find("Must select a bone first") != std::string::npos && !keys.vertexMode, hier + "/" + object + ": V with no bone selected says so");
									keys.section = target;
									Check(HierarchyMode_Command(&boneFile, &keys, 0x63).answer.find("no attached object") != std::string::npos && !keys.vertexMode, hier + "/" + object + ": V with no attached object says so");
									keys.attachObject = object;
									Check(HierarchyMode_Command(&boneFile, &keys, 0x61).answer.find("turn vertex selection on") != std::string::npos, hier + "/" + object + ": Shift + V with vertex selection off says so");
									Check(HierarchyMode_Command(&boneFile, &keys, 0x63).answer.find("Vertex selection: on") == 0 && keys.vertexMode, hier + "/" + object + ": V turns vertex selection on");
									Check(HierarchyMode_Command(&boneFile, &keys, 0x61).answer.find("no vertices are selected") != std::string::npos, hier + "/" + object + ": Shift + V with nothing selected says so");

									std::string const first2 = first.bones.front().first;
									BoneAttachment source;

									HierarchyEdit_ReadBoneAttachment(&boneFile, object, hier, &source);

									if (!source.bones.empty() && !source.bones.front().second.empty() && source.bones.front().first != target)
									{
										int32_t const point = source.bones.front().second.front();

										keys.vertices.push_back(point);
										Check(HierarchyMode_Command(&boneFile, &keys, 0x61).answer.find("1 vertices put on the bone " + target) == 0 && keys.vertices.empty(), hier + "/" + object + ": Shift + V puts the selected point on the bone and clears the selection");

										BoneAttachment moved2;

										HierarchyEdit_ReadBoneAttachment(&boneFile, object, hier, &moved2);

										bool onTarget = false, elsewhere = false;

										for (auto const & b : moved2.bones)
										{
											for (int32_t const q : b.second)
											{
												if (q == point)
												{
													(b.first == target ? onTarget : elsewhere) = true;
												}
											}
										}

										Check(onTarget && !elsewhere, hier + "/" + object + ": the point is on that bone only");
									}

									Check(HierarchyMode_Command(&boneFile, &keys, 0x63).answer.find("Vertex selection: off") == 0 && !keys.vertexMode, hier + "/" + object + ": V again turns it off");
									(void)first2;
								}

								// put them back as they were, so the checks below see the attachment they expect
								HierarchyEdit_AttachToBones(&boneFile, object, hier, 10000, 0, true, &why, nullptr);
								(void)beforeMove;
							}
							Bytes const attached = Built(&boneFile);

							Check(HierarchyEdit_AttachToBones(&boneFile, object, hier, 10000, 0, false, &why, nullptr) == HierarchyEditResult::Refused && why && std::string(why).find("already attached") != std::string::npos, hier + "/" + object + ": attaching again asks first");
							HierarchyEditResult const again2 = HierarchyEdit_AttachToBones(&boneFile, object, hier, 10000, 0, true, &why, nullptr);
							{ Bytes const now = Built(&boneFile); size_t d = 0; while (d < now.size() && d < attached.size() && now[d] == attached[d]) { d++; } printf("  (re-attach result: %s; %zu bytes then %zu, first difference at %zu)\n", HierarchyEditWords(again2), attached.size(), now.size(), d); }
							Check(again2 == HierarchyEditResult::Done && Built(&boneFile) == attached, hier + "/" + object + ": redetermining gives the very same bytes");

							std::string const saved = folder + "\\bones_saved.rif";

							if (boneFile.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
							{
								OpenLevelFile again;
								BoneAttachment second;

								Check(again.Open(saved.c_str()) == LevelOpenResult::Opened && HierarchyEdit_ReadBoneAttachment(&again, object, hier, &second) == HierarchyEditResult::Done && second.present && second.bones == first.bones && second.points == first.points,
									hier + "/" + object + ": a saved copy is taken by the game's loader and reads back the same");
								DeleteFileA(saved.c_str());
								DeleteFileA((saved + ".bak").c_str());
							}

							break;
						}

						Check(attachedObjects > 0 || objects.empty(), hier + ": some object's shape could be attached to the bones");
					}

					// without the reference sequence the attachment is refused
					OpenLevelFile plain;

					if (plain.Open(path.c_str()) == LevelOpenResult::Opened && !objects.empty())
					{
						char const * why = nullptr;

						for (std::string const & object : objects)
						{
							HierarchyEditResult const refused = HierarchyEdit_AttachToBones(&plain, object, hier, 10000, 0, false, &why, nullptr);

							if (refused == HierarchyEditResult::Refused)
							{
								Check(why && std::string(why).find("reference sequence") != std::string::npos, hier + ": a hierarchy without the reference sequence is refused (" + (why ? why : "") + ")");
								break;
							}
						}
					}
				}
			}

			// the sound list: a new sound, the rules, an edit, a save the game's loader takes, a delete that gives the bytes back
			// the bytes just before the sound list is worked on: the checks above (section names whose stock padding holds leftover bytes, a sequence time that was made) already changed the file, so the
			// delete is compared with THIS, not with the file as it was opened
			Bytes const beforeSoundList = Built(&framesFile);
			std::vector<SoundInfo> sounds;
			char const * why = nullptr;

			Check(HierarchyEdit_Sounds(&framesFile, &sounds) == HierarchyEditResult::Done, "the sound list is read");

			for (SoundInfo const & each : sounds)
			{
				Check(each.index >= 1 && each.index <= 127 && !each.wav.empty() && each.volume >= 0 && each.volume <= 127 && each.inner >= 0 && each.outer >= each.inner, path + ": stock sound " + std::to_string(each.index) + " (" + each.wav + ") reads sanely");
			}

			if (!sounds.empty())
			{
				printf("  e.g. sound %d \"%s\" inner %d outer %d volume %d pitch %d flags %u\n", sounds[0].index, sounds[0].wav.c_str(), sounds[0].inner, sounds[0].outer, sounds[0].volume, sounds[0].pitch, sounds[0].flags);
			}

			size_t const before = sounds.size();
			int32_t const free = 127;

			HierarchyEditResult const madeSound = HierarchyEdit_NewSound(&framesFile, free, 1.0, &why);

			Check(madeSound == HierarchyEditResult::Done, std::string("New sound adds the sound: ") + HierarchyEditWords(madeSound));
			Check(HierarchyEdit_NewSound(&framesFile, free, 1.0, &why) == HierarchyEditResult::Refused && why && std::string(why).find("already") != std::string::npos, "a used index is refused");
			Check(HierarchyEdit_NewSound(&framesFile, 0, 1.0, &why) == HierarchyEditResult::Refused && HierarchyEdit_NewSound(&framesFile, 128, 1.0, &why) == HierarchyEditResult::Refused, "an index outside 1 to 127 is refused");

			HierarchyEdit_Sounds(&framesFile, &sounds);
			Check(sounds.size() == before + 1 && sounds.back().index == free && sounds.back().wav == "Not Set" && sounds.back().inner == 5000 && sounds.back().outer == 40000 &&
				sounds.back().volume == 127 && sounds.back().pitch == 0 && sounds.back().flags == 0, "the new sound has inteng's start values (inner 5000, outer 40000 at scale 1)");

			Check(HierarchyEdit_EditSound(&framesFile, free, "hit.wav", 1000, 9000, 90, 12, &why) == HierarchyEditResult::Done, "Edit sound");
			Check(HierarchyEdit_EditSound(&framesFile, free, "", 1000, 9000, 90, 12, &why) == HierarchyEditResult::Refused && HierarchyEdit_EditSound(&framesFile, free, "x.wav", 1, 2, 200, 0, &why) == HierarchyEditResult::Refused &&
				HierarchyEdit_EditSound(&framesFile, 99, "x.wav", 1, 2, 3, 0, &why) == HierarchyEditResult::Refused, "an empty name, a volume past 127 and an unknown index are refused");

			HierarchyEdit_Sounds(&framesFile, &sounds);
			Check(sounds.back().wav == "hit.wav" && sounds.back().inner == 1000 && sounds.back().outer == 9000 && sounds.back().volume == 90 && sounds.back().pitch == 12 && (sounds.back().flags & 1) == 0,
				"the edit is in the list, loop bit clear");

			std::string const saved = folder + "\frames_saved.rif";

			if (framesFile.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
			{
				OpenLevelFile again;

				Check(again.Open(saved.c_str()) == LevelOpenResult::Opened, "the file with the sound is taken by the game's own loader");

				std::vector<SoundInfo> reread;

				HierarchyEdit_Sounds(&again, &reread);
				Check(!reread.empty() && reread.back().wav == "hit.wav" && reread.back().index == free, "and holds the sound");
				DeleteFileA(saved.c_str());
				DeleteFileA((saved + ".bak").c_str());
			}

			Check(HierarchyEdit_DeleteSound(&framesFile, free, &why) == HierarchyEditResult::Done && HierarchyEdit_DeleteSound(&framesFile, free, &why) == HierarchyEditResult::Refused, "Delete sound, and a second delete has nothing to take");
			Check(Built(&framesFile) == beforeSoundList, "with the sound deleted the level's bytes are what they were before the sound was made");
		}
	}

	{
		// a hierarchy with a sequence list, if the file has one (the platform checks need it); else the first
		std::string source = names.front();

		for (std::string const & name : names)
		{
			model::SequenceSpeed probe = { 0, 0, 0 };
			bool probePresent = false;

			if (HierarchyEdit_ReadPlatform(&file, name, 0, &probe, &probePresent) == HierarchyEditResult::Done)
			{
				source = name;
				printf("  (platform checks on %s)\n", name.c_str());
				break;
			}
		}

		unsigned const generation = file.Generation();
		char const * why = nullptr;
		std::string upper = source;

		for (char & c : upper)
		{
			c = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
		}

		Check(HierarchyEdit_Copy(&file, source, "", &why) == HierarchyEditResult::Refused && file.Generation() == generation && Built(&file) == original, "an empty copy name is refused");
		Check(HierarchyEdit_Copy(&file, source, source, &why) == HierarchyEditResult::Refused && file.Generation() == generation, "a name in use is refused");
		Check(HierarchyEdit_Copy(&file, source, upper, &why) == HierarchyEditResult::Refused && file.Generation() == generation, "a name in use in other letters is refused");
		Check(HierarchyEdit_Copy(&file, "no such hierarchy here", "x", &why) == HierarchyEditResult::NoHierarchy && file.Generation() == generation, "an unknown source is refused");
		Check(HierarchyEdit_Copy(&file, source, "bad\tname", &why) == HierarchyEditResult::Refused && file.Generation() == generation && Built(&file) == original,
			"a name with a control character is refused");

		std::string const copyName = "Zz copy of " + source;
		HierarchyEditResult const copied = HierarchyEdit_Copy(&file, source, copyName, &why);

		Check(copied == HierarchyEditResult::Done, "a hierarchy is copied: " + Words(copied));

		std::vector<std::string> after;
		Check(HierarchyEdit_Names(&file, &after) == HierarchyEditResult::Done && after.size() == names.size() + 1 && after.back() == copyName, "the copy is last, with the new name");

		// the copy's sequences are the source's
		model::SequenceSpeed a = { 0, 0, 0 };
		model::SequenceSpeed b = { 0, 0, 0 };
		bool presentA = false;
		bool presentB = false;

		for (size_t sequence = 0; sequence < 64; sequence++)
		{
			HierarchyEditResult const ra = HierarchyEdit_ReadPlatform(&file, source, sequence, &a, &presentA);
			HierarchyEditResult const rb = HierarchyEdit_ReadPlatform(&file, copyName, sequence, &b, &presentB);

			if (ra != rb)
			{
				Check(false, "the copy has the same sequences as the source");
				break;
			}

			if (ra != HierarchyEditResult::Done)
			{
				break;
			}

			Check(presentA == presentB && a.speedMmPerSecond == b.speedMmPerSecond && a.angleDegrees == b.angleDegrees, "the copy's platform numbers are the source's");
		}

		// ---- platform speed and angle, on the first sequence of the copy
		model::PlatformState state = { 1234, 90 };
		model::SequenceSpeed written = { 0, 0, 0 };
		HierarchyEditResult const set = HierarchyEdit_SetPlatform(&file, copyName, 0, state, &written);

		if (set == HierarchyEditResult::NoSequence)
		{
			printf("  (the hierarchy has no sequence: platform checks skipped)\n");
		}
		else
		{
			Check(set == HierarchyEditResult::Done, "the platform speed and angle are written: " + Words(set));

			model::SequenceSpeed read = { 0, 0, 0 };
			bool present = false;

			Check(HierarchyEdit_ReadPlatform(&file, copyName, 0, &read, &present) == HierarchyEditResult::Done && present && read.speedMmPerSecond == 1234 && read.angleDegrees == 90,
				"and read back");

			// the source's own sequence 0 is untouched
			HierarchyEditResult const rs = HierarchyEdit_ReadPlatform(&file, source, 0, &b, &presentB);

			HierarchyEdit_ReadPlatform(&file, source, 0, &a, &presentA);
			Check(rs == HierarchyEditResult::Done && (!presentA || a.speedMmPerSecond != 1234 || b.angleDegrees != 90 || source == copyName), "the source's sequence is not changed");
		}

		unsigned const generationNow = file.Generation();

		Check(HierarchyEdit_SetPlatform(&file, copyName, 100000, state, nullptr) == HierarchyEditResult::NoSequence && file.Generation() == generationNow, "a sequence number that is not there is refused");
		Check(HierarchyEdit_SetPlatform(&file, "no such hierarchy here", 0, state, nullptr) == HierarchyEditResult::NoHierarchy && file.Generation() == generationNow, "an unknown hierarchy is refused");

		// ---- saved, and read by the game's own loader
		std::string const saved = folder + "\\saved.rif";

		if (file.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
		{
			OpenLevelFile second;

			Check(second.Open(saved.c_str()) == LevelOpenResult::Opened, "the saved level is taken by the game's own loader");

			std::vector<std::string> reread;
			std::vector<model::SequenceSpeed> unused;

			Check(HierarchyEdit_Names(&second, &reread) == HierarchyEditResult::Done && reread.size() == names.size() + 1 && reread.back() == copyName, "the saved level holds the copy");

			if (set == HierarchyEditResult::Done)
			{
				model::SequenceSpeed read = { 0, 0, 0 };
				bool present = false;

				Check(HierarchyEdit_ReadPlatform(&second, copyName, 0, &read, &present) == HierarchyEditResult::Done && present && read.speedMmPerSecond == 1234 && read.angleDegrees == 90,
					"and its platform numbers");
			}

			DeleteFileA(saved.c_str());
			DeleteFileA((saved + ".bak").c_str());
		}
		else
		{
			Check(false, "the edited level is saved");
		}
	}
}

static LONG WINAPI Report(EXCEPTION_POINTERS * info)
{
	void * frames[24];
	USHORT const n = CaptureStackBackTrace(0, 24, frames, 0);
	char * const base = (char *)GetModuleHandleA(0);

	printf("  CRASH code %08lx at +%llx\n", info->ExceptionRecord->ExceptionCode, (unsigned long long)((char *)info->ExceptionRecord->ExceptionAddress - base));

	for (USHORT i = 0; i < n; i++)
	{
		printf("    +%llx\n", (unsigned long long)((char *)frames[i] - base));
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char ** argv)
{
	SetUnhandledExceptionFilter(Report);
	if (argc < 2)
	{
		// no level named (the suite runs every test with no arguments): nothing to test here, and that is said, like the other tests that need a level
		printf("usage: host_hierarchy_edit_test <level file> [<level file> ...]: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	setvbuf(stdout, 0, _IONBF, 0);

	std::string const folder = "hierarchy_edit_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), 0))
	{
		printf("  FAILED  the test couldn't make its folder\n");
		return 1;
	}

	for (int i = 1; i < argc; i++)
	{
		TestFile(argv[i], folder);
	}

	RemoveDirectoryA(folder.c_str());
	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
