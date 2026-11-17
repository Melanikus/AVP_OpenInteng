// The test of src/host/bad_guy_edit: edobtype's "Edit object properties" window for a bad guy (resource 129, AvP mode; reassess/object-placement.md OP-132). Written 2026-11-16 by Melanikus (Vasquez)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_bad_guy_edit_test.exe (no files read)

#include <stdio.h>

#include <string>

#include "bad_guy_edit.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static SpecialObject ABadGuy(int32_t type, uint8_t subType)
{
	SpecialObject b;

	b.kind = SpecialKind::BadGuy;
	b.type = type;
	b.subType = subType;
	b.flags = 0x800 | 0x1000 | 0x200000;   // game modes Marine, Alien; NOT Medium (hidden boxes)
	return b;
}

int main()
{
	puts("what the window shows by type (FUN_10005400):");

	SpecialObject marine = ABadGuy(7, 60);

	marine.texture = 1;

	BadGuyControls c = BadGuy_Controls(marine, 7);

	Check(c.types.size() == 9 && c.types[c.typePick].first == "Marine" && c.leftLabel == "Subtype" && c.leftCombo && !c.leftEdit && c.rightLabel.empty() && !c.rightCombo && !c.rightEdit,
		"a Marine: \"Subtype\" over the subtype combo, nothing on the right; the type combo's nine rows, Marine picked");
	Check(c.leftChoices.size() == 18 && c.leftChoices[c.leftPick].first == "Sadar", "its 18 subtypes, 60 picked: Sadar");
	Check(c.textureLive && c.textures.size() == 2 && c.textures[c.texturePick].first == "Original", "Texturing ID live: Random, Original (no hnpcmarine.txt), 1 picked");

	SpecialObject predator = ABadGuy(2, (3 << 3) | 1);

	c = BadGuy_Controls(predator, 2);
	Check(c.leftLabel == "Primary" && c.rightLabel == "Secondary" && c.leftCombo && c.rightCombo && c.leftChoices[c.leftPick].first == "Wristblade" && c.rightChoices[c.rightPick].first == "Staff",
		"a Predator: Primary (bits 0-2: Wristblade) and Secondary (bits 3-5: Staff)");
	Check(!c.textureLive && c.textures.size() == 1 && c.texturePick == 0, "not a Marine: the Texturing ID is \"Random\" alone, picked, and off");

	SpecialObject xenoborg = ABadGuy(4, 0);

	xenoborg.extra1 = 12;
	xenoborg.extra2 = 30;
	c = BadGuy_Controls(xenoborg, 4);
	Check(c.leftLabel == "Active Time 0=default" && c.rightLabel == "Module Range - 0=default" && c.leftEdit && c.rightEdit && c.leftText == "60" && c.rightText == "30", "a Xenoborg: Active Time = extra 1 x 5, Module Range = extra 2");

	SpecialObject sentry = ABadGuy(9, 0);

	sentry.extra1 = 2;
	sentry.extra2 = 16;
	c = BadGuy_Controls(sentry, 9);
	Check(c.leftLabel == "Ammo (30000+ =infinite)" && c.leftEdit && !c.rightEdit && c.rightLabel.empty() && c.leftText == "528", "a Sentry Gun: the ammo = extra 1 x 256 + extra 2");

	c = BadGuy_Controls(marine, 1);
	Check(c.leftLabel == "Subtype" && c.leftChoices.size() == 1 && c.leftChoices[0].first == "Alien" && c.leftPick == -1, "the type picked again (Alien): its one subtype (0), not picked as the subtype is 60");

	puts("OK (FUN_10005de0, DDX 0x10003d20):");

	BadGuyTyped t;

	t.type = 7;
	t.leftPick = 10;
	t.texturePick = 0;
	t.startsInactive = true;

	SpecialObject out = BadGuy_Ok(marine, t);

	Check(out.type == 7 && out.subType == 10 && out.texture == 0 && (out.flags & 0x80000000u) != 0, "a Marine: the subtype and texture picked; Starts Inactive sets 0x80000000");
	Check((out.flags & 0x7fffffffu) == (marine.flags & 0x7fffffffu), "the hidden game mode and difficulty flags come back UNCHANGED (Bishop's condition)");

	t.leftPick = -1;
	t.texturePick = -1;
	out = BadGuy_Ok(marine, t);
	Check(out.subType == 60 && out.texture == 1, "nothing picked: the subtype and texture are kept");

	t = BadGuyTyped();
	t.type = 2;
	t.leftPick = 2;
	t.rightPick = 1;
	Check(BadGuy_Ok(predator, t).subType == ((1 << 3) | 2), "a Predator: (secondary & 7) << 3 | primary");

	t = BadGuyTyped();
	t.type = 4;
	t.leftText = "1300";
	t.rightText = "abc";
	out = BadGuy_Ok(xenoborg, t);
	Check(out.extra1 == 255 && out.extra2 == 30, "a Xenoborg: 1300 s / 5 capped at 255; no number keeps the Module Range (INTENG BUG, FIXED)");
	t.leftText = "-5";
	t.rightText = "300";
	out = BadGuy_Ok(xenoborg, t);
	Check(out.extra1 == 0 && out.extra2 == 255, "below 0 is 0 (INTENG BUG, FIXED: it wrapped to 255); 300 capped at 255");
	t.leftText = "x";
	Check(BadGuy_Ok(xenoborg, t).extra1 == 12, "no number keeps the Active Time (INTENG BUG, FIXED: the old byte was divided by 5)");

	t = BadGuyTyped();
	t.type = 9;
	t.leftText = "40000";
	out = BadGuy_Ok(sentry, t);
	Check(out.extra1 * 256 + out.extra2 == 30000, "a Sentry Gun: the ammo capped at 30000, split into the two bytes");

	if (checksFailed)
	{
		printf("FAILED host_bad_guy_edit_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_bad_guy_edit_test\n");
	return 0;
}
