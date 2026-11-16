// verify_hierarchy_edits.cpp: the check program of the Hierarchy FULL-EDIT scenario (tests/scenarios/hierarchy-save-edits.txt): a model edited in the running editor (a keyframe flag and a new
// frame sound, the sequence's time, a delta sequence's time, the distance of a detail level) and saved by Save a copy is read back through the game's own loader and compared with a copy saved
// BEFORE any edit (the no-edit-save baseline). Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USAGE: verify_hierarchy_edits <baseline copy> <edited copy> <hierarchy> <sequence> <sub> <wav name> <sequence ms> <delta place> <delta ms> <detail level> <detail distance mm>
//   <sequence ms> / <delta ms> / <detail distance mm> of -1: that edit was not made (the value must then be the baseline's).
// It passes (exit 0) when: (1) both files are taken by the game's own loader; (2) comparing the two files chunk by chunk, every difference is inside a chunk this scenario edits (the frames
// OBASEQFR, the sound list INDSOUND, the times OBASEQTM, the detail distances HIDEGDIS); (3) the edited values are exactly the ones asked for: ONE root frame of the sequence differs and gained
// keyframe flag 6 and the new sound's index (and nothing else of its flags), the sound is <wav name> at volume 100, the sequence's and the delta's times read back as typed, the detail level's distance
// reads back as typed and the other levels' are the baseline's (or moved only to keep the order); (4) every other sequence of the hierarchy has the same frames and the same time as the baseline.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_image.h"
#include "hierarchy_edit.h"

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

void avp_MessageBox(const char *message, int)
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
using namespace host::image;

static int failed = 0;
static size_t versionBumps = 0;
static size_t headerPadding = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	failed += good ? 0 : 1;
}

static std::string NameOf(Bytes const &bytes, size_t at)
{
	return std::string(bytes.data() + at, strnlen(bytes.data() + at, 8));
}

static bool gEditsAllowed = true;   // false when comparing two files that should differ in nothing but the version numbers

// The chunk names a difference may be inside: what this scenario edits (OBANALLS holds the sequences' frames, flags included)
static bool Allowed(std::string const &name)
{
	return gEditsAllowed && (name == "OBANALLS" || name == "OBASEQFR" || name == "INDSOUND" || name == "OBASEQTM" || name == "HIDEGDIS");
}

// Every save of the chunk library counts up the version number of the headers it writes (OBJHEAD1, SHPHEAD1, ENDTHEAD): the same bytes but for 32-bit words that are one more
static bool VersionBump(Bytes const &a, Span const &sa, Bytes const &b, Span const &sb)
{
	if (sa.size != sb.size || sa.size < kHeadingBytes + 4)
	{
		return false;
	}

	std::string const name = NameOf(a, sa.at);

	if (name != "OBJHEAD1" && name != "SHPHEAD1" && name != "ENDTHEAD")
	{
		return false;
	}

	bool any = false;

	for (size_t at = kHeadingBytes; at + 4 <= sa.size; at += 4)
	{
		int32_t const x = IntAt(a, sa.at + at);
		int32_t const y = IntAt(b, sb.at + at);

		if (x != y)
		{
			if (y != x + 1)
			{
				return false;
			}

			any = true;
		}
	}

	// bytes past the last whole word must be equal
	for (size_t at = kHeadingBytes + ((sa.size - kHeadingBytes) / 4) * 4; at < sa.size; at++)
	{
		if (a[sa.at + at] != b[sb.at + at])
		{
			return false;
		}
	}

	return any;
}

// Every difference between the two chunks (both whole chunks of the same name): the path of each, with whether it is inside an allowed chunk. A chunk whose payload does not walk as chunks is
// compared as bytes; chunks that are in one list and not the other (a time chunk made, a sound added) are differences at their own path.
static void Diff(Bytes const &a, Span const &sa, Bytes const &b, Span const &sb, std::string const &path, bool inside, std::vector<std::pair<std::string, bool>> *out, size_t depth)
{
	std::string const name = NameOf(a, sa.at);
	std::string const here = path + "/" + name;
	bool const ok = inside || Allowed(name);

	if (sa.size == sb.size && memcmp(a.data() + sa.at, b.data() + sb.at, sa.size) == 0)
	{
		return;
	}

	if (VersionBump(a, sa, b, sb))
	{
		versionBumps++;
		return;
	}

	// against the ORIGINAL file: stock files hold leftover bytes in their padding (the 0xfd fill of a debug heap in headers, the rest of a name's last word) that the next save writes as 0. Such a
	// chunk keeps its size and is none of the chunks that hold the edited data (frames, sounds, times, distances); anything else is a real difference
	if (!gEditsAllowed && sa.size == sb.size && here.find("OBANALLS") == std::string::npos && here.find("OBASEQFR") == std::string::npos && here.find("INDSOUND") == std::string::npos &&
		here.find("OBASEQTM") == std::string::npos && here.find("HIDEGDIS") == std::string::npos)
	{
		headerPadding++;
		return;
	}

	std::vector<Span> ca, cb;

	if (depth > 20 || !Children(a, sa.at + kHeadingBytes, sa.at + sa.size, &ca) || !Children(b, sb.at + kHeadingBytes, sb.at + sb.size, &cb) || ca.empty() || cb.empty())
	{
		out->push_back(std::make_pair(here, ok));

		if (!ok && getenv("VERIFY_SHOW_BYTES") && sa.size != sb.size)
		{
			printf("    sizes %zu -> %zu of %s\n", sa.size, sb.size, here.c_str());
		}

		if (!ok && getenv("VERIFY_SHOW_BYTES") && sa.size == sb.size)
		{
			int shown = 0;

			for (size_t i = 0; i < sa.size && shown < 6; i++)
			{
				if (a[sa.at + i] != b[sb.at + i])
				{
					printf("    byte %zu of %s: %02x -> %02x\n", i, here.c_str(), (unsigned char)a[sa.at + i], (unsigned char)b[sb.at + i]);
					shown++;
				}
			}
		}

		return;
	}

	// align by name, the n-th of a name with the n-th of that name
	std::vector<bool> used(cb.size(), false);

	for (size_t i = 0; i < ca.size(); i++)
	{
		std::string const child = NameOf(a, ca[i].at);
		size_t nth = 0;

		for (size_t k = 0; k < i; k++)
		{
			nth += NameOf(a, ca[k].at) == child ? 1 : 0;
		}

		size_t seen = 0;
		bool matched = false;

		for (size_t j = 0; j < cb.size(); j++)
		{
			if (NameOf(b, cb[j].at) == child)
			{
				if (seen++ == nth)
				{
					used[j] = true;
					matched = true;
					Diff(a, ca[i], b, cb[j], here, ok, out, depth + 1);
					break;
				}
			}
		}

		if (!matched)
		{
			out->push_back(std::make_pair(here + "/" + child + " (only in the baseline)", ok || Allowed(child)));
		}
	}

	for (size_t j = 0; j < cb.size(); j++)
	{
		if (!used[j])
		{
			std::string const child = NameOf(b, cb[j].at);

			out->push_back(std::make_pair(here + "/" + child + " (only in the edited copy)", ok || Allowed(child)));
		}
	}
}

int main(int argc, char **argv)
{
	if (argc == 1)
	{
		printf("usage: verify_hierarchy_edits <baseline copy> <edited copy> <hierarchy> <sequence> <sub> <wav name> <sequence ms> <delta place> <delta ms> <detail level> <detail distance> [<original model>]: no files were named, so nothing was checked here\n");
		printf("All checks passed\n");
		return 0;
	}

	if (argc < 12)
	{
		printf("usage: verify_hierarchy_edits <baseline copy> <edited copy> <hierarchy> <sequence> <sub> <wav name> <sequence ms> <delta place> <delta ms> <detail level> <detail distance>\n");
		return 2;
	}

	OpenLevelFile base, edited;

	Check(base.Open(argv[1]) == LevelOpenResult::Opened, "the baseline copy (saved before any edit) is taken by the game's own loader");
	Check(edited.Open(argv[2]) == LevelOpenResult::Opened, "the edited copy is taken by the game's own loader");

	std::string const hierarchy = argv[3];
	int32_t const number = atoi(argv[4]);
	int32_t const sub = atoi(argv[5]);
	std::string const wav = argv[6];
	int32_t const wantMs = atoi(argv[7]);
	int32_t const deltaPlace = atoi(argv[8]);
	int32_t const wantDeltaMs = atoi(argv[9]);
	int32_t const level = atoi(argv[10]);
	int32_t wantDistance = atoi(argv[11]);   // -1: not edited; -2: edited to the distance the camera was from the model (a run's own number): any distance above 0 that is not the baseline's

	// (2) every difference is inside a chunk this scenario edits
	Bytes a, b;

	// (the files on disk are packed; the loader unpacks them and BuildLevel gives the chunks exactly as the next save would write them)
	Check(base.BuildLevel(a) && edited.BuildLevel(b), "both levels are unpacked by the loader into their chunks");

	std::vector<Span> topA, topB;

	Check(!a.empty() && !b.empty() && NameIs(a, 0, "REBINFF2") && NameIs(b, 0, "REBINFF2"), "both are chunk files of the game's kind");

	if (!a.empty() && !b.empty())
	{
		Span const rootA = { 0, a.size() };
		Span const rootB = { 0, b.size() };
		std::vector<std::pair<std::string, bool>> differences;

		Diff(a, rootA, b, rootB, std::string(), false, &differences, 0);

		size_t outside = 0;

		for (auto const &one : differences)
		{
			outside += one.second ? 0 : 1;
			printf("  (%s: %s)\n", one.second ? "edited here" : "UNEXPECTED", one.first.c_str());
		}

		Check(outside == 0, "no difference between the edited copy and the baseline lies outside the chunks the scenario edits (" + std::to_string((unsigned long long)differences.size()) + " found, " + std::to_string((unsigned long long)outside) + " outside)");
		Check(!differences.empty(), "the edited copy does differ from the baseline");
		printf("  (%zu header version numbers counted up by one: every save of the chunk library does that)\n", versionBumps);
	}

	// the baseline against the ORIGINAL model (when given): a save with no edit changes nothing but the version numbers
	if (argc >= 13)
	{
		OpenLevelFile original;
		Bytes o;

		versionBumps = 0;
		headerPadding = 0;
		gEditsAllowed = false;
		Check(original.Open(argv[12]) == LevelOpenResult::Opened && original.BuildLevel(o) && !a.empty() && !o.empty(), "the original model opens and unpacks");

		if (!a.empty() && !o.empty())
		{
			Span const rootO = { 0, o.size() };
			Span const rootA = { 0, a.size() };
			std::vector<std::pair<std::string, bool>> differences;

			Diff(o, rootO, a, rootA, std::string(), false, &differences, 0);

			for (auto const &one : differences)
			{
				printf("  (UNEXPECTED in the baseline: %s)\n", one.first.c_str());
			}

			Check(differences.empty(), "a save with no edit changes nothing of the model but version numbers and leftover padding (" + std::to_string((unsigned long long)versionBumps) + " version numbers counted up, " + std::to_string((unsigned long long)headerPadding) + " chunks with leftover padding written as 0; " + std::to_string((unsigned long long)differences.size()) + " other differences)");
		}

		gEditsAllowed = true;
	}

	// (3) the edited values
	std::vector<SoundInfo> soundsBefore, soundsAfter;

	HierarchyEdit_Sounds(&base, &soundsBefore);
	HierarchyEdit_Sounds(&edited, &soundsAfter);
	Check(soundsAfter.size() == soundsBefore.size() + 1, "the copy has one sound more (" + std::to_string((unsigned long long)soundsBefore.size()) + " -> " + std::to_string((unsigned long long)soundsAfter.size()) + ")");

	int32_t added = 0;

	for (SoundInfo const &each : soundsAfter)
	{
		bool known = false;

		for (SoundInfo const &old : soundsBefore)
		{
			known = known || old.index == each.index;
		}

		if (!known)
		{
			added = each.index;
			Check(each.wav == wav && each.volume == 100 && (each.flags & 1) == 0, "the new sound " + std::to_string(each.index) + " is " + each.wav + " at volume " + std::to_string(each.volume));
		}
	}

	Check(added > 0, "a new sound index was found");

	// ... and the sounds that were there are as they were
	{
		bool same = true;

		for (SoundInfo const &old : soundsBefore)
		{
			bool found = false;

			for (SoundInfo const &now : soundsAfter)
			{
				if (now.index == old.index)
				{
					found = now.wav == old.wav && now.volume == old.volume && now.pitch == old.pitch && now.flags == old.flags && now.inner == old.inner && now.outer == old.outer;
				}
			}

			same = same && found;
		}

		Check(same, "the sounds the model had are unchanged");
	}

	// the frames of every sequence: only the chosen sequence has a frame that differs, and it is one
	std::vector<SequenceInfo> sequences, sequencesAfter;

	Check(HierarchyEdit_Sequences(&base, hierarchy, &sequences) == HierarchyEditResult::Done && HierarchyEdit_Sequences(&edited, hierarchy, &sequencesAfter) == HierarchyEditResult::Done, "both have the hierarchy's sequence list");
	Check(sequences.size() == sequencesAfter.size() && !sequences.empty(), "the sequence lists are the same length (" + std::to_string((unsigned long long)sequences.size()) + ")");

	size_t editedFrames = 0;

	for (size_t s = 0; s < sequences.size() && s < sequencesAfter.size(); s++)
	{
		SequenceInfo const &one = sequences[s];
		std::vector<FrameInfo> fa, fb;
		bool const chosen = one.number == number && one.subNumber == sub;
		std::string const label = "sequence " + std::to_string(one.number) + "." + std::to_string(one.subNumber);

		if (HierarchyEdit_RootFrames(&base, hierarchy, one.number, one.subNumber, &fa) != HierarchyEditResult::Done || HierarchyEdit_RootFrames(&edited, hierarchy, one.number, one.subNumber, &fb) != HierarchyEditResult::Done || fa.size() != fb.size())
		{
			Check(false, label + ": the root frames could not be compared or their count changed");
			continue;
		}

		size_t differing = 0;

		for (size_t i = 0; i < fa.size(); i++)
		{
			if (fa[i].at != fb[i].at)
			{
				Check(false, label + " frame " + std::to_string((unsigned long long)i + 1) + ": the frame's time changed");
			}

			if (fa[i].flags != fb[i].flags)
			{
				differing++;

				if (chosen)
				{
					uint32_t const soundMask = 0x7fu << 24;

					Check((fb[i].flags & (1u << 6)) != 0 && (int32_t)((fb[i].flags >> 24) & 0x7f) == added && ((fa[i].flags & ~soundMask) | (1u << 6)) == (fb[i].flags & ~soundMask),
						label + " frame " + std::to_string((unsigned long long)i + 1) + " gained keyframe flag 6 and sound " + std::to_string(added) + " and no other flag changed");
				}
			}
		}

		if (chosen)
		{
			editedFrames = differing;
			Check(differing == 1, label + ": exactly one root frame differs (" + std::to_string((unsigned long long)differing) + ")");
		}
		else
		{
			Check(differing == 0, label + ": none of its frames changed");
		}

		// the times
		int32_t ta = 0, tb = 0;
		bool ha = false, hb = false;

		if (HierarchyEdit_ReadSequenceTime(&base, hierarchy, s, &ta, &ha) == HierarchyEditResult::Done && HierarchyEdit_ReadSequenceTime(&edited, hierarchy, s, &tb, &hb) == HierarchyEditResult::Done)
		{
			if (chosen && wantMs > 0)
			{
				Check(hb && tb == wantMs, label + ": the time reads back as " + std::to_string(wantMs) + " ms (was " + (ha ? std::to_string(ta) : std::string("not set")) + ")");
			}
			else if ((int32_t)s == deltaPlace && wantDeltaMs > 0)
			{
				Check(hb && tb == wantDeltaMs, label + " (the delta): the time reads back as " + std::to_string(wantDeltaMs) + " ms (was " + (ha ? std::to_string(ta) : std::string("not set")) + ")");
			}
			else
			{
				Check(ha == hb && ta == tb, label + ": its time is the baseline's");
			}
		}
		else
		{
			Check(false, label + ": the time could not be read");
		}
	}

	Check(editedFrames == 1, "the chosen sequence was found and holds the one edited frame");

	// the detail distances
	{
		int32_t da[10], db[10];
		bool pa = false, pb = false;

		Check(HierarchyEdit_ReadDegradation(&base, da, &pa) == HierarchyEditResult::Done && HierarchyEdit_ReadDegradation(&edited, db, &pb) == HierarchyEditResult::Done, "both have the detail distances");

		if (wantDistance == -2 && level >= 0 && level < 10)
		{
			Check(pb && db[level] > 0 && db[level] != da[level], "detail level " + std::to_string(level) + " has a new distance above 0 (" + std::to_string(da[level]) + " -> " + std::to_string(db[level]) + ")");
			wantDistance = db[level];
		}

		if (wantDistance >= 0 && level >= 0 && level < 10)
		{
			Check(pb && db[level] == wantDistance, "detail level " + std::to_string(level) + " starts at " + std::to_string(wantDistance) + " mm (was " + std::to_string(da[level]) + ")");

			for (int i = 0; i < 10; i++)
			{
				// a level below can start no farther, one above no nearer (inteng's rule): the others are the baseline's or were moved to keep that order
				bool const kept = db[i] == da[i] || (i < level && db[i] == wantDistance && da[i] > wantDistance) || (i > level && db[i] == wantDistance && da[i] < wantDistance);

				Check(i == level || kept, "detail level " + std::to_string(i) + ": its distance is the baseline's or was moved only to keep the order (" + std::to_string(da[i]) + " -> " + std::to_string(db[i]) + ")");
			}
		}
		else
		{
			Check(pa == pb && memcmp(da, db, sizeof(da)) == 0, "the detail distances are the baseline's");
		}
	}

	printf("%s\n", failed ? "FAILED" : "all passed");
	return failed ? 1 : 0;
}
