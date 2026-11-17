// See autolink_records.h. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "autolink_records.h"

#include <map>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"

namespace host
{
	void AutoLink_StandInBox(int32_t const doorExtents[6], PortalShape *box, int32_t boxExtents[6])
	{
		box->points.clear();
		box->polygons.clear();
		box->normals.clear();

		// point i (FUN_00447f10 0x448017-0x448041): x by bit 0, y by bit 1, z by bit 2; a set bit takes the axis's maximum (extents 0 / 2 / 4)
		for (int i = 0; i < 8; i++)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				box->points.push_back(((i >> axis) & 1) != 0 ? doorExtents[2 * axis] : doorExtents[2 * axis + 1]);
			}
		}

		// the six faces (0x4480d3-0x448278: corner offsets / 3) and their normals (0x4480c2-0x4480d0: -x, +x, -y, +y, -z, +z), each pointing out of the box
		static int32_t const faces[6][4] = { { 0, 4, 6, 2 }, { 1, 3, 7, 5 }, { 4, 0, 1, 5 }, { 2, 6, 7, 3 }, { 0, 2, 3, 1 }, { 4, 5, 7, 6 } };
		static int32_t const normals[6][3] = { { -65536, 0, 0 }, { 65536, 0, 0 }, { 0, -65536, 0 }, { 0, 65536, 0 }, { 0, 0, -65536 }, { 0, 0, 65536 } };

		for (int f = 0; f < 6; f++)
		{
			box->polygons.push_back(std::vector<int32_t>(faces[f], faces[f] + 4));

			for (int n = 0; n < 3; n++)
			{
				box->normals.push_back(normals[f][n]);
			}
		}

		// its own extents: the door's widened by 1 on each side (0x44831b-0x448333: max + 1, min - 1)
		for (int axis = 0; axis < 3; axis++)
		{
			boxExtents[2 * axis] = doorExtents[2 * axis] + 1;
			boxExtents[2 * axis + 1] = doorExtents[2 * axis + 1] - 1;
		}
	}

	void AutoLink_Records(OpenLevelFile *open, double levelScale, AutoLinkRecords *out)
	{
		out->boxes.clear();
		out->records.clear();
		out->objectIndexOf.clear();
		out->standsFor.clear();
		out->isDoor.clear();

		if (!open || !open->Level())
		{
			out->scene = AutoLinkScene();
			return;
		}

		AutoLinkScene_Build(open, levelScale, &out->scene);

		// which modules are doors: the object's shape has a SHPMORPH child (FUN_004c6380 0x4c6556-0x4c65a3: FUN_004316e0 "SHPMORPH" on the definition's shape)
		std::map<int32_t, bool> morphing;
		List<Object_Chunk *> all;

		open->Level()->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (!object)
			{
				continue;
			}

			int32_t const number = (int32_t)object->object_data.index_num;

			if (!out->scene.indexOf.count(number) || morphing.count(number))
			{
				continue;
			}

			Shape_Chunk *const shape = object->get_assoc_shape();

			morphing[number] = shape && shape->lookup_single_child("SHPMORPH") != nullptr;
		}

		// records 0 .. N-1: the modules in the file's order
		for (AutoLinkModule const &one : out->scene.list)
		{
			out->records.push_back(one.module);
			out->objectIndexOf.push_back(one.number);
			out->standsFor.push_back(-1);
			out->isDoor.push_back(morphing[one.number] ? 1 : 0);
		}

		// the stand-ins, from the last module down to the first (0x4c6963-0x4c6a20), each appended
		size_t const modules = out->records.size();

		for (size_t k = modules; k-- > 0;)
		{
			if (!out->isDoor[k])
			{
				continue;
			}

			std::unique_ptr<PortalShape> box(new PortalShape());
			PortalModule standIn = out->records[k];

			AutoLink_StandInBox(out->records[k].extents, box.get(), standIn.extents);
			standIn.shape = box.get();
			out->boxes.push_back(std::move(box));
			out->records.push_back(standIn);
			out->objectIndexOf.push_back(out->objectIndexOf[k]);
			out->standsFor.push_back((int32_t)k);
			out->isDoor.push_back(0);
		}
	}
}
