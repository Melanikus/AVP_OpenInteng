// See poly_counts.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "poly_counts.h"

#include <string.h>

#include <exception>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk, Shape_Merge_Data_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects

namespace host
{
	namespace
	{
		Chunk_With_Children const *AsHolder(Chunk *chunk)
		{
			if (!chunk || chunk->r_u_miscellaneous())
			{
				return nullptr;
			}

			return dynamic_cast<Chunk_With_Children const *>(chunk);
		}

		// count(shape), as poly_counts.h says; bitDoubles false: every polygon once (FUN_004725b0 called with no record, the hub's count)
		int32_t CountOfShape(Shape_Chunk *shape, bool bitDoubles = true)
		{
			if (!shape)
			{
				return 0;
			}

			ChunkShape const &data = shape->shape_data;
			int const polygons = data.num_polys;

			if (polygons <= 0)
			{
				return 0;
			}

			Chunk *const found = shape->lookup_single_child("SHPMRGDT");

			if (!found || found->r_u_miscellaneous())
			{
				return polygons;
			}

			Shape_Merge_Data_Chunk const *const merge = (Shape_Merge_Data_Chunk *)found;

			if (!merge->merge_data || merge->num_polys <= 0)
			{
				return polygons;
			}

			int64_t total = 0;

			for (int i = 0; i < polygons && i < merge->num_polys; i++)
			{
				if (merge->merge_data[i] < i)
				{
					bool const twice = bitDoubles && data.poly_list && ((uint32_t)data.poly_list[i].flags & 0x80000000u) != 0;

					total += twice ? 2 : 1;
				}
			}

			return total > 0x7fffffff ? 0x7fffffff : (int32_t)total;
		}
	}

	size_t PolyCounts_Read(File_Chunk *level, std::vector<PolyCountRow> *rows)
	{
		if (!rows)
		{
			return 0;
		}

		rows->clear();

		if (!level)
		{
			return 0;
		}

		try
		{
			List<Object_Chunk *> listed;

			level->list_objects(&listed);

			std::vector<Object_Chunk *> objects;

			for (LIF<Object_Chunk *> each(&listed); !each.done(); each.next())
			{
				if (each())
				{
					objects.push_back(each());
				}
			}

			for (size_t at = 0; at < objects.size() && rows->size() < kPolyCountRowsMost; at++)
			{
				Object_Chunk *const object = objects[at];
				Shape_Chunk *const shape = object->get_assoc_shape();

				if (!shape)
				{
					continue;
				}

				PolyCountRow row;

				memcpy(row.id, &object->object_data.ID, sizeof(row.id));

				size_t length = 0;

				while (length < sizeof(object->object_data.o_name) && object->object_data.o_name[length] != 0)
				{
					length++;
				}

				row.name.assign(object->object_data.o_name, length);
				row.number = (int32_t)object->object_data.index_num;
				row.place[0] = object->object_data.location.x;
				row.place[1] = object->object_data.location.y;
				row.place[2] = object->object_data.location.z;
				row.count = CountOfShape(shape);
				row.seen = 1;
				row.linked = false;
				Object_Header_Chunk *const header = object->get_header();

				row.placed = header && (header->flags & 0x400) != 0;

				// The links: the first MODULEDT's first VMDARRAY (a plain-bytes chunk gives none)
				Chunk_With_Children const *const module = AsHolder(object->lookup_single_child("MODULEDT"));
				Chunk *const array = module ? module->lookup_single_child("VMDARRAY") : nullptr;

				if (array && !array->r_u_miscellaneous())
				{
					VModule_Array_Chunk const *const links = (VModule_Array_Chunk *)array;

					row.linked = links->num_array_items > 0 && links->vmod_array;

					if (row.linked)
					{
						// objects not yet used: all but this one
						std::vector<bool> used(objects.size(), false);

						used[at] = true;

						for (int i = 0; i < links->num_array_items; i++)
						{
							int32_t const wanted = (int32_t)links->vmod_array[i].object_index;

							for (size_t j = 0; j < objects.size(); j++)
							{
								if (!used[j] && (int32_t)objects[j]->object_data.index_num == wanted)
								{
									// found: used up, and counted when its shape is a loaded one (every object with a shape is)
									Shape_Chunk *const other = objects[j]->get_assoc_shape();

									if (other)
									{
										used[j] = true;
										row.seen++;

										int64_t const sum = (int64_t)row.count + CountOfShape(other);

										row.count = sum > 0x7fffffff ? 0x7fffffff : (int32_t)sum;
									}

									break;
								}
							}
						}
					}
				}

				rows->push_back(row);
			}
		}
		catch (std::exception const &)
		{
			rows->clear();
		}

		return rows->size();
	}

	uint32_t PolyCountColour(PolyCountRow const &row)
	{
		if (!row.linked)
		{
			return 0xFFFFFF;
		}

		if (row.count < 1200)
		{
			return 0x00FF00;
		}

		if (row.count < 1500)
		{
			return 0x0000FF;
		}

		if (row.count < 2000)
		{
			return 0xFF7F00;
		}

		return 0xFFFF00;
	}

	std::string PolyCountText(PolyCountRow const &row)
	{
		return std::to_string((long long)row.count) + "\t" + std::to_string((long long)row.seen) + " m\t" + row.name;
	}

	bool PolyCounts_Level(File_Chunk *level, int64_t *shapes, int64_t *placed)
	{
		if (!shapes || !placed)
		{
			return false;
		}

		*shapes = 0;
		*placed = 0;

		if (!level)
		{
			return false;
		}

		try
		{
			List<Shape_Chunk *> listed;

			level->list_shapes(&listed);

			for (LIF<Shape_Chunk *> each(&listed); !each.done(); each.next())
			{
				Shape_Chunk *const shape = each();

				if (!shape)
				{
					continue;
				}

				int64_t const users = shape->list_assoc_objs().size();

				if (users == 0)
				{
					continue;   // 0x472807: a shape no object uses adds nothing
				}

				int64_t const count = CountOfShape(shape, false);

				*shapes += count;
				*placed += users * count;
			}
		}
		catch (std::exception const &)
		{
			*shapes = 0;
			*placed = 0;
			return false;
		}

		return true;
	}
}
