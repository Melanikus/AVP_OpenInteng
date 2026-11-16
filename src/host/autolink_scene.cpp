// See autolink_scene.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "autolink_scene.h"

#include <string.h>

#include <utility>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "level_store.h"
#include "shape_edit.h"

namespace host
{
	void AutoLinkScene_Build(OpenLevelFile *open, double scale, AutoLinkScene *scene)
	{
		scene->moduleNumbers.clear();
		scene->shapes.clear();
		scene->list.clear();
		scene->indexOf.clear();
		scene->names.clear();
		scene->fileLocations.clear();

		ModuleIndexes(open->Level(), &scene->moduleNumbers);

		List<Object_Chunk *> all;

		open->Level()->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (!object || !scene->moduleNumbers.count((int32_t)object->object_data.index_num) || scene->shapes.count((int32_t)object->object_data.index_num))
			{
				continue;
			}

			model::ShapeModel shape;
			std::string readWhy;

			if (ShapeEdit_Read(open->Level(), open, (uint8_t const *)&object->object_data.ID, &shape, nullptr, &readWhy) != ShapeEditResult::Done)
			{
				continue;
			}

			std::unique_ptr<PortalShape> one(new PortalShape());
			AutoLinkModule entry;

			for (model::ShapePoint const &p : shape.points)
			{
				one->points.push_back(AutoLinkPlace(p.x, scale));
				one->points.push_back(AutoLinkPlace(p.y, scale));
				one->points.push_back(AutoLinkPlace(p.z, scale));
			}

			for (size_t k = 0; k < shape.polygons.size(); k++)
			{
				std::vector<int32_t> corners;

				for (int c = 0; c < shape.polygons[k].cornerCount; c++)
				{
					corners.push_back(shape.polygons[k].corners[c]);
				}

				one->polygons.push_back(corners);

				model::ShapeNormal const normal = k < shape.polygonNormals.size() ? shape.polygonNormals[k] : model::ShapeNormal();

				one->normals.push_back(AutoLinkNormal(normal.x));
				one->normals.push_back(AutoLinkNormal(normal.y));
				one->normals.push_back(AutoLinkNormal(normal.z));
			}

			entry.number = (int32_t)object->object_data.index_num;
			entry.module.place[0] = AutoLinkPlace(object->object_data.location.x, scale);
			entry.module.place[1] = AutoLinkPlace(object->object_data.location.y, scale);
			entry.module.place[2] = AutoLinkPlace(object->object_data.location.z, scale);
			entry.module.extents[0] = AutoLinkPlace(shape.extents.maxX, scale);
			entry.module.extents[1] = AutoLinkPlace(shape.extents.minX, scale);
			entry.module.extents[2] = AutoLinkPlace(shape.extents.maxY, scale);
			entry.module.extents[3] = AutoLinkPlace(shape.extents.minY, scale);
			entry.module.extents[4] = AutoLinkPlace(shape.extents.maxZ, scale);
			entry.module.extents[5] = AutoLinkPlace(shape.extents.minZ, scale);
			entry.module.shape = one.get();

			size_t length = 0;

			while (length < sizeof(object->object_data.o_name) && object->object_data.o_name[length] != 0)
			{
				length++;
			}

			scene->names[entry.number].assign(object->object_data.o_name, length);

			std::vector<int32_t> &fileAt = scene->fileLocations[entry.number];

			fileAt.push_back((int32_t)object->object_data.location.x);
			fileAt.push_back((int32_t)object->object_data.location.y);
			fileAt.push_back((int32_t)object->object_data.location.z);
			scene->shapes[entry.number] = std::move(one);
			scene->indexOf[entry.number] = scene->list.size();
			scene->list.push_back(entry);
		}
	}
}
