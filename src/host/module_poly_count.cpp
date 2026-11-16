// See module_poly_count.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "module_poly_count.h"

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "chunk_object.hpp"   // Object_Chunk, Object_Module_Data_Chunk, VModule_Array_Chunk
#include "chunk_shape.hpp"
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile::PayloadOf
#include "polygon_items.h"    // PolygonItems_ReadMergeData
#include "shape_edit.h"       // ShapeEdit_Read

namespace host
{
	int32_t ModulePolyCount_OfShape(size_t polygons, int32_t const *polygonFlags, int32_t const *mergeData, size_t mergeCount)
	{
		if (!mergeData)
		{
			return polygons > 0x7FFFFFFF ? 0x7FFFFFFF : (int32_t)polygons;
		}

		int64_t sum = 0;

		for (size_t i = 0; i < polygons && i < mergeCount; i++)
		{
			if (mergeData[i] < (int32_t)i)
			{
				sum += (polygonFlags && ((uint32_t)polygonFlags[i] & 0x80000000u)) ? 2 : 1;
			}
		}

		return sum > 0x7FFFFFFF ? 0x7FFFFFFF : (int32_t)sum;
	}

	uint32_t ModulePolyCount_Colour(bool hasFigure, int32_t count)
	{
		if (!hasFigure)
		{
			return 0xFFFFFFFFu;
		}

		return count >= 2000 ? 0xFFFFFF00u : count >= 1500 ? 0xFFFF7F00u : count >= 1200 ? 0xFF0000FFu : 0xFF00FF00u;
	}

	namespace
	{
		// The count of the shape of the object with this ID, by the rule; false when it can't be read
		bool CountOfObject(OpenLevelFile const *store, File_Chunk *level, uint8_t const *id, int32_t *count)
		{
			model::ShapeModel shape;
			std::string why;
			std::vector<int32_t> merge;
			bool hasMerge = false;

			if (ShapeEdit_Read(level, store, id, &shape, nullptr, &why) != ShapeEditResult::Done || !PolygonItems_ReadMergeData(store, level, id, &merge, &hasMerge))
			{
				return false;
			}

			if (hasMerge && merge.size() != shape.polygons.size())
			{
				return false;
			}

			std::vector<int32_t> flags;

			for (model::ShapePolygon const &polygon : shape.polygons)
			{
				flags.push_back(polygon.flags);
			}

			*count = ModulePolyCount_OfShape(shape.polygons.size(), flags.data(), hasMerge ? merge.data() : nullptr, merge.size());
			return true;
		}
	}

	bool ModulePolyCount_Of(OpenLevelFile const *store, File_Chunk *level, uint8_t const *objectId, int32_t *count, bool *hasFigure)
	{
		if (!store || !level || !objectId || !count || !hasFigure)
		{
			return false;
		}

		*count = 0;
		*hasFigure = false;

		List<Object_Chunk *> found;
		Object_Chunk *self = nullptr;

		level->list_objects(&found);

		for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
		{
			if (each() && memcmp(&each()->object_data.ID, objectId, 8) == 0)
			{
				self = each();
				break;
			}
		}

		if (!self)
		{
			return false;
		}

		int32_t own = 0;

		if (!CountOfObject(store, level, objectId, &own))
		{
			return false;
		}

		Chunk *const moduleData = self->lookup_single_child("MODULEDT");
		Chunk *const array = moduleData ? static_cast<Chunk_With_Children *>(moduleData)->lookup_single_child("VMDARRAY") : nullptr;

		if (!array)
		{
			*count = own;
			return true;   // no figure
		}

		std::vector<unsigned char> bytes;

		if (!store->PayloadOf(array, &bytes) || bytes.size() < 4)
		{
			*count = own;
			return true;
		}

		int32_t items = 0;

		memcpy(&items, bytes.data(), 4);

		if (items < 0 || (size_t)items > (bytes.size() - 4) / 16)
		{
			return false;   // a count the bytes cannot hold: the shape is not trusted
		}

		int64_t total = own;
		std::vector<Object_Chunk *> used;

		used.push_back(self);

		for (int32_t k = 0; k < items; k++)
		{
			int32_t objectIndex = 0;

			memcpy(&objectIndex, bytes.data() + 4 + (size_t)k * 16, 4);

			for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
			{
				Object_Chunk *const other = each();

				if (!other || other->object_data.index_num != objectIndex || std::find(used.begin(), used.end(), other) != used.end())
				{
					continue;
				}

				int32_t more = 0;

				if (CountOfObject(store, level, reinterpret_cast<uint8_t const *>(&other->object_data.ID), &more))
				{
					total += more;
					used.push_back(other);
				}

				break;   // the FIRST object with that index, found or not (inteng takes the first)
			}
		}

		*count = total > 0x7FFFFFFF ? 0x7FFFFFFF : (int32_t)total;
		*hasFigure = true;
		return true;
	}
}
