// The test for the placement picker's ORDER BY KIND (src/host/shape_picker.h ShapePicker_KindGroup, ShapePicker_KindOrder). Written 2026-11-24 by Melanikus
// (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The owner, via Bishop: "1 weapons & pickups, lights,
// screens, furniture, props, switches, tracks, fans and rest, fragment, debirs and unkonw last"; the data and Bishop's ruling (fragments & debris one group):
// AvP-docs tools/frost/picker-sort/PLAN.md. No file is touched: the identity table is the built-in one.
//   (1) EVERY ROW OF THE TABLE in its group, counted (measured 2026-11-24 on the 343 rows: 29 pickups, 11 lights, 5 screens, 23 furniture, 36 props, 1 switch,
//       6 tracks, 17 the rest (the fan and the 16 avp_huds shapes no object places), 215 fragments & debris (no object places them; Steam's levels alone hold
//       them), none unknown); and the rows PLAN.md names, each in its place (fanf and flrdest4 by their kind, props; shell, held by avp_huds too, the rest)
//   (2) UNKNOWN (no row) is last, whatever the words; words the table doesn't hold, and near misses, are "the rest", never a pickup
//   (3) THE ORDER: by group, then by name in small letters, then as they came; a group outside 1..10 counts as unknown, last; no tile lost or twice
// RED FIRST: these functions don't exist before this card (the test doesn't build on bishop-int c2f3bc10).
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "shape_identity.h"
#include "shape_picker.h"

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

// the group of the table's row of that file name, or -1 when the table has no such row
static int GroupOfFile(char const *fileName)
{
	for (size_t i = 0; i < kShapeIdentityRowCount; i++)
	{
		if (strcmp(kShapeIdentityRows[i].fileName, fileName) == 0)
		{
			return ShapePicker_KindGroup(true, kShapeIdentityRows[i].kind, kShapeIdentityRows[i].levels);
		}
	}

	return -1;
}

int main()
{
	// ---- (1) every row of the table
	int counts[kShapePickerKindGroups + 1] = {};
	size_t outside = 0;

	for (size_t i = 0; i < kShapeIdentityRowCount; i++)
	{
		int const g = ShapePicker_KindGroup(true, kShapeIdentityRows[i].kind, kShapeIdentityRows[i].levels);

		if (g < 1 || g > kShapePickerKindGroups)
		{
			outside++;
			continue;
		}

		counts[g]++;
	}

	int const want[kShapePickerKindGroups + 1] = { 0, 29, 11, 5, 23, 36, 1, 6, 17, 215, 0 };
	std::string got;
	bool same = outside == 0;

	for (int g = 1; g <= kShapePickerKindGroups; g++)
	{
		got += (g > 1 ? " " : "") + std::to_string(g) + ":" + std::to_string(counts[g]);
		same = same && counts[g] == want[g];
	}

	Check(kShapeIdentityRowCount == 343, "(1) the table holds 343 rows (" + std::to_string((unsigned long long)kShapeIdentityRowCount) + ")");
	Check(same, "(1) every row in its group, counted: " + got + (outside ? " and " + std::to_string((unsigned long long)outside) + " outside 1..10" : std::string()) +
		" (want 1:29 2:11 3:5 4:23 5:36 6:1 7:6 8:17 9:215 10:0)");

	struct { char const *file; int group; char const *why; } const named[] = {
		{ "medbox01.rif", 1, "health" }, { "pistol.rif", 1, "weapon: marine pistol" }, { "smrtam01.rif", 1, "ammo: smartgun" },
		{ "simple.rif", 2, "placed light" }, { "fmvscreen.rif", 3, "video screen" }, { "timdesk.rif", 4, "furniture" },
		{ "fanf.rif", 5, "static (a fragment type names it too: by its kind)" }, { "flrdest4.rif", 5, "static (a fragment type names it too)" },
		{ "edbutton.rif", 6, "multi switch" }, { "shell.rif", 8, "no kind, in avp_rifs AND avp_huds: the rest" }, { "missile.rif", 8, "no kind, avp_huds" },
		{ "bencfrg1.rif", 9, "no kind, a fragment type names it" }, { "object01_antfrags.rif", 9, "no kind, nothing uses it" },
	};

	for (auto const &one : named)
	{
		int const g = GroupOfFile(one.file);

		Check(g == one.group, std::string("(1) ") + one.file + " (" + one.why + ") is group " + std::to_string(one.group) + " (it is " + std::to_string(g) + ")");
	}

	// ---- (2) unknown last; hostile and near-miss words
	Check(ShapePicker_KindGroup(false, "health", "avp_rifs/Derelict.RIF") == 10, "(2) no row: unknown (10), even with a pickup's words");
	Check(ShapePicker_KindGroup(false, "", "") == 10, "(2) no row and no words: unknown (10)");

	char const *const rest[] = { "fan", "Health", "health ", " health", "weapon:pulse rifle", "Weapon: pulse rifle", "ammo", "placed light ", "Static",
		"track destruct2", "key", "special pickup!" };

	for (char const *words : rest)
	{
		int const g = ShapePicker_KindGroup(true, words, "avp_rifs/Derelict.RIF");

		Check(g == 8, std::string("(2) \"") + words + "\" is the rest (8), never a named group (it is " + std::to_string(g) + ")");
	}

	std::string const many(100000, 'w');

	Check(ShapePicker_KindGroup(true, many, many) == 8, "(2) 100,000 letters of words: the rest (8)");
	Check(ShapePicker_KindGroup(true, std::string("health\0x", 8), "") == 8, "(2) \"health\" with a zero after it: the rest (8), compared whole");
	Check(ShapePicker_KindGroup(true, "", "AVP_HUDS/Predator.rif") == 8, "(2) no kind in avp_huds, any capitals: the rest (8)");
	Check(ShapePicker_KindGroup(true, "", "") == 9, "(2) no kind and no stock file: fragments & debris (9)");

	// ---- (3) the order
	std::vector<std::pair<int, std::string>> const tiles = {
		{ 10, "zebra" }, { 1, "Smrtam01" }, { 9, "bencfrg1" }, { 1, "pistol" }, { 4, "timdesk" }, { 1, "pistol" }, { 0, "bad0" }, { -5, "badneg" },
		{ 11, "bad11" }, { INT_MAX, "badmax" }, { 2, "Simple" }, { 10, "apple" },
	};
	std::vector<size_t> const order = ShapePicker_KindOrder(tiles);
	std::string shown;

	for (size_t k : order)
	{
		shown += (shown.empty() ? "" : " ") + (k < tiles.size() ? tiles[k].second : std::string("?"));
	}

	// pickers first by small letters (pistol twice, as they came: place 3 before 5), then the light, the desk, the fragment, then every unknown and every group
	// outside 1..10 together, by name
	Check(shown == "pistol pistol Smrtam01 Simple timdesk bencfrg1 apple bad0 bad11 badmax badneg zebra", "(3) by group, then name in small letters: " + shown);
	Check(order.size() == tiles.size() && order[0] == 3 && order[1] == 5, "(3) the same name twice keeps the order it came in (3 then 5)");

	std::vector<int> seen(tiles.size(), 0);
	bool each = order.size() == tiles.size();

	for (size_t k : order)
	{
		each = each && k < tiles.size() && ++seen[k] == 1;
	}

	Check(each, "(3) every tile once, none lost or twice");
	Check(ShapePicker_KindOrder({}).empty(), "(3) no tiles: an empty order");

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
