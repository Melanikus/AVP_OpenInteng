// The test for the file-polygon to game-item map (polygon_items.h). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt.
// No window, none of the game. The cases are worked by hand from the loader's own walk (avpx chunk_load.cpp:2372-2412); the stock levels are checked by the editor's own
// -checkpolygonmap run, which lays the map against the game's loaded shapes.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "polygon_items.h"

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

static bool Is(std::vector<int32_t> const &got, std::vector<int32_t> const &want)
{
	return got == want;
}

int main()
{
	puts("The file polygon to game item map:");

	std::vector<int32_t> toItem, firstFile;

	// no merge data: the items are the file's polygons one for one
	{
		uint8_t const corners[3] = { 3, 4, 5 };

		Check(PolygonItems_Map(nullptr, 0, 3, corners, 3, &toItem, &firstFile) == PolygonItemResult::Ok && Is(toItem, { 0, 1, 2 }) && Is(firstFile, { 0, 1, 2 }),
			"no SHPMRGDT: polygon n is item n");
		Check(PolygonItems_Map(nullptr, 0, 3, corners, 2, &toItem, &firstFile) == PolygonItemResult::ItemsRanOut && toItem.empty() && firstFile.empty(),
			"no SHPMRGDT and fewer items than polygons: refused, nothing handed back");
		Check(PolygonItems_Map(nullptr, 0, 2, corners, 3, &toItem, &firstFile) == PolygonItemResult::ItemsLeftOver, "no SHPMRGDT and more items than polygons: refused");
		Check(PolygonItems_Map(nullptr, 0, 0, nullptr, 0, &toItem, &firstFile) == PolygonItemResult::Ok && toItem.empty() && firstFile.empty(), "an empty shape maps to nothing");
	}

	// merge data: a joined pair (0, 1) -> one quad; singles 2 and 3; a pair (4, 5) that stayed apart -> two triangles
	{
		int32_t const merge[6] = { 1, 0, -1, -1, 5, 4 };
		uint8_t const corners[5] = { 4, 3, 3, 3, 3 };

		Check(PolygonItems_Map(merge, 6, 6, corners, 5, &toItem, &firstFile) == PolygonItemResult::Ok && Is(toItem, { 0, 0, 1, 2, 3, 4 }) && Is(firstFile, { 0, 2, 3, 4, 5 }),
			"a joined pair is one item for both polygons, a pair left apart is two items, a single is one");
	}

	// a joined pair whose partner has the LOWER number's partner listed after: (0, 3) joined, 1 and 2 single
	{
		int32_t const merge[4] = { 3, -1, -1, 0 };
		uint8_t const corners[3] = { 4, 3, 3 };

		Check(PolygonItems_Map(merge, 4, 4, corners, 3, &toItem, &firstFile) == PolygonItemResult::Ok && Is(toItem, { 0, 1, 2, 0 }) && Is(firstFile, { 0, 1, 2 }),
			"a pair whose partner comes later: the partner gets the pair's item when the first comes up");
	}

	// every refusal
	{
		uint8_t const corners[3] = { 3, 3, 3 };
		int32_t const outOfRange[3] = { 7, -1, -1 };
		int32_t const oneWay[3] = { 1, -1, -1 };
		int32_t const itself[3] = { 0, -1, -1 };
		int32_t const negative[3] = { -2, -1, -1 };
		int32_t const none[3] = { -1, -1, -1 };

		Check(PolygonItems_Map(outOfRange, 3, 3, corners, 3, &toItem, &firstFile) == PolygonItemResult::BadPartner, "a partner past the polygons: refused");
		Check(PolygonItems_Map(oneWay, 3, 3, corners, 3, &toItem, &firstFile) == PolygonItemResult::BadPartner, "a partner that does not name it back: refused");
		Check(PolygonItems_Map(itself, 3, 3, corners, 3, &toItem, &firstFile) == PolygonItemResult::BadPartner, "a polygon that is its own partner: refused");
		Check(PolygonItems_Map(negative, 3, 3, corners, 3, &toItem, &firstFile) == PolygonItemResult::BadPartner, "a negative partner other than -1: refused");
		Check(PolygonItems_Map(none, 2, 3, corners, 3, &toItem, &firstFile) == PolygonItemResult::MergeSizeWrong, "merge data with the wrong count: refused");
		Check(PolygonItems_Map(none, 3, 3, corners, 2, &toItem, &firstFile) == PolygonItemResult::ItemsRanOut, "all singles but too few items: the walk runs out");
		Check(PolygonItems_Map(none, 3, 3, corners, 3, &toItem, &firstFile) == PolygonItemResult::Ok && Is(toItem, { 0, 1, 2 }), "all singles: one item each");

		int32_t const pair[3] = { 1, 0, -1 };
		uint8_t const apart[3] = { 3, 3, 3 };

		Check(PolygonItems_Map(pair, 3, 3, apart, 2, &toItem, &firstFile) == PolygonItemResult::ItemsRanOut, "a pair left apart needs two items, and the shape has only one more: refused");
		uint8_t const four[4] = { 3, 3, 3, 3 };

		Check(PolygonItems_Map(pair, 3, 3, four, 4, &toItem, &firstFile) == PolygonItemResult::ItemsLeftOver, "items the walk never reaches: refused");

		uint8_t const twoCorners[3] = { 2, 3, 3 };
		uint8_t const sixCorners[3] = { 3, 6, 3 };

		Check(PolygonItems_Map(none, 3, 3, twoCorners, 3, &toItem, &firstFile) == PolygonItemResult::ItemCornersWrong, "an item with 2 corners: refused");
		Check(PolygonItems_Map(none, 3, 3, sixCorners, 3, &toItem, &firstFile) == PolygonItemResult::ItemCornersWrong, "an item with 6 corners: refused");

		Check(PolygonItems_Map(none, 3, 3, nullptr, 3, &toItem, &firstFile) == PolygonItemResult::BadCall, "items without their corner counts: refused");
		Check(PolygonItems_Map(nullptr, 3, 3, corners, 3, &toItem, &firstFile) == PolygonItemResult::BadCall, "a merge count without merge data: refused");
		Check(PolygonItems_Map(none, 3, 3, corners, 3, nullptr, &firstFile) == PolygonItemResult::BadCall && PolygonItems_Map(none, 3, 3, corners, 3, &toItem, nullptr) == PolygonItemResult::BadCall,
			"no place for the answer: refused");
		Check(PolygonItems_Map(none, 1u << 21, 1u << 21, corners, 3, &toItem, &firstFile) == PolygonItemResult::BadCall, "a count past the bound: refused before anything is sized");
	}

	// seeded cases made the other way round: build the polygons and the items together, then see the walk recover the same map
	{
		uint32_t state = 12345;
		auto next = [&]() { state = state * 1664525u + 1013904223u; return state >> 8; };
		size_t good = 0, cases = 20000;

		for (size_t c = 0; c < cases; c++)
		{
			size_t const n = 1 + next() % 40;
			std::vector<int32_t> merge(n, -1);
			std::vector<size_t> loose;

			for (size_t i = 0; i < n; i++)
			{
				loose.push_back(i);
			}

			// pair some polygons at random
			for (size_t k = 0; k + 1 < loose.size(); k++)
			{
				if (next() % 3 == 0)
				{
					size_t const other = k + 1 + next() % (loose.size() - k - 1);

					merge[loose[k]] = (int32_t)loose[other];
					merge[loose[other]] = (int32_t)loose[k];
					loose.erase(loose.begin() + (std::ptrdiff_t)other);
				}
			}

			std::vector<int32_t> wantToItem(n, -1), wantFirst;
			std::vector<uint8_t> corners;

			for (size_t i = 0; i < n; i++)
			{
				if (merge[i] == -1)
				{
					wantToItem[i] = (int32_t)corners.size();
					wantFirst.push_back((int32_t)i);
					corners.push_back((uint8_t)(3 + next() % 3));
				}
				else if ((size_t)merge[i] > i)
				{
					if (next() % 2)
					{
						wantToItem[i] = wantToItem[(size_t)merge[i]] = (int32_t)corners.size();
						wantFirst.push_back((int32_t)i);
						corners.push_back(4);
					}
					else
					{
						wantToItem[i] = (int32_t)corners.size();
						wantFirst.push_back((int32_t)i);
						corners.push_back(3);
						wantToItem[(size_t)merge[i]] = (int32_t)corners.size();
						wantFirst.push_back(merge[i]);
						corners.push_back(3);
					}
				}
			}

			// a single polygon of 3 corners could be confused with a pair's apart item only by the walk reading the wrong item: singles never are asked
			if (PolygonItems_Map(merge.data(), n, n, corners.data(), corners.size(), &toItem, &firstFile) == PolygonItemResult::Ok && toItem == wantToItem && firstFile == wantFirst)
			{
				good++;
			}
		}

		Check(good == cases, std::to_string(cases) + " seeded shapes (random pairs, joined or left apart, singles of 3 to 5 corners): the walk recovers every polygon's item");
	}

	printf(checksFailed ? "\n%d checks FAILED\n" : "\nAll checks passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
