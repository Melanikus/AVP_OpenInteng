// See shape_recalc_edit.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "shape_recalc_edit.h"

#include <string.h>

#include <set>
#include <vector>

#include "chunk_object.hpp"   // Object_Chunk, Shape_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile
#include "shape_edit.h"       // ShapeEdit_Read, ShapeEdit_WriteSameCounts
#include "shape_maths.h"      // RecalculateNormalsAndExtents

namespace host
{
	namespace
	{
		bool SameNormals(std::vector<model::ShapeNormal> const &a, std::vector<model::ShapeNormal> const &b)
		{
			return a.size() == b.size() && (a.empty() || memcmp(a.data(), b.data(), a.size() * sizeof(model::ShapeNormal)) == 0);
		}

		bool SameExtents(model::ShapeExtents const &a, model::ShapeExtents const &b)
		{
			return memcmp(&a, &b, sizeof(a)) == 0;
		}
	}

	bool ShapeRecalc_All(OpenLevelFile *store, File_Chunk *level, ShapeRecalcReport *report)
	{
		if (report)
		{
			memset(report, 0, sizeof(*report));
		}

		if (!store || !level || !report)
		{
			return false;
		}

		// the IDs first: nothing is held across the writes
		struct Entry
		{
			uint8_t id[8];
			void const *shape;
		};
		std::vector<Entry> entries;
		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (object && object->get_assoc_shape())
			{
				Entry entry;

				memcpy(entry.id, &object->object_data.ID, 8);
				entry.shape = object->get_assoc_shape();
				entries.push_back(entry);
			}
		}

		report->objectsWithShape = entries.size();

		std::set<void const *> seen;

		for (size_t i = 0; i < entries.size(); i++)
		{
			if (!seen.insert(entries[i].shape).second)
			{
				continue;   // another object of the same shape did it
			}

			model::ShapeModel shape;
			std::string why;

			if (ShapeEdit_Read(level, store, entries[i].id, &shape, nullptr, &why) != ShapeEditResult::Done)
			{
				report->shapesSkipped++;
				continue;
			}

			model::ShapeModel const before = shape;

			if (model::RecalculateNormalsAndExtents(&shape) != model::ShapeMathsResult::Ok ||
				ShapeEdit_WriteSameCounts(store, level, entries[i].id, shape, &why) != ShapeEditResult::Done)
			{
				report->shapesSkipped++;
				continue;
			}

			report->shapesDone++;

			if (!SameNormals(before.polygonNormals, shape.polygonNormals) || !SameNormals(before.pointNormals, shape.pointNormals) ||
				!SameExtents(before.extents, shape.extents))
			{
				report->shapesChanged++;
			}
		}

		return true;
	}

	std::string ShapeRecalc_Words(ShapeRecalcReport const &report)
	{
		std::string words = "Normals and extents recalculated: " + std::to_string(report.shapesDone) + " shapes (" + std::to_string(report.shapesChanged) + " changed)";

		if (report.shapesSkipped)
		{
			words += ", " + std::to_string(report.shapesSkipped) + " skipped";
		}

		return words;
	}
}
