// host_movable_test.cpp: movable.h's interface on its own (the kind table, the selection's ref, the checks every pose goes through, the call to the right adapter), with
// stand-in adapters that count their calls: no level is opened here (host_movable_placed_object_test does that). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>

#include "movable.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

// THE STAND-INS: every adapter answers what the test sets, and remembers what it was given
static int gets = 0, sets = 0;
static host::MovableKind lastKind = host::MovableKind::None;
static host::MovablePose given, toGive;
static host::MovableResult answer = host::MovableResult::Done;

#define STAND_IN(Name)                                                                                                  \
	host::MovableResult host::Movable##Name##_Get(OpenLevelFile *, MovableRef const &ref, MovablePose *pose)               \
	{                                                                                                                   \
		gets++;                                                                                                         \
		lastKind = ref.kind;                                                                                            \
		*pose = toGive;                                                                                                 \
		return answer;                                                                                                  \
	}                                                                                                                   \
	host::MovableResult host::Movable##Name##_Set(OpenLevelFile *, MovableRef const &ref, MovablePose const &pose)         \
	{                                                                                                                   \
		sets++;                                                                                                         \
		lastKind = ref.kind;                                                                                            \
		given = pose;                                                                                                   \
		return answer;                                                                                                  \
	}

STAND_IN(PlacedObject)
STAND_IN(PlacedHierarchy)
STAND_IN(Special)
STAND_IN(VirtualObject)
STAND_IN(Light)
STAND_IN(EntryPoint)
STAND_IN(Waypoint)
STAND_IN(TrackEnd)

static host::MovableRef Ref(host::MovableKind kind)
{
	host::MovableRef ref;

	ref.kind = kind;
	ref.id[0] = 7;
	return ref;
}

static host::MovablePose Pose(int32_t x, int32_t y, int32_t z, float qx, float qy, float qz, float qw)
{
	host::MovablePose pose;

	pose.place[0] = x;
	pose.place[1] = y;
	pose.place[2] = z;
	pose.turn[0] = qx;
	pose.turn[1] = qy;
	pose.turn[2] = qz;
	pose.turn[3] = qw;
	return pose;
}

int main()
{
	using host::MovableKind;
	using host::MovableResult;

	// a level is never read by the stand-ins: any non-null pointer stands for an open one
	static char levelStandIn;
	OpenLevelFile *const open = reinterpret_cast<OpenLevelFile *>(&levelStandIn);

	printf("THE KINDS\n");
	{
		bool const turns[] = { false, true, true, true, true, false, false, false, false, true, false, false, true };
		bool all = true;

		for (int k = 0; k < (int)MovableKind::Count; k++)
		{
			all = all && host::Movable_CanRotate((MovableKind)k) == turns[k];
		}

		Check(all, "the turn table: placed objects, hierarchies, player starts, particle generators, lights and track ends turn; sounds, generators, bad guys, virtual objects, entry points, waypoint boxes don't");
		Check(!host::Movable_CanRotate(MovableKind::Count) && !host::Movable_CanRotate((MovableKind)200), "Count and past it hold no turn");

		bool inLevel = true;

		for (int k = 0; k < (int)MovableKind::Count; k++)
		{
			bool const memory = (MovableKind)k == MovableKind::TrackSectionEnd || (MovableKind)k == MovableKind::WaypointBox;

			inLevel = inLevel && host::Movable_PoseInLevel((MovableKind)k) == !memory;
		}

		Check(inLevel, "every kind's pose is in the level but a track's section end and a waypoint box (held in the editor's memory while edited: read every time)");
		Check(strcmp(host::Movable_KindName(MovableKind::Sound), "sound") == 0 && strcmp(host::Movable_KindName((MovableKind)200), "nothing") == 0, "the names, and \"nothing\" past the end");
		Check(strcmp(host::Movable_ResultWords(MovableResult::NotBuilt), "the mouse can't move this kind yet") == 0, "an answer's words");
	}

	printf("THE SELECTION'S REF\n");
	{
		host::Selected object = { host::SelectedKind::Object, 3, { 1, 2, 3, 4, 5, 6, 7, 8 }, 0 };
		host::MovableRef const ref = host::Movable_FromSelection(object);

		Check(ref.kind == MovableKind::PlacedObject && memcmp(ref.id, object.objectId, 8) == 0 && ref.module == 3, "a selected object is a placed object, its ID and module kept");

		host::Selected noId = { host::SelectedKind::Object, 3, { 0 }, 0 };
		host::Selected module = { host::SelectedKind::Module, 3, { 1 }, 0 };
		host::Selected nothing = { host::SelectedKind::Nothing, -1, { 0 }, 0 };

		Check(host::Movable_FromSelection(noId).kind == MovableKind::None, "an object with an all-0 ID: nothing movable");
		Check(host::Movable_FromSelection(module).kind == MovableKind::None && host::Movable_FromSelection(nothing).kind == MovableKind::None, "a module, or nothing: nothing movable");
	}

	printf("THE CHECKS\n");
	{
		host::MovablePose usable;
		int32_t const most = host::kMovableMostPlace;

		Check(host::Movable_CheckPose(Pose(most, -most, 0, 0, 0, 0, 1), &usable) == MovableResult::Done, "a place at the limit on two axes is taken");
		Check(host::Movable_CheckPose(Pose(most + 1, 0, 0, 0, 0, 0, 1), &usable) == MovableResult::PlaceRefused, "one past it is refused (x)");
		Check(host::Movable_CheckPose(Pose(0, 0, -most - 1, 0, 0, 0, 1), &usable) == MovableResult::PlaceRefused, "and on z, below");
		Check(host::Movable_CheckPose(Pose(0, INT32_MIN, 0, 0, 0, 0, 1), &usable) == MovableResult::PlaceRefused, "the smallest whole number is refused");

		float const nan = std::numeric_limits<float>::quiet_NaN();
		float const inf = std::numeric_limits<float>::infinity();

		Check(host::Movable_CheckPose(Pose(0, 0, 0, nan, 0, 0, 1), &usable) == MovableResult::TurnRefused, "a turn with not-a-number in it is refused");
		Check(host::Movable_CheckPose(Pose(0, 0, 0, 0, inf, 0, 1), &usable) == MovableResult::TurnRefused, "and one with an infinity");
		Check(host::Movable_CheckPose(Pose(0, 0, 0, 0, 0, 0, 0), &usable) == MovableResult::TurnRefused, "a turn 0 long is refused");
		Check(host::Movable_CheckPose(Pose(0, 0, 0, 1e-5f, 0, 0, 0), &usable) == MovableResult::TurnRefused, "and one about 0 long");

		Check(host::Movable_CheckPose(Pose(10, 20, 30, 0, 3, 0, 4), &usable) == MovableResult::Done && fabs(usable.turn[1] - 0.6f) < 1e-6 && fabs(usable.turn[3] - 0.8f) < 1e-6 &&
			usable.place[0] == 10 && usable.place[2] == 30, "a turn 5 long is made 1 long (0 0.6 0 0.8), the place kept");
		Check(host::Movable_CheckPose(Pose(0, 0, 0, 3e38f, 3e38f, 0, 0), &usable) == MovableResult::Done && fabs(usable.turn[0] - 0.70710678f) < 1e-5, "even a huge one: its square fits a double");
	}

	printf("THE CALL TO THE KIND'S ADAPTER\n");
	{
		host::MovablePose pose;

		gets = sets = 0;
		Check(host::Movable_Get(open, Ref(MovableKind::None), &pose) == MovableResult::KindRefused && host::Movable_Set(open, Ref(MovableKind::Count), Pose(0, 0, 0, 0, 0, 0, 1)) ==
			MovableResult::KindRefused && host::Movable_Set(open, Ref((MovableKind)200), Pose(0, 0, 0, 0, 0, 0, 1)) == MovableResult::KindRefused && gets == 0 && sets == 0,
			"None, Count and past it: refused, no adapter called");
		Check(host::Movable_Get(nullptr, Ref(MovableKind::Light), &pose) == MovableResult::NothingOpen && host::Movable_Set(nullptr, Ref(MovableKind::Light), Pose(0, 0, 0, 0, 0, 0, 1)) ==
			MovableResult::NothingOpen && gets == 0 && sets == 0, "no level open: NothingOpen, no adapter called");

		answer = MovableResult::Done;
		Check(host::Movable_Set(open, Ref(MovableKind::PlacedObject), Pose(1, 2, 3, 0, 0, 0, 2)) == MovableResult::Done && sets == 1 && lastKind == MovableKind::PlacedObject &&
			given.place[1] == 2 && fabs(given.turn[3] - 1.0f) < 1e-6, "a placed object's Set reaches its adapter with the turn made 1 long");
		Check(host::Movable_Set(open, Ref(MovableKind::PlacedObject), Pose(host::kMovableMostPlace + 5, 0, 0, 0, 0, 0, 1)) == MovableResult::PlaceRefused && sets == 1,
			"a refused place never reaches the adapter");
		Check(host::Movable_Set(open, Ref(MovableKind::Sound), Pose(5, 6, 7, 0.5f, 0.5f, 0.5f, 0.5f)) == MovableResult::Done && sets == 2 && lastKind == MovableKind::Sound &&
			given.place[0] == 5 && given.turn[0] == 0.0f && given.turn[3] == 1.0f, "a sound (no turn): its adapter gets the place and the identity turn");

		toGive = Pose(100, 200, 300, 0, 0, 0, 1);
		Check(host::Movable_Get(open, Ref(MovableKind::TrackSectionEnd), &pose) == MovableResult::Done && gets == 1 && lastKind == MovableKind::TrackSectionEnd && pose.place[2] == 300,
			"Get reaches the kind's adapter and gives its pose");

		toGive = Pose(0, 0, 0, 0.6f, 0, 0, 0.8f);
		Check(host::Movable_Get(open, Ref(MovableKind::BadGuy), &pose) == MovableResult::Done && pose.turn[0] == 0.0f && pose.turn[3] == 1.0f, "a bad guy's Get: the identity turn, whatever was read");

		toGive = Pose(host::kMovableMostPlace * 2, 0, 0, 0, 0, 0, 1);
		pose = Pose(9, 9, 9, 0, 0, 0, 1);
		Check(host::Movable_Get(open, Ref(MovableKind::PlacedObject), &pose) == MovableResult::PlaceRefused && pose.place[0] == 9, "a level's place past the limit: refused, *pose untouched");

		toGive = Pose(0, 0, 0, std::numeric_limits<float>::quiet_NaN(), 0, 0, 1);
		Check(host::Movable_Get(open, Ref(MovableKind::PlacedObject), &pose) == MovableResult::TurnRefused && pose.place[0] == 9, "a level's turn of not-a-number: refused");

		answer = MovableResult::NotBuilt;
		Check(host::Movable_Set(open, Ref(MovableKind::WaypointBox), Pose(0, 0, 0, 0, 0, 0, 1)) == MovableResult::NotBuilt,
			"an adapter's own answer comes back (NotBuilt)");
		answer = MovableResult::WriterRefused;
		Check(host::Movable_Set(open, Ref(MovableKind::PlacedObject), Pose(0, 0, 0, 0, 0, 0, 1)) == MovableResult::WriterRefused, "and WriterRefused");
	}

	printf("THE FIRST OF TWO THAT READS (the selection's, else the screen's)\n");
	{
		host::MovablePose pose = Pose(1, 1, 1, 0, 0, 0, 1);
		host::MovableRef used;

		answer = MovableResult::Done;
		toGive = Pose(10, 20, 30, 0, 0, 0, 1);
		gets = 0;
		Check(host::Movable_GetFirst(open, Ref(MovableKind::PlacedObject), Ref(MovableKind::Sound), &used, &pose) == MovableResult::Done && used.kind == MovableKind::PlacedObject &&
			pose.place[0] == 10 && gets == 1, "the first reads: it is used, the second is not asked");

		host::MovableRef const none;

		Check(host::Movable_GetFirst(open, none, Ref(MovableKind::Light), &used, &pose) == MovableResult::Done && used.kind == MovableKind::Light && gets == 2,
			"the first names nothing: the second is used");

		toGive = Pose(host::kMovableMostPlace * 2, 0, 0, 0, 0, 0, 1);   // every read refused (the place)
		used = none;
		pose = Pose(5, 5, 5, 0, 0, 0, 1);
		Check(host::Movable_GetFirst(open, Ref(MovableKind::PlacedObject), Ref(MovableKind::VirtualObject), &used, &pose) == MovableResult::PlaceRefused && used.kind ==
			MovableKind::None && pose.place[0] == 5 && gets == 4, "neither reads: the second's answer, *used and *pose untouched");
		Check(host::Movable_GetFirst(open, Ref(MovableKind::PlacedObject), none, &used, &pose) == MovableResult::PlaceRefused && gets == 5,
			"the second names nothing: the first's answer");
		Check(host::Movable_GetFirst(open, none, none, &used, &pose) == MovableResult::KindRefused && gets == 5, "both name nothing: KindRefused, nothing read");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
