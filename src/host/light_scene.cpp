// See light_scene.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "light_scene.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <map>
#include <set>

#include "chunk.hpp"
#include "chunk_light.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "environment_chunk.hpp"
#include "mishchnk.hpp"
#include "autolink_portal.h"
#include "level_chunks.h"
#include "level_file.h"
#include "shape_edit.h"
#include "tables.h"

namespace host
{
	namespace
	{
		// The light set named NORMALLT inside the environment data, or null
		Chunk_With_Children *FindLightSet(File_Chunk *level, char const *setName)
		{
			Chunk *const env = level ? level->lookup_single_child("REBENVDT") : nullptr;
			Chunk_With_Children *const holder = env && !env->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(env) : nullptr;

			if (!holder)
			{
				return nullptr;
			}

			List<Chunk *> sets;
			holder->lookup_child("LIGHTSET", sets);

			for (LIF<Chunk *> each(&sets); !each.done(); each.next())
			{
				Chunk_With_Children *const set = dynamic_cast<Chunk_With_Children *>(each());
				List<Chunk *> headers;

				if (!set)
				{
					continue;
				}

				set->lookup_child("LTSETHDR", headers);

				for (LIF<Chunk *> h(&headers); !h.done(); h.next())
				{
					Light_Set_Header_Chunk *const header = dynamic_cast<Light_Set_Header_Chunk *>(h());

					if (header && strncmp(header->light_set_name, setName, 8) == 0)
					{
						return set;
					}
				}
			}

			return nullptr;
		}

		double ScaleOf(File_Chunk *level)
		{
			Chunk *const env = level ? level->lookup_single_child("REBENVDT") : nullptr;
			Chunk_With_Children *const holder = env && !env->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(env) : nullptr;

			if (!holder)
			{
				return 1.0;
			}

			List<Chunk *> found;
			holder->lookup_child("ENVSDSCL", found);

			for (LIF<Chunk *> each(&found); !each.done(); each.next())
			{
				Environment_Scale_Chunk *const scale = dynamic_cast<Environment_Scale_Chunk *>(each());

				if (scale && scale->scale > 0.0)
				{
					return scale->scale;
				}
			}

			return 1.0;
		}

		// A light as FUN_004ae5d0 makes one from a STDLIGHT (0x4aecc6-0x4aee8f)
		light::Light MakeLight(Light_Data const &d, double scale)
		{
			light::Light out;

			out.place[0] = AutoLinkPlace((int32_t)d.location.x, scale);
			out.place[1] = AutoLinkPlace((int32_t)d.location.y, scale);
			out.place[2] = AutoLinkPlace((int32_t)d.location.z, scale);
			out.direction[0] = (int32_t)d.orientation.mat31;
			out.direction[1] = (int32_t)d.orientation.mat32;
			out.direction[2] = (int32_t)d.orientation.mat33;
			out.brightness = (int32_t)d.brightness;
			out.spread = (int32_t)d.spread;
			out.cosineOfSpread = (d.spread >= 0 && d.spread < kTableSize) ? cosine[d.spread] : 0;
			out.range = AutoLinkPlace((int32_t)d.range, scale);
			out.colour = (uint32_t)d.colour;
			out.flags = (uint32_t)d.engine_light_flags;
			out.noPreLight = (d.local_light_flags & LOFlag_NoPreLight) != 0;

			if (out.noPreLight)
			{
				out.flags |= 0x300;
			}

			// FUN_004ae5d0 0x4aee64: a light without the shadow bit gets the engine's "no shadows" bit; one with it has shadows on
			out.shadows = (d.local_light_flags & LOFlag_ShadowData) != 0;

			if (!out.shadows)
			{
				out.flags |= 0x10;
			}

			out.on = true;
			return out;
		}
	}

	bool LightScene_Markers(OpenLevelFile *open, std::vector<LightMarker> *out)
	{
		out->clear();

		if (!open || !open->Level())
		{
			return false;
		}

		Chunk_With_Children *const set = FindLightSet(open->Level(), "NORMALLT");

		if (!set)
		{
			return false;
		}

		List<Chunk *> lights;
		set->lookup_child("STDLIGHT", lights);

		for (LIF<Chunk *> each(&lights); !each.done(); each.next())
		{
			Light_Chunk *const one = dynamic_cast<Light_Chunk *>(each());

			if (!one)
			{
				continue;
			}

			Light_Data const &d = one->light;
			LightMarker marker;

			marker.place[0] = (double)d.location.x;
			marker.place[1] = (double)d.location.y;
			marker.place[2] = (double)d.location.z;
			marker.matrix[0] = (int32_t)d.orientation.mat11; marker.matrix[1] = (int32_t)d.orientation.mat12; marker.matrix[2] = (int32_t)d.orientation.mat13;
			marker.matrix[3] = (int32_t)d.orientation.mat21; marker.matrix[4] = (int32_t)d.orientation.mat22; marker.matrix[5] = (int32_t)d.orientation.mat23;
			marker.matrix[6] = (int32_t)d.orientation.mat31; marker.matrix[7] = (int32_t)d.orientation.mat32; marker.matrix[8] = (int32_t)d.orientation.mat33;
			marker.hidden = (d.local_light_flags & 0x20) != 0;   // LOFlag_Invisible

			// what the editor's own bytes say now (Hide All, the I key and Fly light change them with SetPayload, which the parsed chunk does not see): the place (@4), the 3 x 3 (@16) and the local flags (@72) of the 84 bytes
			std::vector<unsigned char> bytes;

			if (open->PayloadOf(one, &bytes) && bytes.size() >= 76)
			{
				auto const at = [&](size_t offset) { int32_t value = 0; memcpy(&value, bytes.data() + offset, 4); return value; };

				marker.place[0] = at(4); marker.place[1] = at(8); marker.place[2] = at(12);

				for (size_t i = 0; i < 9; i++)
				{
					marker.matrix[i] = at(16 + 4 * i);
				}

				marker.hidden = (at(72) & 0x20) != 0;
			}
			out->push_back(marker);
		}

		return true;
	}

	bool LightScene_ReadLights(OpenLevelFile *open, LightSceneLights *out, std::string *why)
	{
		out->lights.clear();
		out->ambience = 0;
		out->scale = 1.0;

		if (!open || !open->Level())
		{
			if (why)
			{
				*why = "no level is open";
			}

			return false;
		}

		out->scale = ScaleOf(open->Level());

		Chunk_With_Children *const set = FindLightSet(open->Level(), "NORMALLT");

		if (!set)
		{
			if (why)
			{
				*why = "the level has no light set named NORMALLT";
			}

			return false;
		}

		List<Chunk *> ambience;
		set->lookup_child("AMBIENCE", ambience);

		for (LIF<Chunk *> each(&ambience); !each.done(); each.next())
		{
			Lighting_Ambience_Chunk *const one = dynamic_cast<Lighting_Ambience_Chunk *>(each());

			if (one)
			{
				out->ambience = (int32_t)one->ambience;
				break;
			}
		}

		List<Chunk *> lights;
		set->lookup_child("STDLIGHT", lights);

		for (LIF<Chunk *> each(&lights); !each.done(); each.next())
		{
			Light_Chunk *const one = dynamic_cast<Light_Chunk *>(each());

			if (one)
			{
				out->lights.push_back(MakeLight(one->light, out->scale));
			}
		}

		// the placed lights (PLOBJLIT in an object, FUN_004ae5d0 0x4ae9ce): the light is at the object's place; its colour is the up colour, its engine flags the chunk's with 0x300 added
		List<Object_Chunk *> objects;
		open->Level()->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();
			List<Chunk *> found;

			if (!object)
			{
				continue;
			}

			object->lookup_child("PLOBJLIT", found);

			for (LIF<Chunk *> f(&found); !f.done(); f.next())
			{
				Placed_Object_Light_Chunk *const placed = dynamic_cast<Placed_Object_Light_Chunk *>(f());

				if (!placed)
				{
					continue;
				}

				light::Light one;

				one.place[0] = AutoLinkPlace((int32_t)object->object_data.location.x, out->scale);
				one.place[1] = AutoLinkPlace((int32_t)object->object_data.location.y, out->scale);
				one.place[2] = AutoLinkPlace((int32_t)object->object_data.location.z, out->scale);
				one.brightness = (int32_t)placed->light.brightness;
				one.range = AutoLinkPlace((int32_t)placed->light.range, out->scale);
				one.colour = (uint32_t)placed->light.up_colour;
				one.flags = (uint32_t)placed->light.engine_light_flags | 0x300;
				one.noPreLight = (placed->light.local_light_flags & LOFlag_NoPreLight) != 0;
				one.shadows = (placed->light.local_light_flags & LOFlag_ShadowData) != 0;

				if (!one.shadows)
				{
					one.flags |= 0x10;   // FUN_004ae5d0 0x4aeb59: the same "no shadows" rule as a level's own light
				}

				one.on = true;
				out->placedLights.push_back(one);
			}
		}

		return true;
	}

	uint32_t LightScene_LightVertex(LightSceneLights const &lights, bool toolsSave, int32_t const vertex[3], int32_t const normal[3], light::ShadowScene const *shadowScene)
	{
		int32_t sum[3] = { 0, 0, 0 };

		// the level's lights, then the placed lights of its objects: all of them add to a vertex's colour when they are on (and, for the game's save, not No PreLight)
		for (size_t index = 0; index < lights.lights.size() + lights.placedLights.size(); index++)
		{
			light::Light const &one = index < lights.lights.size() ? lights.lights[index] : lights.placedLights[index - lights.lights.size()];

			if (!one.on || (one.noPreLight && !toolsSave))
			{
				continue;
			}

			uint16_t contribution = 0;

			if (shadowScene && one.shadows && one.point && (one.flags & light::kNoShadows) == 0)
			{
				// FUN_004fe730: the vertex moved by its normal shifted right 8 has to be nearer than the range and see the light
				int32_t const moved[3] = { (int32_t)((uint32_t)vertex[0] + (uint32_t)(normal[0] >> 8)), (int32_t)((uint32_t)vertex[1] + (uint32_t)(normal[1] >> 8)),
					(int32_t)((uint32_t)vertex[2] + (uint32_t)(normal[2] >> 8)) };

				if (light::Distance(moved, one.place) < one.range && shadowScene->Visible(moved, one.place))
				{
					contribution = light::Contribution(vertex, normal, one);
				}
			}
			else
			{
				contribution = light::Contribution(vertex, normal, one);
			}

			light::AddLight(sum, contribution, one.colour);
		}

		return light::SavedValue(sum, lights.ambience);
	}

	namespace
	{
		// The points and vertex normals of the first sub-shape of a shape (the other state of a morphing shape), put in place of the shape's own when the counts agree
		void SwapInTheMorphShape(OpenLevelFile *open, Shape_Chunk *own, model::ShapeModel *shape)
		{
			if (!own)
			{
				return;
			}

			Chunk *const morph = own->lookup_single_child("SHPMORPH");

			if (!morph)
			{
				return;
			}

			List<Chunk *> subs;
			Chunk_With_Children *const morphHolder = dynamic_cast<Chunk_With_Children *>(morph);

			if (!morphHolder)
			{
				return;
			}

			morphHolder->lookup_child("SUBSHAPE", subs);

			for (LIF<Chunk *> each(&subs); !each.done(); each.next())
			{
				Chunk_With_Children *const sub = dynamic_cast<Chunk_With_Children *>(each());
				Chunk *const points = sub ? sub->lookup_single_child("SHPRAWVT") : nullptr;
				Chunk *const normals = sub ? sub->lookup_single_child("SHPVNORM") : nullptr;
				std::vector<unsigned char> pointBytes, normalBytes;

				if (!points || !open->PayloadOf(points, &pointBytes) || pointBytes.size() != shape->points.size() * 12)
				{
					continue;
				}

				for (size_t v = 0; v < shape->points.size(); v++)
				{
					int32_t xyz[3];
					memcpy(xyz, pointBytes.data() + v * 12, sizeof(xyz));
					shape->points[v].x = xyz[0];
					shape->points[v].y = xyz[1];
					shape->points[v].z = xyz[2];
				}

				if (normals && open->PayloadOf(normals, &normalBytes) && normalBytes.size() == shape->points.size() * 12)
				{
					shape->pointNormals.resize(shape->points.size());

					for (size_t v = 0; v < shape->points.size(); v++)
					{
						float xyz[3];
						memcpy(xyz, normalBytes.data() + v * 12, sizeof(xyz));
						shape->pointNormals[v].x = xyz[0];
						shape->pointNormals[v].y = xyz[1];
						shape->pointNormals[v].z = xyz[2];
					}
				}

				return;
			}
		}
	}

	void LightScene_LightModules(OpenLevelFile *open, LightSceneLights const &lights, bool toolsSave, bool doorsClosed, std::vector<LitObject> *out, bool flatNormals)
	{
		out->clear();
		light::ShadowScene shadowScene;

		if (!open || !open->Level())
		{
			return;
		}

		std::set<int32_t> modules;
		ModuleIndexes(open->Level(), &modules);

		List<Object_Chunk *> all;
		open->Level()->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (!object || !modules.count((int32_t)object->object_data.index_num))
			{
				continue;
			}

			model::ShapeModel shape;
			std::string readWhy;

			if (ShapeEdit_Read(open->Level(), open, (uint8_t const *)&object->object_data.ID, &shape, nullptr, &readWhy) != ShapeEditResult::Done)
			{
				continue;
			}

			if (doorsClosed)
			{
				// Doors > Close (FUN_004a9f40): a shape with morph frames (SHPMORPH > FRMMORPH, whose frames name sub-shapes: -1 is the shape itself) has its points and vertex normals
				// exchanged with the other shape of its first frame, a SUBSHAPE inside the shape chunk
				SwapInTheMorphShape(open, object->get_assoc_shape(), &shape);
			}

			LitObject lit;
			lit.number = (int32_t)object->object_data.index_num;

			size_t length = 0;

			while (length < sizeof(object->object_data.o_name) && object->object_data.o_name[length] != 0)
			{
				length++;
			}

			lit.name.assign(object->object_data.o_name, length);
			lit.location[0] = (int32_t)object->object_data.location.x;
			lit.location[1] = (int32_t)object->object_data.location.y;
			lit.location[2] = (int32_t)object->object_data.location.z;

			int32_t const shapePlace[3] = { AutoLinkPlace((int32_t)object->object_data.location.x, lights.scale), AutoLinkPlace((int32_t)object->object_data.location.y, lights.scale),
				AutoLinkPlace((int32_t)object->object_data.location.z, lights.scale) };
			int64_t const radius = AutoLinkPlace((int32_t)shape.radius, lights.scale);

			// every light has a contribution array for a shape it reaches, on or off, and the placed lights too (FUN_004fe5e0)
			for (int pass = 0; pass < 2 && !lit.reached; pass++)
			{
				std::vector<light::Light> const &list = pass == 0 ? lights.lights : lights.placedLights;

				for (light::Light const &one : list)
				{
					if ((int64_t)light::Distance(shapePlace, one.place) < radius + (int64_t)one.range)
					{
						lit.reached = true;
						break;
					}
				}
			}

			int32_t const place[3] = { AutoLinkPlace((int32_t)object->object_data.location.x, lights.scale), AutoLinkPlace((int32_t)object->object_data.location.y, lights.scale),
				AutoLinkPlace((int32_t)object->object_data.location.z, lights.scale) };

			// FUN_004b15e0: the vertex normals made again from the polygons
			std::vector<int32_t> flat;

			if (flatNormals)
			{
				light::FlatVertexNormals(shape, &flat);
			}

			for (size_t v = 0; v < shape.points.size(); v++)
			{
				int32_t const vertex[3] = { (int32_t)((uint32_t)AutoLinkPlace(shape.points[v].x, lights.scale) + (uint32_t)place[0]),
					(int32_t)((uint32_t)AutoLinkPlace(shape.points[v].y, lights.scale) + (uint32_t)place[1]), (int32_t)((uint32_t)AutoLinkPlace(shape.points[v].z, lights.scale) + (uint32_t)place[2]) };
				model::ShapeNormal const n = v < shape.pointNormals.size() ? shape.pointNormals[v] : model::ShapeNormal();
				int32_t const normal[3] = { flat.empty() ? AutoLinkNormal(n.x) : flat[3 * v], flat.empty() ? AutoLinkNormal(n.y) : flat[3 * v + 1], flat.empty() ? AutoLinkNormal(n.z) : flat[3 * v + 2] };

				lit.vertices.insert(lit.vertices.end(), vertex, vertex + 3);
				lit.normals.insert(lit.normals.end(), normal, normal + 3);
			}

			// this module's shape for the shadow test: the points moved by the place, the polygons with their normals, the box moved by the place
			light::ShadowScene::Shape blocker;

			blocker.points = lit.vertices;

			for (size_t k = 0; k < shape.polygons.size(); k++)
			{
				std::vector<int32_t> corners;

				for (int c = 0; c < shape.polygons[k].cornerCount; c++)
				{
					corners.push_back(shape.polygons[k].corners[c]);
				}

				blocker.polygons.push_back(corners);

				int32_t const normalIndex = shape.polygons[k].normalIndex;
				model::ShapeNormal const pn = normalIndex >= 0 && (size_t)normalIndex < shape.polygonNormals.size() ? shape.polygonNormals[(size_t)normalIndex] : model::ShapeNormal();

				blocker.normals.push_back(AutoLinkNormal(pn.x));
				blocker.normals.push_back(AutoLinkNormal(pn.y));
				blocker.normals.push_back(AutoLinkNormal(pn.z));
			}

			// the box of the points actually used (the header's extents are the shape's own, and the morph shape's differ)
			for (int axis = 0; axis < 3; axis++)
			{
				int32_t low = INT32_MAX, high = INT32_MIN;

				for (size_t v = 0; v < lit.vertices.size() / 3; v++)
				{
					low = (std::min)(low, lit.vertices[3 * v + (size_t)axis]);
					high = (std::max)(high, lit.vertices[3 * v + (size_t)axis]);
				}

				blocker.box[2 * axis] = high;
				blocker.box[2 * axis + 1] = low;
			}

			shadowScene.Add(blocker);

			List<Chunk *> ints;
			object->lookup_child("SHPVTINT", ints);

			for (LIF<Chunk *> i(&ints); !i.done(); i.next())
			{
				Shape_Vertex_Intensities_Chunk *const iv = dynamic_cast<Shape_Vertex_Intensities_Chunk *>(i());

				if (iv && strncmp(iv->light_set_name, "NORMALLT", 8) == 0 && iv->num_vertices >= 0)
				{
					for (int v = 0; v < iv->num_vertices; v++)
					{
						lit.stored.push_back((uint32_t)iv->intensity_array[v]);
					}

					lit.hasStored = true;
					break;
				}
			}

			out->push_back(lit);
		}

		// the second pass: every vertex of every module, with the shadow scene of all of them
		for (LitObject &lit : *out)
		{
			for (size_t v = 0; v < lit.vertices.size() / 3; v++)
			{
				lit.computed.push_back(LightScene_LightVertex(lights, toolsSave, &lit.vertices[3 * v], &lit.normals[3 * v], &shadowScene));
			}
		}
	}
}
