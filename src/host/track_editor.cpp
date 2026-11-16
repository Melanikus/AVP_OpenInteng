// See track_editor.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "track_editor.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

namespace host
{
	namespace
	{
		TrackObjectPlaceFn placeFn = nullptr;
		TrackPoseFn poseFn = nullptr;
		TrackTurnFn turnFn = nullptr;

		std::string Number(size_t n)
		{
			return std::to_string(n);
		}

		bool OnPage(TrackState state, int32_t command)
		{
			switch (state)
			{
			case TrackState::Loaded:
				switch (command)
				{
				case 0x514: case 0x516: case 0x518: case 0x519: case 0x51a: case 0x51b: case 0x51c: case 0x51f: case 0x520:
				case 0x524: case 0x525: case 0x527: case 0x528: case 0x529: case 0x52a: case 0x52b:
					return true;
				}
				return false;
			case TrackState::EndEdit:
				switch (command)
				{
				case 0x517: case 0x518: case 0x519: case 0x51a: case 0x51c: case 0x51d: case 0x521: case 0x522:
					return true;
				}
				return false;
			case TrackState::Playing:
				return command == 0x515 || command == 0x51c;
			case TrackState::PivotEdit:
				return command == 0x51e || command == 0x523;
			default:
				return false;
			}
		}

		// "section 2 of 5"
		std::string Which(TrackEditor const &ed, TrackLoaded const &loaded)
		{
			return "section " + Number(ed.current + 1) + " of " + Number(loaded.track.sections.size());
		}

		bool Load(OpenLevelFile *file, TrackEditor *ed, TrackLoaded *loaded, TrackAction *action)
		{
			if (ed->working)
			{
				*loaded = *ed->working;   // the track in memory: the level's bytes are as they were when it was loaded
			}
			else
			{
				TrackEditResult const result = TrackLoad(file, ed->id, loaded);

				if (result != TrackEditResult::Done)
				{
					action->words = std::string("Track Editing: ") + TrackEditWords(result);
					return false;
				}
			}

			if (ed->current >= loaded->track.sections.size())
			{
				ed->current = loaded->track.sections.empty() ? 0 : loaded->track.sections.size() - 1;
			}

			return true;
		}

		bool Store(OpenLevelFile *file, TrackEditor *ed, TrackLoaded const &loaded, TrackAction *action)
		{
			if (ed->working)
			{
				*ed->working = loaded;   // kept in memory: written to the level when Stop Track Edit lets the track go (FUN_004f1300)
				action->changed = true;   // the track was changed (the screen is drawn again); the level's bytes were not
				return true;
			}

			TrackEditResult const result = TrackStore(file, ed->id, loaded);

			if (result != TrackEditResult::Done)
			{
				action->words = std::string("Track Editing: ") + TrackEditWords(result);
				return false;
			}

			action->changed = true;
			return true;
		}

		void CursorToEnd(TrackLoaded const &loaded, size_t section, TrackAction *action)
		{
			TrackData_MarkerAt(loaded.track, section, true, placeFn, action->cursor);
			action->cursorSet = true;
		}

		bool FinishEnd(OpenLevelFile *file, TrackEditor *ed, TrackLoaded *loaded, int32_t const cursor[3], TrackAction *action)
		{
			// THE MARKER OBJECT was turned with the keys since it was last taken (the editor says by how much): the section's end turn is the marker's (FUN_004f0cd0 0x4f0cd0 copies the marker's matrix into the end, when the
			// section has no spin), and the object stays where the marker is, or turns about the pivot (P)
			if ((ed->markerTurn[0] != 0.0 || ed->markerTurn[1] != 0.0 || ed->markerTurn[2] != 0.0) && !loaded->track.sections.empty() && ed->current < loaded->track.sections.size())
			{
				float quat[4];

				if (!turnFn || !turnFn(loaded->track.sections[ed->current].quatEnd, ed->markerTurn[0], ed->markerTurn[1], ed->markerTurn[2], quat) || !TrackData_SetEndTurn(&loaded->track, ed->current, quat, placeFn, !ed->markerAboutPivot))
				{
					action->words = "Finish End Pos Edit: that turn of the marker is not one the game's loader takes";
					return false;
				}

				ed->markerTurn[0] = ed->markerTurn[1] = ed->markerTurn[2] = 0.0;
				action->markerTurnTaken = true;
			}

			if (!TrackData_FinishEnd(&loaded->track, ed->current, cursor, placeFn))
			{
				action->words = "Finish End Pos Edit: that place is outside the level (more than 16,777,216 from the middle)";
				return false;
			}

			return Store(file, ed, *loaded, action);
		}

		std::string SecondsText(int32_t time)
		{
			char shown[48];

			_snprintf_s(shown, sizeof(shown), _TRUNCATE, "%.5f", (double)time / 65536.0);

			size_t length = strlen(shown);

			while (length > 1 && shown[length - 1] == '0')
			{
				shown[--length] = 0;
			}

			if (length > 1 && shown[length - 1] == '.')
			{
				shown[--length] = 0;
			}

			return shown;
		}

		std::string FlagLetters(uint32_t flags)
		{
			std::string letters;

			if (flags & 1) letters += 'L';
			if (flags & 4) letters += 'B';
			if (flags & 2) letters += 'P';
			if (flags & 8) letters += 'S';
			return letters.empty() ? std::string("0") : letters;
		}

		TrackSound *FindSound(TrackLoaded *loaded, int index)
		{
			for (TrackSound &sound : loaded->sounds)
			{
				if (sound.index == index)
				{
					return &sound;
				}
			}

			return nullptr;
		}

		char const *SoundWords(int index)
		{
			return index == 0 ? "Sound" : (index == 1 ? "Start Sound" : "End Sound");
		}

		// Words and numbers split at spaces
		std::vector<std::string> Split(std::string const &typed)
		{
			std::vector<std::string> parts;
			std::string now;

			for (char ch : typed)
			{
				if (ch == ' ' || ch == '\t')
				{
					if (!now.empty())
					{
						parts.push_back(now);
						now.clear();
					}
				}
				else
				{
					now += ch;
				}
			}

			if (!now.empty())
			{
				parts.push_back(now);
			}

			return parts;
		}

		bool WholeNumber(std::string const &text, int64_t least, int64_t most, int64_t *value)
		{
			if (text.empty() || text.size() > 12)
			{
				return false;
			}

			size_t at = 0;
			bool negative = false;

			if (text[0] == '-' || text[0] == '+')
			{
				negative = text[0] == '-';
				at = 1;
			}

			if (at >= text.size())
			{
				return false;
			}

			int64_t made = 0;

			for (; at < text.size(); at++)
			{
				if (!isdigit((unsigned char)text[at]))
				{
					return false;
				}

				made = made * 10 + (text[at] - '0');
			}

			made = negative ? -made : made;

			if (made < least || made > most)
			{
				return false;
			}

			*value = made;
			return true;
		}
	}

	void TrackEdit_UseObjectPlace(TrackObjectPlaceFn fn)
	{
		placeFn = fn;
	}

	void TrackEdit_UsePose(TrackPoseFn fn)
	{
		poseFn = fn;
	}

	void TrackEdit_UseTurn(TrackTurnFn fn)
	{
		turnFn = fn;
	}

	bool TrackEdit_IsTrackCommand(int32_t command)
	{
		return (command >= 0x514 && command <= 0x52b && command != 0x52c) ;
	}

	bool TrackEdit_WorkingClip(TrackEditor const &editor, uint8_t const *objectId, TrackClip *clip)
	{
		if (!clip || !objectId || editor.state == TrackState::Off || !editor.working || memcmp(editor.id, objectId, 8) != 0)
		{
			return false;
		}

		std::vector<uint8_t> payload;

		TrackData_Bytes(editor.working->track, &payload);
		clip->track = TrackChunkBytes("OBJTRAK2", payload);
		clip->sounds.clear();
		return true;
	}

	bool TrackEdit_Enter(OpenLevelFile *file, uint8_t const *objectId, TrackEditor *editor, TrackAction *action, bool followTheObject)
	{
		*action = TrackAction();

		if (!file || !objectId || !editor)
		{
			action->words = "Edit Track: no level is open";
			return false;
		}

		TrackEditor now;
		TrackLoaded loaded;

		memcpy(now.id, objectId, 8);
		now.state = TrackState::Loaded;
		now.current = 0;

		if (!Load(file, &now, &loaded, action))
		{
			return false;
		}

		// inteng loads the track with FUN_004f0490 run on it (FUN_004eff90 0x4f01d2, its third argument 0 for Edit Track): the track is turned and moved so that it starts at the object's place and turn,
		// as an object that was moved since the track was made brings its track along. (Paste Track Absolute passes a third argument that is not 0, and the track is as it was pasted.)
		if (followTheObject && loaded.had)
		{
			TrackData moved = loaded.track;
			std::vector<uint8_t> was, now2;

			TrackData_Bytes(loaded.track, &was);

			if (TrackRebasedOnObject(file, objectId, &moved))
			{
				TrackData_Bytes(moved, &now2);

				if (now2 != was)
				{
					loaded.track = moved;   // in memory only, as inteng's: written when the track is let go
				}
			}
		}

		now.working = std::make_shared<TrackLoaded>(loaded);
		*editor = now;
		action->words = "Edit Track: " + Which(*editor, loaded) + (loaded.had ? "" : " (the object had no track: this is the one-section track inteng starts with)");
		return true;
	}

	void TrackEdit_Command(OpenLevelFile *file, TrackEditor *ed, int32_t command, int32_t const cursor[3], TrackAction *action)
	{
		*action = TrackAction();

		if (!ed || ed->state == TrackState::Off)
		{
			action->words = "Not editing a track: choose Edit Track on Object Placement first";
			return;
		}

		if (!OnPage(ed->state, command))
		{
			action->words = "That item is not on this page of inteng's Track Editing menu (state " + Number((size_t)ed->state) + ")";
			return;
		}

		TrackLoaded loaded;

		if (!Load(file, ed, &loaded, action))
		{
			return;
		}

		bool const editing = ed->state == TrackState::EndEdit;

		switch (command)
		{
		case 0x514:   // Play Track
			action->startPlay = true;
			action->playData = loaded.track;
			action->playSounds = loaded.sounds;
			ed->state = TrackState::Playing;
			action->words = "Play Track: the game's own track code moves the object along " + Number(loaded.track.sections.size()) + " section" + (loaded.track.sections.size() == 1 ? "" : "s");
			return;

		case 0x515:   // Stop track play
			action->stopPlay = true;
			ed->state = TrackState::Loaded;
			action->words = "Stopped: " + Which(*ed, loaded);
			return;

		case 0x516:   // Edit Section End Position
			ed->state = TrackState::EndEdit;
			CursorToEnd(loaded, ed->current, action);
			action->words = "Edit Section End Position: " + Which(*ed, loaded) + " ends at the end marker (the 3D cursor): press F to fly it with the camera, F again to let it go, then Finish End Pos Edit";
			return;

		case 0x517:   // Finish End Pos Edit
			if (!FinishEnd(file, ed, &loaded, cursor, action))
			{
				return;
			}

			ed->state = TrackState::Loaded;
			action->words = "Finish End Pos Edit: " + Which(*ed, loaded) + " now ends at " + std::to_string(cursor[0]) + " " + std::to_string(cursor[1]) + " " + std::to_string(cursor[2]) +
				(ed->current + 1 < loaded.track.sections.size() ? " (the next section starts there)" : "");
			return;

		case 0x518:   // Add Section
		{
			if (editing && !FinishEnd(file, ed, &loaded, cursor, action))
			{
				return;
			}

			size_t added = 0;

			if (!TrackData_AddSection(&loaded.track, ed->current, &added))
			{
				action->words = "Add Section: the track is at its most (" + Number(kTrackMostSections) + " sections)";
				return;
			}

			if (!Store(file, ed, loaded, action))
			{
				return;
			}

			ed->current = added;
			ed->state = TrackState::EndEdit;
			CursorToEnd(loaded, ed->current, action);
			action->words = "Add Section: " + Which(*ed, loaded) + " added after it, 5 seconds, from the old end: press F to fly the end marker with the camera, F again to let it go, then Finish End Pos Edit";
			return;
		}

		case 0x519:   // Next Section
		case 0x51a:   // Previous Section
		{
			if (editing && !FinishEnd(file, ed, &loaded, cursor, action))
			{
				return;
			}

			size_t const count = loaded.track.sections.size();

			ed->current = command == 0x519 ? (ed->current + 1) % count : (ed->current + count - 1) % count;

			if (editing)
			{
				CursorToEnd(loaded, ed->current, action);   // FUN_004f0c70
			}

			action->words = std::string(command == 0x519 ? "Next" : "Previous") + " Section: " + Which(*ed, loaded);
			return;
		}

		case 0x51b:   // Properties > Section
			action->prompt = TrackPrompt::SectionTime;
			action->promptDefault = SecondsText(loaded.track.sections[ed->current].time);
			action->words = "Time for section (" + Which(*ed, loaded) + ")";
			return;

		case 0x527:   // Properties > Track
			action->prompt = TrackPrompt::TrackFlags;
			action->promptDefault = FlagLetters(loaded.track.flags);
			action->words = "Track Properties: type the letters of the flags that are on";
			return;

		case 0x524: case 0x528: case 0x52a:   // Sound / Start Sound / End Sound > Edit
		{
			int const index = command == 0x524 ? 0 : (command == 0x528 ? 1 : 2);
			TrackSound const *have = FindSound(&loaded, index);
			TrackSound now;

			if (have)
			{
				now = *have;
			}
			else
			{
				now.wav.clear();   // inteng's "Not Set" is no name a person can type: the field starts empty
			}

			action->prompt = TrackPrompt::Sound;
			action->soundIndex = index;
			action->promptDefault = now.wav + "\t" + std::to_string(now.innerRange) + " " + std::to_string(now.outerRange) + " " + std::to_string(now.maxVolume) + " " + std::to_string(now.pitch) + " " +
				((now.flags & 1) ? "1" : "0");
			action->words = std::string(SoundWords(index));
			return;
		}

		case 0x525: case 0x529: case 0x52b:   // Delete the sound
		{
			int const index = command == 0x525 ? 0 : (command == 0x529 ? 1 : 2);
			size_t const before = loaded.sounds.size();

			loaded.sounds.erase(std::remove_if(loaded.sounds.begin(), loaded.sounds.end(), [index](TrackSound const &s) { return s.index == index; }), loaded.sounds.end());

			if (loaded.sounds.size() == before)
			{
				action->words = std::string("Delete ") + SoundWords(index) + ": the track has none";
				return;
			}

			if (Store(file, ed, loaded, action))
			{
				action->words = std::string("Deleted the ") + SoundWords(index);
			}

			return;
		}

		case 0x51f:   // Delete > Section (the whole track when it has only one section)
		{
			if (loaded.track.sections.size() < 2)
			{
				TrackEditResult const result = TrackRemove(file, ed->id);

				if (result != TrackEditResult::Done)
				{
					action->words = std::string("Delete Section: ") + TrackEditWords(result);
					return;
				}

				action->changed = true;
				action->leave = true;
				ed->state = TrackState::Off;
				ed->working.reset();
				action->words = "Delete Section: it was the only section, so the track is deleted";
				return;
			}

			size_t newCurrent = 0;

			if (!TrackData_DeleteSection(&loaded.track, ed->current, &newCurrent, placeFn) || !Store(file, ed, loaded, action))
			{
				return;
			}

			ed->current = newCurrent;
			action->words = "Deleted the section: " + Which(*ed, loaded) + " is now the one shown";
			return;
		}

		case 0x520:   // Delete > Track
			action->prompt = TrackPrompt::DeleteTrack;
			action->words = "Delete this track?";
			return;

		case 0x51d:   // Edit Pivot Point
			if (!FinishEnd(file, ed, &loaded, cursor, action))
			{
				return;
			}

			ed->state = TrackState::PivotEdit;
			TrackData_CursorForPivot(loaded.track, ed->current, action->cursor);
			action->cursorSet = true;
			action->words = "Edit Pivot Point: " + Which(*ed, loaded) + ": click a view to place the pivot, then Stop Pivot Edit";
			return;

		case 0x523:   // Move Pivot To Centre
			for (int axis = 0; axis < 3; axis++)
			{
				action->cursor[axis] = 0;
			}

			action->cursorSet = true;
			action->words = "Move Pivot To Centre: the 3D cursor is at 0 0 0";
			return;

		case 0x51e:   // Stop Pivot Edit
			if (!TrackData_SetPivotFromCursor(&loaded.track, ed->current, cursor, placeFn))
			{
				action->words = "Stop Pivot Edit: that place is outside the level (more than 16,777,216 from the middle)";
				return;
			}

			if (!Store(file, ed, loaded, action))
			{
				return;
			}

			ed->state = TrackState::EndEdit;   // FUN_004f3cf0 then FUN_004f0c70 and page 3 (0x4f4061..0x4f4089)
			CursorToEnd(loaded, ed->current, action);
			action->words = "Stop Pivot Edit: the object's offset from the pivot is minus the cursor (" + Which(*ed, loaded) + ")";
			return;

		case 0x521:   // Jump To Start > This Section
		case 0x522:   // Jump To Start > Next Section
		{
			size_t const at = command == 0x521 ? ed->current : (ed->current + 1) % loaded.track.sections.size();

			TrackData_MarkerAt(loaded.track, at, false, placeFn, action->cursor);
			action->cursorSet = true;
			action->words = std::string("Jump To Start: the 3D cursor is at the start of section ") + Number(at + 1);
			return;
		}

		case 0x51c:   // Stop Track Edit
		{
			if (editing && !FinishEnd(file, ed, &loaded, cursor, action))
			{
				return;
			}

			// inteng writes the track when the editing stops (FUN_004eff90(0,0) -> FUN_004f1300): an object that had none gets the one-section track it started with. The game's loader checks it now; if it refuses, nothing is
			// written and the editing goes on (the track in memory is kept)
			bool const made = !loaded.had;

			{
				TrackEditResult const written = TrackStore(file, ed->id, loaded);

				if (written != TrackEditResult::Done)
				{
					action->words = std::string("Stop Track Edit: the track could not be written, the editing goes on: ") + TrackEditWords(written);
					return;
				}

				action->changed = true;
			}

			// ... and the object takes the pose the track has at its timer start (FUN_004eff90 0x4effba, FUN_004f0870 writing the object's matrix and place): its record in the level now says so
			if (poseFn && !loaded.track.sections.empty())
			{
				int32_t place[3];
				float quat[4];

				if (poseFn(loaded.track, place, quat))
				{
					TrackEditResult const posed = TrackSetObjectPose(file, ed->id, place, quat);

					if (posed != TrackEditResult::Done)
					{
						action->words = std::string("Stop Track Edit: the object could not take the track's start pose: ") + TrackEditWords(posed);
						return;
					}

					action->changed = true;
				}
			}

			action->stopPlay = ed->state == TrackState::Playing;
			ed->state = TrackState::Off;
			ed->working.reset();
			action->leave = true;
			action->words = made ? "Stop Track Edit: the object now has the one-section track" : "Stop Track Edit: the track is kept in the level's bytes until Save a copy";
			return;
		}
		}

		action->words = "That item is not a Track Editing item";
	}

	void TrackEdit_AnswerTime(OpenLevelFile *file, TrackEditor *ed, std::string const &typed, TrackAction *action)
	{
		*action = TrackAction();

		TrackLoaded loaded;

		if (!ed || ed->state == TrackState::Off || !Load(file, ed, &loaded, action))
		{
			if (action->words.empty())
			{
				action->words = "Not editing a track";
			}

			return;
		}

		char *end = nullptr;
		double const seconds = strtod(typed.c_str(), &end);

		if (typed.empty() || !end || *end != 0 || !(seconds > 0.0) || seconds > 32767.0)
		{
			action->words = "Time for section: a number of seconds, more than 0.001, was expected";
			return;
		}

		int32_t const time = (int32_t)floor(seconds * 65536.0 + 0.5);

		if (!TrackData_SetTime(&loaded.track, ed->current, time))
		{
			action->words = "Time for section: the game's loader refuses a section shorter than 65 (about a thousandth of a second)";
			return;
		}

		if (Store(file, ed, loaded, action))
		{
			action->words = "Time for section: " + Which(*ed, loaded) + " takes " + SecondsText(time) + " seconds";
		}
	}

	void TrackEdit_AnswerFlags(OpenLevelFile *file, TrackEditor *ed, std::string const &typed, TrackAction *action)
	{
		*action = TrackAction();

		TrackLoaded loaded;

		if (!ed || ed->state == TrackState::Off || !Load(file, ed, &loaded, action))
		{
			if (action->words.empty())
			{
				action->words = "Not editing a track";
			}

			return;
		}

		uint32_t bits = 0;

		for (char ch : typed)
		{
			switch (toupper((unsigned char)ch))
			{
			case 'L': bits |= 1; break;
			case 'P': bits |= 2; break;
			case 'B': bits |= 4; break;
			case 'S': bits |= 8; break;
			case '0': case ' ': case 'N': break;
			default:
				action->words = "Track Properties: the letters are L (Loop Track), B (Loop back and forth), P (Playing at start), S (Use Track Smoothing), or 0 for none";
				return;
			}
		}

		TrackData_SetFlags(&loaded.track, bits);

		if (Store(file, ed, loaded, action))
		{
			action->words = "Track Properties: flags " + FlagLetters(bits);
		}
	}

	void TrackEdit_AnswerSound(OpenLevelFile *file, TrackEditor *ed, int soundIndex, std::string const &typed, TrackAction *action)
	{
		*action = TrackAction();

		TrackLoaded loaded;

		if (!ed || ed->state == TrackState::Off || soundIndex < 0 || soundIndex > 2 || !Load(file, ed, &loaded, action))
		{
			if (action->words.empty())
			{
				action->words = "Not editing a track";
			}

			return;
		}

		std::vector<std::string> const parts = Split(typed);

		if (parts.empty() || parts.size() > 6)
		{
			action->words = "Sound: type the file name and (if you like) the inner range, outer range, volume, pitch and 1 to loop";
			return;
		}

		std::string const &name = parts[0];

		if (name.size() > kTrackSoundNameMost)
		{
			action->words = "Sound: the file name is longer than " + Number(kTrackSoundNameMost) + " letters";
			return;
		}

		for (char ch : name)
		{
			if (!(isalnum((unsigned char)ch) || ch == '.' || ch == '_' || ch == '-'))
			{
				action->words = "Sound: a file name has letters, digits, . _ and - only";
				return;
			}
		}

		TrackSound sound;
		TrackSound const *const have = FindSound(&loaded, soundIndex);

		if (have)
		{
			sound = *have;
		}

		sound.index = soundIndex;
		sound.wav = name;

		int64_t value = 0;

		if (parts.size() > 1)
		{
			if (!WholeNumber(parts[1], 0, 100000000, &value)) { action->words = "Sound: the inner range is a whole number from 0 to 100000000"; return; }
			sound.innerRange = (uint32_t)value;
		}

		if (parts.size() > 2)
		{
			if (!WholeNumber(parts[2], 0, 100000000, &value)) { action->words = "Sound: the outer range is a whole number from 0 to 100000000"; return; }
			sound.outerRange = (uint32_t)value;
		}

		if (parts.size() > 3)
		{
			if (!WholeNumber(parts[3], 0, 127, &value)) { action->words = "Sound: the volume is a whole number from 0 to 127"; return; }
			sound.maxVolume = (int32_t)value;
		}

		if (parts.size() > 4)
		{
			if (!WholeNumber(parts[4], -32768, 32767, &value)) { action->words = "Sound: the pitch is a whole number from -32768 to 32767"; return; }
			sound.pitch = (int32_t)value;
		}

		if (parts.size() > 5)
		{
			if (!WholeNumber(parts[5], 0, 1, &value)) { action->words = "Sound: the last number is 1 to loop or 0 not to"; return; }
			sound.flags = (int32_t)value;
		}

		if (sound.innerRange > sound.outerRange)
		{
			action->words = "Sound: the inner range is more than the outer range";
			return;
		}

		if (have)
		{
			*FindSound(&loaded, soundIndex) = sound;
		}
		else
		{
			loaded.sounds.push_back(sound);
		}

		// the sounds go in the order middle, start, end (inteng saves them in that order: FUN_004f1300)
		std::sort(loaded.sounds.begin(), loaded.sounds.end(), [](TrackSound const &a, TrackSound const &b) { return a.index < b.index; });

		if (Store(file, ed, loaded, action))
		{
			action->words = std::string(SoundWords(soundIndex)) + ": " + sound.wav + ", ranges " + std::to_string(sound.innerRange) + " to " + std::to_string(sound.outerRange) +
				", volume " + std::to_string(sound.maxVolume);
		}
	}

	void TrackEdit_AnswerDelete(OpenLevelFile *file, TrackEditor *ed, std::string const &typed, TrackAction *action)
	{
		*action = TrackAction();

		if (!ed || ed->state == TrackState::Off)
		{
			action->words = "Not editing a track";
			return;
		}

		std::string lower;

		for (char ch : typed)
		{
			lower += (char)tolower((unsigned char)ch);
		}

		if (lower != "y" && lower != "yes")
		{
			action->words = "The track is kept";
			return;
		}

		TrackEditResult const result = TrackRemove(file, ed->id);

		if (result != TrackEditResult::Done)
		{
			action->words = std::string("Delete Track: ") + TrackEditWords(result);
			return;
		}

		action->changed = true;
		action->leave = true;
		action->stopPlay = ed->state == TrackState::Playing;
		ed->state = TrackState::Off;
		ed->working.reset();
		action->words = "Delete Track: the object's track and its sounds are deleted";
	}

	void TrackEdit_PlayEnded(TrackEditor *ed)
	{
		if (ed && ed->state == TrackState::Playing)
		{
			ed->state = TrackState::Loaded;
		}
	}
}
