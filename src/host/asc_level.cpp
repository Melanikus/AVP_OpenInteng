// See asc_level.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "asc_level.h"

#include <string.h>

#include <exception>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk, Object_Header_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk, Shape_Sub_Shape_Chunk, Shape_Header_Chunk, Shape_Merge_Data_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects

namespace host
{
	namespace
	{
		// A shape's numbers as the writer needs them, from the game's own copy of a shape (a shape or a sub-shape). mergeParent is the chunk that
		// may hold the SHPMRGDT.
		bool FromShapeData(ChunkShape const &data, Chunk_With_Children *mergeParent, AscShape *out)
		{
			out->points.clear();
			out->polygons.clear();

			if (data.num_verts < 0 || data.num_polys < 0 || (data.num_verts > 0 && !data.v_list) || (data.num_polys > 0 && !data.poly_list))
			{
				return false;
			}

			for (int i = 0; i < data.num_verts; i++)
			{
				AscPoint point;

				point.x = data.v_list[i].x;
				point.y = data.v_list[i].y;
				point.z = data.v_list[i].z;
				out->points.push_back(point);
			}

			Shape_Merge_Data_Chunk *const merge = mergeParent ? (Shape_Merge_Data_Chunk *)mergeParent->lookup_single_child("SHPMRGDT") : nullptr;

			for (int i = 0; i < data.num_polys; i++)
			{
				AscPolygon polygon;

				for (int c = 0; c < 3; c++)
				{
					polygon.corner[c] = data.poly_list[i].vert_ind[c];
				}

				polygon.mergedWith = merge && !merge->r_u_miscellaneous() && merge->merge_data && i < merge->num_polys ? merge->merge_data[i] : -1;
				out->polygons.push_back(polygon);
			}

			return true;
		}

		std::string NameOf(char const *name, size_t room)
		{
			void const *const zero = memchr(name, 0, room);

			return std::string(name, zero ? (size_t)((char const *)zero - name) : room);
		}
	}

	size_t AscWalkLevel(File_Chunk *level, bool doorsClosed, double scale, std::vector<AscLevelObject> *out, AscOffset *offset)
	{
		if (!out || !offset)
		{
			return 0;
		}

		out->clear();

		AscOffset const none = { 0, 0, 0 };

		*offset = none;

		if (!level)
		{
			return 0;
		}

		size_t objectCount = 0;

		try
		{
			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			// Step 1: the offset over every object's shape (its own points, not a sub-shape's)
			std::vector<AscShape> shapes;
			std::vector<AscShape const *> pointers;

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();
				Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

				objectCount++;

				if (shape)
				{
					AscShape one;

					if (FromShapeData(shape->shape_data, shape, &one))
					{
						shapes.push_back(one);
					}
				}
			}

			for (AscShape const &one : shapes)
			{
				pointers.push_back(&one);
			}

			*offset = AscOffsetOf(pointers);

			// Steps 2: each object whose shape is not flagged out
			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();
				Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

				if (!shape)
				{
					continue;
				}

				Shape_Header_Chunk *const header = shape->get_header();

				if (header && ((uint32_t)header->flags & kAscSkipShapeFlag))
				{
					continue;
				}

				AscShape numbers;
				bool made = false;

				if (doorsClosed)
				{
					Chunk_With_Children *const morph = (Chunk_With_Children *)shape->lookup_single_child("SHPMORPH");

					if (morph && !morph->r_u_miscellaneous())
					{
						Shape_Sub_Shape_Chunk *const first = (Shape_Sub_Shape_Chunk *)morph->lookup_single_child("SUBSHAPE");

						if (first && !first->r_u_miscellaneous())
						{
							made = FromShapeData(first->shape_data, first, &numbers);
						}
					}
				}

				if (!made && !FromShapeData(shape->shape_data, shape, &numbers))
				{
					continue;
				}

				int32_t const place[3] = { (int32_t)object->object_data.location.x, (int32_t)object->object_data.location.y,
					(int32_t)object->object_data.location.z };
				AscLevelObject written;

				written.objectName = NameOf(object->object_data.o_name, sizeof(object->object_data.o_name));
				written.numbers = MakeAscObject(numbers, place, *offset, scale);
				out->push_back(written);
			}
		}
		catch (std::exception const &)
		{
			// Out of memory: what was made so far is given
		}

		return objectCount;
	}
}
