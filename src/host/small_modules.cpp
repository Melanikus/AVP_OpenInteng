// See small_modules.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "small_modules.h"

#include <math.h>
#include <string.h>

#include <exception>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk (shape_data: max, min)
#include "mishchnk.hpp"       // File_Chunk::list_objects

#include "level_file.h"

namespace host
{
	namespace
	{
		uint32_t const PlacedObjectFlag = 0x400;   // OBJECT_FLAG_PLACED_OBJECT
		double const MostLimit = 1073741824.0;     // 2^30
	}

	int32_t SmallModules_LimitFromMetres(double metres)
	{
		double const millimetres = metres * 1000.0;

		if (!(millimetres > 0.0))
		{
			return 0;   // a negative number, zero or not a number
		}

		return millimetres >= MostLimit ? (int32_t)MostLimit : (int32_t)millimetres;   // __ftol: cut toward zero
	}

	double SmallModules_MetresOfLimit(int32_t limit)
	{
		return (double)limit / 1000.0;
	}

	SmallModulesResult SmallModules_Find(OpenLevelFile *file, int32_t limit, std::vector<SmallModule> *found)
	{
		found->clear();

		File_Chunk *const level = file ? file->Level() : 0;

		if (!level)
		{
			return SmallModulesResult::NothingOpen;
		}

		try
		{
			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();
				Chunk *const header = object ? object->get_header() : 0;
				Shape_Chunk *const shape = object ? object->get_assoc_shape() : 0;
				std::vector<unsigned char> payload;

				if (!header || !shape || !file->PayloadOf(header, &payload) || payload.size() < 4)
				{
					continue;
				}

				uint32_t flags = 0;

				memcpy(&flags, payload.data(), 4);

				if (flags & PlacedObjectFlag)
				{
					continue;   // inteng tests the objects whose +0x2c word has 0x400 CLEAR
				}

				// the extent on each axis: the shape's max less its min (FUN_0042f8a0 on the two points), as whole numbers without wrapping
				int64_t const size[3] = { (int64_t)shape->shape_data.max.x - shape->shape_data.min.x,
				                         (int64_t)shape->shape_data.max.y - shape->shape_data.min.y,
				                         (int64_t)shape->shape_data.max.z - shape->shape_data.min.z };
				int64_t const smaller = size[2] <= size[0] ? size[2] : size[0];   // :0x44bfdc: the first, or the third when that is not above it

				if (smaller > limit)
				{
					continue;
				}

				SmallModule made;

				memcpy(made.id, &object->object_data.ID, 8);

				size_t length = 0;

				while (length < sizeof(object->object_data.o_name) && object->object_data.o_name[length] != 0)
				{
					length++;
				}

				made.name.assign(object->object_data.o_name, length);
				made.place[0] = object->object_data.location.x;
				made.place[1] = object->object_data.location.y;
				made.place[2] = object->object_data.location.z;

				for (int axis = 0; axis < 3; axis++)
				{
					made.size[axis] = (int32_t)(size[axis] > 0x7fffffff ? 0x7fffffff : (size[axis] < -0x7fffffff ? -0x7fffffff : size[axis]));
				}

				found->push_back(made);
			}
		}
		catch (std::exception const &)
		{
			found->clear();
		}

		return SmallModulesResult::Done;
	}
}
