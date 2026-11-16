// See polygon_items.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "polygon_items.h"

#include <string.h>

namespace host
{
	char const *PolygonItemWords(PolygonItemResult result)
	{
		switch (result)
		{
			case PolygonItemResult::Ok:
				return "ok";
			case PolygonItemResult::BadCall:
				return "the call was wrong";
			case PolygonItemResult::MergeSizeWrong:
				return "the shape's merge data does not have one number for each polygon";
			case PolygonItemResult::BadPartner:
				return "the shape's merge data names a partner polygon that is not there, is the polygon itself, or does not name it back";
			case PolygonItemResult::ItemsRanOut:
				return "the game's shape has fewer polygons than the file's merge data accounts for";
			case PolygonItemResult::ItemsLeftOver:
				return "the game's shape has polygons the file's merge data does not account for";
			case PolygonItemResult::ItemCornersWrong:
				return "a polygon of the game's shape has a number of corners the game does not use";
		}

		return "unknown";
	}

	PolygonItemResult PolygonItems_Map(int32_t const *mergeData, size_t mergeCount, size_t filePolygons, uint8_t const *itemCorners, size_t items,
		std::vector<int32_t> *fileToItem, std::vector<int32_t> *itemFirstFile)
	{
		if (!fileToItem || !itemFirstFile || filePolygons > kMostPolygonItems || items > kMostPolygonItems || (items && !itemCorners) || (mergeCount && !mergeData))
		{
			return PolygonItemResult::BadCall;
		}

		fileToItem->clear();
		itemFirstFile->clear();

		for (size_t j = 0; j < items; j++)
		{
			if (itemCorners[j] < 3 || itemCorners[j] > 5)
			{
				return PolygonItemResult::ItemCornersWrong;
			}
		}

		std::vector<int32_t> toItem(filePolygons, -1);
		std::vector<int32_t> firstFile(items, -1);

		if (!mergeData)
		{
			// no SHPMRGDT: the loader makes no change, the items are the file's polygons one for one
			if (filePolygons != items)
			{
				return filePolygons < items ? PolygonItemResult::ItemsLeftOver : PolygonItemResult::ItemsRanOut;
			}

			for (size_t i = 0; i < filePolygons; i++)
			{
				toItem[i] = (int32_t)i;
				firstFile[i] = (int32_t)i;
			}

			*fileToItem = toItem;
			*itemFirstFile = firstFile;
			return PolygonItemResult::Ok;
		}

		if (mergeCount != filePolygons)
		{
			return PolygonItemResult::MergeSizeWrong;
		}

		// the merge data: -1, or a partner that is another polygon and names this one back
		for (size_t i = 0; i < filePolygons; i++)
		{
			int32_t const partner = mergeData[i];

			if (partner == -1)
			{
				continue;
			}

			if (partner < 0 || (size_t)partner >= filePolygons || (size_t)partner == i || mergeData[partner] != (int32_t)i)
			{
				return PolygonItemResult::BadPartner;
			}
		}

		// The walk (chunk_load.cpp:2386-2412). j is the item the walk is at; a pair whose item has no fourth corner stayed apart (two items), one that has was joined (one item).
		size_t j = 0;

		for (size_t i = 0; i < filePolygons; i++)
		{
			int32_t const partner = mergeData[i];

			if (partner == -1)
			{
				if (j >= items)
				{
					return PolygonItemResult::ItemsRanOut;
				}

				toItem[i] = (int32_t)j;
				firstFile[j] = (int32_t)i;
				j++;
			}
			else if ((size_t)partner > i)
			{
				if (j >= items)
				{
					return PolygonItemResult::ItemsRanOut;
				}

				if (itemCorners[j] == 3)
				{
					// the pair stayed apart: two triangles, the first polygon's, then its partner's
					if (j + 1 >= items)
					{
						return PolygonItemResult::ItemsRanOut;
					}

					toItem[i] = (int32_t)j;
					firstFile[j] = (int32_t)i;
					j++;
					toItem[partner] = (int32_t)j;
					firstFile[j] = partner;
					j++;
				}
				else
				{
					// joined: one item for both
					toItem[i] = (int32_t)j;
					toItem[partner] = (int32_t)j;
					firstFile[j] = (int32_t)i;
					j++;
				}
			}
			// partner < i: this polygon was given its item when its partner came up
		}

		if (j != items)
		{
			return j < items ? PolygonItemResult::ItemsLeftOver : PolygonItemResult::ItemsRanOut;
		}

		for (size_t i = 0; i < filePolygons; i++)
		{
			if (toItem[i] < 0)
			{
				return PolygonItemResult::BadPartner;   // a polygon the walk never gave an item: not possible with merge data that names each other back, so refuse
			}
		}

		*fileToItem = toItem;
		*itemFirstFile = firstFile;
		return PolygonItemResult::Ok;
	}
}
