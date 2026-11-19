// host_movable_live_test.cpp: movable_live.h, the gizmo's dragged thing drawn where the drag has it (the owner's "the object doesn't move with it"): which thing each drawing question
// matches (a special object by its kind and number, the five movable kinds mapped to the four special kinds; a placed hierarchy by its ID, or by its number when the ref has none; a
// virtual object by its ID), nothing after Clear or for a kind of no icon, and the state line's currenticon. Written 2026-11-19 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <string.h>

#include <string>

#include "movable_live.h"
#include "state_line.h"

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

static host::MovablePose PoseAt(int32_t x, int32_t y, int32_t z)
{
	host::MovablePose pose;

	pose.place[0] = x;
	pose.place[1] = y;
	pose.place[2] = z;
	pose.turn[0] = 0.5f;
	pose.turn[3] = 0.866f;
	return pose;
}

int main()
{
	host::MovablePose got;
	host::MovableRef ref;
	uint8_t const idA[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, idB[8] = { 9, 9, 9, 9, 9, 9, 9, 9 };

	printf("NOTHING DRAGGED\n");
	Check(!host::MovableLive_PoseOfSpecial(host::SpecialKind::PlayerStart, 0, &got) && !host::MovableLive_PoseOfHierarchy(idA, 0, &got) &&
		!host::MovableLive_PoseOfVirtual(idA, &got), "no drag: every thing is drawn as the level holds it");

	printf("A SPECIAL OBJECT\n");
	ref.kind = host::MovableKind::BadGuy;
	ref.number = 3;
	host::MovableLive_Show(ref, PoseAt(100, -20, 300));
	Check(host::MovableLive_PoseOfSpecial(host::SpecialKind::BadGuy, 3, &got) && got.place[0] == 100 && got.place[1] == -20 && got.place[2] == 300 && got.turn[0] == 0.5f,
		"bad guy 3: drawn at the drag's pose, its turn too");
	Check(!host::MovableLive_PoseOfSpecial(host::SpecialKind::BadGuy, 2, &got) && !host::MovableLive_PoseOfSpecial(host::SpecialKind::Generator, 3, &got),
		"bad guy 2 and generator 3: as the level holds them");

	ref.kind = host::MovableKind::AvpGenerator;
	host::MovableLive_Show(ref, PoseAt(1, 2, 3));
	Check(host::MovableLive_PoseOfSpecial(host::SpecialKind::Generator, 3, &got) && !host::MovableLive_PoseOfSpecial(host::SpecialKind::BadGuy, 3, &got),
		"an AVP generator is the special kind Generator (movable_special.cpp's mapping), not BadGuy");

	host::MovableKind const movable[3] = { host::MovableKind::PlayerStart, host::MovableKind::ParticleGenerator, host::MovableKind::Sound };
	host::SpecialKind const special[3] = { host::SpecialKind::PlayerStart, host::SpecialKind::ParticleGenerator, host::SpecialKind::Sound };
	bool mapped = true;

	for (int k = 0; k < 3; k++)
	{
		ref.kind = movable[k];
		ref.number = 0;
		host::MovableLive_Show(ref, PoseAt(k, k, k));
		mapped = mapped && host::MovableLive_PoseOfSpecial(special[k], 0, &got) && got.place[0] == k;
	}

	Check(mapped, "player start, particle generator and sound map to their own special kinds");

	host::MovableLive_Clear();
	Check(!host::MovableLive_PoseOfSpecial(host::SpecialKind::Sound, 0, &got), "Clear: the sound is drawn as the level holds it again");

	printf("A PLACED HIERARCHY\n");
	ref = host::MovableRef();
	ref.kind = host::MovableKind::PlacedHierarchy;
	memcpy(ref.id, idA, 8);
	ref.number = 5;
	host::MovableLive_Show(ref, PoseAt(7, 8, 9));
	Check(host::MovableLive_PoseOfHierarchy(idA, 0, &got) && got.place[2] == 9, "by its ID when the ref has one (whatever its number)");
	Check(!host::MovableLive_PoseOfHierarchy(idB, 5, &got), "another ID, even with the ref's number: not it");

	memset(ref.id, 0, 8);
	host::MovableLive_Show(ref, PoseAt(4, 5, 6));
	Check(host::MovableLive_PoseOfHierarchy(idB, 5, &got) && got.place[0] == 4 && !host::MovableLive_PoseOfHierarchy(idB, 4, &got), "a ref with no ID: by its number");
	Check(!host::MovableLive_PoseOfSpecial(host::SpecialKind::BadGuy, 5, &got) && !host::MovableLive_PoseOfVirtual(idB, &got), "a placed hierarchy is no special object nor virtual object");

	printf("A VIRTUAL OBJECT\n");
	ref = host::MovableRef();
	ref.kind = host::MovableKind::VirtualObject;
	memcpy(ref.id, idB, 8);
	host::MovableLive_Show(ref, PoseAt(-5, 0, 5));
	Check(host::MovableLive_PoseOfVirtual(idB, &got) && got.place[0] == -5 && !host::MovableLive_PoseOfVirtual(idA, &got) && !host::MovableLive_PoseOfVirtual(nullptr, &got),
		"by its ID; another ID or none: not it");

	ref.kind = host::MovableKind::PlacedObject;
	host::MovableLive_Show(ref, PoseAt(1, 1, 1));
	Check(!host::MovableLive_PoseOfVirtual(idB, &got) && !host::MovableLive_PoseOfHierarchy(idB, 0, &got), "a placed object (Drake's live copy) matches none of these");
	ref.kind = host::MovableKind::None;
	host::MovableLive_Show(ref, PoseAt(1, 1, 1));
	Check(!host::MovableLive_PoseOfVirtual(idB, &got), "kind None: nothing is dragged");
	host::MovableLive_Clear();

	printf("THE STATE LINE\n");
	{
		double const at[3] = { 10.4, -20.6, 30.5 };

		StateLine_NewFrame();
		host::MovableLive_ForgetDrawn();
		host::MovableLive_StateLine();

		std::string const empty = StateLine_Words();

		StateLine_NewFrame();
		host::MovableLive_NoteDrawn("player start", at);
		host::MovableLive_StateLine();

		std::string const noted = StateLine_Words();

		double const axes[9] = { 1, 0, 0, 0, 1, 0, 0.6, -0.0004, 0.8 };

		StateLine_NewFrame();
		host::MovableLive_NoteDrawn("player start", at, axes);
		host::MovableLive_StateLine();

		std::string const turned = StateLine_Words();

		Check(empty.find("currenticon \"\"") != std::string::npos, "nothing drawn: currenticon is empty: " + empty);
		Check(noted.find("currenticon \"player start 10 -21 31\"") != std::string::npos, "drawn unturned: its words and its place to the nearest: " + noted);
		Check(turned.find("currenticon \"player start 10 -21 31 facing 600 0 800\"") != std::string::npos, "drawn turned: and its third axis (the way it faces) in thousandths: " + turned);
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
