// autolink_rules.h: EVERY number of inteng's AutoLink, in ONE place, each with the address it is read from (Bishop's ruling 2026-11-16, for Melanikus: "we might want to tune their algorithm soon
// after seeing the results, otherwise (the position of the shots/etc) should stay loyal to code"). PHASE 1 is 1:1: the cameras (Drake), the meet test (Vasquez), the read-back and the
// driver (Dietrich) use THESE constants, never their own copies; tuning comes later, only on his word, measured against his inteng-saved oracle. Rows: reassess/module-linking-map-dietrich.md
// ML-A-*, reassess/autolink-pictures-drake.md AP-*. Units: inteng's memory units (the file's numbers x 100 x the level scale, row ML-E-7). Written 2026-11-16 by Melanikus (Dietrich, the
// AutoLink lead) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

namespace host
{
	namespace autolink_rules
	{
		// ---- the picture (FUN_004c6380's screen, FUN_0057f520(..., 0xa0, 0x78, 0xa0, 0xa0, 0xa0, 0, 0x140, 0, 0xf0, ...) at 0x4c6c00)
		int32_t const kPictureWidth = 0x140;      // 320
		int32_t const kPictureHeight = 0xf0;      // 240
		int32_t const kCentreX = 0xa0;            // 160
		int32_t const kCentreY = 0x78;            // 120
		int32_t const kLens = 0xa0;               // 160 (the lens on both axes)

		// ---- the read-back (FUN_004c6fb0)
		int32_t const kReadRows = 0xee;           // 238 rows scanned (0x4c708f / 0x4c7146): the 3 x 3 block stays inside the picture
		int32_t const kReadColumns = 0x13e;       // 318 columns (0x4c7110 / 0x4c71ae)
		int32_t const kSeenBlock = 3;             // a module is SEEN when a 3 x 3 block holds its ID (0x4c70c7-0x4c70ed, 0x4c7178-0x4c7194)
		int32_t const kIdOfNumber = 1;            // a module's ID = its number + 1 (FUN_004c6380 0x4c6bb1); 0 = nothing drawn (inteng's 16-bit mode: 0x7fff, 0x4c70a3)

		// ---- the views (FUN_004c7ab0; reassess/autolink-pictures-drake.md AP-1..AP-6, read on dump-full-2 and listing-all-2; added 2026-11-16 by Melanikus (Drake))
		int32_t const kStepBack = 10000;          // the camera on the contact axis: (planeA + planeB) / 2 - direction x 10000 (0x4c7e56); off the axis the overlap's middle (lo + hi) / 2 (0x4c7db3)
		int32_t const kOne = 0x10000;             // 1.0 in 16.16
		int32_t const kTurnA = 0x5798;            // 0.342 = cos 70 degrees: the four turns are about 70 degrees (their entries set at 0x4c7c64-0x4c7d54; the products at 0x4c7f64 / 0x4c8046 / 0x4c8128 / 0x4c81fc)
		int32_t const kTurnB = 0xf08f;            // 0.940 = sin 70 degrees
		int32_t const kShift = 500;               // the two extra views stand 500 units BEYOND THE OVERLAP'S SIDE on the OTHER horizontal axis (z for a contact on x, x for a contact on z): view 6 on one
		                                          // side, view 7 on the other (0x4c82ff-0x4c8313, 0x4c841e-0x4c8432; 0x4c84fe-0x4c8512, 0x4c85ca-0x4c85de)
		int32_t const kShiftSkipsAxis = 1;        // ... on the x and z axes only: the y axis (1) has no extra views (0x4c84cf)

		// The six base matrices (AP-1: the locals filled at 0x4c7ad3-0x4c7c5d, chosen by axis and direction at 0x4c7dcc-0x4c7e28), 16.16 rows: the camera's right, down and forward in the
		// level. [axis 0 x / 1 y / 2 z][direction -1 / +1]
		int32_t const kBase[3][2][9] =
		{
			{ { 0, 0, kOne, 0, kOne, 0, -kOne, 0, 0 },     // x, -1: local_12c
			  { 0, 0, -kOne, 0, kOne, 0, kOne, 0, 0 } },   // x, +1: local_30
			{ { 0, 0, -kOne, kOne, 0, 0, 0, -kOne, 0 },    // y, -1: local_54
			  { 0, 0, kOne, kOne, 0, 0, 0, kOne, 0 } },    // y, +1: local_e4
			{ { -kOne, 0, 0, 0, kOne, 0, 0, 0, -kOne },    // z, -1: local_108
			  { kOne, 0, 0, 0, kOne, 0, 0, 0, kOne } },    // z, +1: local_9c
		};

		// The four turns (AP-3, 0x4c7c64-0x4c7d54), in the order the views use them: T1 local_174 and T2 local_150 about the camera's down axis, T3 local_c0 and T4 local_78 about its right axis.
		// A turned view's matrix is turn x base, each product the 64-bit product floored by 16 (FUN_004b34b0, FUN_00499b30: IMUL, SHRD 16; AP-4).
		int32_t const kTurn[4][9] =
		{
			{ kTurnA, 0, -kTurnB, 0, kOne, 0, kTurnB, 0, kTurnA },
			{ kTurnA, 0, kTurnB, 0, kOne, 0, -kTurnB, 0, kTurnA },
			{ kOne, 0, 0, 0, kTurnA, -kTurnB, 0, kTurnB, kTurnA },
			{ kOne, 0, 0, 0, kTurnA, kTurnB, 0, -kTurnB, kTurnA },
		};

		// The views' order (AP-5, AP-6): the picture is taken before each product, so view 1 the base, views 2-5 T1..T4 at the place; then (x and z contacts) view 6 by T2 and view 7 by T1
		// (read from the stack offsets: [ESP+0x94] = local_150 at 0x4c82df / 0x4c84de, then local_174 at 0x4c83fe / 0x4c85aa)
		int const kShiftedTurn[2] = { 1, 0 };     // kTurn index of view 6, of view 7

		// ---- the meet test (FUN_004c4750; every one CHECKED line by line on dump-full-2 61547-62369 by Vasquez 2026-11-16 ~20:00, reassess/autolink-meet-vasquez.md AM-*)
		int32_t const kMeetWide = 29999;          // an axis whose overlap is above this counts as wide (`29999 < overlap`, 0x4c48c1)
		int32_t const kMeetWideAxes = 2;          // fewer wide axes than this: no meet (`wide < 2`, 0x4c48e6)
		int32_t const kMeetGap = -1000;           // the contact axis's overlap must be at least this (the boxes may be 1 m apart, `least < -1000`: no meet, 0x4c4907)
		int32_t const kPlaneNear = 1000;          // a point within this of the contact plane is ON it (0x3e9 = 1001: `|d| < 0x3e9`, A's points 0x4c4a94, B's 0x4c4b83)
		int32_t const kNormalAlong = 10000;       // a polygon's normal is along the axis when its component passes +-10000 (`n < 0x2711` else along +, `n < -10000` along -: A 0x4c4d5c /
		                                          // 0x4c4d81, B 0x4c4f82 / 0x4c4fa7)
		int32_t const kPortalWide = 1000;         // the portal must be wider than this on both axes that are not the contact axis (`lower - 1000 <= upper`: no meet, 0x4c56d2)
		double const kNarrowRatioStart = 2.0;     // the narrowing passes' least ratio starts here: an axis is taken only when its ratio is below it (A 0x4c5297, B 0x4c5574)
		int32_t const kCornerBoxStart = 1000000000;   // the narrowing passes' box of a polygon's corners starts at +1000000000 (its minimum) and -1000000000 (its maximum) (A 0x4c51e5-0x4c51fe,
		                                              // B 0x4c54bd-0x4c54d6)

		// ---- the distance cut and Set Visibility (FUN_004c6fb0 0x4c7317-0x4c73c0; FUN_004c1a90 0xa4 0x4c3620-0x4c3685)
		int32_t const kVisibilityPerMetre = 100000;   // DAT_00704378 = the metres typed x 100000; 0 = no cut (any other value, a negative one too, cuts)

		// ---- the setup (FUN_004c6380)
		int32_t const kMostWithoutHardware = 0xfa;    // 250: the 8-bit mode's limit (modules + extras), "Too many modules to link without hardware acceleration" (0x4c6753); not used by ours (32-bit)

		// ---- the driver's words (FUN_004c8930, FUN_004c8af0, FUN_004c8be0)
		int32_t const kCancelX = 200;             // "Right click to cancel" (0x5e3c88) at (200, 220) (0x4c8977); (200, 200) for Load from file (0x4c8d9c)
		int32_t const kCancelY = 0xdc;
		int32_t const kCancelYFromFile = 0xc8;
		int32_t const kCounterX = 0xe6;           // "%03d" (0x5e249c) at (230, 180) (0x4c8a20)
		int32_t const kCounterY = 0xb4;
	}
}
