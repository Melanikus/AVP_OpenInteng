// See hierarchy_mode.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "hierarchy_mode.h"

#include "sequence_names.h"

#include <stdlib.h>
#include <string.h>

#include "frame_sound_index.h"
#include "hierarchy_collections.h"
#include "hierarchy_name.h"
#include "sequence_speed.h"

namespace host
{
	namespace
	{
		char const *const kSelectHierarchyFirst = "Please select a hierarchy first";
		char const *const kSelectCollectionFirst = "Please select a shape set collection first";

		std::string Number(long long value)
		{
			return std::to_string(value);
		}

		char Fold(char c)
		{
			return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
		}

		bool Contains(std::string const &line, std::string const &part)
		{
			if (part.empty() || part.size() > line.size())
			{
				return false;
			}

			for (size_t at = 0; at + part.size() <= line.size(); at++)
			{
				size_t i = 0;

				while (i < part.size() && Fold(line[at + i]) == Fold(part[i]))
				{
					i++;
				}

				if (i == part.size())
				{
					return true;
				}
			}

			return false;
		}

		// The line a typed answer names: its place in the list (1 to the count) or part of a line's name (the first line that has it); -1 for none
		int Pick(HierarchyMode const &mode, std::string const &typed)
		{
			if (typed.empty())
			{
				return -1;
			}

			char *end = nullptr;
			long const place = strtol(typed.c_str(), &end, 10);

			if (end && *end == 0 && place >= 1 && (size_t)place <= mode.list.size())
			{
				return (int)(place - 1);
			}

			for (size_t i = 0; i < mode.list.size(); i++)
			{
				// the line is "<n>  <name>...": the number in front is not part of the name
				size_t const gap = mode.list[i].find("  ");

				if (Contains(gap == std::string::npos ? mode.list[i] : mode.list[i].substr(gap + 2), typed))
				{
					return (int)i;
				}
			}

			return -1;
		}

		// how a sequence is listed: the name the file holds for it (inteng's chooser shows that string: FUN_0048c640 / FUN_0048bc10), or its numbers when the file gives none
		std::string SequenceLine(SequenceInfo const &one)
		{
			return one.name.empty() ? model::SequenceWords(one.number, one.subNumber) + " (" + Number(one.number) + "." + Number(one.subNumber) + ")" : one.name;
		}

		bool WholeNumber(std::string const &typed, int32_t *value)
		{
			char *end = nullptr;
			long long const parsed = strtoll(typed.c_str(), &end, 10);

			if (typed.empty() || !end || *end != 0 || parsed < INT32_MIN || parsed > INT32_MAX)
			{
				return false;
			}

			*value = (int32_t)parsed;
			return true;
		}

		HierarchyStep Say(std::string const &words)
		{
			HierarchyStep step;

			step.answer = words;
			return step;
		}

		// inteng's message box (FUN_00474030 with the words, FUN_0057c800(100, 100)): Select Sequence's and Add delta's refusals (FUN_0048bc10, FUN_0048c640)
		HierarchyStep Box(std::string const &words)
		{
			HierarchyStep step;

			step.answer = words;
			step.messageBox = true;
			return step;
		}

		// inteng's two help texts (FUN_0048ab60: 0x5d58e4 and 0x5d5738, read from the exe), one line each
		std::vector<std::string> HelpLines(int page)
		{
			static char const *const first[] = { "H  Select hierarchy", "S  *Select sequence", "-  *Decrease sequence time", "+  *Increase sequence time", "P  *Start sequence", "O  *Stop sequence", ">  *Sequence forwards",
				"<  *Sequence backwards", "N  Previous key frame", "M  Next key frame", "", "Shift + commands marked with * applies to delta sequence", "", "0-9  Apply key frame flag", "Shift + 0-9  Remove key frame flag",
				"Q  Apply frame sound", "Shift + Q  Remove frame sound", "C  Toggle use of camera root (for hud weapons)", "Ctrl+R  Rotate sequence by 180 degrees", "D  Previous sequence", "F  Next sequence",
				"U,I  Alter detail level (shows lower poly count models)", "Shift+D  Set distance for current detail level", "Return  Save changes (Saves on exit anyway)", "", "T  Toggle Text", "W  Toggle wireframe" };
			static char const *const second[] = { "Boned object commands", "", "B  Toggle bone display", "V  Toggle vertex selection (Must select a bone first)", "Shift + V  Add selected vertices to selected bone", "",
				"Turret section commands", "", "K  Rotate Left", "L  Rotate Right", "Shift + T  Add 'turret delta; sequence", "R  Restart all seqences (set timer to 0)",
				"Y  If pressed , the play commands will affect the turret and delta sequences.", "   as well as the main sequence.Press again to turn off" };
			std::vector<std::string> out;

			if (page == 0)
			{
				for (char const *line : first)
				{
					out.push_back(line);
				}
			}
			else
			{
				for (char const *line : second)
				{
					out.push_back(line);
				}
			}

			return out;
		}

		HierarchyStep Ask(HierarchyMode *mode, HierarchyAsk ask, std::string const &title, std::string const &defaultText)
		{
			HierarchyStep step;

			mode->ask = ask;
			step.prompt = true;
			step.title = title;
			step.defaultText = defaultText;
			return step;
		}

		// inteng's Yes / No box (FUN_00474a40): the answer is "yes" or "no"
		HierarchyStep AskYesNo(HierarchyMode *mode, HierarchyAsk ask, std::string const &title)
		{
			HierarchyStep step = Ask(mode, ask, title, std::string());

			step.confirm = true;
			return step;
		}

		// inteng's message box (FUN_00474030 + FUN_0057c800, Ok), then the question `after` asks (asked again in inteng's loop)
		HierarchyStep BoxThen(std::string const &words, HierarchyStep after)
		{
			after.answer = words;
			after.messageBox = true;
			return after;
		}

		// inteng's text dialog (FUN_00474170): the words and one edit holding the number or text offered, Ok / Cancel
		HierarchyStep AskInDialog(HierarchyMode *mode, HierarchyAsk ask, std::string const &title, std::string const &defaultText)
		{
			HierarchyStep step = Ask(mode, ask, title, defaultText);

			step.textDialog = true;
			return step;
		}

		// The words after a sound chooser's own (0x5d4d54: six spaces, then the right click's words), as FUN_00481740 joins them
		std::string SoundChooserTitle(char const *words)
		{
			return std::string(words) + "      (Right click to listen to sound)";
		}

		std::string Why(HierarchyEditResult result, char const *why)
		{
			return why && *why ? std::string(why) : std::string(HierarchyEditWords(result));
		}

		// A chooser: the lines are "<place>  <words>"
		void Line(HierarchyMode *mode, std::string const &words, int32_t value)
		{
			mode->list.push_back(Number((long long)mode->list.size() + 1) + "  " + words);
			mode->listValues.push_back(value);
		}

		void ClearList(HierarchyMode *mode)
		{
			mode->list.clear();
			mode->listValues.clear();
		}

		// The collection with this number, from the level; false when the level cannot give the list or has no such one
		bool CollectionNamed(OpenLevelFile *file, int32_t number, model::ShapeSetCollection *out)
		{
			std::vector<model::ShapeSetCollection> all;

			if (HierarchyEdit_Collections(file, &all) != HierarchyEditResult::Done)
			{
				return false;
			}

			for (model::ShapeSetCollection const &each : all)
			{
				if (each.number == number)
				{
					*out = each;
					return true;
				}
			}

			return false;
		}

		// The shape sets a collection applies: its members that are alternate shape sets of the level, in the collection's order
		std::vector<int32_t> AppliedOf(OpenLevelFile *file, model::ShapeSetCollection const &collection)
		{
			std::vector<model::AlternateSetInfo> alternates;
			std::vector<int32_t> applied;

			if (HierarchyEdit_AlternateSets(file, &alternates) != HierarchyEditResult::Done)
			{
				return applied;
			}

			for (int32_t const member : collection.shapeSets)
			{
				for (model::AlternateSetInfo const &each : alternates)
				{
					if (each.number == member)
					{
						applied.push_back(member);
						break;
					}
				}
			}

			return applied;
		}

		// The name of an alternate shape set, or "shape set <n>" when the level has none of that number
		std::string SetWords(std::vector<model::AlternateSetInfo> const &alternates, int32_t number)
		{
			for (model::AlternateSetInfo const &each : alternates)
			{
				if (each.number == number)
				{
					return each.name + " (shape set " + std::to_string((long long)number) + ")";
				}
			}

			return "shape set " + std::to_string((long long)number);
		}

		// The current sequence's number and sub-number
		bool SequenceNumbers(OpenLevelFile *file, HierarchyMode const *mode, SequenceInfo *out)
		{
			std::vector<SequenceInfo> sequences;

			if (HierarchyEdit_Sequences(file, mode->hierarchy, &sequences) != HierarchyEditResult::Done || mode->sequence >= sequences.size())
			{
				return false;
			}

			*out = sequences[mode->sequence];
			return true;
		}

		// The platform write, when inteng does it: a hierarchy open with a current sequence, the "Save changes to sequence times" switch (0x48, DAT_00637120) on and the
		// platform switch (0x4c, DAT_00637118) on (0x489844-0x48990d)
		std::string WritePlatformIfDue(OpenLevelFile *file, HierarchyMode *mode, std::string const &said)
		{
			if (mode->hierarchy.empty() || !mode->sequenceSelected || !mode->saveTimesOn || !mode->platformOn)
			{
				return said + " (kept for the next write: it is written only when a sequence is selected and both Save changes to sequence times and the platform switch are on)";
			}

			model::SequenceSpeed written = { 0, 0, 0 };
			HierarchyEditResult const done = HierarchyEdit_SetPlatform(file, mode->hierarchy, mode->sequence, mode->platform, &written);

			if (done != HierarchyEditResult::Done)
			{
				return said + ", but the chunk was not written: " + HierarchyEditWords(done);
			}

			return said + " and written to the sequence (speed " + Number(written.speedMmPerSecond) + " mm/s, angle " + Number(written.angleDegrees) + ")";
		}
	}

	bool HierarchyMode_Does(int32_t command)
	{
		switch (command)
		{
		case 0x68: case 0x69: case 0x6a: case 0x6f: case 0x70: case 0x71: case 0x72: case 0x48:
		case 0x43: case 0x58:
		case 0xd: case 0xe: case 0xf: case 0x10: case 0x5e: case 0x5b:
		case 0x11: case 0x12: case 0x13: case 0x14: case 0x9: case 0xa: case 0x46: case 0x62:
		case 0x4d: case 0x4e: case 0x4f: case 0x50: case 0x51:
		case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f: case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25:
		case 0x26: case 0x27: case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f: case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36:
		case 0x45: case 0x5c: case 0x5d: case 0x3c: case 0x3d: case 0x38:
		case 0x07: case 0x63: case 0x61:
		case 0x03: case 0x04: case 0x49: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: case 0x59: case 0x4a: case 0x4b: case 0x4c:
			return true;
		}

		return false;
	}

	void HierarchyMode_Reset(HierarchyMode *mode)
	{
		*mode = HierarchyMode();
	}

	void HierarchyMode_Cancel(HierarchyMode *mode)
	{
		mode->ask = HierarchyAsk::None;
		ClearList(mode);
	}

	// FUN_0048b550(hierarchy, 0): the hierarchy becomes the current one and inteng goes straight on to Select Sequence (its last call, 0x48b83a, is FUN_0048bc10); when it is already the current one nothing is done
	HierarchyStep ChooseHierarchy(OpenLevelFile *file, HierarchyMode *mode, std::string const &name, bool again = false)
	{
		if (!again && mode->hierarchy == name)
		{
			return HierarchyStep();   // FUN_0048b550 0x48b61a: the player is up for that hierarchy already: nothing, no words
		}

		mode->hierarchy = name;
		mode->sequenceSelected = false;
		mode->sequence = 0;
		mode->deltaSelected = false;
		mode->playersMade++;   // a new player (FUN_00477450): its +0x88 is 0 (0x477603)

		// the delta frames as the open builds them (HD-13)
		mode->deltaFrameHeld.clear();

		std::vector<SequenceInfo> held;

		if (HierarchyEdit_Sequences(file, name, &held) == HierarchyEditResult::Done)
		{
			for (SequenceInfo const &one : held)
			{
				int32_t frame = -1;

				if (HierarchyEdit_DeltaFrame(file, name, one.number, one.subNumber, &frame) != HierarchyEditResult::Done)
				{
					frame = -1;
				}

				mode->deltaFrameHeld[(int64_t)one.number * 65536 + one.subNumber] = frame;
			}
		}

		mode->vertexMode = false;
		mode->vertices.clear();
		return HierarchyMode_Command(file, mode, 0x04);
	}

	// Attach shape to bones' hierarchy picker, FUN_0048b1e0("Attach to which hierarchy?") (0x48ea58): the file's OBJCHIER chunks by their OBHIERNM names, the LIST dialog even for one
	// (FUN_00475bf0, width 600, FUN_0057c800(10, 10)); none: 0, nothing happens
	HierarchyStep AttachHierarchyPicker(OpenLevelFile *file, HierarchyMode *mode)
	{
		std::vector<std::string> hierarchies;

		if (HierarchyEdit_Names(file, &hierarchies) != HierarchyEditResult::Done || hierarchies.empty())
		{
			return HierarchyStep();
		}

		for (size_t i = 0; i < hierarchies.size(); i++)
		{
			Line(mode, hierarchies[i], (int32_t)i);
		}

		return Ask(mode, HierarchyAsk::AttachHierarchy, "Attach to which hierarchy?", std::string());
	}

	HierarchyStep HierarchyMode_Command(OpenLevelFile *file, HierarchyMode *mode, int32_t command)
	{
		mode->ask = HierarchyAsk::None;
		ClearList(mode);

		switch (command)
		{
		case 0x03:   // Select Hierarchy
		{
			std::vector<std::string> names;
			HierarchyEditResult const read = HierarchyEdit_Names(file, &names);

			if (read != HierarchyEditResult::Done)
			{
				return Say(std::string("Select Hierarchy: ") + HierarchyEditWords(read));
			}

			// FUN_0048b8a0: none found, nothing is shown and nothing changes; exactly one is opened at once (FUN_0048b550(it, 0)); two or more are offered in the list dialog "Select Hierarchy". The names are the
			// OBJCHIER objects of the OPENED FILE: a model file of avp_huds is not offered here (the earlier typed model name was a substitute and is gone).
			if (names.empty())
			{
				return Say("Select Hierarchy: the open file holds no hierarchy, so nothing happens (as in inteng)");
			}

			if (names.size() == 1)
			{
				return ChooseHierarchy(file, mode, names[0]);
			}

			for (size_t i = 0; i < names.size(); i++)
			{
				Line(mode, names[i], (int32_t)i);
			}

			return Ask(mode, HierarchyAsk::SelectHierarchy, "Select Hierarchy", std::string());
		}

		case 0x07:   // Add 'turret delta' (FUN_0048c070): a hierarchy, a main sequence, then the chooser "Select Turret Sequence" over the hierarchy's sequences (the caller checks the turret section)
		{
			if (mode->hierarchy.empty())
			{
				return Box("Please select hierarchy first");
			}

			if (!mode->sequenceSelected)
			{
				return Box("Please select a main sequence first");   // FUN_0048c070's own words (0x5d5c4c), a message box
			}

			std::vector<SequenceInfo> sequences;
			HierarchyEditResult const read = HierarchyEdit_Sequences(file, mode->hierarchy, &sequences);

			if (read != HierarchyEditResult::Done || sequences.empty())
			{
				return read != HierarchyEditResult::Done ? Say(std::string("Add turret delta: ") + HierarchyEditWords(read)) : Box("Hierarchy has no sequences");
			}

			for (size_t i = 0; i < sequences.size(); i++)
			{
				Line(mode, SequenceLine(sequences[i]), (int32_t)i);
			}

			return Ask(mode, HierarchyAsk::TurretSequence, "Select Turret Sequence: type its number (1 to " + Number((long long)sequences.size()) + ")", std::string());
		}

		case 0x04:   // Select Sequence
		{
			if (mode->hierarchy.empty())
			{
				std::vector<std::string> only;

				if (HierarchyEdit_Names(file, &only) == HierarchyEditResult::Done && only.size() == 1)
				{
					mode->hierarchy = only[0];
				}
				else
				{
					return Box("Please select hierarchy first");   // inteng's message box at 100, 100 (FUN_00474030 + FUN_0057c800(100, 100))
				}
			}

			std::vector<SequenceInfo> sequences;
			HierarchyEditResult const read = HierarchyEdit_Sequences(file, mode->hierarchy, &sequences);

			if (read != HierarchyEditResult::Done)
			{
				return Say(std::string("Select Sequence: ") + HierarchyEditWords(read));
			}

			if (sequences.empty())
			{
				return Say("The hierarchy has no sequences");
			}

			for (size_t i = 0; i < sequences.size(); i++)
			{
				Line(mode, SequenceLine(sequences[i]), (int32_t)i);
			}

			return Ask(mode, HierarchyAsk::SelectSequence, "Select Sequence", std::string());
		}

		case 0x49:   // Copy Hierarchy
			if (mode->hierarchy.empty())
			{
				return Say("Copy Hierarchy: no hierarchy is selected, so nothing happens (as in inteng): use Select Hierarchy first");
			}

			return AskInDialog(mode, HierarchyAsk::CopyName, "Type name of new hierarchy", std::string());   // FUN_00474bc0 (0x5d49b4), empty

		case 0x52:   // Select Collection
		{
			if (mode->hierarchy.empty())
			{
				return Say("Select Collection: no hierarchy is selected, so nothing happens (as in inteng): use Select Hierarchy first");
			}

			std::vector<model::ShapeSetCollection> all;
			HierarchyEditResult const read = HierarchyEdit_Collections(file, &all);

			if (read != HierarchyEditResult::Done)
			{
				return Say(std::string("Select Collection: ") + HierarchyEditWords(read));
			}

			if (all.empty())
			{
				return Say("The level has no shape set collections: Create Collection makes one");
			}

			for (model::ShapeSetCollection const &each : all)
			{
				Line(mode, each.name, each.number);   // the row is the collection's name (+0x28), as inteng's
			}

			return Ask(mode, HierarchyAsk::SelectCollection, "Select Shape Collection", std::string());   // inteng's list dialog (FUN_00475bf0, 0x5d5b9c)
		}

		case 0x53:   // Create Collection
			if (mode->hierarchy.empty())
			{
				return Say(kSelectHierarchyFirst);
			}

			return AskInDialog(mode, HierarchyAsk::CreateName, "Name of new shape collection", std::string());   // FUN_00474bc0 (0x5d5bb4), empty

		case 0x57:   // Rename Collection
		{
			if (!mode->collectionSelected)
			{
				return Say(kSelectCollectionFirst);
			}

			model::ShapeSetCollection current;

			if (!CollectionNamed(file, mode->collection, &current))
			{
				mode->collectionSelected = false;
				return Say("The selected shape set collection is no longer in the level");
			}

			return AskInDialog(mode, HierarchyAsk::RenameName, "New name for shape collection", current.name);   // FUN_00474bc0 (0x5d4938), the current name
		}

		case 0x54:   // Delete Collection
			if (!mode->collectionSelected)
			{
				return Say(kSelectCollectionFirst);
			}

			return AskYesNo(mode, HierarchyAsk::DeleteYes, "Delete the current shape set collection?");   // FUN_00474a40 (0x5d48e0)

		case 0x59:   // Collection Voice Index
		{
			if (!mode->collectionSelected)
			{
				return Say(kSelectCollectionFirst);
			}

			model::ShapeSetCollection current;

			if (!CollectionNamed(file, mode->collection, &current))
			{
				mode->collectionSelected = false;
				return Say("The selected shape set collection is no longer in the level");
			}

			return AskInDialog(mode, HierarchyAsk::VoiceIndex, "Select the voice index for this collection", Number(current.voiceIndex));   // FUN_00474170 (0x5d4964)
		}

		case 0x43:   // Select Single Set: "Original" (no set) or one alternate shape set becomes the applied list (a view change: nothing in the file changes)
		{
			// case 0x43 (0x488f12-0x4896c0, reassess/hierarchy-edit-frost.md HF-0x43): no hierarchy chosen (DAT_0063711c): nothing, no words; the file's alternate shape sets (OBHALTSH,
			// FUN_004316e0) none: nothing; else the rows "Original" (0x5d5724, value 0) and each set's NAME (+0x28) alone, the sorted list window "Select Shape Set" (0x5d5710, FUN_00475bf0
			// width 0xe1), FUN_0057c800(10, 10); a row: FUN_00477b40(that set or 0, the detail level); Escape: nothing
			if (mode->hierarchy.empty())
			{
				return HierarchyStep();
			}

			std::vector<model::AlternateSetInfo> alternates;

			if (HierarchyEdit_AlternateSets(file, &alternates) != HierarchyEditResult::Done || alternates.empty())
			{
				return HierarchyStep();
			}

			Line(mode, "Original", -1);

			for (model::AlternateSetInfo const &each : alternates)
			{
				Line(mode, each.name, each.number);
			}

			return Ask(mode, HierarchyAsk::SelectSingleSet, "Select Shape Set", std::string());
		}

		case 0x58:   // Set Properties: the single applied shape set's "Female" flag
		{
			// case 0x58 (0x489b4c-0x489bef, HF-0x58): no player: nothing; the applied list not exactly one set (+0x2c != 1): the message box "Please select a single shape set" (0x5d5684);
			// else that set's window FUN_0048d6a0 (600 x 420, "Shape set properties", the label "Female" at (0x12, 0x28), its check box FUN_0057d670 at (0x76, 0x28) holding the set's flags
			// bit 0 (+0x40, 0x48d84b), "Ok" at (200, 0x172) and "Cancel" at (0x15e, 0x172)), run by FUN_0057c800(10, 10)
			if (mode->hierarchy.empty())
			{
				return HierarchyStep();
			}

			if (mode->applied.size() != 1)
			{
				return Box("Please select a single shape set");
			}

			uint32_t flags = 0;
			HierarchyEditResult const read = HierarchyEdit_ReadShapeSetFlags(file, mode->applied.front(), &flags);

			if (read != HierarchyEditResult::Done)
			{
				mode->applied.clear();
				return Say("Set Properties: " + std::string(HierarchyEditWords(read)));
			}

			HierarchyStep window = Ask(mode, HierarchyAsk::FemaleYes, "Shape set properties", std::string());

			window.femaleForm = true;
			window.femaleChecked = (flags & 1) != 0;
			return window;
		}

		case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f: case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25:
		case 0x26: case 0x27: case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f: case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36:
		{
			// Add / Remove keyframe flag (FUN_004789a0 / FUN_00478a30): flag number n = command - 0x15 to add, command - 0x26 to remove, on the root section's frame that holds the position
			static char const *const names[17] = { "Blocking", "Cause Damage", "Can Be Blocked", "Invincible", "Left Step", "Right Step", "Hit Floor Thud", "Hit Floor Sound", "Metal Chink",
				"Jump Now", "Combat Roll Left", "Combat Roll Right", "Swipe Sound", "Start Pushing", "Stop Pushing", "Start Moving", "Stop Moving" };
			bool const add = command <= 0x25;
			int const flag = add ? command - 0x15 : command - 0x26;
			std::string const what = std::string(add ? "Add" : "Remove") + " keyframe flag " + Number(flag) + " " + names[flag];

			if (mode->hierarchy.empty() || !mode->sequenceSelected)
			{
				return Say(what + ": select a hierarchy and a sequence first (inteng does nothing without them)");
			}

			SequenceInfo number = { 0, 0, 0, false };

			if (!SequenceNumbers(file, mode, &number))
			{
				return Say(what + ": the sequence could not be read");
			}

			size_t frame = 0;
			HierarchyEditResult const done = HierarchyEdit_ChangeFrameAt(file, mode->hierarchy, number.number, number.subNumber, mode->position, add ? 1u << flag : 0u, add ? 0u : 1u << flag, -1, &frame);

			if (done != HierarchyEditResult::Done)
			{
				return Say(what + ": " + (done == HierarchyEditResult::NoSequence ? "no frame of the root section holds the position" : HierarchyEditWords(done)));
			}

			return Say(what + " (frame " + Number((long long)frame + 1) + ")");
		}

		case 0x4d:   // New sound
		{
			std::vector<SoundInfo> sounds;
			HierarchyEditResult const read = HierarchyEdit_Sounds(file, &sounds);

			if (read != HierarchyEditResult::Done)
			{
				return Say(std::string("New sound: ") + HierarchyEditWords(read));
			}

			std::vector<int32_t> used;

			for (SoundInfo const &each : sounds)
			{
				used.push_back(each.index);
			}

			// FUN_00481440: the text dialog "Select sound index" (0x5d4d14) holding the first index no sound has, counting from 1 (0x48145b-0x48147a)
			return AskInDialog(mode, HierarchyAsk::SoundIndex, "Select sound index", Number(model::FirstFreeSoundIndex(used)));
		}

		case 0x4e: case 0x4f: case 0x50:   // Edit sound, Delete sound, Apply frame sound: each first asks which sound
		{
			char const *const words = command == 0x4e ? "Select sound to edit" : command == 0x4f ? "Delete which sound?" : "Apply which sound?";

			if (command == 0x50 && (mode->hierarchy.empty() || !mode->sequenceSelected))
			{
				return Say("Apply frame sound: select a hierarchy and a sequence first (inteng does nothing without them)");
			}

			std::vector<SoundInfo> sounds;
			HierarchyEditResult const read = HierarchyEdit_Sounds(file, &sounds);

			if (read != HierarchyEditResult::Done)
			{
				return Say(std::string(words) + ": " + HierarchyEditWords(read));
			}

			if (sounds.empty())
			{
				return Box("Please add a sound first");   // FUN_00481740: the message box (0x5d4d88), nothing chosen
			}

			for (SoundInfo const &each : sounds)
			{
				char numbered[16];

				_snprintf_s(numbered, sizeof(numbered), _TRUNCATE, "%03d", each.index);
				Line(mode, std::string(numbered) + " " + each.wav, each.index);
			}

			// FUN_00481740: the list dialog FUN_00475bf0 titled "<words>      (Right click to listen to sound)" (0x5d4d54), a row "%03d %s" (0x5d4d80) per sound
			return Ask(mode, command == 0x4e ? HierarchyAsk::SoundEditPick : command == 0x4f ? HierarchyAsk::SoundDeletePick : HierarchyAsk::SoundApplyPick,
				SoundChooserTitle(words), std::string());
		}

		case 0x51:   // Clear frame sound
		{
			if (mode->hierarchy.empty() || !mode->sequenceSelected)
			{
				return Say("Clear frame sound: select a hierarchy and a sequence first (inteng does nothing without them)");
			}

			SequenceInfo number = { 0, 0, 0, false };

			if (!SequenceNumbers(file, mode, &number))
			{
				return Say("Clear frame sound: the sequence could not be read");
			}

			size_t frame = 0;
			HierarchyEditResult const done = HierarchyEdit_ChangeFrameAt(file, mode->hierarchy, number.number, number.subNumber, mode->position, 0, 0, 0, &frame);

			if (done != HierarchyEditResult::Done)
			{
				return Say(std::string("Clear frame sound: ") + (done == HierarchyEditResult::NoSequence ? "no frame of the root section holds the position" : HierarchyEditWords(done)));
			}

			return Say("Frame sound cleared (frame " + Number((long long)frame + 1) + ")");
		}

		case 0xd: case 0xe:   // the keys "+" (Increase sequence time) and "-" (Decrease): 100 ms more or less, never below 100 (0x488b6d-0x488bf6)
		{
			if (mode->hierarchy.empty() || !mode->sequenceSelected)
			{
				return Say("Sequence time: select a hierarchy and a sequence first (inteng does nothing without them)");
			}

			int32_t const next = command == 0xd ? mode->sequenceMs + 100 : (mode->sequenceMs - 100 < 100 ? 100 : mode->sequenceMs - 100);

			mode->sequenceMs = next;

			std::string said = "Sequence time " + Number(next) + " ms";

			if (!mode->saveTimesOn)
			{
				return Say(said + " (not saved: Save changes to sequence times is off)");
			}

			HierarchyEditResult const done = HierarchyEdit_SetSequenceTime(file, mode->hierarchy, mode->sequence, next);

			if (done != HierarchyEditResult::Done)
			{
				return Say(said + ", but the chunk was not written: " + HierarchyEditWords(done));
			}

			said += " and written to the sequence";

			if (mode->platformOn)
			{
				said = WritePlatformIfDue(file, mode, said);
			}

			return Say(said);
		}

		case 0xf: case 0x10:   // Shift + "+" and Shift + "-": the same for the DELTA object (inteng's view +0x8): 100 ms more or less, never below 100 (0x488c05-0x488c82); its length follows; with Save changes to sequence times on, the delta sequence's OBASEQTM is written (FUN_0047da50 on the delta object). No platform write: that is the main sequence's alone.
		{
			if (!mode->deltaSelected)
			{
				return Say("Delta time: choose a delta sequence with Add delta first (inteng does nothing without one)");
			}

			int32_t const next = command == 0xf ? mode->deltaMs + 100 : (mode->deltaMs - 100 < 100 ? 100 : mode->deltaMs - 100);

			mode->deltaMs = next;
			mode->deltaTimeRequested = true;

			std::string said = "Delta time " + Number(next) + " ms";

			if (!mode->saveTimesOn)
			{
				return Say(said + " (not saved: Save changes to sequence times is off)");
			}

			HierarchyEditResult const done = HierarchyEdit_SetSequenceTime(file, mode->hierarchy, mode->deltaPlace, next);

			if (done != HierarchyEditResult::Done)
			{
				return Say(said + ", but the chunk was not written: " + HierarchyEditWords(done));
			}

			return Say(said + " and written to the delta sequence");
		}

		case 0x11: case 0x12: case 0x13: case 0x14:   // Not a delta sequence / Deltas relative to the first, middle, current frame (FUN_0047eee0 over the section tree)
		{
			static char const *const names[4] = { "Not a delta sequence", "Deltas relative to first frame", "Deltas relative to middle frame", "Deltas relative to current frame" };
			std::string const what = names[command - 0x11];

			if (mode->hierarchy.empty() || !mode->sequenceSelected)
			{
				return Say(what + ": select a hierarchy and a sequence first (inteng does nothing without them)");
			}

			SequenceInfo number = { 0, 0, 0, false };

			if (!SequenceNumbers(file, mode, &number))
			{
				return Say(what + ": the sequence could not be read");
			}

			size_t sections = 0;
			// inteng's 0x13 (middle) runs on into 0x14's code (the listing at 0x488d02-0x488d08 has no jump between them): after it has set the middle frame it clears that and sets the CURRENT
			// frame, so "middle" ends as "current". Kept as inteng has it (named in unfinished-in-inteng.md).
			DeltaMode const mode2 = command == 0x11 ? DeltaMode::None : command == 0x12 ? DeltaMode::First : DeltaMode::Current;
			HierarchyEditResult const done = HierarchyEdit_SetDelta(file, mode->hierarchy, number.number, number.subNumber, mode2, mode->position, &sections);

			if (done != HierarchyEditResult::Done)
			{
				return Say(what + ": " + (done == HierarchyEditResult::NoSequence ? "no section has frames for this sequence" : HierarchyEditWords(done)));
			}

			// HD-13: only the middle / current paths write the held delta frame (0x13 runs on into 0x14, so both end at the current frame); "not a delta" and "first frame" leave it
			if (command == 0x13 || command == 0x14)
			{
				int32_t frame = -1;

				if (HierarchyEdit_DeltaFrame(file, mode->hierarchy, number.number, number.subNumber, &frame) == HierarchyEditResult::Done)
				{
					mode->deltaFrameHeld[(int64_t)number.number * 65536 + number.subNumber] = frame;
				}
			}

			return Say(what + " (" + Number((long long)sections) + " sections)" + (command == 0x13 ? " (as inteng does it, the current frame is what ends up the reference)" : ""));
		}

		case 0x9:   // Add delta (FUN_0048c640): "Select Delta Sequence" over the sequences the file flags as deltas
		{
			if (mode->hierarchy.empty())
			{
				return Box("Please select hierarchy first");
			}

			if (!mode->sequenceSelected)
			{
				return Box("Please select main sequence first");
			}

			std::vector<SequenceInfo> sequences;
			HierarchyEditResult const read = HierarchyEdit_Sequences(file, mode->hierarchy, &sequences);

			if (read != HierarchyEditResult::Done || sequences.empty())
			{
				return read != HierarchyEditResult::Done ? Say(std::string("Add delta: ") + HierarchyEditWords(read)) : Box("Hierarchy has no sequences");
			}

			// FUN_00478930 != -1: the HELD delta frame (HD-13), read from the file only for a sequence the open did not see
			for (size_t i = 0; i < sequences.size(); i++)
			{
				int32_t frame = -1;
				const std::map<int64_t, int32_t>::const_iterator held = mode->deltaFrameHeld.find((int64_t)sequences[i].number * 65536 + sequences[i].subNumber);

				if (held != mode->deltaFrameHeld.end())
				{
					frame = held->second;
				}
				else if (HierarchyEdit_DeltaFrame(file, mode->hierarchy, sequences[i].number, sequences[i].subNumber, &frame) != HierarchyEditResult::Done)
				{
					frame = -1;
				}

				if (frame >= 0)
				{
					Line(mode, SequenceLine(sequences[i]), (int32_t)i);
				}
			}

			if (mode->list.empty())
			{
				return Box("None of the sequences have been flagged as delta sequences");
			}

			return Ask(mode, HierarchyAsk::SelectDelta, "Select Delta Sequence (type its place, or part of its name)", std::string());
		}

		case 0x62:   // Attach shape to bones (FUN_0048e610): an object (the only one is taken at once), then a hierarchy
		{
			std::vector<std::string> objects;
			HierarchyEditResult const read = HierarchyEdit_ObjectNames(file, &objects);

			if (read != HierarchyEditResult::Done)
			{
				return Say(std::string("Attach shape to bones: ") + HierarchyEditWords(read));
			}

			// FUN_004be4f0: every RBOBJECT of the file. None: the message box "Hmm.\nNo objects." (0x5d5fc8, FUN_00474030 + FUN_0057c800(100, 100)); one: taken without asking (0x48ea44);
			// more: the LIST dialog "Select object to attach to bones" (0x5d5fa4, FUN_00475bf0 width 0xe1, FUN_0057c800(10, 10)), Escape nothing (0x48e926)
			if (objects.empty())
			{
				return Box("Hmm.\nNo objects.");
			}

			if (objects.size() == 1)
			{
				mode->attachObject = objects[0];
				return AttachHierarchyPicker(file, mode);
			}

			for (size_t i = 0; i < objects.size(); i++)
			{
				Line(mode, objects[i], (int32_t)i);
			}

			return Ask(mode, HierarchyAsk::AttachObject, "Select object to attach to bones", std::string());
		}

		case 0x46:   // Calculate Bounding Box (FUN_004796f0): needs a hierarchy; the caller does the playing
			if (mode->hierarchy.empty())
			{
				return Say("Please select a hierarchy first");
			}

			mode->boundsRequested = true;
			return Say("Calculating the bounding box: every sequence is played through its keyframes");

		case 0x63:   // V, Toggle vertex selection (FUN_004889d0 case 0x63, DAT_00637130): "Must select a bone first"; needs the object that was attached to the bones (Attach shape to bones)
		{
			if (mode->hierarchy.empty())
			{
				return Say("Please select a hierarchy first");
			}

			if (mode->section.empty())
			{
				return Say("Must select a bone first (click a section of the model)");
			}

			if (mode->vertexMode)
			{
				mode->vertexMode = false;
				mode->vertices.clear();
				return Say("Vertex selection: off");
			}

			if (mode->attachObject.empty())
			{
				return Say("Vertex selection: there is no attached object to take points of: use Attach shape to bones first");
			}

			mode->vertexMode = true;
			mode->vertices.clear();
			return Say("Vertex selection: on (click the points of " + mode->attachObject + "; Shift + V puts them on the bone " + mode->section + ")");
		}

		case 0x61:   // Shift + V, Add selected vertices to selected bone (FUN_0047acf0 -> FUN_0041f8c0 for each point; the points' selection marks are cleared)
		{
			if (!mode->vertexMode || mode->section.empty())
			{
				return Say("Add selected vertices to the bone: turn vertex selection on (V) with a bone selected first");
			}

			if (mode->vertices.empty())
			{
				return Say("Add selected vertices to the bone: no vertices are selected");
			}

			char const *why = nullptr;
			size_t moved = 0;
			std::vector<int32_t> const chosen = mode->vertices;
			HierarchyEditResult const done = HierarchyEdit_MovePointsToBone(file, mode->attachObject, mode->hierarchy, mode->section, chosen, &why, &moved);

			if (done != HierarchyEditResult::Done)
			{
				return Say(std::string("Add selected vertices to the bone: ") + Why(done, why));
			}

			mode->vertices.clear();
			return Say(Number((long long)chosen.size()) + " vertices put on the bone " + mode->section + " (" + Number((long long)moved) + " changed bone)");
		}

		case 0xa:   // Remove delta
			mode->removeDeltasRequested = true;
			mode->deltaSelected = false;
			return Say("Remove delta: the deltas playing over the main sequence are taken away");

		case 0x5b:   // Copy between hierarchies: the frame sounds of the sequences two hierarchies share (FUN_0048d8b0)
		{
			std::vector<std::string> names;
			HierarchyEditResult const read = HierarchyEdit_Names(file, &names);

			if (read != HierarchyEditResult::Done)
			{
				return Say(std::string("Copy between hierarchies: ") + HierarchyEditWords(read));
			}

			if (names.size() < 2)
			{
				return Say("Copy between hierarchies: the file has fewer than two hierarchies");
			}

			for (size_t i = 0; i < names.size(); i++)
			{
				Line(mode, names[i], (int32_t)i);
			}

			return Ask(mode, HierarchyAsk::CopyFrom, "Copy sounds FROM which hierarchy? (type its place, or part of its name)", std::string());
		}

		case 0x5e:   // Change object for section (FUN_0048df60)
		{
			if (mode->hierarchy.empty())
			{
				return Say("Please select a hierarchy first");
			}

			if (mode->section.empty())
			{
				return Say("Please select an object first");
			}

			return Ask(mode, HierarchyAsk::ChangeObjectList, "Would you like to select the new object for this section from a list? (yes or no)", "yes");
		}

		case 0x4a:   // Set speed
			return AskInDialog(mode, HierarchyAsk::Speed, "Platform speed in mm/s", Number(mode->platform.lastSpeed));   // FUN_00474170 (0x5d56c0), DAT_005d4e4c

		case 0x45:   // Adaptive Degradation (0x4896e5): DAT_006370f4 flips and the item's tick follows; each frame it is on, FUN_00479b80 sets the detail level from the distance
			mode->adaptiveDegradation = !mode->adaptiveDegradation;
			return Say(std::string("Adaptive Degradation: ") + (mode->adaptiveDegradation ? "on (the view picks the model's detail level by the distance of the camera: a model with several detail levels, set with U and I and Shift+D)" : "off"));

		case 0x5c:   // Dummy Objects (0x48970a): DAT_006370f8 flips; the cube and axes at the model's place show when it is on
			mode->dummyObjects = !mode->dummyObjects;
			return Say(std::string("Dummy Objects: ") + (mode->dummyObjects ? "on (a cube of " + Number(mode->dummyObjectSize / 100) + " mm and three axes are shown at the model's place)" : std::string("off")));

		case 0x5d:   // Dummy Object Size (0x48972e): "Dummy object size in mm", the size now / 100 offered; a number above 0 is kept x 100
			return AskInDialog(mode, HierarchyAsk::DummySize, "Dummy object size in mm", Number(mode->dummyObjectSize / 100));

		case 0x3c:   // Alien (0x488e86): the view's projection scale +0x34 and +0x38 become 0xa0
			mode->viewKind = 1;
			return Say("Alien: the view's projection scale is 160 (the alien's wider view)");

		case 0x3d:   // Marine (0x488e73): ... 0x140
			mode->viewKind = 2;
			return Say("Marine: the view's projection scale is 320 (the marine's view)");

		case 0x38:   // Help (FUN_0048ab60): two message boxes, the model's keys and the boned and turret commands
			mode->helpPage = 0;
			mode->list = HelpLines(0);
			mode->listValues.clear();
			return Ask(mode, HierarchyAsk::Help, "Hierarchy help (Enter: next page, Esc: close)", std::string());

		case 0x4b:   // Set angle
			return AskInDialog(mode, HierarchyAsk::Angle, "Platform angle (0-360)", Number(mode->platform.lastAngle));   // FUN_00474170 (0x5d56a8), DAT_00637114

		case 0x68: case 0x69: case 0x6a: case 0x6f: case 0x70: case 0x71: case 0x72:   // Sequence Flags: Type (Full, Upper, Lower), No Interpolation, Half Frame Rate
		{
			// inteng does these only with a hierarchy open and a sequence current (0x489937-0x489a4b); the bits are the game's (animation_objects.hpp:209-214)
			uint32_t setMask = 0;
			uint32_t clearMask = 0;
			char const *what = "";

			switch (command)
			{
			case 0x68: clearMask = 3; what = "Sequence Type: Full"; break;
			case 0x69: setMask = 1; clearMask = 2; what = "Sequence Type: Upper"; break;
			case 0x6a: clearMask = 1; setMask = 2; what = "Sequence Type: Lower"; break;
			case 0x6f: setMask = 0x10; what = "No Interpolation added"; break;
			case 0x70: clearMask = 0x10; what = "No Interpolation removed"; break;
			case 0x71: setMask = 0x20; what = "Half Frame Rate added"; break;
			default: clearMask = 0x20; what = "Half Frame Rate removed"; break;
			}

			if (mode->hierarchy.empty() || !mode->sequenceSelected)
			{
				return Say(std::string(what) + ": select a hierarchy and a sequence first (inteng does nothing without them)");
			}

			uint32_t flags = 0;
			HierarchyEditResult const done = HierarchyEdit_ChangeSequenceFlags(file, mode->hierarchy, mode->sequence, setMask, clearMask, &flags);

			if (done != HierarchyEditResult::Done)
			{
				return Say(std::string(what) + ": " + HierarchyEditWords(done));
			}

			char shown[16];

			_snprintf_s(shown, sizeof(shown), _TRUNCATE, "%x", flags);
			return Say(std::string(what) + " (the sequence's flags are now 0x" + shown + ")");
		}

		case 0x48:   // Save changes to sequence times: a switch (its tick is the menu item's); inteng flips it and nothing else (the keys that change a sequence's time are not built)
			mode->saveTimesOn = !mode->saveTimesOn;
			return Say(std::string("Save changes to sequence times: ") + (mode->saveTimesOn ? "on" : "off"));

		case 0x4c:   // Toggle on/off
			mode->platformOn = !mode->platformOn;
			return Say(std::string("Platform: ") + (mode->platformOn ? "on" : "off"));

		case 0x55:   // Add to Collection (FUN_00479f20, HF-0x55): no collection: the message box (0x5d4864); the sets not applied by NAME, "Add which shape set?" (0x5d4820); none: the box (0x5d4838)
		{
			if (!mode->collectionSelected)
			{
				return Box("You must select a shape collection first");
			}

			model::ShapeSetCollection current;
			std::vector<model::AlternateSetInfo> alternates;
			std::vector<model::AlternateSetInfo> offered;

			if (!CollectionNamed(file, mode->collection, &current) || HierarchyEdit_AlternateSets(file, &alternates) != HierarchyEditResult::Done)
			{
				return Say("Add to Collection: the shape set collections or the alternate shape sets could not be read");
			}

			// inteng offers the shape sets that are not in its applied list (view +0x28)
			model::ShapeSetCollection appliedAsCollection = current;

			appliedAsCollection.shapeSets = mode->applied;
			model::OfferedShapeSets(alternates, appliedAsCollection, &offered);

			if (offered.empty())
			{
				return Box("All shape sets have already been applied");
			}

			for (model::AlternateSetInfo const &each : offered)
			{
				Line(mode, each.name, each.number);
			}

			return Ask(mode, HierarchyAsk::AddSet, "Add which shape set?", std::string());
		}

		case 0x56:   // Remove from Collection (FUN_0047a380, HF-0x56): no collection: the box (0x5d4864); no applied set: the box (0x5d48a8); the applied sets by NAME, "Remove which shape set?" (0x5d4890)
		{
			if (!mode->collectionSelected)
			{
				return Box("You must select a shape collection first");
			}

			model::ShapeSetCollection current;
			std::vector<model::AlternateSetInfo> alternates;

			if (!CollectionNamed(file, mode->collection, &current) || HierarchyEdit_AlternateSets(file, &alternates) != HierarchyEditResult::Done)
			{
				return Say("Remove from Collection: the shape set collections or the alternate shape sets could not be read");
			}

			// inteng lists the shape sets in its applied list (view +0x28)
			if (mode->applied.empty())
			{
				return Box("This shape collection contains no alternate shape sets");
			}

			for (int32_t const member : mode->applied)
			{
				std::string name = SetWords(alternates, member);

				for (model::AlternateSetInfo const &each : alternates)
				{
					name = each.number == member ? each.name : name;
				}

				Line(mode, name, member);
			}

			return Ask(mode, HierarchyAsk::RemoveSet, "Remove which shape set?", std::string());
		}
		}

		return HierarchyStep();
	}

	HierarchyStep HierarchyMode_Typed(OpenLevelFile *file, HierarchyMode *mode, std::string const &typed)
	{
		HierarchyAsk const ask = mode->ask;
		char const *why = nullptr;

		mode->ask = HierarchyAsk::None;

		switch (ask)
		{
		case HierarchyAsk::None:
			return HierarchyStep();

		case HierarchyAsk::SelectHierarchy:
		{
			int const at = Pick(*mode, typed);

			if (at < 0)
			{
				ClearList(mode);
				return Say("Select Hierarchy: nothing matches \"" + typed + "\", nothing selected");
			}

			std::string const picked = mode->list[(size_t)at].substr(mode->list[(size_t)at].find("  ") + 2);

			ClearList(mode);
			return ChooseHierarchy(file, mode, picked);
		}

		case HierarchyAsk::OpenModel:
		{
			int const at = Pick(*mode, typed);

			if (at < 0)
			{
				ClearList(mode);
				return Say("Open model: nothing matches \"" + typed + "\", nothing opened");
			}

			mode->modelRequested = mode->models[(size_t)mode->listValues[(size_t)at]];
			ClearList(mode);
			return Say("Opening model " + mode->modelRequested);
		}

		case HierarchyAsk::SoundIndex:
		{
			// FUN_00481440 0x4814af-0x4815e9: the edit's number (FUN_00474340); below 1 or above 127 the message "Index must be between 1 and 127" (0x5d4cd0), an index a sound has already
			// "That index is already being used" (0x5d4cf0), and nothing more (no second asking); else the new sound, whose own Sound Edit window opens at once (FUN_004ebc80 calls
			// FUN_004ebd60 at 0x4ebcfd: the window's Cancel leaves the start values), at the end of the list
			int32_t const index = HierarchyMode_EditNumber(typed);

			if (index < 1 || index > 0x7f)
			{
				return Box("Index must be between 1 and 127");
			}

			std::vector<SoundInfo> sounds;
			HierarchyEditResult const read = HierarchyEdit_Sounds(file, &sounds);

			if (read != HierarchyEditResult::Done)
			{
				return Say(std::string("New sound: ") + HierarchyEditWords(read));
			}

			for (SoundInfo const &each : sounds)
			{
				if (each.index == index)
				{
					return Box("That index is already being used");
				}
			}

			HierarchyEditResult const done = HierarchyEdit_NewSound(file, index, mode->levelScale, &why);

			if (done != HierarchyEditResult::Done)
			{
				return Say("New sound: " + Why(done, why));
			}

			sounds.clear();
			HierarchyEdit_Sounds(file, &sounds);

			for (SoundInfo const &each : sounds)
			{
				if (each.index == index)
				{
					HierarchyStep step = Say("Sound " + Number(index) + " added");

					mode->soundBeingEdited = each;
					step.soundForm = true;
					return step;
				}
			}

			return Say("New sound: the new sound could not be read back");
		}

		case HierarchyAsk::SoundEditPick:
		{
			int const at = Pick(*mode, typed);
			int32_t const index = at < 0 ? 0 : mode->listValues[(size_t)at];

			ClearList(mode);

			if (at < 0)
			{
				return Say("Edit sound: nothing matches \"" + typed + "\", nothing changed");
			}

			std::vector<SoundInfo> sounds;

			HierarchyEdit_Sounds(file, &sounds);

			for (SoundInfo const &each : sounds)
			{
				if (each.index == index)
				{
					// FUN_004816f0: the sound's own Sound Edit window (FUN_004ebd60), one window with every field (HierarchyMode_SoundForm takes its answers)
					HierarchyStep step;

					mode->soundBeingEdited = each;
					step.soundForm = true;
					return step;
				}
			}

			return Say("Edit sound: that sound is no longer there");
		}

		case HierarchyAsk::SoundDeletePick:
		case HierarchyAsk::SoundApplyPick:
		{
			int const at = Pick(*mode, typed);
			int32_t const index = at < 0 ? 0 : mode->listValues[(size_t)at];
			bool const apply = ask == HierarchyAsk::SoundApplyPick;

			ClearList(mode);

			if (at < 0)
			{
				return Say(std::string(apply ? "Apply frame sound" : "Delete sound") + ": nothing matches \"" + typed + "\", nothing changed");
			}

			if (!apply)
			{
				HierarchyEditResult const done = HierarchyEdit_DeleteSound(file, index, &why);

				return Say(done == HierarchyEditResult::Done ? "Sound " + Number(index) + " deleted" : "Delete sound: " + Why(done, why));
			}

			SequenceInfo number = { 0, 0, 0, false };

			if (!SequenceNumbers(file, mode, &number))
			{
				return Say("Apply frame sound: the sequence could not be read");
			}

			size_t frame = 0;
			HierarchyEditResult const done = HierarchyEdit_ChangeFrameAt(file, mode->hierarchy, number.number, number.subNumber, mode->position, 0, 0, index, &frame);

			if (done != HierarchyEditResult::Done)
			{
				return Say(std::string("Apply frame sound: ") + (done == HierarchyEditResult::NoSequence ? "no frame of the root section holds the position" : HierarchyEditWords(done)));
			}

			return Say("Sound " + Number(index) + " applied to frame " + Number((long long)frame + 1));
		}

		case HierarchyAsk::AttachObject:
		{
			int const at = Pick(*mode, typed);

			if (at < 0)
			{
				ClearList(mode);
				return Say("Attach shape to bones: nothing matches \"" + typed + "\", nothing attached");
			}

			mode->attachObject = mode->list[(size_t)at].substr(mode->list[(size_t)at].find("  ") + 2);
			ClearList(mode);
			return AttachHierarchyPicker(file, mode);
		}

		case HierarchyAsk::AttachHierarchy:
		case HierarchyAsk::AttachRedo:
		{
			bool redo = false;

			if (ask == HierarchyAsk::AttachHierarchy)
			{
				int const at = Pick(*mode, typed);

				if (at < 0)
				{
					ClearList(mode);
					return Say("Attach shape to bones: nothing matches \"" + typed + "\", nothing attached");
				}

				mode->attachHierarchy = mode->list[(size_t)at].substr(mode->list[(size_t)at].find("  ") + 2);
				ClearList(mode);

				// 0x48ea71-0x48ea92: the hierarchy needs an OBJCHIER whose OBANSEQS holds sequence 10000, sub 0 (FUN_00407c80(10000, 0), "the reference sequence"); without, the message box
				// "The hierarchy doesn't have the reference sequence" (0x5d5f54), Ok, and nothing more. This comes BEFORE the "already attached" question.
				std::vector<SequenceInfo> sequences;
				bool reference = false;

				if (HierarchyEdit_Sequences(file, mode->attachHierarchy, &sequences) == HierarchyEditResult::Done)
				{
					for (SequenceInfo const &one : sequences)
					{
						reference = reference || (one.number == 10000 && one.subNumber == 0);
					}
				}

				if (!reference)
				{
					return Box("The hierarchy doesn't have the reference sequence");
				}
			}
			else
			{
				// the Yes / No box (FUN_00474a40 + FUN_0057c800(100, 100)): anything but Yes (3) ends with nothing (0x48eb2d)
				redo = _stricmp(typed.c_str(), "yes") == 0 || _stricmp(typed.c_str(), "y") == 0;

				if (!redo)
				{
					return HierarchyStep();
				}
			}

			size_t points = 0;
			HierarchyEditResult const done = HierarchyEdit_AttachToBones(file, mode->attachObject, mode->attachHierarchy, 10000, 0, redo, &why, &points);

			// the shape already has bone data for the hierarchy with its points' count unchanged (FUN_0041cf70 then FUN_0041d540, 0x48eae9-0x48eb06): the Yes / No box (0x5d5ee8)
			if (done == HierarchyEditResult::Refused && why && std::string(why).find("already attached") != std::string::npos)
			{
				return AskYesNo(mode, HierarchyAsk::AttachRedo, "The shape is already attached to the hierarchy.\nDo you want to redetermine the vertex connections anyway?");
			}

			if (done != HierarchyEditResult::Done)
			{
				return Say("Attach shape to bones: " + Why(done, why));   // ours: a refusal of the file's bytes (inteng has no such branch)
			}

			// FUN_0041f4d0 attached the points (0x48eb51), then FUN_0048b550(hierarchy, 1) makes that hierarchy's player afresh and goes on to Select Sequence (0x48eb59); DAT_00637134 = 1 (changed)
			return ChooseHierarchy(file, mode, mode->attachHierarchy, true);
		}

		case HierarchyAsk::SelectDelta:
		{
			int const at = Pick(*mode, typed);
			int32_t const place = at < 0 ? -1 : mode->listValues[(size_t)at];

			ClearList(mode);

			if (place < 0)
			{
				return Say("Add delta: nothing matches \"" + typed + "\", nothing added");
			}

			std::vector<SequenceInfo> sequences;

			if (HierarchyEdit_Sequences(file, mode->hierarchy, &sequences) != HierarchyEditResult::Done || (size_t)place >= sequences.size())
			{
				return Say("Add delta: the sequence could not be read");
			}

			mode->addDeltaRequested = true;
			mode->addDeltaNumber = sequences[(size_t)place].number;
			mode->addDeltaSub = sequences[(size_t)place].subNumber;
			mode->deltaSelected = true;
			mode->deltaPlace = (size_t)place;
			mode->deltaMs = 1000;

			{
				int32_t milliseconds = 0;
				bool has = false;

				if (HierarchyEdit_ReadSequenceTime(file, mode->hierarchy, mode->deltaPlace, &milliseconds, &has) == HierarchyEditResult::Done && has && milliseconds > 0)
				{
					mode->deltaMs = milliseconds;
				}
			}

			return Say("Delta sequence " + Number(mode->addDeltaNumber) + "." + Number(mode->addDeltaSub) + " added");
		}

		case HierarchyAsk::CopyFrom:
		{
			int const at = Pick(*mode, typed);

			if (at < 0)
			{
				ClearList(mode);
				return Say("Copy between hierarchies: nothing matches \"" + typed + "\", nothing copied");
			}

			mode->copyFrom = mode->list[(size_t)at].substr(mode->list[(size_t)at].find("  ") + 2);
			return Ask(mode, HierarchyAsk::CopyTo, "Copy sounds TO which hierarchy? (type its place, or part of its name)", std::string());
		}

		case HierarchyAsk::CopyTo:
		{
			int const at = Pick(*mode, typed);

			if (at < 0)
			{
				ClearList(mode);
				return Say("Copy between hierarchies: nothing matches \"" + typed + "\", nothing copied");
			}

			std::string const to = mode->list[(size_t)at].substr(mode->list[(size_t)at].find("  ") + 2);

			ClearList(mode);

			if (to == mode->copyFrom)
			{
				return Say("Copy between hierarchies: the two hierarchies are the same one, nothing copied");
			}

			size_t copied = 0;
			std::vector<std::string> errors;
			HierarchyEditResult const done = HierarchyEdit_CopyFrameSounds(file, mode->copyFrom, to, &copied, &errors);

			if (done != HierarchyEditResult::Done)
			{
				return Say("Copy between hierarchies: " + std::string(HierarchyEditWords(done)));
			}

			std::string said = "Copied sounds for " + Number((long long)copied) + " sequences. From " + mode->copyFrom + " To " + to;

			for (std::string const &each : errors)
			{
				said += "; error copying sounds for sequence " + each;
			}

			return Say(said);
		}

		case HierarchyAsk::ChangeObjectList:
		{
			bool const yes = _stricmp(typed.c_str(), "yes") == 0 || _stricmp(typed.c_str(), "y") == 0;
			bool const no = _stricmp(typed.c_str(), "no") == 0 || _stricmp(typed.c_str(), "n") == 0;

			if (!yes && !no)
			{
				return Say("Change object for section: nothing changed (it wants yes or no)");
			}

			if (no)
			{
				return Ask(mode, HierarchyAsk::ChangeObjectName, "Type in new object name for this section", std::string());
			}

			std::vector<std::string> objects;
			HierarchyEditResult const read = HierarchyEdit_ObjectNames(file, &objects);

			if (read != HierarchyEditResult::Done)
			{
				return Say(std::string("Change object for section: ") + HierarchyEditWords(read));
			}

			// the chooser lists the level's objects but the light kind: a name of 4 letters or more that begins with "L" and has "#" as its third letter (0x48e1d0-0x48e1f3)
			for (size_t i = 0; i < objects.size(); i++)
			{
				std::string const &name = objects[i];

				if (name.size() < 4 || name[0] != 'L' || name[2] != '#')
				{
					Line(mode, name, (int32_t)i);
				}
			}

			if (mode->list.empty())
			{
				return Say("Change object for section: the level has no object to choose");
			}

			return Ask(mode, HierarchyAsk::ChangeObjectPick, "Select new object for this section (type its place, or part of its name)", std::string());
		}

		case HierarchyAsk::ChangeObjectPick:
		case HierarchyAsk::ChangeObjectName:
		{
			std::string chosen = typed;

			if (ask == HierarchyAsk::ChangeObjectPick)
			{
				int const at = Pick(*mode, typed);

				if (at < 0)
				{
					ClearList(mode);
					return Say("Change object for section: nothing matches \"" + typed + "\", nothing changed");
				}

				chosen = mode->list[(size_t)at].substr(mode->list[(size_t)at].find("  ") + 2);
				ClearList(mode);
			}

			HierarchyEditResult const done = HierarchyEdit_ChangeSectionObject(file, mode->hierarchy, mode->section, chosen, &why);

			if (done != HierarchyEditResult::Done)
			{
				return Say("Change object for section: " + Why(done, why));
			}

			mode->section = chosen;
			return Say("The section now uses the object \"" + chosen + "\"");
		}

		case HierarchyAsk::TurretSequence:
		{
			int const at = Pick(*mode, typed);
			int32_t const place = at < 0 ? -1 : mode->listValues[(size_t)at];

			ClearList(mode);

			if (place < 0)
			{
				return Say("Select Turret Sequence: nothing matches \"" + typed + "\", no turret sequence added");
			}

			mode->turretChosen = true;
			mode->turretSequence = (size_t)place;
			return Say("Turret sequence " + Number(place + 1) + " chosen");
		}

		case HierarchyAsk::SelectSequence:
		{
			int const at = Pick(*mode, typed);
			int32_t const place = at < 0 ? -1 : mode->listValues[(size_t)at];

			ClearList(mode);

			if (place < 0)
			{
				return Say("Select Sequence: nothing matches \"" + typed + "\", nothing selected");
			}

			mode->sequenceSelected = true;
			mode->sequence = (size_t)place;

			// inteng's sequence object starts at 1000 ms and takes the OBASEQTM number when it is above 0 (FUN_0047d6f0 0x47d982)
			{
				int32_t milliseconds = 0;
				bool has = false;

				mode->sequenceMs = 1000;

				if (HierarchyEdit_ReadSequenceTime(file, mode->hierarchy, mode->sequence, &milliseconds, &has) == HierarchyEditResult::Done && has && milliseconds > 0)
				{
					mode->sequenceMs = milliseconds;
				}
			}

			// inteng's sequence object takes the chunk's speed and angle as the last ones when the chunk is there (FUN_0047d7xx, 0x47d9b0-0x47d9c6)
			model::SequenceSpeed read = { 0, 0, 0 };
			bool present = false;

			if (HierarchyEdit_ReadPlatform(file, mode->hierarchy, mode->sequence, &read, &present) == HierarchyEditResult::Done && present)
			{
				mode->platform.lastSpeed = read.speedMmPerSecond;
				mode->platform.lastAngle = read.angleDegrees;
				return Say("Sequence " + Number(place + 1) + " selected (platform speed " + Number(read.speedMmPerSecond) + " mm/s, angle " + Number(read.angleDegrees) + ")");
			}

			return Say("Sequence " + Number(place + 1) + " selected (no platform speed in it yet)");
		}

		case HierarchyAsk::SelectCollection:
		{
			int const at = Pick(*mode, typed);
			int32_t const number = at < 0 ? -1 : mode->listValues[(size_t)at];
			std::string const line = at < 0 ? std::string() : mode->list[(size_t)at];

			ClearList(mode);

			if (at < 0)
			{
				return Say("Select Collection: nothing matches \"" + typed + "\", nothing selected");
			}

			mode->collectionSelected = true;
			mode->collection = number;

			model::ShapeSetCollection chosen;

			mode->applied.clear();

			if (CollectionNamed(file, number, &chosen))
			{
				mode->applied = AppliedOf(file, chosen);
			}

			(void)line;
			return HierarchyStep();   // FUN_0048abc0 0x48ae6b: FUN_00477d10(that collection, detail), no words (HF-0x52)
		}

		case HierarchyAsk::CopyName:
		{
			if (typed.empty())
			{
				return Say("Copy Hierarchy: no name typed, nothing copied");
			}

			HierarchyEditResult const done = HierarchyEdit_Copy(file, mode->hierarchy, typed, &why);

			if (done == HierarchyEditResult::Refused && Why(done, why) == model::HierarchyNameWords(model::HierarchyNameResult::InUse))
			{
				return Box("That name is already in use");   // the message box (FUN_00474030, 0x5d4998), Ok; nothing made
			}

			return Say(done == HierarchyEditResult::Done ? "Hierarchy copied as \"" + typed + "\"" : "Copy Hierarchy: " + Why(done, why));
		}

		case HierarchyAsk::CreateName:
		case HierarchyAsk::RenameName:
		{
			bool const create = ask == HierarchyAsk::CreateName;
			char const *const title = create ? "Name of new shape collection" : "New name for shape collection";

			if (typed.empty())
			{
				return AskInDialog(mode, ask, title, std::string());   // inteng's loop asks again (FUN_0048af00; Rename's empty name OPEN, HE-F-0047a880)
			}

			if (create)
			{
				int32_t number = 0;
				HierarchyEditResult const done = HierarchyEdit_CreateCollection(file, typed, &number, &why);

				if (done == HierarchyEditResult::Refused)
				{
					return BoxThen("That name is already used", AskInDialog(mode, ask, title, std::string()));   // 0x5cd664, Ok, and asked again
				}

				if (done != HierarchyEditResult::Done)
				{
					return Say("Create Collection: " + Why(done, why));
				}

				mode->collectionSelected = true;
				mode->collection = number;
				mode->applied.clear();
				return HierarchyStep();   // FUN_0048af00 0x48b100-0x48b117: made (number: the smallest free from 2) and current, DAT_00637134 = 1, no words (HF-0x53)
			}

			HierarchyEditResult const done = HierarchyEdit_RenameCollection(file, mode->collection, typed, &why);

			if (done == HierarchyEditResult::Refused)
			{
				return BoxThen("That name is already used", AskInDialog(mode, ask, title, std::string()));   // 0x5cd664, Ok, and asked again
			}

			return Say(done == HierarchyEditResult::Done ? "Shape set collection renamed \"" + typed + "\"" : "Rename Collection: " + Why(done, why));
		}

		case HierarchyAsk::DeleteYes:
		{
			if (_stricmp(typed.c_str(), "yes") != 0 && _stricmp(typed.c_str(), "y") != 0)
			{
				return Say("Delete Collection: nothing deleted");
			}

			HierarchyEditResult const done = HierarchyEdit_DeleteCollection(file, mode->collection, &why);

			if (done == HierarchyEditResult::Done)
			{
				mode->collectionSelected = false;
				mode->applied.clear();
				return Say("Shape set collection deleted");
			}

			return Say("Delete Collection: " + Why(done, why));
		}

		case HierarchyAsk::VoiceIndex:
		{
			int32_t const value = HierarchyMode_EditNumber(typed);   // FUN_0047ac10: Ok sets +0x40 to the edit's number, no range check

			HierarchyEditResult const done = HierarchyEdit_SetCollectionVoiceIndex(file, mode->collection, value, &why);

			return Say(done == HierarchyEditResult::Done ? "Voice index set to " + Number(value) : "Collection Voice Index: " + Why(done, why));
		}

		case HierarchyAsk::Speed:
		{
			int32_t const value = HierarchyMode_EditNumber(typed);

			if (model::SetPlatformSpeed(&mode->platform, value) != model::SequenceSpeedResult::Ok)
			{
				return Say(std::string());   // case 0x4a: below 0, nothing (no words: the Hierarchy screen has no status line)
			}

			return Say(WritePlatformIfDue(file, mode, "Platform speed " + Number(value) + " mm/s"));
		}

		case HierarchyAsk::Angle:
		{
			int32_t const value = HierarchyMode_EditNumber(typed);

			if (model::SetPlatformAngle(&mode->platform, value, nullptr) != model::SequenceSpeedResult::Ok)
			{
				return Say(std::string());   // case 0x4b: outside 0 to 360, nothing
			}

			return Say(WritePlatformIfDue(file, mode, "Platform angle " + Number(value)));
		}

		case HierarchyAsk::DummySize:
		{
			// 0x48975d-0x489773: Ok with the edit's number above 0 sets DAT_005d4e24 = the number x 100 (the exe's 32-bit product); 0, below 0 or not a number: nothing
			int32_t const value = HierarchyMode_EditNumber(typed);

			if (value <= 0)
			{
				return Say(std::string());
			}

			mode->dummyObjectSize = (int32_t)((uint32_t)value * 100u);
			return Say("Dummy object size " + Number(value) + " mm");
		}

		case HierarchyAsk::Help:
		{
			if (mode->helpPage == 0)
			{
				mode->helpPage = 1;
				mode->list = HelpLines(1);
				return Ask(mode, HierarchyAsk::Help, "Hierarchy help, boned objects and the turret (Enter or Esc: close)", std::string());
			}

			ClearList(mode);
			mode->helpPage = 0;
			return Say(std::string());
		}

		case HierarchyAsk::SelectSingleSet:
		{
			int const at = Pick(*mode, typed);
			int32_t const number = at < 0 ? 0 : mode->listValues[(size_t)at];
			std::string const line = at < 0 ? std::string() : mode->list[(size_t)at];

			ClearList(mode);

			if (at < 0)
			{
				return Say("Select Single Set: nothing matches \"" + typed + "\", nothing changed");
			}

			// FUN_00477b40(set, detail): the collection let go (+0x38 = 0), the set list made of that one set (none for "Original"), the shapes chosen again; no words
			mode->applied.clear();
			mode->collectionSelected = false;

			if (number >= 0)
			{
				mode->applied.push_back(number);
			}

			(void)line;
			return HierarchyStep();
		}

		case HierarchyAsk::FemaleYes:
		{
			bool const yes = _stricmp(typed.c_str(), "yes") == 0 || _stricmp(typed.c_str(), "y") == 0;
			bool const no = _stricmp(typed.c_str(), "no") == 0 || _stricmp(typed.c_str(), "n") == 0;

			if ((!yes && !no) || mode->applied.size() != 1)
			{
				return Say("Set Properties: nothing changed (it wants yes or no)");
			}

			uint32_t flags = 0;
			HierarchyEditResult const done = HierarchyEdit_ChangeShapeSetFlags(file, mode->applied.front(), yes ? 1u : 0u, yes ? 0u : 1u, &flags);

			// FUN_0048d820 0x48d864-0x48d897: Ok (2): the box read, the set's flags bit 0 set or cleared, DAT_00637134 = 1; no words
			return done == HierarchyEditResult::Done ? HierarchyStep() : Say("Set Properties: " + Why(done, why));
		}

		case HierarchyAsk::AddSet:
		case HierarchyAsk::RemoveSet:
		{
			int const at = Pick(*mode, typed);
			int32_t const setNumber = at < 0 ? 0 : mode->listValues[(size_t)at];

			ClearList(mode);

			if (at < 0)
			{
				return Say(std::string(ask == HierarchyAsk::AddSet ? "Add to Collection" : "Remove from Collection") + ": nothing matches \"" + typed + "\", nothing changed");
			}

			HierarchyEditResult const done = ask == HierarchyAsk::AddSet ? HierarchyEdit_AddShapeSet(file, mode->collection, setNumber, &why) :
				HierarchyEdit_RemoveShapeSet(file, mode->collection, setNumber, &why);

			if (done != HierarchyEditResult::Done)
			{
				return Say(std::string(ask == HierarchyAsk::AddSet ? "Add to Collection: " : "Remove from Collection: ") + Why(done, why));
			}

			if (ask == HierarchyAsk::AddSet)
			{
				mode->applied.push_back(setNumber);
			}
			else
			{
				for (size_t i = 0; i < mode->applied.size(); i++)
				{
					if (mode->applied[i] == setNumber)
					{
						mode->applied.erase(mode->applied.begin() + (std::ptrdiff_t)i);
						break;
					}
				}
			}

			return HierarchyStep();   // DAT_00637134 = 1 and the shapes chosen again (0x47a2fb / 0x47a705), no words
		}
		}

		return HierarchyStep();
	}

	int32_t HierarchyMode_EditNumber(std::string const &text)
	{
		// FUN_0057d620: [this+0x18] is the edit's text; a "-" first, then the digits; any other letter after them gives 0
		size_t at = !text.empty() && text[0] == '-' ? 1 : 0;
		uint32_t sum = 0;

		while (at < text.size() && text[at] >= '0' && text[at] <= '9')
		{
			sum = sum * 10u + (uint32_t)(text[at] - '0');
			at++;
		}

		if (at != text.size())
		{
			return 0;
		}

		return !text.empty() && text[0] == '-' ? (int32_t)(0u - sum) : (int32_t)sum;
	}

	namespace
	{
		// MFC's float edit (DDX_Text on a float, AfxSimpleFloatParse): spaces round one number strtod reads whole; nothing else is a number (the DLL then says "Please enter a number.")
		bool MfcFloat(std::string const &text, double *value)
		{
			size_t from = 0;
			size_t to = text.size();

			while (from < to && (text[from] == ' ' || text[from] == '\t'))
			{
				from++;
			}

			while (to > from && (text[to - 1] == ' ' || text[to - 1] == '\t'))
			{
				to--;
			}

			if (from == to || to - from > 63)
			{
				return false;
			}

			std::string const part = text.substr(from, to - from);
			char *end = nullptr;
			double const read = strtod(part.c_str(), &end);

			if (!end || *end != '\0' || !(read == read) || read > 3.4e38 || read < -3.4e38)
			{
				return false;
			}

			*value = (double)(float)read;   // the DLL keeps it in a float
			return true;
		}

		// MFC's DDX_Text shows a float as "%.*g" with FLT_DIG (6) digits
		std::string MfcShown(float value)
		{
			char text[64];

			_snprintf_s(text, sizeof(text), _TRUNCATE, "%.*g", 6, (double)value);
			return text;
		}

		// ftol: toward 0, held inside the 32-bit range the exe's word has
		int32_t Ftol(double value)
		{
			if (!(value == value))
			{
				return 0;
			}

			if (value >= 2147483647.0)
			{
				return 2147483647;
			}

			if (value <= -2147483648.0)
			{
				return (int32_t)-2147483647 - 1;
			}

			return (int32_t)value;
		}

		double InteUnit(double levelScale)
		{
			return 100.0 * (levelScale > 0.0 && levelScale <= 1000000.0 ? levelScale : 1.0);
		}
	}

	std::vector<std::string> HierarchyMode_SoundFormShown(HierarchyMode const &mode)
	{
		SoundInfo const &s = mode.soundBeingEdited;
		double const unit = InteUnit(mode.levelScale);

		// FUN_004ebd60 0x4ebe12-0x4ebe88: inner and outer (inteng's unit, as unsigned words) x 0.00001, pitch / 128, volume x 100 x 0.007874016, each kept in a float; the DLL shows the volume as
		// ftol(% + 0.5) (edobtype 0x10006c0f)
		float const inner = (float)((double)(uint32_t)Ftol(s.inner * unit) * 1e-05);
		float const outer = (float)((double)(uint32_t)Ftol(s.outer * unit) * 1e-05);
		float const pitch = (float)((double)s.pitch * 0.0078125);
		float const volume = (float)((double)s.volume * 100.0 * 0.007874016);

		return { s.wav, MfcShown(inner), MfcShown(outer), MfcShown(pitch), std::to_string((long long)Ftol((double)volume + 0.5)) };
	}

	HierarchyStep HierarchyMode_SoundForm(OpenLevelFile *file, HierarchyMode *mode, std::vector<std::string> const &values)
	{
		HierarchyStep step;

		if (!mode || values.size() < 5)
		{
			return step;
		}

		// the DLL's DoDataExchange order (edobtype 0x10006830): the name, then inner 0..1000, outer 0..2000, pitch -48..48, volume 0..100 (DDV_MinMaxFloat); the first that fails says so and
		// the window stays (MFC's words, from the DLL's own strings: edobtype-resources.json)
		struct Limit { double lowest, highest; char const *words; };
		static Limit const limits[4] = { { 0.0, 1000.0, "Please enter a number between 0 and 1000." }, { 0.0, 2000.0, "Please enter a number between 0 and 2000." },
			{ -48.0, 48.0, "Please enter a number between -48 and 48." }, { 0.0, 100.0, "Please enter a number between 0 and 100." } };
		double read[4] = { 0, 0, 0, 0 };

		if (values[0].size() > 255)
		{
			step.answer = "Please enter no more than 255 characters.";
			step.messageBox = true;
			step.soundForm = true;
			return step;
		}

		for (int i = 0; i < 4; i++)
		{
			if (!MfcFloat(values[(size_t)i + 1], &read[i]))
			{
				step.answer = "Please enter a number.";
				step.messageBox = true;
				step.soundForm = true;
				return step;
			}

			if (read[i] < limits[i].lowest || read[i] > limits[i].highest)
			{
				step.answer = limits[i].words;
				step.messageBox = true;
				step.soundForm = true;
				return step;
			}
		}

		// back (0x4ebf13-0x4ebf5d): ftol(m x 100000), ftol(semitones x 128), ftol((% + 50/127) x 0.01 x 127); the file's ranges are inteng's / (100 x levelScale)
		double const unit = InteUnit(mode->levelScale);
		SoundInfo s = mode->soundBeingEdited;

		s.wav = values[0];
		s.inner = Ftol((double)Ftol(read[0] * 100000.0) / unit);
		s.outer = Ftol((double)Ftol(read[1] * 100000.0) / unit);
		s.pitch = Ftol(read[2] * 128.0);
		s.volume = Ftol((read[3] + 0.3937007874) * 0.01 * 127.0);

		char const *why = nullptr;
		HierarchyEditResult const done = HierarchyEdit_EditSound(file, s.index, s.wav, s.inner, s.outer, s.volume, s.pitch, &why);

		step.answer = done == HierarchyEditResult::Done ? "Sound " + std::to_string((long long)s.index) + " edited" : "Edit sound: " + Why(done, why);
		return step;
	}
}
