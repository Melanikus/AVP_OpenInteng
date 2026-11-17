// The test for src/host/track_sections, track_edit (TrackLoad / TrackStore / TrackRemove) and track_editor: inteng's Track Editing commands over the STOCK levels, each result put
// through the game's own loader (OpenLevelFile::ReplaceLevel). Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// It is given level files on its command line (tests/host_track_editor_test.runargs: every stock level) and runs, for every object of every level that has an OBJTRAK2:
// - the track read as numbers and made into bytes again is the chunk's own payload (every stock track round-trips exactly), and stored back it changes nothing (the sounds are compared by their
//   numbers and name: the game's own save leaves the bytes after a sound's name uninitialised, win95/chunk_object.cpp:1135);
// - for the first two tracks of a level: Edit Track, Add Section (the track has one more, current = the new one, state "end edit"), Finish End Pos Edit at a place (the new section ends there and the
//   next one, if any, starts there), Delete Section (the track is as long as before, the first section's start unchanged), Properties > Section (2.5 seconds = 163840), Properties > Track
//   (Loop, Playing at start), Start Sound > Edit (a name and numbers): each read back from the level the loader took;
// - hostile answers are refused (a time under 65, a bad letter, a sound name with a slash, ranges the wrong way round) and change nothing;
// - Delete Track takes the OBJTRAK2 and every TRAKSOUN out; a track of one section deleted by Delete > Section does the same; an object with no track starts with the one-section default.
// A level that has no track at all is passed; at least one track in all the levels given has to be found, or the test fails (a test that tests nothing is not a pass).
//
// WHAT IT CAN'T SHOW: that the object moves right when the track is played, and where the game puts an object whose section has an offset: that is the game's own code (track_play_game.cpp),
// which this test does not link; the editor's -trackproof run shows both in the running editor.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "track_edit.h"
#include "track_editor.h"
#include "track_sections.h"

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *, int)
{
}

void avp_exit(int code)
{
	exit(code);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static int checksFailed = 0;
static int checksRun = 0;

static void Check(bool good, std::string const &what)
{
	checksRun++;

	if (!good)
	{
		checksFailed++;
		printf("  FAILED  %s\n", what.c_str());
		fflush(stdout);
	}
}

// the game's own save leaves the bytes after a sound's name uninitialised (fill_data_block copies only strlen + 1): the numbers and the name are what is compared
static bool SameSounds(std::vector<std::vector<char> > const &a, std::vector<std::vector<char> > const &b)
{
	if (a.size() != b.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.size(); i++)
	{
		if (a[i].size() < 37 || b[i].size() < 37 || memcmp(a[i].data(), b[i].data(), 36) != 0 || strncmp(a[i].data() + 36, b[i].data() + 36, a[i].size() - 36) != 0)
		{
			return false;
		}
	}

	return true;
}

static int spinSections = 0;   // sections the file marks as spin sections (spare bit 0): inteng's < and > step their angle

static void OneObject(OpenLevelFile *f, uint8_t const *id, std::string const &where, int *tracks, int *edited, int *offsets)
{
	host::TrackClip clip;

	if (host::TrackCopy(f, id, &clip) != host::TrackEditResult::Done)
	{
		return;
	}

	(*tracks)++;

	// inteng (FUN_004f3330) asks the object it pastes on for a track first, then the clipboard: here the object has a track and nothing was copied
	host::TrackClip nothingCopied;

	Check(host::TrackExists(f, id), where + ": an object with a track is known to have one");
	Check(host::TrackPasteAbsolute(f, id, nothingCopied) == host::TrackEditResult::ClipboardEmpty && host::TrackPasteRelative(f, id, nothingCopied) == host::TrackEditResult::ClipboardEmpty, where + ": a paste with nothing copied says Use copy track first");

	host::TrackLoaded loaded;

	if (host::TrackLoad(f, id, &loaded) != host::TrackEditResult::Done)
	{
		Check(false, where + ": the track could not be read");
		return;
	}

	for (host::TrackSection const &s : loaded.track.sections)
	{
		if (s.offset[0] || s.offset[1] || s.offset[2])
		{
			(*offsets)++;
		}

		spinSections += (s.spare & 1) ? 1 : 0;
	}

	std::vector<uint8_t> payload;

	host::TrackData_Bytes(loaded.track, &payload);

	bool const same = !loaded.track.sections.empty() && clip.track.size() == payload.size() + 12 && memcmp(clip.track.data() + 12, payload.data(), payload.size()) == 0;

	Check(same || loaded.track.sections.empty(), where + ": the track as numbers is the chunk's own payload");

	if (!same)
	{
		return;
	}

	{
		host::TrackEditResult const r = host::TrackStore(f, id, loaded);
		host::TrackClip again;

		host::TrackCopy(f, id, &again);
		Check(r == host::TrackEditResult::Done && again.track == clip.track && SameSounds(again.sounds, clip.sounds), where + ": stored back unchanged");
	}

	if (*edited >= 2)
	{
		return;
	}

	(*edited)++;

	host::TrackEditor ed;
	host::TrackAction act;

	if (!host::TrackEdit_Enter(f, id, &ed, &act))
	{
		Check(false, where + ": Edit Track: " + act.words);
		return;
	}

	size_t const before = loaded.track.sections.size();
	int32_t const place[3] = { 1234, -5678, 91011 };

	host::TrackEdit_Command(f, &ed, 0x518, place, &act);
	Check(act.changed && ed.state == host::TrackState::EndEdit && ed.current == 1 && act.cursorSet, where + ": Add Section: " + act.words);

	// a command that is not on the page the state shows is refused and changes nothing
	host::TrackEdit_Command(f, &ed, 0x514, place, &act);
	Check(!act.changed && !act.startPlay && ed.state == host::TrackState::EndEdit, where + ": Play Track on page 3 is refused");

	host::TrackEdit_Command(f, &ed, 0x517, place, &act);
	Check(act.changed && ed.state == host::TrackState::Loaded, where + ": Finish End Pos Edit: " + act.words);

	// the track is in memory while it is edited (inteng's): the level still holds the old one until Stop Track Edit
	{
		host::TrackLoaded inLevel;

		host::TrackLoad(f, id, &inLevel);
		Check(ed.working && inLevel.track.sections.size() == before, where + ": the level still holds the track it had while it is edited");
	}

	host::TrackLoaded now;

	if (ed.working)
	{
		now = *ed.working;
	}
	Check(now.track.sections.size() == before + 1 && now.track.sections[1].pivotEnd[0] == 1234 && now.track.sections[1].pivotEnd[1] == -5678 && now.track.sections[1].pivotEnd[2] == 91011,
		where + ": the new section ends where it was finished");

	if (now.track.sections.size() > 2)
	{
		Check(now.track.sections[2].pivotStart[0] == 1234 && now.track.sections[2].pivotStart[2] == 91011, where + ": the section after starts at the new end");
	}

	host::TrackEdit_Command(f, &ed, 0x51f, place, &act);

	host::TrackLoaded gone;

	if (ed.working)
	{
		gone = *ed.working;
	}
	Check(act.changed && gone.track.sections.size() == before, where + ": Delete Section: " + act.words);

	if (before > 1)
	{
		Check(gone.track.sections[0].pivotStart[0] == loaded.track.sections[0].pivotStart[0], where + ": the first section's start is unchanged");
	}

	// the prompts' answers
	host::TrackEdit_AnswerTime(f, &ed, "2.5", &act);
	Check(act.changed, where + ": time: " + act.words);
	host::TrackEdit_AnswerTime(f, &ed, "0.0001", &act);
	Check(!act.changed, where + ": a time under 65 is refused");
	host::TrackEdit_AnswerFlags(f, &ed, "LP", &act);
	Check(act.changed, where + ": flags: " + act.words);
	host::TrackEdit_AnswerFlags(f, &ed, "Q", &act);
	Check(!act.changed, where + ": a flag letter that is not one is refused");
	host::TrackEdit_AnswerSound(f, &ed, 1, "door_open.wav 100 200 100 0 0", &act);
	Check(act.changed, where + ": sound: " + act.words);
	host::TrackEdit_AnswerSound(f, &ed, 1, "..\\evil.wav", &act);
	Check(!act.changed, where + ": a sound name with a slash is refused");
	host::TrackEdit_AnswerSound(f, &ed, 2, "x.wav 500 100", &act);
	Check(!act.changed, where + ": ranges the wrong way round are refused");

	// T-1: Sound > Edit hands the sound to edobtype's Sound Edit window (FUN_004f2bf0): the track's own, or (none) inteng's new one (FUN_004f2a60: "Not Set", flags 1, made in the editor's units)
	host::TrackEdit_Command(f, &ed, 0x528, place, &act);
	Check(act.prompt == host::TrackPrompt::Sound && act.soundIndex == 1 && !act.soundNew && act.sound.wav == "door_open.wav" && act.sound.innerRange == 100 && act.sound.outerRange == 200,
		where + ": Start Sound > Edit hands over the start sound as the track holds it");
	host::TrackEdit_Command(f, &ed, 0x52b, place, &act);
	host::TrackEdit_Command(f, &ed, 0x52a, place, &act);
	Check(act.prompt == host::TrackPrompt::Sound && act.soundIndex == 2 && act.soundNew && act.sound.wav == "Not Set" && act.sound.flags == 1 && act.sound.maxVolume == 127 && act.sound.pitch == 0,
		where + ": End Sound > Edit with no end sound hands over inteng's new one (Not Set, loops, volume 127, pitch 0)");

	// what the window gives back is taken whole, as inteng's __ftol takes it: "Not Set" (a Cancel on a new one), ranges the wrong way round
	{
		host::TrackSound back;

		back.wav = "Not Set";
		back.innerRange = 6250;
		back.outerRange = 3125;
		back.maxVolume = 101;
		back.pitch = -128;
		back.flags = 1;
		host::TrackEdit_SetSound(f, &ed, 2, back, &act);
		Check(act.changed, where + ": the window's sound is set whatever its numbers: " + act.words);

		host::TrackLoaded now;
		bool endIs = false;

		if (ed.working)
		{
			now = *ed.working;
		}

		for (host::TrackSound const &s : now.sounds)
		{
			endIs = endIs || (s.index == 2 && s.wav == "Not Set" && s.innerRange == 6250 && s.outerRange == 3125 && s.maxVolume == 101 && s.pitch == -128 && s.flags == 1);
		}

		Check(endIs, where + ": the end sound in memory is the window's");

		// HOSTILE: a name longer than the file's bound or with a zero inside is never written (the game copies the name to its first zero)
		back.wav = std::string(host::kTrackSoundNameMost + 1, 'a');
		host::TrackEdit_SetSound(f, &ed, 2, back, &act);
		Check(!act.changed, where + ": a name longer than " + std::to_string(host::kTrackSoundNameMost) + " letters is refused");
		back.wav = std::string("a\0b.wav", 7);
		host::TrackEdit_SetSound(f, &ed, 2, back, &act);
		Check(!act.changed, where + ": a name with a zero inside is refused");
		back.wav.clear();
		host::TrackEdit_SetSound(f, &ed, 2, back, &act);
		Check(!act.changed, where + ": an empty name is refused");
	}

	host::TrackLoaded fin;

	if (ed.working)
	{
		fin = *ed.working;
	}

	bool soundIs = false;

	for (host::TrackSound const &s : fin.sounds)
	{
		soundIs = soundIs || (s.index == 1 && s.wav == "door_open.wav" && s.innerRange == 100 && s.outerRange == 200 && s.maxVolume == 100);
	}

	Check(fin.track.sections.size() > ed.current && fin.track.sections[ed.current].time == 163840 && (fin.track.flags & 0xf) == 3 && soundIs, where + ": time, flags and the start sound are read back from the track in memory");

	// the end of the session: Stop Track Edit leaves the track in the level
	host::TrackEdit_Command(f, &ed, 0x51c, place, &act);
	Check(act.leave && ed.state == host::TrackState::Off && !ed.working, where + ": Stop Track Edit leaves");

	// ... and writes the track: the level holds it now (through the game's loader)
	{
		host::TrackLoaded written;

		Check(host::TrackLoad(f, id, &written) == host::TrackEditResult::Done && written.track.sections.size() == fin.track.sections.size() && written.track.sections[ed.current].time == 163840, where + ": Stop Track Edit wrote the track");
	}

	// Delete Track (yes) takes the chunks out; a track that was there is gone
	host::TrackEdit_Enter(f, id, &ed, &act);
	host::TrackEdit_Command(f, &ed, 0x520, place, &act);
	Check(act.prompt == host::TrackPrompt::DeleteTrack, where + ": Delete > Track asks");
	host::TrackEdit_AnswerDelete(f, &ed, "no", &act);
	Check(!act.changed && !act.leave, where + ": a no keeps the track");
	host::TrackEdit_AnswerDelete(f, &ed, "yes", &act);

	host::TrackClip none;

	Check(act.leave && host::TrackCopy(f, id, &none) == host::TrackEditResult::NoTrack, where + ": Delete Track took the OBJTRAK2 out");

	// the object has no track now. inteng's FUN_004f3330 (0x4f335c-0x4f33c4) asks only whether the object has its level object ("Please use Mode->Save and try again" for a new one not
	// yet saved: every object here has one) and then for the clipboard: with nothing copied, "Use copy track first", and nothing is changed. (OUR BUG, FIXED 2026-11-17 by Frost: since
	// 60ae1e5 a paste asked the object for a track of its own first and refused an object with none, "Please use Mode->Save and try again".)
	Check(!host::TrackExists(f, id) && host::TrackPasteAbsolute(f, id, none) == host::TrackEditResult::ClipboardEmpty && host::TrackPasteRelative(f, id, none) == host::TrackEditResult::ClipboardEmpty &&
		!host::TrackExists(f, id), where + ": with nothing copied, a paste onto an object with no track says Use copy track first, and nothing is changed");

	// an object with no track starts with the one-section default, and Stop Track Edit makes it
	host::TrackLoaded def;

	Check(host::TrackLoad(f, id, &def) == host::TrackEditResult::Done && !def.had && def.track.sections.size() == 1, where + ": an object with no track starts with one section");

	// and a track pasted onto an object with no track is taken: it holds the copied track again (inteng: the object's own OBJTRAK2, if any, taken out at 0x4f3431, the clipboard's made)
	host::TrackClip back;

	Check(host::TrackPasteAbsolute(f, id, clip) == host::TrackEditResult::Done && host::TrackExists(f, id) && host::TrackCopy(f, id, &back) == host::TrackEditResult::Done && back.track == clip.track,
		where + ": a track pasted onto an object with no track is taken, the copied one");
}

int main(int argc, char **argv)
{
	int levels = 0, tracks = 0, edited = 0, offsets = 0;

	for (int a = 1; a < argc; a++)
	{
		OpenLevelFile f;

		if (f.Open(argv[a]) != LevelOpenResult::Opened)
		{
			continue;
		}

		levels++;

		List<Object_Chunk *> objects;

		f.Level()->list_objects(&objects);

		// the IDs first: every edit replaces the level, so the list of chunks is not kept
		std::vector<std::vector<uint8_t> > ids;

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const o = each();

			if (o)
			{
				ids.push_back(std::vector<uint8_t>((uint8_t const *)&o->object_data.ID, (uint8_t const *)&o->object_data.ID + 8));
			}
		}

		std::string const name = argv[a];
		int doneHere = 0;

		for (std::vector<uint8_t> const &id : ids)
		{
			OneObject(&f, id.data(), name.substr(name.find_last_of("\\/") + 1), &tracks, &doneHere, &offsets);
		}

		edited += doneHere;
	}

	Check(levels > 0 && tracks > 0, "levels were given and held tracks (a test that tests nothing is not a pass)");
	printf("levels %d; tracks %d; sections with an object offset %d; spin sections %d; edit sequences %d; checks %d; failed %d\n", levels, tracks, offsets, spinSections, edited, checksRun, checksFailed);
	return checksFailed != 0;
}
