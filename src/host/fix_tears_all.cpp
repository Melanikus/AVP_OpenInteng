// See fix_tears_all.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "fix_tears_all.h"

#include <string.h>

#include <vector>

#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "file_pick.h"
#include "level_file.h"
#include "shape_edit.h"
#include "shape_model.h"

namespace host
{
	namespace
	{
		struct Boxed
		{
			uint8_t id[8];
			int64_t low[3];
			int64_t high[3];
		};
	}

	bool FixTearsAll(OpenLevelFile *store, File_Chunk *level, int32_t tolerance, FixTearsAllCounts *counts)
	{
		FixTearsAllCounts none;
		FixTearsAllCounts &c = counts ? *counts : none;

		c = FixTearsAllCounts();

		if (!store || !level || tolerance < 0)
		{
			return false;
		}

		std::vector<Boxed> boxes;
		List<Object_Chunk *> found;

		level->list_objects(&found);

		for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
		{
			Boxed one;

			memcpy(one.id, &each()->object_data.ID, 8);

			// placed objects (header flag 0x400) are left out, as inteng leaves them out
			Object_Header_Chunk *const header = each()->get_header();
			std::vector<unsigned char> bytes;

			if (!header || !store->PayloadOf(header, &bytes) || bytes.size() < 4 || (*(uint32_t const *)bytes.data() & 0x400u) != 0)
			{
				continue;
			}

			model::ShapeModel shape;
			std::vector<int32_t> points, normals;

			if (FilePick_World(store, level, one.id, &shape, &points, &normals) != FilePickResult::Done || points.size() < 3)
			{
				continue;
			}

			for (int axis = 0; axis < 3; axis++)
			{
				one.low[axis] = one.high[axis] = points[(size_t)axis];
			}

			for (size_t i = 0; i + 2 < points.size(); i += 3)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					one.low[axis] = points[i + (size_t)axis] < one.low[axis] ? points[i + (size_t)axis] : one.low[axis];
					one.high[axis] = points[i + (size_t)axis] > one.high[axis] ? points[i + (size_t)axis] : one.high[axis];
				}
			}

			boxes.push_back(one);
		}

		c.objects = boxes.size();

		for (size_t m = 0; m < boxes.size(); m++)
		{
			for (size_t r = 0; r < boxes.size(); r++)
			{
				if (m == r)
				{
					continue;
				}

				bool overlap = true;

				for (int axis = 0; axis < 3; axis++)
				{
					// a weld moves a point of the moved shape onto a point of the other within the tolerance, so the boxes have to come within it (grown by 2 x the tolerance: earlier welds may have moved points that far)
					overlap = overlap && boxes[m].low[axis] - 2 * (int64_t)tolerance <= boxes[r].high[axis] && boxes[r].low[axis] - 2 * (int64_t)tolerance <= boxes[m].high[axis];
				}

				if (!overlap)
				{
					continue;
				}

				size_t welded = 0;
				std::string why;
				ShapeEditResult const result = ShapeEdit_FixTears(store, level, boxes[m].id, boxes[r].id, tolerance, &welded, &why);

				if (result == ShapeEditResult::Done)
				{
					c.pairs++;
					c.welded += welded;
				}
				else
				{
					c.refused++;

					if (c.firstWhy.empty())
					{
						c.firstWhy = why.empty() ? ShapeEditWords(result) : why;
					}
				}
			}
		}

		return true;
	}
}
