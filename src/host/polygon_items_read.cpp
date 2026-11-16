// See polygon_items.h: the part that reads the level file (the pure walk is in polygon_items.cpp). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "polygon_items.h"

#include <math.h>
#include <string.h>

#include "chunk_object.hpp"   // Object_Chunk, Shape_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk's children (Chunk_With_Children::lookup_single_child)
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile::PayloadOf

namespace host
{
	bool PolygonItems_ReadMergeData(OpenLevelFile const *store, File_Chunk *level, uint8_t const *id, std::vector<int32_t> *numbers, bool *hasMergeData)
	{
		if (!store || !level || !id || !numbers || !hasMergeData)
		{
			return false;
		}

		numbers->clear();
		*hasMergeData = false;

		List<Object_Chunk *> found;

		level->list_objects(&found);

		for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (!object || memcmp(&object->object_data.ID, id, 8) != 0)
			{
				continue;
			}

			Shape_Chunk *const shape = object->get_assoc_shape();

			if (!shape)
			{
				return false;
			}

			Chunk *const chunk = shape->lookup_single_child("SHPMRGDT");

			if (!chunk)
			{
				return true;   // no such chunk: the items are the file's polygons
			}

			std::vector<unsigned char> bytes;

			if (!store->PayloadOf(chunk, &bytes) || bytes.size() % 4 != 0 || bytes.size() / 4 > kMostPolygonItems)
			{
				return false;
			}

			numbers->resize(bytes.size() / 4);

			if (!bytes.empty())
			{
				memcpy(numbers->data(), bytes.data(), bytes.size());
			}

			*hasMergeData = true;
			return true;
		}

		return false;
	}

	bool PolygonItems_ShapeHasOwnPictureList(File_Chunk *level, uint8_t const *id)
	{
		if (!level || !id)
		{
			return false;
		}

		List<Object_Chunk *> found;

		level->list_objects(&found);

		for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (!object || memcmp(&object->object_data.ID, id, 8) != 0)
			{
				continue;
			}

			Shape_Chunk *const shape = object->get_assoc_shape();
			Chunk *const external = shape ? shape->lookup_single_child("SHPEXTFL") : nullptr;

			return external && static_cast<Chunk_With_Children *>(external)->lookup_single_child("BMPLSTST") != nullptr;
		}

		return false;
	}

	bool PolygonItems_ModuleObject(File_Chunk *level, char const *name, int32_t const world[3], double scale, uint8_t id[8])
	{
		if (!level || !name || !name[0] || !world || !id || !(scale > 0.0) || scale > 1.0e9)
		{
			return false;
		}

		List<Object_Chunk *> found;
		Object_Chunk *byName = nullptr;
		Object_Chunk *byPlace = nullptr;
		int sameName = 0;

		level->list_objects(&found);

		for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (!object || !object->object_data.o_name || !object->get_header() || (object->get_header()->flags & OBJECT_FLAG_PLACED_OBJECT) ||
				_stricmp(object->object_data.o_name, name) != 0)
			{
				continue;
			}

			sameName++;
			byName = byName ? byName : object;

			double const at[3] = { (double)object->object_data.location.x * scale, (double)object->object_data.location.y * scale, (double)object->object_data.location.z * scale };

			if (!byPlace && fabs(at[0] - world[0]) <= 2.0 && fabs(at[1] - world[1]) <= 2.0 && fabs(at[2] - world[2]) <= 2.0)
			{
				byPlace = object;
			}
		}

		Object_Chunk *const chosen = byPlace ? byPlace : (sameName == 1 ? byName : nullptr);

		if (!chosen)
		{
			return false;
		}

		memcpy(id, &chosen->object_data.ID, 8);
		return true;
	}
}
