// See module_overlaps.h. Written 2026-11-15 by Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "module_overlaps.h"

#include <string.h>

#include "chunk_object.hpp"   // Object_Chunk, Object_Header_Chunk
#include "chunk_shape.hpp"
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile::PayloadOf

#include "shape_edit.h"       // ShapeEdit_Read
#include "shape_model.h"

namespace host
{
	namespace
	{
		int32_t const kPlacedObjectFlag = 0x400;   // OBJECT_FLAG_PLACED_OBJECT (chunk_object.hpp): a module is an object without it

		// FUN_00444c80: each part of a box edge is divided by 100 as a whole number (toward zero, as C does) before they are added
		int32_t Edge(int32_t extent, int32_t place)
		{
			return extent / 100 + place / 100;
		}

		int32_t Smaller(int32_t a, int32_t b)
		{
			return a < b ? a : b;
		}

		int32_t Larger(int32_t a, int32_t b)
		{
			return a > b ? a : b;
		}
	}

	OverlapSeverity ModuleOverlaps_Between(ModuleBox const &a, ModuleBox const &b)
	{
		int32_t smallest = 0;

		for (int axis = 0; axis < 3; axis++)
		{
			int32_t const maxA = Edge(a.extents[axis * 2], a.place[axis]);
			int32_t const minA = Edge(a.extents[axis * 2 + 1], a.place[axis]);
			int32_t const maxB = Edge(b.extents[axis * 2], b.place[axis]);
			int32_t const minB = Edge(b.extents[axis * 2 + 1], b.place[axis]);
			int32_t const overlap = Smaller(maxA, maxB) - Larger(minA, minB);

			smallest = axis == 0 ? overlap : Smaller(smallest, overlap);
		}

		if (smallest <= 0)
		{
			return OverlapSeverity::None;
		}

		if (smallest > 1000)
		{
			return OverlapSeverity::Large;
		}

		if (smallest > 100)
		{
			return OverlapSeverity::Medium;
		}

		return smallest < 3 ? OverlapSeverity::Touching : OverlapSeverity::Small;
	}

	std::vector<OverlapSeverity> ModuleOverlaps_Worst(std::vector<ModuleBox> const &boxes)
	{
		std::vector<OverlapSeverity> worst(boxes.size(), OverlapSeverity::None);

		for (size_t i = 0; i < boxes.size(); i++)
		{
			for (size_t j = i + 1; j < boxes.size(); j++)
			{
				OverlapSeverity const between = ModuleOverlaps_Between(boxes[i], boxes[j]);

				if (between > worst[i])
				{
					worst[i] = between;
				}

				if (between > worst[j])
				{
					worst[j] = between;
				}
			}
		}

		return worst;
	}

	uint32_t ModuleOverlaps_Rgb(OverlapSeverity severity)
	{
		switch (severity)
		{
		case OverlapSeverity::Touching: return 0xFFFF00u;
		case OverlapSeverity::Small: return 0xFF7E00u;
		case OverlapSeverity::Medium: return 0x7F007Fu;
		case OverlapSeverity::Large: return 0xFF0000u;
		default: return 0;
		}
	}

	bool ModuleOverlaps_ReadBoxes(OpenLevelFile *file, std::vector<ModuleBox> *boxes)
	{
		boxes->clear();

		if (!file || !file->Level())
		{
			return false;
		}

		List<Object_Chunk *> objects;
		bool whole = true;

		file->Level()->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();
			Object_Header_Chunk *const header = object ? object->get_header() : nullptr;
			std::vector<unsigned char> bytes;

			if (!object || !object->get_assoc_shape() || !header || !file->PayloadOf(header, &bytes) || bytes.size() < 56)
			{
				continue;
			}

			int32_t flags;
			ModuleBox box;

			memcpy(&flags, &bytes[0], 4);

			if (flags & kPlacedObjectFlag)
			{
				continue;
			}

			model::ShapeModel shape;
			std::string why;

			if (ShapeEdit_Read(file->Level(), file, (uint8_t const *)&object->object_data.ID, &shape, nullptr, &why) != ShapeEditResult::Done)
			{
				whole = false;
				continue;
			}

			box.objectIndex = (int32_t)object->object_data.index_num;
			box.extents[0] = shape.extents.maxX;
			box.extents[1] = shape.extents.minX;
			box.extents[2] = shape.extents.maxY;
			box.extents[3] = shape.extents.minY;
			box.extents[4] = shape.extents.maxZ;
			box.extents[5] = shape.extents.minZ;
			memcpy(box.place, &bytes[20], sizeof(box.place));
			boxes->push_back(box);
		}

		return whole;
	}
}
