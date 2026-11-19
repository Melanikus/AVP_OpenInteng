// verify_movable.cpp: the check program of the gizmo scenarios for P2's kinds (tests/scenarios/acceptance-gizmo-*.txt): a thing's place and turn in a level saved by Save a copy, read back
// through the game's own loader, compared with the same thing in the level as it was opened. Each kind is read with its movable.h adapter, whose own readers (the special objects', the
// lights', the virtual objects', the entry points', the placed hierarchies') the unit tests check against the chunks. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USAGE: verify_movable <the level as opened> <the saved copy> <kind> <which> <check> [<check> ...]
//   kind: start, particle, sound, generator, badguy (their number among their kind, from 0), hierarchy (its number), light (its place in the set, from 0),
//         virtual (its name), entry (<module object index>:<marker number>)
//   check: moved x | moved y | moved z   the copy's place is 10 mm or more from the opened one's along that axis, and within 1 mm on the other two (the turn as it was)
//          moved-any                     the copy's place is 10 mm or more from the opened one's (an entry point is held inside a box by its writer)
//          turned                        the copy's turn is 5 degrees or more from the opened one's, and its place within 1 mm
//          same                          the copy's place within 1 mm and its turn within half a degree
// It passes (exit 0) when every check does; the lines say what was found.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <windows.h>

#include "level_file.h"
#include "movable.h"
#include "virtual_object.h"

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

static int failed = 0;

static void Check(bool ok, std::string const &what)
{
	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
	failed += ok ? 0 : 1;
}

static bool WholeNumber(char const *text, int32_t *out)
{
	char *end = nullptr;
	long const value = strtol(text, &end, 10);

	if (!text[0] || !end || *end || value < -(1L << 30) || value > (1L << 30))
	{
		return false;
	}

	*out = (int32_t)value;
	return true;
}

// The ref the words name; kind None when they name none
static host::MovableRef RefOf(std::string const &kind, char const *which)
{
	host::MovableRef ref;
	int32_t number = -1;

	struct Named
	{
		char const *word;
		host::MovableKind kind;
	};

	static Named const numbered[] = {
		{ "start", host::MovableKind::PlayerStart }, { "particle", host::MovableKind::ParticleGenerator }, { "sound", host::MovableKind::Sound },
		{ "generator", host::MovableKind::AvpGenerator }, { "badguy", host::MovableKind::BadGuy }, { "hierarchy", host::MovableKind::PlacedHierarchy },
		{ "light", host::MovableKind::Light },
	};

	for (Named const &one : numbered)
	{
		if (kind == one.word && WholeNumber(which, &number) && number >= 0)
		{
			ref.kind = one.kind;
			ref.number = number;
			return ref;
		}
	}

	if (kind == "virtual" && strlen(which) > 0 && strlen(which) <= host::kVirtualObjectMostName)
	{
		ref.kind = host::MovableKind::VirtualObject;
		host::VirtualObject_Id(which, ref.id);
		return ref;
	}

	if (kind == "entry")
	{
		std::string const words(which);
		size_t const colon = words.find(':');
		int32_t module = -1;

		if (colon != std::string::npos && WholeNumber(words.substr(0, colon).c_str(), &module) && WholeNumber(words.substr(colon + 1).c_str(), &number) && module >= 0 && number >= 0)
		{
			ref.kind = host::MovableKind::EntryPoint;
			ref.module = module;
			ref.number = number;
		}
	}

	return ref;
}

// The angle between two turns, in degrees (a turn and its negative are the same)
static double DegreesApart(float const a[4], float const b[4])
{
	double dot = 0.0;

	for (int i = 0; i < 4; i++)
	{
		dot += (double)a[i] * (double)b[i];
	}

	dot = fabs(dot);
	dot = dot > 1.0 ? 1.0 : dot;
	return 2.0 * acos(dot) * 180.0 / 3.14159265358979;
}

static std::string Words(host::MovablePose const &pose)
{
	char text[160];

	_snprintf_s(text, sizeof(text), _TRUNCATE, "place %d %d %d, turn %.4f %.4f %.4f %.4f", (int)pose.place[0], (int)pose.place[1], (int)pose.place[2], pose.turn[0], pose.turn[1],
		pose.turn[2], pose.turn[3]);
	return text;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 6)
	{
		printf("  FAILED  usage: verify_movable <the level as opened> <the saved copy> <kind> <which> <check> [<check> ...]\n");
		return 1;
	}

	host::MovableRef const ref = RefOf(argv[3], argv[4]);

	if (ref.kind == host::MovableKind::None)
	{
		printf("  FAILED  \"%s %s\" names no kind and thing this program knows\n", argv[3], argv[4]);
		return 1;
	}

	OpenLevelFile before, after;
	host::MovablePose was, now;

	Check(before.Open(argv[1]) == LevelOpenResult::Opened, std::string("the level as opened is taken by the game's loader: ") + argv[1]);
	Check(after.Open(argv[2]) == LevelOpenResult::Opened, std::string("the saved copy is taken by the game's loader: ") + argv[2]);

	if (failed)
	{
		return 1;
	}

	bool const wasRead = host::Movable_Get(&before, ref, &was) == host::MovableResult::Done;
	bool const nowRead = host::Movable_Get(&after, ref, &now) == host::MovableResult::Done;

	Check(wasRead, std::string("the ") + host::Movable_KindName(ref.kind) + " is in the level as opened: " + Words(was));
	Check(nowRead, std::string("and in the copy: ") + Words(now));

	if (failed)
	{
		return 1;
	}

	double const turnApart = DegreesApart(was.turn, now.turn);
	double apart[3], farthest = 0.0;

	for (int axis = 0; axis < 3; axis++)
	{
		apart[axis] = fabs((double)now.place[axis] - (double)was.place[axis]);
		farthest = apart[axis] > farthest ? apart[axis] : farthest;
	}

	for (int a = 5; a < argc; a++)
	{
		std::string const check = argv[a];

		if (check == "moved" && a + 1 < argc && strlen(argv[a + 1]) == 1 && strchr("xyz", argv[a + 1][0]))
		{
			int const axis = argv[++a][0] - 'x';
			bool othersStill = true;

			for (int other = 0; other < 3; other++)
			{
				othersStill = othersStill && (other == axis || apart[other] <= 1.0);
			}

			Check(apart[axis] >= 10.0 && othersStill && turnApart < 0.5, std::string("moved along ") + argv[a] + " only: " + std::to_string((int)apart[0]) + " " + std::to_string((int)apart[1]) +
				" " + std::to_string((int)apart[2]) + " mm apart, the turn " + std::to_string(turnApart) + " degrees");
		}
		else if (check == "moved-any")
		{
			Check(farthest >= 10.0, "moved: " + std::to_string((int)farthest) + " mm on its farthest axis");
		}
		else if (check == "turned")
		{
			Check(turnApart >= 5.0 && farthest <= 1.0, "turned " + std::to_string(turnApart) + " degrees, the place " + std::to_string((int)farthest) + " mm apart at most");
		}
		else if (check == "same")
		{
			Check(farthest <= 1.0 && turnApart < 0.5, "as it was: " + std::to_string((int)farthest) + " mm and " + std::to_string(turnApart) + " degrees apart");
		}
		else
		{
			Check(false, "a check this program does not know: " + check);
		}
	}

	return failed ? 1 : 0;
}
