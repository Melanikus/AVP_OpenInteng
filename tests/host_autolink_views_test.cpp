// The test of AutoLink's cameras and read-back (src/host/autolink_views.*), against the rows AvP-docs\engine\inteng\reassess\autolink-pictures-drake.md AP-1..AP-8. Written 2026-11-16 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". It reads no file. Each check's words say what it proves.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "autolink_views.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Is(host::AutoLinkView const &v, int32_t x, int32_t y, int32_t z, std::vector<int32_t> const &turn)
{
	return v.place[0] == x && v.place[1] == y && v.place[2] == z && turn.size() == 9 && memcmp(v.turn, turn.data(), 36) == 0;
}

int main()
{
	int32_t const one = 0x10000, s = 0x5798, c = 0xf08f;

	puts("The views of a portal (AP-1..AP-6):");

	host::Portal x = {};

	x.lo[0] = 1000; x.hi[0] = 1000; x.lo[1] = -301; x.hi[1] = 600; x.lo[2] = 2000; x.hi[2] = 4000;
	x.planeA = 1000; x.planeB = 1001; x.axis = 0; x.direction = 1;

	std::vector<host::AutoLinkView> views;

	host::AutoLink_ViewsFor(x, &views);
	Check(views.size() == 7, "a contact on x: 7 views (5 + the two shifted, AP-6)");
	Check(views.size() == 7 && Is(views[0], 1000 - 10000, 149, 3000, { 0, 0, -one, 0, one, 0, one, 0, 0 }) && views[0].step == 0,
		"view 1: x +1's base (local_30), at the planes' middle 10000 back into A ((1000 + 1001) / 2 = 1000), the overlap's middle elsewhere ((-301 + 600) / 2 = 149, toward 0)");
	// T1 x base, every product floored (FUN_00499b30): row 0 = s * (0,0,-1) + (-c) * (1,0,0) ...
	Check(views.size() == 7 && views[1].step == 1 && views[1].turn[2] == (int32_t)(((int64_t)s * -one) >> 16) && views[1].turn[0] == (int32_t)(((int64_t)-c * one) >> 16),
		"view 2: T1 x base (out = turn x base, AP-4)");
	Check(views.size() == 7 && views[5].step == 5 && views[5].place[2] == 2000 - 500 && views[5].place[0] == 1000 - 10000,
		"view 6 (x, direction +1): T2, z on the low side minus 500 (0x4c82ff-0x4c8313)");
	Check(views.size() == 7 && views[6].step == 6 && views[6].place[2] == 4000 + 500, "view 7: T1, z on the high side plus 500 (0x4c841e-0x4c8432)");

	int32_t t2[9], t1[9];

	memcpy(t2, views.size() == 7 ? views[2].turn : t2, 36);
	memcpy(t1, views.size() == 7 ? views[1].turn : t1, 36);
	Check(views.size() == 7 && memcmp(views[5].turn, t2, 36) == 0 && memcmp(views[6].turn, t1, 36) == 0, "the shifted views turn by T2 then T1 (read from the stack offsets at 0x4c82df / 0x4c83fe)");

	host::Portal y = x;

	y.axis = 1;
	host::AutoLink_ViewsFor(y, &views);
	Check(views.size() == 5, "a contact on y: 5 views, no shifted ones (0x4c84cf)");

	host::Portal z = x;

	z.axis = 2; z.direction = -1; z.lo[0] = -100; z.hi[0] = 100;
	host::AutoLink_ViewsFor(z, &views);
	Check(views.size() == 7 && Is(views[0], 0, 149, (1000 + 1001) / 2 + 10000, { -one, 0, 0, 0, one, 0, 0, 0, -one }), "z -1: local_108, 10000 back the other way");
	Check(views.size() == 7 && views[5].place[0] == -100 - 500 && views[6].place[0] == 100 + 500, "z, direction -1: view 6 on the low x side, view 7 on the high");

	host::Portal bad = x;

	bad.axis = 3;
	host::AutoLink_ViewsFor(bad, &views);
	Check(views.empty(), "a portal with an axis that is not 0-2 gives no view");

	puts("The read-back (AP-8):");

	std::vector<uint32_t> picture(320 * 240, 0);
	std::vector<int32_t> seen;
	auto put = [&](int r, int col, uint32_t id) { picture[(size_t)r * 320 + (size_t)col] = id; };

	Check(host::AutoLink_Seen(picture, 5, &seen) && seen.empty(), "an empty picture: true, nothing seen");

	for (int r = 10; r < 13; r++) for (int col = 20; col < 23; col++) put(r, col, 3);
	Check(host::AutoLink_Seen(picture, 5, &seen) && seen.size() == 1 && seen[0] == 2, "a 3 x 3 block of ID 3: module number 2 seen");

	put(50, 50, 4); put(50, 51, 4); put(51, 50, 4); put(51, 51, 4);
	Check(host::AutoLink_Seen(picture, 5, &seen) && seen.size() == 1, "a 2 x 2 block of ID 4: not seen");

	for (int r = 237; r < 240; r++) for (int col = 317; col < 320; col++) put(r, col, 5);
	Check(host::AutoLink_Seen(picture, 5, &seen) && seen.size() == 2 && seen[1] == 4, "a block in the last corner (start 237, 317, the last of the 318 x 238): seen");

	put(239, 319, 0);
	Check(host::AutoLink_Seen(picture, 5, &seen) && seen.size() == 1, "one pixel short in that corner: not seen");

	put(100, 100, 6);
	Check(!host::AutoLink_Seen(picture, 5, &seen) && seen.empty(), "an ID above the module count: false, nothing (\"unexpected colour found\", 0x4c7114)");

	put(100, 100, 0);
	put(239, 100, 6);
	Check(host::AutoLink_Seen(picture, 5, &seen), "an ID above the count in the last row, which no block starts in, is not looked at (inteng's starts are 318 x 238)");

	put(10, 20, 0xFF000003u);
	Check(host::AutoLink_Seen(picture, 5, &seen) && !seen.empty() && seen[0] == 2, "the high byte (alpha) is not part of the ID");

	std::vector<uint32_t> small(10, 1);
	Check(!host::AutoLink_Seen(small, 5, &seen), "a picture that is not 320 x 240: false");

	if (checksFailed)
	{
		printf("FAILED host_autolink_views_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_autolink_views_test\n");
	return 0;
}
