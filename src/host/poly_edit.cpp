// See poly_edit.h. Written 2026-11-15 by Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "poly_edit.h"

#include <math.h>
#include <algorithm>
#include <set>
#include <string.h>

#include "chunk_object.hpp"   // Object_Chunk, Shape_Chunk
#include "chunk_shape.hpp"    // Chunk_With_Children::lookup_single_child
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: PayloadOf, BuildLevel, ReplaceLevel

#include "shape_edit.h"       // ShapeEdit_Read
#include "shape_model.h"
#include "uv_model.h"
#include "shape_maths.h"
#include "anim_edit.h"
#include "object_make.h"     // CreateObjectWithShapeChunk
#include "object_type_edit.h" // ObjectType_Change: As Module

namespace host
{
	namespace
	{
		typedef std::vector<char> Image;
		typedef std::vector<unsigned char> Payload;

		size_t const kHeadingBytes = 12;      // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const kShapeNumberAt = 20;     // in SHPHEAD1's payload (shape_model.h)

		// inteng's thresholds are whole numbers of 65536 (FUN_00564fc0)
		double const kOne = 65536.0;
		double const kNearPerpendicular = 500.0 / kOne;
		double const kMostOffPlane = 400.0 / kOne;
		double const kFarthestSquared = 1000000000.0;   // inteng's start for "nearest" (local_28)

		bool Say(std::string *why, char const *words)
		{
			if (why)
			{
				*why = words;
			}

			return false;
		}

		struct Vec
		{
			double x, y, z;
		};

		Vec VSub(Vec a, Vec b)
		{
			Vec r = { a.x - b.x, a.y - b.y, a.z - b.z };
			return r;
		}

		double VDot(Vec a, Vec b)
		{
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		Vec VCross(Vec a, Vec b)
		{
			Vec r = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
			return r;
		}

		// False for a vector with no length (or one that is not a number): inteng would divide by 0 there
		bool VUnit(Vec *v)
		{
			double const length = sqrt(VDot(*v, *v));

			if (!(length > 0.0) || !isfinite(length))
			{
				return false;
			}

			v->x /= length;
			v->y /= length;
			v->z /= length;
			return true;
		}

		Vec PointOf(model::ShapePoint const &p)
		{
			Vec r = { (double)p.x, (double)p.y, (double)p.z };
			return r;
		}

		// A triangle's corners and its new unit normal: inteng's FUN_00565460 (the two edges made unit, their cross made unit), turned round when the camera looks along it
		bool MakeTriangle(std::vector<model::ShapePoint> const &points, int32_t a, int32_t b, int32_t c, Vec const *view, model::ShapePolygon *polygon, model::ShapeNormal *normal, int32_t number)
		{
			Vec ab = VSub(PointOf(points[(size_t)b]), PointOf(points[(size_t)a]));
			Vec ac = VSub(PointOf(points[(size_t)c]), PointOf(points[(size_t)a]));

			if (!VUnit(&ab) || !VUnit(&ac))
			{
				return false;
			}

			Vec n = VCross(ab, ac);

			if (!VUnit(&n))
			{
				return false;   // the three corners are in a line
			}

			bool const turn = view && VDot(*view, n) > 0.0;

			memset(polygon, 0, sizeof(*polygon));
			polygon->engineType = 2;          // I_Polygon, a plain colour
			polygon->normalIndex = number;
			polygon->flags = 0;
			polygon->colour = 0x7fff;
			polygon->cornerCount = 3;
			polygon->corners[0] = a;
			polygon->corners[1] = turn ? c : b;
			polygon->corners[2] = turn ? b : c;
			polygon->corners[3] = -1;
			polygon->corners[4] = -1;
			normal->x = (float)(turn ? -n.x : n.x);
			normal->y = (float)(turn ? -n.y : n.y);
			normal->z = (float)(turn ? -n.z : n.z);
			return true;
		}

		int32_t IntAt(Image const &bytes, size_t at)
		{
			int32_t value;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		void PutIntAt(Image *bytes, size_t at, int32_t value)
		{
			memcpy(bytes->data() + at, &value, sizeof(value));
		}

		struct Span
		{
			size_t at;
			size_t size;
		};

		bool ChunkSizeAt(Image const &bytes, size_t at, size_t end, size_t *size)
		{
			if (end > bytes.size() || at > end || end - at < kHeadingBytes)
			{
				return false;
			}

			int32_t const stored = IntAt(bytes, at + 8);

			if (stored < (int32_t)kHeadingBytes || (size_t)stored > end - at)
			{
				return false;
			}

			*size = (size_t)stored;
			return true;
		}

		bool NameIs(Image const &bytes, size_t at, char const *name)
		{
			return memcmp(bytes.data() + at, name, 8) == 0;
		}

		bool Children(Image const &bytes, size_t from, size_t to, std::vector<Span> *out)
		{
			out->clear();

			for (size_t at = from; at < to; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, to, &size))
				{
					return false;
				}

				Span const span = { at, size };

				out->push_back(span);
				at += size;
			}

			return true;
		}

		void AppendChunk(Image *out, char const *name, Payload const &payload)
		{
			size_t const start = out->size();

			out->resize(start + kHeadingBytes + payload.size());
			memcpy(out->data() + start, name, 8);
			PutIntAt(out, start + 8, (int32_t)(kHeadingBytes + payload.size()));

			if (!payload.empty())
			{
				memcpy(out->data() + start + kHeadingBytes, payload.data(), payload.size());
			}
		}

		void AppendBytes(Image *out, Image const &from, size_t at, size_t size)
		{
			out->insert(out->end(), from.begin() + (std::ptrdiff_t)at, from.begin() + (std::ptrdiff_t)(at + size));
		}

		Object_Chunk *ObjectWithId(File_Chunk *level, uint8_t const *id)
		{
			List<Object_Chunk *> found;

			level->list_objects(&found);

			for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && memcmp(&object->object_data.ID, id, 8) == 0)
				{
					return object;
				}
			}

			return nullptr;
		}
	}

	// ---------------------------------------------------------------- the pick

	bool PolyEdit_NearestCorner(PolyCornerSpot const *spots, size_t count, double cx, double cy, int32_t *point)
	{
		if (!point || (count > 0 && !spots))
		{
			return false;
		}

		double best = kFarthestSquared;
		bool found = false;

		for (size_t i = 0; i < count; i++)
		{
			if (!spots[i].onScreen)
			{
				continue;
			}

			double const dx = spots[i].x - cx;
			double const dy = spots[i].y - cy;
			double const distance = dx * dx + dy * dy;

			if (distance < best)
			{
				best = distance;
				*point = spots[i].point;
				found = true;
			}
		}

		return found;
	}

	char const *PolyVertexClickWords(PolyVertexClick result)
	{
		switch (result)
		{
		case PolyVertexClick::Added: return "corner taken";
		case PolyVertexClick::AddedAndOrdered: return "fourth corner taken: the four make a flat quad";
		case PolyVertexClick::Removed: return "corner taken out";
		case PolyVertexClick::OtherShape: return "that corner is of another shape than the ones held";
		case PolyVertexClick::AlreadyHeld: return "that corner is held already";
		case PolyVertexClick::NotHeld: return "that corner is not held";
		case PolyVertexClick::NotAQuad: return "the four corners would not make a flat convex quad, so the fourth was not taken";
		case PolyVertexClick::ListFull: return "four corners are held already (Shift takes one out)";
		case PolyVertexClick::BadCall: return "nothing to click";
		}

		return "unknown answer";
	}

	bool PolyEdit_QuadOrder(std::vector<model::ShapePoint> const &points, int32_t const four[4], int32_t order[4])
	{
		if (!four || !order)
		{
			return false;
		}

		Vec p[4];

		for (int i = 0; i < 4; i++)
		{
			if (four[i] < 0 || (size_t)four[i] >= points.size())
			{
				return false;
			}

			p[i] = PointOf(points[(size_t)four[i]]);
		}

		// The edges of the order as given, each made unit (FUN_00564fc0's first loop)
		Vec e[4];

		for (int i = 0; i < 4; i++)
		{
			e[i] = VSub(p[i], p[(i + 1) & 3]);

			if (!VUnit(&e[i]))
			{
				return false;
			}
		}

		// The plane's normal: from the first two edges when they are about at right angles, else from the first and third; the last edge has to lie in that plane
		Vec n = fabs(VDot(e[0], e[1])) < kNearPerpendicular ? VCross(e[0], e[1]) : VCross(e[0], e[2]);

		if (!VUnit(&n) || fabs(VDot(n, e[3])) > kMostOffPlane)
		{
			return false;
		}

		// The first order (p, q, r, s) that goes round the same way at every corner
		for (int a = 0; a < 4; a++)
		{
			for (int b = 0; b < 4; b++)
			{
				if (b == a)
				{
					continue;
				}

				for (int c = 0; c < 4; c++)
				{
					if (c == a || c == b)
					{
						continue;
					}

					int const d = 6 - a - b - c;
					int const at[4] = { a, b, c, d };
					Vec v[4];

					for (int i = 0; i < 4; i++)
					{
						v[i] = VSub(p[at[i]], p[at[(i + 1) & 3]]);
					}

					bool all = true;

					for (int i = 0; i < 4 && all; i++)
					{
						Vec turn = VCross(v[i], v[(i + 1) & 3]);

						// a dot that rounds to 0 in 16.16 is not above 0
						all = VUnit(&turn) && VDot(turn, n) * kOne >= 1.0;
					}

					if (all)
					{
						for (int i = 0; i < 4; i++)
						{
							order[i] = four[at[i]];
						}

						return true;
					}
				}
			}
		}

		return false;
	}

	void PolyEdit_VertexClear(PolyVertexList *list)
	{
		if (list)
		{
			memset(list, 0, sizeof(*list));
		}
	}

	PolyVertexClick PolyEdit_VertexClick(PolyVertexList *list, uint8_t const *id, int32_t point, bool shift, std::vector<model::ShapePoint> const &points)
	{
		if (!list || !id || list->count > kPolyVertexMost || point < 0 || (size_t)point >= points.size())
		{
			return PolyVertexClick::BadCall;
		}

		if (list->count > 0 && memcmp(list->id, id, 8) != 0)
		{
			return PolyVertexClick::OtherShape;
		}

		size_t held = list->count;

		for (size_t i = 0; i < list->count; i++)
		{
			if (list->point[i] == point)
			{
				held = i;
				break;
			}
		}

		if (shift)
		{
			if (held == list->count)
			{
				return PolyVertexClick::NotHeld;
			}

			for (size_t i = held; i + 1 < list->count; i++)
			{
				list->point[i] = list->point[i + 1];
			}

			list->count--;
			return PolyVertexClick::Removed;
		}

		if (list->count == kPolyVertexMost)
		{
			return PolyVertexClick::ListFull;
		}

		if (held < list->count)
		{
			return PolyVertexClick::AlreadyHeld;
		}

		if (list->count == 0)
		{
			memcpy(list->id, id, 8);
		}

		list->point[list->count++] = point;

		if (list->count == kPolyVertexMost)
		{
			int32_t order[4];

			if (!PolyEdit_QuadOrder(points, list->point, order))
			{
				list->count--;
				return PolyVertexClick::NotAQuad;
			}

			for (size_t i = 0; i < 4; i++)
			{
				list->point[i] = order[i];
			}

			return PolyVertexClick::AddedAndOrdered;
		}

		return PolyVertexClick::Added;
	}

	// ---------------------------------------------------------------- create

	char const *PolyCreateWords(PolyCreateResult result)
	{
		switch (result)
		{
		case PolyCreateResult::Done: return "done";
		case PolyCreateResult::BadCall: return "three or four different corners of the shape are needed";
		case PolyCreateResult::Degenerate: return "those corners are in a line or at one place, so they make no polygon";
		case PolyCreateResult::TooMany: return "the shape would have more polygons than the editor reads";
		case PolyCreateResult::NoObject: return "no object has that name";
		case PolyCreateResult::NoShape: return "the object has no shape the editor can read";
		case PolyCreateResult::Morphing: return "a shape with a SHPMORPH is not edited (inteng does not edit it either)";
		case PolyCreateResult::Unreadable: return "a chunk of the shape could not be read or put together";
		case PolyCreateResult::CantBuild: return "the level couldn't be put together from its bytes";
		case PolyCreateResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	PolyCreateResult PolyEdit_CreateInModel(model::ShapeModel *shape, std::vector<int32_t> *mergeData, int32_t const *corners, size_t count, double const viewForward[3], size_t *first)
	{
		if (!shape || !corners || (count != 3 && count != 4))
		{
			return PolyCreateResult::BadCall;
		}

		for (size_t i = 0; i < count; i++)
		{
			if (corners[i] < 0 || (size_t)corners[i] >= shape->points.size())
			{
				return PolyCreateResult::BadCall;
			}

			for (size_t j = 0; j < i; j++)
			{
				if (corners[j] == corners[i])
				{
					return PolyCreateResult::BadCall;
				}
			}
		}

		size_t const now = shape->polygons.size();
		size_t const added = count - 2;

		if (shape->polygonNormals.size() != now || (mergeData && mergeData->size() != now))
		{
			return PolyCreateResult::Unreadable;
		}

		if (now > model::kMostShapePolygons || added > model::kMostShapePolygons - now)
		{
			return PolyCreateResult::TooMany;
		}

		// Three corners make one triangle; four make (0,1,3) and (1,2,3)
		int32_t const last = corners[count - 1];
		int32_t const triangles[2][3] = { { corners[0], corners[1], count == 4 ? last : corners[2] }, { corners[1], corners[2], last } };
		Vec view = { 0, 0, 0 };
		bool const flip = viewForward && (viewForward[0] != 0.0 || viewForward[1] != 0.0 || viewForward[2] != 0.0);

		if (flip)
		{
			view.x = viewForward[0];
			view.y = viewForward[1];
			view.z = viewForward[2];
		}

		model::ShapePolygon made[2];
		model::ShapeNormal normals[2];

		for (size_t k = 0; k < added; k++)
		{
			if (!MakeTriangle(shape->points, triangles[k][0], triangles[k][1], triangles[k][2], flip ? &view : nullptr, &made[k], &normals[k], (int32_t)(now + k)))
			{
				return PolyCreateResult::Degenerate;
			}
		}

		for (size_t k = 0; k < added; k++)
		{
			shape->polygons.push_back(made[k]);
			shape->polygonNormals.push_back(normals[k]);

			if (mergeData)
			{
				mergeData->push_back(-1);
			}
		}

		if (mergeData && added == 2)
		{
			(*mergeData)[now] = (int32_t)(now + 1);
			(*mergeData)[now + 1] = (int32_t)now;
		}

		if (first)
		{
			*first = now;
		}

		return PolyCreateResult::Done;
	}

	namespace
	{
		enum class Rebuilt
		{
			Done,
			CantBuild,
			LoaderRefused,
		};

		struct NewChunk
		{
			char const *name;
			Payload const *payload;
			bool addIfMissing = false;   // put the chunk at the end of the shape's chunks when it has none
		};

		// TM-C H1: what inteng's removal FUN_00435080 does last (Delete polys' FUN_00563d90, and Split's FUN_00564300 on its copy of the shape), through the removal's point map and the shape's new
		// point count. FUN_00435a00, each SHPBONHD of the shape: SHPBONIN's point count (its third number) becomes the shape's (0x435aaf); each SHPBONE keeps the records (20 bytes: the point, then
		// four more numbers) of the points kept, in their order, each moved whole with its point renumbered (0x435b23-0x435b4b), and a SHPBONE left with no record is deleted (0x435bb0).
		// FUN_00435c80, each SHPVTINT of every object using the shape: one number per point of the shape, a kept point's old one, 0xffffffff for any other (0x435dde-0x435e25).
		// NAMED (input is hostile): inteng reads its map past its end for a record whose point is past the old shape's points (FUN_00435a00 reads param_2[point]) and for a colour list longer than the
		// old shape (FUN_00435c80 reads param_2[i] for every i of the list); here such a record goes, and such a colour is dropped.
		struct PointRemap
		{
			std::vector<int32_t> const *map;   // one per old point: its new number, or -1 (PolyRemoveMap::point)
			int32_t count;                     // the shape's points now
		};

		// A SHPBONDT (the bone's name with its zero, padded to 4; the number of records; the records; the numbers after them) with the records of the points kept, renumbered; *kept counts them
		bool RemapBoneRecords(Image const &image, Span const &data, PointRemap const &remap, Image *out, size_t *kept)
		{
			size_t const end = data.at + data.size;
			size_t const nameAt = data.at + kHeadingBytes;

			if (nameAt >= end)
			{
				return false;
			}

			char const *const first = image.data() + nameAt;
			char const *const zero = (char const *)memchr(first, 0, end - nameAt);

			if (!zero)
			{
				return false;
			}

			size_t const padded = ((size_t)(zero - first) + 4) & ~(size_t)3;
			size_t at = nameAt + padded;

			if (at > end || end - at < 4)
			{
				return false;
			}

			int32_t const count = IntAt(image, at);

			at += 4;

			if (count < 0 || (size_t)count > (end - at) / 20)
			{
				return false;
			}

			Image made;

			AppendBytes(&made, image, data.at, kHeadingBytes + padded);

			size_t const countAt = made.size();
			size_t n = 0;

			made.resize(countAt + 4);

			for (int32_t k = 0; k < count; k++, at += 20)
			{
				int32_t const point = IntAt(image, at);

				if (point < 0 || (size_t)point >= remap.map->size() || (*remap.map)[(size_t)point] < 0)
				{
					continue;
				}

				size_t const recordAt = made.size();

				AppendBytes(&made, image, at, 20);
				PutIntAt(&made, recordAt, (*remap.map)[(size_t)point]);
				n++;
			}

			AppendBytes(&made, image, at, end - at);   // the numbers after the records, as they were
			PutIntAt(&made, countAt, (int32_t)n);
			PutIntAt(&made, 8, (int32_t)made.size());
			out->insert(out->end(), made.begin(), made.end());
			*kept += n;
			return true;
		}

		// A SHPBONE with its SHPBONDT remapped (anything else in it copied); nothing at all when none of its points is kept
		bool RemapBone(Image const &image, Span const &bone, PointRemap const &remap, Image *out)
		{
			std::vector<Span> parts;

			if (!Children(image, bone.at + kHeadingBytes, bone.at + bone.size, &parts))
			{
				return false;
			}

			Image made;
			size_t kept = 0;

			AppendBytes(&made, image, bone.at, kHeadingBytes);

			for (Span const &part : parts)
			{
				if (!NameIs(image, part.at, "SHPBONDT"))
				{
					AppendBytes(&made, image, part.at, part.size);
				}
				else if (!RemapBoneRecords(image, part, remap, &made, &kept))
				{
					return false;
				}
			}

			if (kept == 0)
			{
				return true;   // FUN_00435a00 0x435bb0: a bone left with no point is deleted
			}

			PutIntAt(&made, 8, (int32_t)made.size());
			out->insert(out->end(), made.begin(), made.end());
			return true;
		}

		// A SHPBONHD remapped: SHPBONIN's point count is the shape's, each SHPBONE as above, anything else copied
		bool RemapBoneHeader(Image const &image, Span const &header, PointRemap const &remap, Image *out)
		{
			std::vector<Span> parts;

			if (!Children(image, header.at + kHeadingBytes, header.at + header.size, &parts))
			{
				return false;
			}

			Image made;

			AppendBytes(&made, image, header.at, kHeadingBytes);

			for (Span const &part : parts)
			{
				if (NameIs(image, part.at, "SHPBONIN"))
				{
					if (part.size < kHeadingBytes + 12)
					{
						return false;
					}

					size_t const at = made.size();

					AppendBytes(&made, image, part.at, part.size);
					PutIntAt(&made, at + kHeadingBytes + 8, remap.count);
				}
				else if (NameIs(image, part.at, "SHPBONE"))
				{
					if (!RemapBone(image, part, remap, &made))
					{
						return false;
					}
				}
				else
				{
					AppendBytes(&made, image, part.at, part.size);
				}
			}

			PutIntAt(&made, 8, (int32_t)made.size());
			out->insert(out->end(), made.begin(), made.end());
			return true;
		}

		// Whether a top-level RBOBJECT names this shape (its OBJHEAD1's payload holds the shape's number at 56)
		bool ObjectUsesShape(Image const &image, Span const &object, int32_t shapeNumber)
		{
			std::vector<Span> parts;

			if (!NameIs(image, object.at, "RBOBJECT") || !Children(image, object.at + kHeadingBytes, object.at + object.size, &parts))
			{
				return false;
			}

			for (Span const &part : parts)
			{
				if (NameIs(image, part.at, "OBJHEAD1") && part.size >= kHeadingBytes + 60 && IntAt(image, part.at + kHeadingBytes + 56) == shapeNumber)
				{
					return true;
				}
			}

			return false;
		}

		// An object's chunk with each SHPVTINT (the set's name, a pad, the count, one number per point: chunk_light.cpp Shape_Vertex_Intensities_Chunk) made one number per point of the shape
		bool RemapObjectTints(Image const &image, Span const &object, PointRemap const &remap, Image *out)
		{
			std::vector<Span> parts;

			if (!Children(image, object.at + kHeadingBytes, object.at + object.size, &parts))
			{
				return false;
			}

			Image made;

			AppendBytes(&made, image, object.at, kHeadingBytes);

			for (Span const &part : parts)
			{
				if (!NameIs(image, part.at, "SHPVTINT"))
				{
					AppendBytes(&made, image, part.at, part.size);
					continue;
				}

				if (part.size < kHeadingBytes + 16)
				{
					return false;
				}

				int32_t const count = IntAt(image, part.at + kHeadingBytes + 12);

				if (count < 0 || (size_t)count > (part.size - kHeadingBytes - 16) / 4)
				{
					return false;
				}

				std::vector<int32_t> now((size_t)remap.count, -1);

				for (int32_t i = 0; i < count && (size_t)i < remap.map->size(); i++)
				{
					int32_t const to = (*remap.map)[(size_t)i];

					if (to >= 0 && to < remap.count)
					{
						now[(size_t)to] = IntAt(image, part.at + kHeadingBytes + 16 + (size_t)i * 4);
					}
				}

				size_t const start = made.size();

				AppendBytes(&made, image, part.at, kHeadingBytes + 16);   // the heading, the set's name, the pad and the count
				PutIntAt(&made, start + kHeadingBytes + 12, remap.count);
				made.resize(start + kHeadingBytes + 16 + now.size() * 4);

				if (!now.empty())
				{
					memcpy(made.data() + start + kHeadingBytes + 16, now.data(), now.size() * 4);
				}

				PutIntAt(&made, start + 8, (int32_t)(made.size() - start));
			}

			PutIntAt(&made, 8, (int32_t)made.size());
			out->insert(out->end(), made.begin(), made.end());
			return true;
		}

		// The level as the next save would write it, with the chunks named in `chunks` of the shape numbered shapeNumber made again from their payloads (each must be in the shape, the others are
		// copied as they were), put in by ReplaceLevel, which reads it with the game's loader and swaps it in only if the loader takes it. appendTop (none by default): a whole top-level chunk put at
		// the end of the file in the same image (Merge's copy of a shared shape, TM-B 2d: inteng's copy is the file's last child, FUN_0050e880). remap (none by default): the shape's points were
		// renumbered (a removal), so its bone attachments and the vertex colours of the objects using it follow, in the same image (PointRemap, TM-C H1).
		Rebuilt RebuildShape(OpenLevelFile *file, int32_t shapeNumber, NewChunk const *chunks, size_t chunkCount, Image const *appendTop = nullptr, PointRemap const *remap = nullptr)
		{
			Image image;

			if (chunkCount > 12 || !file->BuildLevel(image) || image.size() < kHeadingBytes || !NameIs(image, 0, "REBINFF2"))
			{
				return Rebuilt::CantBuild;
			}

			std::vector<Span> top;

			if (!Children(image, kHeadingBytes, image.size(), &top))
			{
				return Rebuilt::CantBuild;
			}

			// The shape's chunk, found by its number
			size_t at_Shape = top.size();

			for (size_t i = 0; i < top.size() && at_Shape == top.size(); i++)
			{
				if (!NameIs(image, top[i].at, "REBSHAPE"))
				{
					continue;
				}

				std::vector<Span> inside;

				if (!Children(image, top[i].at + kHeadingBytes, top[i].at + top[i].size, &inside))
				{
					return Rebuilt::CantBuild;
				}

				for (Span const &child : inside)
				{
					if (NameIs(image, child.at, "SHPHEAD1") && child.size >= kHeadingBytes + kShapeNumberAt + 4 && IntAt(image, child.at + kHeadingBytes + kShapeNumberAt) == shapeNumber)
					{
						at_Shape = i;
						break;
					}
				}
			}

			if (at_Shape == top.size())
			{
				return Rebuilt::CantBuild;
			}

			// The new image: every top-level chunk as it was, except the shape, whose named chunks are made again
			Image out;

			AppendBytes(&out, image, 0, kHeadingBytes);

			for (size_t i = 0; i < top.size(); i++)
			{
				if (i != at_Shape)
				{
					if (remap && ObjectUsesShape(image, top[i], shapeNumber))
					{
						if (!RemapObjectTints(image, top[i], *remap, &out))
						{
							return Rebuilt::CantBuild;
						}
					}
					else
					{
						AppendBytes(&out, image, top[i].at, top[i].size);
					}

					continue;
				}

				std::vector<Span> inside;

				if (!Children(image, top[i].at + kHeadingBytes, top[i].at + top[i].size, &inside))
				{
					return Rebuilt::CantBuild;
				}

				size_t const start = out.size();
				bool done[12] = { false };

				AppendBytes(&out, image, top[i].at, kHeadingBytes);

				for (Span const &child : inside)
				{
					size_t which = chunkCount;

					for (size_t n = 0; n < chunkCount; n++)
					{
						if (!done[n] && NameIs(image, child.at, chunks[n].name))
						{
							which = n;
							break;
						}
					}

					if (which == chunkCount && remap && NameIs(image, child.at, "SHPBONHD"))
					{
						if (!RemapBoneHeader(image, child, *remap, &out))
						{
							return Rebuilt::CantBuild;
						}
					}
					else if (which == chunkCount)
					{
						AppendBytes(&out, image, child.at, child.size);
					}
					else
					{
						done[which] = true;
						if (chunks[which].payload)   // a null payload drops the chunk
						{
							AppendChunk(&out, chunks[which].name, *chunks[which].payload);
						}
					}
				}

				for (size_t n = 0; n < chunkCount; n++)
				{
					if (!done[n] && chunks[n].payload)
					{
						if (!chunks[n].addIfMissing)
						{
							return Rebuilt::CantBuild;   // the shape lacked a chunk that was to be made again
						}

						AppendChunk(&out, chunks[n].name, *chunks[n].payload);   // a chunk the shape does not have yet goes at the end of its chunks
					}
				}

				PutIntAt(&out, start + 8, (int32_t)(out.size() - start));
			}

			if (appendTop)
			{
				out.insert(out.end(), appendTop->begin(), appendTop->end());
			}

			PutIntAt(&out, 8, (int32_t)out.size());

			switch (file->ReplaceLevel(out))
			{
			case LevelOpenResult::Opened: return Rebuilt::Done;
			case LevelOpenResult::CantRead: return Rebuilt::CantBuild;
			default: return Rebuilt::LoaderRefused;
			}
		}
	}

	PolyCreateResult PolyEdit_Create(OpenLevelFile *file, uint8_t const *id, int32_t const *corners, size_t count, double const viewForward[3], size_t *first, size_t *added, std::string *why)
	{
		if (!file || !id || !corners || !file->Level())
		{
			return PolyCreateResult::BadCall;
		}

		File_Chunk *const level = file->Level();
		Object_Chunk *const object = ObjectWithId(level, id);

		if (!object)
		{
			return PolyCreateResult::NoObject;
		}

		Shape_Chunk *const shape = object->get_assoc_shape();

		if (!shape)
		{
			return PolyCreateResult::NoShape;
		}

		if (shape->lookup_single_child("SHPMORPH"))
		{
			return PolyCreateResult::Morphing;
		}

		model::ShapeModel model;

		if (ShapeEdit_Read(level, file, id, &model, nullptr, why) != ShapeEditResult::Done)
		{
			return PolyCreateResult::NoShape;
		}

		// The shape's merge numbers, one per polygon, when it has the chunk
		Chunk *const mergeChunk = shape->lookup_single_child("SHPMRGDT");
		std::vector<int32_t> merge;
		bool const hasMerge = mergeChunk != nullptr;

		if (hasMerge)
		{
			Payload bytes;

			if (!file->PayloadOf(mergeChunk, &bytes) || bytes.size() % 4 != 0 || bytes.size() / 4 != model.polygons.size())
			{
				Say(why, "the shape's merge data is not one number for each polygon");
				return PolyCreateResult::Unreadable;
			}

			merge.resize(bytes.size() / 4);

			if (!bytes.empty())
			{
				memcpy(merge.data(), bytes.data(), bytes.size());
			}
		}

		size_t firstNew = 0;
		PolyCreateResult const made = PolyEdit_CreateInModel(&model, hasMerge ? &merge : nullptr, corners, count, viewForward, &firstNew);

		if (made != PolyCreateResult::Done)
		{
			return made;
		}

		// The new bytes of the chunks that change
		Payload headerBytes;
		Chunk *const headerChunk = shape->lookup_single_child("SHPHEAD1");

		if (!headerChunk || !file->PayloadOf(headerChunk, &headerBytes))
		{
			return PolyCreateResult::Unreadable;
		}

		Payload polygonBytes(model.polygons.size() * model::kShapePolygonBytes);
		Payload normalBytes(model.polygonNormals.size() * model::kShapeNormalBytes);
		Payload mergeBytes(merge.size() * 4);

		if (model::WriteShapePolygons(model, &polygonBytes) != model::ShapeModelResult::Ok ||
			model::WriteShapeNormals(model.polygonNormals, &normalBytes) != model::ShapeModelResult::Ok ||
			model::WriteShapeHeaderSettingCounts(model, &headerBytes) != model::ShapeModelResult::Ok)
		{
			Say(why, "the new polygons' numbers wouldn't go into chunks");
			return PolyCreateResult::Unreadable;
		}

		if (!merge.empty())
		{
			memcpy(mergeBytes.data(), merge.data(), mergeBytes.size());
		}

		NewChunk chunks[4] = { { "SHPHEAD1", &headerBytes }, { "SHPPOLYS", &polygonBytes }, { "SHPPNORM", &normalBytes }, { "SHPMRGDT", &mergeBytes } };

		switch (RebuildShape(file, model.number, chunks, hasMerge ? 4 : 3))
		{
		case Rebuilt::Done:
			if (first)
			{
				*first = firstNew;
			}

			if (added)
			{
				*added = count - 2;
			}

			return PolyCreateResult::Done;
		case Rebuilt::CantBuild: return PolyCreateResult::CantBuild;
		default: return PolyCreateResult::LoaderRefused;
		}
	}

	// ---------------------------------------------------------------- polygon type changes

	char const *PolyTypeWords(PolyTypeResult result)
	{
		switch (result)
		{
		case PolyTypeResult::Done: return "done";
		case PolyTypeResult::BadCall: return "nothing to change, a polygon named twice, or a colour the game would refuse";
		case PolyTypeResult::NoObject: return "no object has that name";
		case PolyTypeResult::NoShape: return "the object has no shape the editor can read";
		case PolyTypeResult::NoPolygon: return "a polygon the shape does not have";
		case PolyTypeResult::Animated: return "a polygon is in the shape's texture animation: take it out of the animation first";
		case PolyTypeResult::TooManyCorners: return "a polygon with five corners cannot be textured (a list of texture coordinates holds four)";
		case PolyTypeResult::NoPicture: return "the shape has no picture of that number, or no list chunk to put the coordinates in";
		case PolyTypeResult::Unreadable: return "a chunk of the shape could not be read or put together";
		case PolyTypeResult::CantBuild: return "the level couldn't be put together from its bytes";
		case PolyTypeResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	namespace
	{
		struct TypeWork
		{
			model::ShapeModel shape;
			std::vector<model::UvList> lists;
			std::vector<std::string> names;
			bool hasUvChunk;
		};

		// Lists of texture coordinates that were used by polygons that are plain now (or gone): `freed` are their numbers. A list that no textured polygon of the shape uses any more goes out of the
		// lists, and the lists after it move down in every textured polygon's colour word. *dropped says whether any went. False (with *why) for a polygon that names a list the shape does not have.
		bool DropFreedLists(model::ShapeModel *shape, std::vector<model::UvList> *lists, std::set<int32_t> const &freed, bool *dropped, std::string *why)
		{
			std::set<int32_t> used;

			*dropped = false;

			for (model::ShapePolygon const &polygon : shape->polygons)
			{
				if (model::EngineTypeIsTextured(polygon.engineType))
				{
					used.insert(model::PolygonUvListNumber(polygon.colour));
				}
			}

			std::set<int32_t> gone;

			for (int32_t const list : freed)
			{
				if (list < 0 || (size_t)list >= lists->size())
				{
					return Say(why, "a polygon names a list of texture coordinates the shape does not have");
				}

				if (used.count(list) == 0)
				{
					gone.insert(list);
				}
			}

			if (gone.empty())
			{
				return true;
			}

			std::vector<int32_t> moved(lists->size(), -1);
			std::vector<model::UvList> kept;

			for (size_t i = 0; i < lists->size(); i++)
			{
				if (gone.count((int32_t)i) == 0)
				{
					moved[i] = (int32_t)kept.size();
					kept.push_back((*lists)[i]);
				}
			}

			for (model::ShapePolygon &polygon : shape->polygons)
			{
				if (model::EngineTypeIsTextured(polygon.engineType))
				{
					int32_t const old = model::PolygonUvListNumber(polygon.colour);

					if (old < 0 || (size_t)old >= moved.size() || moved[(size_t)old] < 0)
					{
						return Say(why, "a polygon names a list of texture coordinates the shape does not have");
					}

					polygon.colour = (int32_t)(((uint32_t)moved[(size_t)old] << 16) | ((uint32_t)polygon.colour & 0xffffu));
				}
			}

			lists->swap(kept);
			*dropped = true;
			return true;
		}

		// Everything the two type changes read first. The polygons are checked here: some, all different, all in the shape.
		PolyTypeResult PrepareType(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, TypeWork *work, std::string *why)
		{
			if (!file || !id || !polygons || count == 0 || count > model::kMostShapePolygons || !file->Level())
			{
				return PolyTypeResult::BadCall;
			}

			File_Chunk *const level = file->Level();
			Object_Chunk *const object = ObjectWithId(level, id);

			if (!object)
			{
				return PolyTypeResult::NoObject;
			}

			Shape_Chunk *const shape = object->get_assoc_shape();

			if (!shape || ShapeEdit_Read(level, file, id, &work->shape, nullptr, why) != ShapeEditResult::Done)
			{
				return PolyTypeResult::NoShape;
			}

			std::vector<int32_t> sorted(polygons, polygons + count);

			std::sort(sorted.begin(), sorted.end());

			for (size_t i = 0; i < sorted.size(); i++)
			{
				if (sorted[i] < 0 || (size_t)sorted[i] >= work->shape.polygons.size())
				{
					return PolyTypeResult::NoPolygon;
				}

				if (i > 0 && sorted[i] == sorted[i - 1])
				{
					return PolyTypeResult::BadCall;
				}
			}

			Chunk *const uv = shape->lookup_single_child("SHPUVCRD");
			Chunk *const names = shape->lookup_single_child("SHPTEXFN");
			Payload bytes;

			work->hasUvChunk = uv != nullptr;

			if (uv && (!file->PayloadOf(uv, &bytes) || model::ReadUvLists(bytes.data(), bytes.size(), &work->lists) != model::UvModelResult::Ok))
			{
				Say(why, "the shape's list of texture coordinates could not be read");
				return PolyTypeResult::Unreadable;
			}

			if (names && (!file->PayloadOf(names, &bytes) || model::ReadTextureNames(bytes.data(), bytes.size(), &work->names) != model::UvModelResult::Ok))
			{
				Say(why, "the shape's picture names could not be read");
				return PolyTypeResult::Unreadable;
			}

			return PolyTypeResult::Done;
		}

		// SHPUVCRD's payload: the count, then for each list its count of corners and the corners' u and v
		Payload UvBytes(std::vector<model::UvList> const &lists)
		{
			Payload out;
			int32_t const count = (int32_t)lists.size();

			out.resize(4);
			memcpy(out.data(), &count, 4);

			for (model::UvList const &list : lists)
			{
				int32_t const corners = list.cornerCount;
				size_t const at = out.size();

				out.resize(at + 4 + (size_t)corners * 8);
				memcpy(out.data() + at, &corners, 4);

				for (int j = 0; j < corners; j++)
				{
					memcpy(out.data() + at + 4 + (size_t)j * 8, &list.corners[j].u, 4);
					memcpy(out.data() + at + 4 + (size_t)j * 8 + 4, &list.corners[j].v, 4);
				}
			}

			return out;
		}

		PolyTypeResult Finish(OpenLevelFile *file, TypeWork const &work, bool listsChanged)
		{
			Payload polygonBytes(work.shape.polygons.size() * model::kShapePolygonBytes);
			Payload uvBytes;

			if (model::WriteShapePolygons(work.shape, &polygonBytes) != model::ShapeModelResult::Ok)
			{
				return PolyTypeResult::Unreadable;
			}

			if (listsChanged)
			{
				uvBytes = UvBytes(work.lists);
			}

			NewChunk chunks[2] = { { "SHPPOLYS", &polygonBytes }, { "SHPUVCRD", &uvBytes } };

			switch (RebuildShape(file, work.shape.number, chunks, listsChanged ? 2 : 1))
			{
			case Rebuilt::Done: return PolyTypeResult::Done;
			case Rebuilt::CantBuild: return PolyTypeResult::CantBuild;
			default: return PolyTypeResult::LoaderRefused;
			}
		}
	}

	PolyTypeResult PolyEdit_MakePlain(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, int32_t colourWord, size_t *changed, std::string *why)
	{
		TypeWork work;
		PolyTypeResult const ready = PrepareType(file, id, polygons, count, &work, why);

		if (ready != PolyTypeResult::Done)
		{
			return ready;
		}

		if (colourWord < 0)
		{
			return PolyTypeResult::BadCall;
		}

		// a polygon in the shape's texture animation is the caller's to take out first
		int32_t shapeNumber = 0;
		model::TextureAnim anim;

		if (AnimEdit_Read(file, id, &shapeNumber, &anim) != AnimEditResult::Done)
		{
			return PolyTypeResult::NoShape;
		}

		for (size_t i = 0; i < count; i++)
		{
			for (model::AnimPolygon const &animated : anim.polygons)
			{
				if (animated.polygon == polygons[i])
				{
					return PolyTypeResult::Animated;
				}
			}
		}

		std::set<int32_t> freed;
		size_t wereTextured = 0;

		for (size_t i = 0; i < count; i++)
		{
			model::ShapePolygon &polygon = work.shape.polygons[(size_t)polygons[i]];

			if (model::EngineTypeIsTextured(polygon.engineType))
			{
				freed.insert(model::PolygonUvListNumber(polygon.colour));
				wereTextured++;
			}

			polygon.engineType = 2;
			polygon.colour = colourWord;
		}

		bool listsGone = false;

		if (!DropFreedLists(&work.shape, &work.lists, freed, &listsGone, why))
		{
			return PolyTypeResult::Unreadable;
		}


		PolyTypeResult const done = Finish(file, work, listsGone);

		if (done == PolyTypeResult::Done && changed)
		{
			*changed = wereTextured;
		}

		return done;
	}

	PolyTypeResult PolyEdit_MakeTextured(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, int32_t pictureNumber, size_t *changed, std::string *why)
	{
		TypeWork work;
		PolyTypeResult const ready = PrepareType(file, id, polygons, count, &work, why);

		if (ready != PolyTypeResult::Done)
		{
			return ready;
		}

		int32_t const picture = pictureNumber < 0 ? 0 : pictureNumber;
		size_t plain = 0;

		for (size_t i = 0; i < count; i++)
		{
			model::ShapePolygon const &polygon = work.shape.polygons[(size_t)polygons[i]];

			if (!model::EngineTypeIsTextured(polygon.engineType))
			{
				if (polygon.cornerCount > model::kMostUvCorners)
				{
					return PolyTypeResult::TooManyCorners;
				}

				plain++;
			}
		}

		if (plain == 0)
		{
			if (changed)
			{
				*changed = 0;
			}

			return PolyTypeResult::Done;   // every one is textured already: nothing to change
		}

		if (!work.hasUvChunk || (!work.names.empty() && (size_t)picture >= work.names.size()) || (size_t)picture > 0x7fff || work.lists.size() + plain > 0xffffu)
		{
			return PolyTypeResult::NoPicture;
		}

		for (size_t i = 0; i < count; i++)
		{
			model::ShapePolygon &polygon = work.shape.polygons[(size_t)polygons[i]];

			if (model::EngineTypeIsTextured(polygon.engineType))
			{
				continue;
			}

			model::UvList list = {};

			list.cornerCount = polygon.cornerCount;   // every corner at (0, 0)
			polygon.engineType = 7;                   // I_3dTexturedPolygon
			polygon.colour = (int32_t)(((uint32_t)work.lists.size() << 16) | (uint32_t)picture);
			work.lists.push_back(list);
		}

		PolyTypeResult const done = Finish(file, work, true);

		if (done == PolyTypeResult::Done && changed)
		{
			*changed = plain;
		}

		return done;
	}

	// ---------------------------------------------------------------- taking polygons out

	char const *PolyRemoveWords(PolyRemoveResult result)
	{
		switch (result)
		{
		case PolyRemoveResult::Done: return "done";
		case PolyRemoveResult::BadCall: return "nothing to take out, or a polygon named twice";
		case PolyRemoveResult::AllPolygons: return "that is every polygon of the shape (inteng deletes the whole object then: use Object Placement's Delete)";
		case PolyRemoveResult::NoObject: return "no object has that name";
		case PolyRemoveResult::NoShape: return "the object has no shape the editor can read";
		case PolyRemoveResult::NoPolygon: return "a polygon the shape does not have";
		case PolyRemoveResult::Unsupported: return "the shape has a chunk that polygons cannot be taken out of yet";
		case PolyRemoveResult::Unreadable: return "a chunk of the shape could not be read or put together";
		case PolyRemoveResult::CantBuild: return "the level couldn't be put together from its bytes";
		case PolyRemoveResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	PolyRemoveResult PolyEdit_RemoveInModel(model::ShapeModel *shape, std::vector<int32_t> *mergeData, int32_t const *polygons, size_t count, PolyRemoveMap *map)
	{
		if (!shape || !polygons || count == 0)
		{
			return PolyRemoveResult::BadCall;
		}

		size_t const n = shape->polygons.size();
		std::vector<char> gone(n, 0);

		for (size_t i = 0; i < count; i++)
		{
			if (polygons[i] < 0 || (size_t)polygons[i] >= n)
			{
				return PolyRemoveResult::NoPolygon;
			}

			if (gone[(size_t)polygons[i]])
			{
				return PolyRemoveResult::BadCall;
			}

			gone[(size_t)polygons[i]] = 1;
		}

		if (count >= n)
		{
			return PolyRemoveResult::AllPolygons;
		}

		bool const pointNormals = !shape->pointNormals.empty();

		if (shape->polygonNormals.empty() || (mergeData && mergeData->size() != n) || (pointNormals && shape->pointNormals.size() != shape->points.size()))
		{
			return PolyRemoveResult::Unreadable;
		}

		// the new numbers
		std::vector<int32_t> polygonMap(n, -1), pointMap(shape->points.size(), -1);
		int32_t kept = 0;

		for (size_t i = 0; i < n; i++)
		{
			if (!gone[i])
			{
				polygonMap[i] = kept++;

				for (int c = 0; c < shape->polygons[i].cornerCount; c++)
				{
					int32_t const point = shape->polygons[i].corners[c];

					if (point < 0 || (size_t)point >= pointMap.size())
					{
						return PolyRemoveResult::Unreadable;
					}

					pointMap[(size_t)point] = 0;   // used
				}
			}
		}

		int32_t pointsKept = 0;

		for (size_t p = 0; p < pointMap.size(); p++)
		{
			if (pointMap[p] != -1)
			{
				pointMap[p] = pointsKept++;
			}
		}

		// the new shape, made aside and put in only when it is whole
		model::ShapeModel now = *shape;

		now.polygons.clear();
		now.polygonNormals.clear();
		now.points.clear();
		now.pointNormals.clear();

		for (size_t i = 0; i < n; i++)
		{
			if (gone[i])
			{
				continue;
			}

			model::ShapePolygon polygon = shape->polygons[i];

			if (polygon.normalIndex < 0 || (size_t)polygon.normalIndex >= shape->polygonNormals.size())
			{
				return PolyRemoveResult::Unreadable;
			}

			now.polygonNormals.push_back(shape->polygonNormals[(size_t)polygon.normalIndex]);
			polygon.normalIndex = polygonMap[i];

			for (int c = 0; c < polygon.cornerCount; c++)
			{
				polygon.corners[c] = pointMap[(size_t)polygon.corners[c]];
			}

			now.polygons.push_back(polygon);
		}

		for (size_t p = 0; p < pointMap.size(); p++)
		{
			if (pointMap[p] != -1)
			{
				now.points.push_back(shape->points[p]);

				if (pointNormals)
				{
					now.pointNormals.push_back(shape->pointNormals[p]);
				}
			}
		}

		if (model::ShapeExtentsOf(now.points, &now.extents) != model::ShapeMathsResult::Ok)
		{
			return PolyRemoveResult::Unreadable;
		}

		now.radius = model::ShapeRadiusOf(now.points);

		std::vector<int32_t> newMerge;

		if (mergeData)
		{
			newMerge.assign(now.polygons.size(), -1);

			for (size_t i = 0; i < n; i++)
			{
				if (polygonMap[i] >= 0)
				{
					int32_t const partner = (*mergeData)[i];

					newMerge[(size_t)polygonMap[i]] = partner >= 0 && (size_t)partner < n ? polygonMap[(size_t)partner] : -1;
				}
			}

			mergeData->swap(newMerge);
		}

		*shape = now;

		if (map)
		{
			map->polygon.swap(polygonMap);
			map->point.swap(pointMap);
		}

		return PolyRemoveResult::Done;
	}

	PolyRemoveResult PolyEdit_Remove(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, PolyRemoveMap *map, std::string *why)
	{
		if (!file || !id || !polygons || count == 0 || !file->Level())
		{
			return PolyRemoveResult::BadCall;
		}

		File_Chunk *const level = file->Level();
		Object_Chunk *const object = ObjectWithId(level, id);

		if (!object)
		{
			return PolyRemoveResult::NoObject;
		}

		Shape_Chunk *const shape = object->get_assoc_shape();

		if (!shape)
		{
			return PolyRemoveResult::NoShape;
		}

		for (char const *const name : { "SHPMORPH", "ANIMSEQU", "SHPPCINF", "SHPFRAGS", "SHPFRGTP" })
		{
			if (shape->lookup_single_child(name))
			{
				Say(why, (std::string("the shape has a ") + name + " chunk").c_str());
				return PolyRemoveResult::Unsupported;
			}
		}

		model::ShapeModel model;

		if (ShapeEdit_Read(level, file, id, &model, nullptr, why) != ShapeEditResult::Done)
		{
			return PolyRemoveResult::NoShape;
		}

		// the chunks that follow the polygons
		Chunk *const mergeChunk = shape->lookup_single_child("SHPMRGDT");
		Chunk *const uvChunk = shape->lookup_single_child("SHPUVCRD");
		Chunk *const centreChunk = shape->lookup_single_child("SHPCENTR");
		std::vector<int32_t> merge;
		std::vector<model::UvList> lists;
		Payload bytes;

		if (mergeChunk)
		{
			if (!file->PayloadOf(mergeChunk, &bytes) || bytes.size() % 4 != 0 || bytes.size() / 4 != model.polygons.size())
			{
				Say(why, "the shape's merge data is not one number for each polygon");
				return PolyRemoveResult::Unreadable;
			}

			merge.resize(bytes.size() / 4);

			if (!bytes.empty())
			{
				memcpy(merge.data(), bytes.data(), bytes.size());
			}
		}

		if (uvChunk && (!file->PayloadOf(uvChunk, &bytes) || model::ReadUvLists(bytes.data(), bytes.size(), &lists) != model::UvModelResult::Ok))
		{
			Say(why, "the shape's list of texture coordinates could not be read");
			return PolyRemoveResult::Unreadable;
		}

		Payload centreBytes;

		if (centreChunk && (!file->PayloadOf(centreChunk, &centreBytes) || centreBytes.size() != 16))
		{
			Say(why, "the shape's centre is not what the editor reads");
			return PolyRemoveResult::Unreadable;
		}

		int32_t shapeNumber = 0;
		model::TextureAnim anim;

		if (AnimEdit_Read(file, id, &shapeNumber, &anim) != AnimEditResult::Done)
		{
			return PolyRemoveResult::NoShape;
		}

		// the lists the polygons taken out were using
		std::set<int32_t> freed;

		for (size_t i = 0; i < count; i++)
		{
			if (polygons[i] >= 0 && (size_t)polygons[i] < model.polygons.size() && model::EngineTypeIsTextured(model.polygons[(size_t)polygons[i]].engineType) && !(model.polygons[(size_t)polygons[i]].colour & 0x8000))
			{
				freed.insert(model::PolygonUvListNumber(model.polygons[(size_t)polygons[i]].colour));
			}
		}

		PolyRemoveMap made;
		PolyRemoveResult const removed = PolyEdit_RemoveInModel(&model, mergeChunk ? &merge : nullptr, polygons, count, &made);

		if (removed != PolyRemoveResult::Done)
		{
			return removed;
		}

		bool listsGone = false;

		if (!DropFreedLists(&model, &lists, freed, &listsGone, why))
		{
			return PolyRemoveResult::Unreadable;
		}

		// the animation: the records of the polygons taken out go, the others are renumbered
		bool animChanged = false;
		model::TextureAnim keptAnim;

		for (model::AnimPolygon const &one : anim.polygons)
		{
			if (one.polygon >= 0 && (size_t)one.polygon < made.polygon.size() && made.polygon[(size_t)one.polygon] >= 0)
			{
				model::AnimPolygon moved = one;

				moved.polygon = made.polygon[(size_t)one.polygon];
				animChanged = animChanged || moved.polygon != one.polygon;
				keptAnim.polygons.push_back(moved);
			}
			else
			{
				animChanged = true;
			}
		}

		// the new bytes
		Payload headerBytes, pointsBytes(model.points.size() * model::kShapePointBytes), polygonBytes(model.polygons.size() * model::kShapePolygonBytes);
		Payload polyNormalBytes(model.polygonNormals.size() * model::kShapeNormalBytes), pointNormalBytes(model.pointNormals.size() * model::kShapeNormalBytes);
		Payload mergeBytes(merge.size() * 4), uvBytes, animBytes;
		Chunk *const headerChunk = shape->lookup_single_child("SHPHEAD1");

		if (!headerChunk || !file->PayloadOf(headerChunk, &headerBytes))
		{
			return PolyRemoveResult::Unreadable;
		}

		if (model::WriteShapePoints(model, &pointsBytes) != model::ShapeModelResult::Ok || model::WriteShapePolygons(model, &polygonBytes) != model::ShapeModelResult::Ok ||
			model::WriteShapeNormals(model.polygonNormals, &polyNormalBytes) != model::ShapeModelResult::Ok || model::WriteShapeNormals(model.pointNormals, &pointNormalBytes) != model::ShapeModelResult::Ok ||
			model::WriteShapeHeaderSettingCounts(model, &headerBytes) != model::ShapeModelResult::Ok)
		{
			Say(why, "the new numbers wouldn't go into chunks");
			return PolyRemoveResult::Unreadable;
		}

		if (!merge.empty())
		{
			memcpy(mergeBytes.data(), merge.data(), mergeBytes.size());
		}

		if (listsGone)
		{
			uvBytes = UvBytes(lists);
		}

		if (animChanged && !keptAnim.polygons.empty() && model::WriteTextureAnim(keptAnim, &animBytes) != model::TextureAnimResult::Ok)
		{
			Say(why, "the shape's animation wouldn't go into its chunk");
			return PolyRemoveResult::Unreadable;
		}

		// SHPCENTR is worked out again from the new extents, as inteng's rebuild does (FUN_0050f570): the middle of the extents, each sum halved toward zero, and the longest distance of a point from it
		if (centreChunk)
		{
			int32_t const mid[3] =
			{
				(int32_t)(((int64_t)model.extents.maxX + (int64_t)model.extents.minX) / 2),
				(int32_t)(((int64_t)model.extents.maxY + (int64_t)model.extents.minY) / 2),
				(int32_t)(((int64_t)model.extents.maxZ + (int64_t)model.extents.minZ) / 2),
			};
			float const aboutCentre = model::ShapeRadiusAboutCentre(model);

			memcpy(centreBytes.data(), mid, 12);
			memcpy(centreBytes.data() + 12, &aboutCentre, 4);
		}

		NewChunk chunks[10];
		size_t used = 0;

		chunks[used++] = { "SHPHEAD1", &headerBytes };
		chunks[used++] = { "SHPRAWVT", &pointsBytes };
		chunks[used++] = { "SHPPOLYS", &polygonBytes };
		chunks[used++] = { "SHPPNORM", &polyNormalBytes };

		if (!model.pointNormals.empty())
		{
			chunks[used++] = { "SHPVNORM", &pointNormalBytes };
		}

		if (mergeChunk)
		{
			chunks[used++] = { "SHPMRGDT", &mergeBytes };
		}

		if (listsGone)
		{
			chunks[used++] = { "SHPUVCRD", &uvBytes };
		}

		if (animChanged)
		{
			chunks[used++] = { "TEXTANIM", keptAnim.polygons.empty() ? nullptr : &animBytes };
		}

		if (centreChunk)
		{
			chunks[used++] = { "SHPCENTR", &centreBytes };
		}

		// TM-C H1 (FUN_00435a00, FUN_00435c80): the bone attachments and the users' vertex colours follow the points
		PointRemap const remap = { &made.point, (int32_t)model.points.size() };

		switch (RebuildShape(file, model.number, chunks, used, nullptr, &remap))
		{
		case Rebuilt::Done:
			if (map)
			{
				*map = made;
			}

			return PolyRemoveResult::Done;
		case Rebuilt::CantBuild: return PolyRemoveResult::CantBuild;
		default: return PolyRemoveResult::LoaderRefused;
		}
	}

	bool PolyEdit_CanFlagForDeletion(OpenLevelFile *file, uint8_t const *id)
	{
		if (!file || !id || !file->Level())
		{
			return false;
		}

		Object_Chunk *const object = ObjectWithId(file->Level(), id);
		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

		return shape && !shape->lookup_single_child("SHPMORPH") && !shape->lookup_single_child("ANIMSEQU");
	}

	// ---------------------------------------------------------------- unmerge

	char const *PolyUnmergeWords(PolyUnmergeResult result)
	{
		switch (result)
		{
		case PolyUnmergeResult::Done: return "done";
		case PolyUnmergeResult::BadCall: return "nothing to unmerge";
		case PolyUnmergeResult::NoObject: return "no object has that name";
		case PolyUnmergeResult::NoShape: return "the object has no shape the editor can read";
		case PolyUnmergeResult::NoPolygon: return "a polygon the shape does not have";
		case PolyUnmergeResult::NoMergeData: return "the shape has no merge data, so nothing is merged";
		case PolyUnmergeResult::Unreadable: return "the shape's merge data is not one number for each polygon";
		case PolyUnmergeResult::CantBuild: return "the level couldn't be put together from its bytes";
		case PolyUnmergeResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	PolyUnmergeResult PolyEdit_Unmerge(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, size_t *parted, std::string *why)
	{
		if (!file || !id || !polygons || count == 0 || !file->Level())
		{
			return PolyUnmergeResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(file->Level(), id);
		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

		if (!object)
		{
			return PolyUnmergeResult::NoObject;
		}

		model::ShapeModel model;

		if (!shape || ShapeEdit_Read(file->Level(), file, id, &model, nullptr, why) != ShapeEditResult::Done)
		{
			return PolyUnmergeResult::NoShape;
		}

		Chunk *const mergeChunk = shape->lookup_single_child("SHPMRGDT");
		Payload bytes;

		if (!mergeChunk)
		{
			return PolyUnmergeResult::NoMergeData;
		}

		if (!file->PayloadOf(mergeChunk, &bytes) || bytes.size() % 4 != 0 || bytes.size() / 4 != model.polygons.size())
		{
			return PolyUnmergeResult::Unreadable;
		}

		Payload const original = bytes;
		std::vector<int32_t> merge(bytes.size() / 4);

		memcpy(merge.data(), bytes.data(), bytes.size());

		for (size_t i = 0; i < count; i++)
		{
			if (polygons[i] < 0 || (size_t)polygons[i] >= merge.size())
			{
				return PolyUnmergeResult::NoPolygon;
			}
		}

		// FUN_00573e20: each selected polygon that has a partner is parted from it, both numbers made -1
		size_t made = 0;

		for (size_t i = 0; i < count; i++)
		{
			int32_t const partner = merge[(size_t)polygons[i]];

			if (partner >= 0 && (size_t)partner < merge.size())
			{
				merge[(size_t)polygons[i]] = -1;
				merge[(size_t)partner] = -1;
				made++;
			}
		}

		if (made > 0)
		{
			memcpy(bytes.data(), merge.data(), bytes.size());

			if (!file->SetPayload(mergeChunk, bytes))
			{
				return PolyUnmergeResult::Unreadable;
			}

			// the loader reads the level again so the game's items are the new quads and triangles
			Image image;

			if (!file->BuildLevel(image))
			{
				file->SetPayload(mergeChunk, original);
				return PolyUnmergeResult::CantBuild;
			}

			switch (file->ReplaceLevel(image))
			{
			case LevelOpenResult::Opened: break;
			case LevelOpenResult::CantRead: file->SetPayload(mergeChunk, original); return PolyUnmergeResult::CantBuild;
			default: file->SetPayload(mergeChunk, original); return PolyUnmergeResult::LoaderRefused;
			}
		}

		if (parted)
		{
			*parted = made;
		}

		return PolyUnmergeResult::Done;
	}

	// ---------------------------------------------------------------- merge

	namespace
	{
		// FUN_0055a0d0 (false = the two triangles can be one quad): the free corner of each, then the quad's two turns, each made unit; a turn of 0 or less everywhere
		bool QuadTurnsWrong(model::ShapeModel const &shape, size_t a, size_t b, int freeA, int freeB)
		{
			model::ShapePolygon const &pa = shape.polygons[a];
			model::ShapePolygon const &pb = shape.polygons[b];
			model::ShapeNormal const &na = shape.polygonNormals[(size_t)pa.normalIndex];
			Vec const n = { na.x, na.y, na.z };
			Vec const free1 = PointOf(shape.points[(size_t)pa.corners[freeA]]);
			Vec const next = PointOf(shape.points[(size_t)pa.corners[(freeA + 1) % 3]]);
			Vec const prev = PointOf(shape.points[(size_t)pa.corners[(freeA + 2) % 3]]);
			Vec const freeB2 = PointOf(shape.points[(size_t)pb.corners[freeB]]);

			Vec e1 = VSub(next, free1);
			Vec e2 = VSub(freeB2, next);

			if (!VUnit(&e1) || !VUnit(&e2))
			{
				return true;
			}

			if (VDot(VCross(e1, n), e2) * kOne >= 1.0)
			{
				return true;
			}

			Vec f1 = VSub(prev, freeB2);
			Vec f2 = VSub(free1, prev);

			if (!VUnit(&f1) || !VUnit(&f2))
			{
				return true;
			}

			return VDot(VCross(f1, n), f2) * kOne >= 1.0;
		}

		int FreeCorner(model::ShapePolygon const &of, model::ShapePolygon const &other)
		{
			for (int i = 0; i < 3; i++)
			{
				bool found = false;

				for (int j = 0; j < 3; j++)
				{
					found = found || of.corners[i] == other.corners[j];
				}

				if (!found)
				{
					return i;
				}
			}

			return -1;
		}

		// FUN_00573b70: the cosine of the angle at a triangle's free corner, the larger of the two triangles'
		double ApexCosine(model::ShapeModel const &shape, model::ShapePolygon const &of, int free)
		{
			Vec const origin = PointOf(shape.points[(size_t)of.corners[free]]);
			Vec e1 = VSub(PointOf(shape.points[(size_t)of.corners[(free + 1) % 3]]), origin);
			Vec e2 = VSub(PointOf(shape.points[(size_t)of.corners[(free + 2) % 3]]), origin);

			if (!VUnit(&e1) || !VUnit(&e2))
			{
				return 1.0;
			}

			return VDot(e1, e2);
		}
	}

	bool PolyEdit_Joinable(model::ShapeModel const &shape, int32_t a, int32_t b, bool fake, double *score)
	{
		size_t const n = shape.polygons.size();

		if (a < 0 || b < 0 || a == b || (size_t)a >= n || (size_t)b >= n || shape.polygonNormals.size() != n)
		{
			return false;
		}

		model::ShapePolygon const &pa = shape.polygons[(size_t)a];
		model::ShapePolygon const &pb = shape.polygons[(size_t)b];

		if (pa.cornerCount != 3 || pb.cornerCount != 3 || pa.normalIndex < 0 || pb.normalIndex < 0 || (size_t)pa.normalIndex >= n || (size_t)pb.normalIndex >= n)
		{
			return false;
		}

		for (int i = 0; i < 3; i++)
		{
			if (pa.corners[i] < 0 || pb.corners[i] < 0 || (size_t)pa.corners[i] >= shape.points.size() || (size_t)pb.corners[i] >= shape.points.size())
			{
				return false;
			}
		}

		int shared = 0;

		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				shared += pa.corners[i] == pb.corners[j] ? 1 : 0;
			}
		}

		if (shared != 2)
		{
			return false;
		}

		model::ShapeNormal const &na = shape.polygonNormals[(size_t)pa.normalIndex];
		model::ShapeNormal const &nb = shape.polygonNormals[(size_t)pb.normalIndex];
		double const dot = (double)na.x * nb.x + (double)na.y * nb.y + (double)na.z * nb.z;

		int const freeB = FreeCorner(pb, pa);
		int const freeA = FreeCorner(pa, pb);

		if (freeA < 0 || freeB < 0)
		{
			return false;
		}

		if (!fake)
		{
			if (dot < 0.9998)
			{
				// not the same way round: the free corner of the second has to lie in the first one's plane within 1 unit (FUN_0055a360)
				Vec const edge = VSub(PointOf(shape.points[(size_t)pb.corners[freeB]]), PointOf(shape.points[(size_t)pb.corners[(freeB + 1) % 3]]));
				double const out = edge.x * na.x + edge.y * na.y + edge.z * na.z;

				if (out < -1.0 || out > 1.0)
				{
					return false;
				}
			}
		}
		else if (dot < 0.6)
		{
			return false;
		}

		if (QuadTurnsWrong(shape, (size_t)a, (size_t)b, freeA, freeB))
		{
			return false;
		}

		if (score)
		{
			double const sa = ApexCosine(shape, pa, freeA);
			double const sb = ApexCosine(shape, pb, freeB);

			*score = sb < sa ? sa : sb;
		}

		return true;
	}

	size_t PolyEdit_MergeInModel(model::ShapeModel const &shape, std::vector<int32_t> *mergeData, int32_t const *polygons, size_t count, bool fake)
	{
		if (!mergeData || !polygons || mergeData->size() != shape.polygons.size())
		{
			return 0;
		}

		// the selected polygons that are not paired yet, in the order they were selected (FUN_00573620 takes the others out of its list)
		std::vector<int32_t> pool;

		for (size_t i = 0; i < count; i++)
		{
			if (polygons[i] >= 0 && (size_t)polygons[i] < mergeData->size() && (*mergeData)[(size_t)polygons[i]] == -1 && std::find(pool.begin(), pool.end(), polygons[i]) == pool.end())
			{
				pool.push_back(polygons[i]);
			}
		}

		struct Pair
		{
			int32_t a, b;
			double score;
		};

		std::vector<Pair> pairs;

		while (pool.size() >= 2)
		{
			int32_t const a = pool.front();

			pool.erase(pool.begin());

			for (int32_t const b : pool)
			{
				double score = 0;

				if (PolyEdit_Joinable(shape, a, b, fake, &score))
				{
					// inserted before the first pair whose score is not smaller: lowest first, a tie goes in front
					size_t at = 0;

					while (at < pairs.size() && pairs[at].score < score)
					{
						at++;
					}

					Pair const made = { a, b, score };

					pairs.insert(pairs.begin() + (std::ptrdiff_t)at, made);
				}
			}
		}

		size_t joined = 0;

		for (Pair const &pair : pairs)
		{
			if ((*mergeData)[(size_t)pair.a] == -1 && (*mergeData)[(size_t)pair.b] == -1)
			{
				(*mergeData)[(size_t)pair.a] = pair.b;
				(*mergeData)[(size_t)pair.b] = pair.a;
				joined++;
			}
		}

		return joined;
	}

	char const *PolyMergeWords(PolyMergeResult result)
	{
		switch (result)
		{
		case PolyMergeResult::Done: return "done";
		case PolyMergeResult::BadCall: return "nothing to merge";
		case PolyMergeResult::NoObject: return "no object has that name";
		case PolyMergeResult::NoShape: return "the object has no shape the editor can read";
		case PolyMergeResult::NoPolygon: return "a polygon the shape does not have";
		case PolyMergeResult::Unreadable: return "the shape's merge data is not one number for each polygon";
		case PolyMergeResult::CantBuild: return "the level couldn't be put together from its bytes";
		case PolyMergeResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	namespace
	{
		// FUN_0055cdf0's shared corners: each of a's first three corners at the same point as one of b's first three (inteng looks at three: a merged pair is two triangles). Their places on the
		// picture are compared as whole texels, as the game's loader holds them (uv_model.h: chunk_load.cpp:2789, 2863). A corner a list lacks is no match.
		bool SameUvsAtSharedCorners(model::ShapePolygon const &a, model::ShapePolygon const &b, model::UvList const *aList, model::UvList const *bList)
		{
			if (!aList || !bList)
			{
				return false;
			}

			for (int c = 0; c < 3; c++)
			{
				for (int k = 0; k < 3; k++)
				{
					if (a.corners[c] < 0 || a.corners[c] != b.corners[k])
					{
						continue;
					}

					if (c >= aList->cornerCount || k >= bList->cornerCount)
					{
						return false;
					}

					if ((int32_t)aList->corners[c].u != (int32_t)bList->corners[k].u || (int32_t)aList->corners[c].v != (int32_t)bList->corners[k].v)
					{
						return false;
					}
				}
			}

			return true;
		}
	}

	size_t PolyEdit_CheckMergedPairs(model::ShapeModel *model, std::vector<model::UvList> const &lists, std::vector<int32_t> *merge, bool *polygonsChanged)
	{
		bool changed = false;
		size_t parted = 0;

		if (polygonsChanged)
		{
			*polygonsChanged = false;
		}

		if (!model || !merge || merge->size() != model->polygons.size())
		{
			return 0;
		}

		// the list of a polygon's texture coordinates, or null when the shape has none at its number
		auto const listOf = [&lists](model::ShapePolygon const &polygon) -> const model::UvList *
		{
			uint32_t const number = (uint32_t)model::PolygonUvListNumber(polygon.colour);

			return number < lists.size() ? &lists[number] : nullptr;
		};

		// the type as Texture Map holds it (FUN_0055d770): a textured type with no list or an empty one is 2 (0x55d8d2: the game's loader leaves it untextured, chunk_load.cpp:2849), 6 / 0x15 / 0x16
		// are 5 (0x55da15), 0x17 / 0x18 / 0x19 are 7 (0x55da1e)
		auto const heldType = [&listOf](model::ShapePolygon const &polygon) -> int32_t
		{
			int32_t const type = polygon.engineType;

			if (!model::EngineTypeIsTextured(type))
			{
				return type;
			}

			model::UvList const *const list = listOf(polygon);

			if (!list || list->cornerCount == 0)
			{
				return 2;
			}

			return type == 7 || type >= 0x17 ? 7 : 5;
		};

		for (size_t i = 0; i < merge->size(); i++)
		{
			int32_t const partner = (*merge)[i];

			if (partner < 0 || (size_t)partner >= i)
			{
				continue;
			}

			model::ShapePolygon &a = model->polygons[i];
			model::ShapePolygon &b = model->polygons[(size_t)partner];
			bool part = false;

			// the flags made one: each gets both (0x55d558)
			if (a.flags != b.flags)
			{
				a.flags |= b.flags;
				b.flags = a.flags;
				changed = true;
			}

			int32_t const aType = heldType(a), bType = heldType(b);
			bool const aTextured = model::EngineTypeIsTextured(aType), bTextured = model::EngineTypeIsTextured(bType);

			// the types: an untextured one parts the pair (0x55d5e3); both textured, the higher-numbered takes its partner's (0x55d5d8)
			if (aType != bType)
			{
				if (!aTextured || !bTextured)
				{
					part = true;
				}
				else
				{
					a.engineType = b.engineType;
					changed = true;
				}
			}

			if (!aTextured || part)
			{
				// the higher-numbered takes its partner's colour (0x55d6d6). INTENG BUG, FIXED: inteng copies the number it holds for the polygon whatever the two types are, and for a textured
				// polygon that number is its picture: a pair parted for being one textured and one plain gave the plain one a picture's number for a colour, or the textured one a colour for a
				// picture. Ours copies a colour only between two polygons the file holds untextured.
				if (!model::EngineTypeIsTextured(a.engineType) && !model::EngineTypeIsTextured(b.engineType) && a.colour != b.colour)
				{
					a.colour = b.colour;
					changed = true;
				}
			}
			else if (model::PolygonPictureNumber(a.colour) != model::PolygonPictureNumber(b.colour) || !SameUvsAtSharedCorners(a, b, listOf(a), listOf(b)))
			{
				// both textured: another picture (0x55d636) or another place on it at a corner they share (0x55d684) parts the pair
				part = true;
			}

			if (part)
			{
				(*merge)[i] = -1;
				(*merge)[(size_t)partner] = -1;
				parted++;
			}
		}

		if (polygonsChanged)
		{
			*polygonsChanged = changed;
		}

		return parted;
	}

	namespace
	{
		// The shape's lists of texture coordinates (none when it has no SHPUVCRD); false when its chunk can't be read
		bool ShapeUvLists(OpenLevelFile *file, Shape_Chunk *shape, std::vector<model::UvList> *lists)
		{
			Chunk *const uvChunk = shape->lookup_single_child("SHPUVCRD");
			Payload bytes;

			lists->clear();

			return !uvChunk || (file->PayloadOf(uvChunk, &bytes) && model::ReadUvLists(bytes.data(), bytes.size(), lists) == model::UvModelResult::Ok);
		}

		// FUN_0055d500 on the pairing about to be written: *numbers parted where the checks part them, *polygonBytes (the shape's SHPPOLYS, same size) set when a polygon's flags, type or colour
		// changed (left empty otherwise). False when the shape's lists or polygons can't be read or written.
		bool CheckPairsForTheSave(OpenLevelFile *file, Shape_Chunk *shape, model::ShapeModel model, std::vector<int32_t> *numbers, Payload *polygonBytes, size_t *parted, std::string *why)
		{
			std::vector<model::UvList> lists;
			bool polygonsChanged = false;

			polygonBytes->clear();

			if (!ShapeUvLists(file, shape, &lists))
			{
				Say(why, "the shape's list of texture coordinates could not be read");
				return false;
			}

			size_t const broken = PolyEdit_CheckMergedPairs(&model, lists, numbers, &polygonsChanged);

			if (parted)
			{
				*parted = broken;
			}

			if (!polygonsChanged)
			{
				return true;
			}

			Chunk *const polygonChunk = shape->lookup_single_child("SHPPOLYS");

			if (!polygonChunk || !file->PayloadOf(polygonChunk, polygonBytes) || model::WriteShapePolygons(model, polygonBytes) != model::ShapeModelResult::Ok)
			{
				polygonBytes->clear();
				Say(why, "the shape's polygons could not be written");
				return false;
			}

			return true;
		}
	}

	// The shape's SHPMRGDT numbers written as `merge` (one per polygon): a chunk of the same size put in place, then the loader reads the level again; a shape without one gets the chunk (inteng's save
	// writes the pairing into SHPMRGDT: FUN_00566d60). subShapesToo (Merge's save, FUN_0055b900 0x55bb8e / 0x55bd84): each SUBSHAPE of the shape's first SHPMORPH and first ASALTTEX whose SHPMRGDT has the
	// same count and whose points are as many (inteng compares the counts: +0x64 / +0x58 against the shape's +0x7c / +0x70) gets the same numbers. polygonBytes (when not null and not empty): the
	// shape's SHPPOLYS put in too (Merge's checks, FUN_0055d500). On any refusal every chunk is put back.
	static PolyMergeResult WriteMergeNumbers(OpenLevelFile *file, Shape_Chunk *shape, model::ShapeModel const &model, Chunk *mergeChunk, Payload const &original, std::vector<int32_t> const &merge, bool subShapesToo,
		Payload const *polygonBytes = nullptr)
	{
		bool const withPolygons = polygonBytes && !polygonBytes->empty();

		Payload mergeBytes(merge.size() * 4);

		if (!mergeBytes.empty())
		{
			memcpy(mergeBytes.data(), merge.data(), mergeBytes.size());
		}

		if (mergeChunk)
		{
			std::vector<std::pair<Chunk *, Payload>> puts;   // every chunk written, with its bytes before, to put back

			if (!file->SetPayload(mergeChunk, mergeBytes))
			{
				return PolyMergeResult::Unreadable;
			}

			puts.push_back(std::make_pair(mergeChunk, original));

			if (withPolygons)
			{
				Chunk *const polygonChunk = shape->lookup_single_child("SHPPOLYS");
				Payload before;

				if (!polygonChunk || !file->PayloadOf(polygonChunk, &before) || !file->SetPayload(polygonChunk, *polygonBytes))
				{
					file->SetPayload(mergeChunk, original);
					return PolyMergeResult::Unreadable;
				}

				puts.push_back(std::make_pair(polygonChunk, before));
			}

			for (char const *const holderName : { "SHPMORPH", "ASALTTEX" })
			{
				Chunk_With_Children *const holder = subShapesToo ? dynamic_cast<Chunk_With_Children *>(shape->lookup_single_child(holderName)) : nullptr;
				List<Chunk *> subs;

				if (!holder)
				{
					continue;
				}

				holder->lookup_child("SUBSHAPE", subs);

				for (LIF<Chunk *> each(&subs); !each.done(); each.next())
				{
					Chunk_With_Children *const sub = dynamic_cast<Chunk_With_Children *>(each());
					Chunk *const subMerge = sub ? sub->lookup_single_child("SHPMRGDT") : nullptr;
					Chunk *const subPoints = sub ? sub->lookup_single_child("SHPRAWVT") : nullptr;
					Payload subBefore, pointBytes;

					if (!subMerge || !file->PayloadOf(subMerge, &subBefore) || subBefore.size() != mergeBytes.size() || !subPoints || !file->PayloadOf(subPoints, &pointBytes) ||
						pointBytes.size() != model.points.size() * 12)
					{
						continue;
					}

					if (file->SetPayload(subMerge, mergeBytes))
					{
						puts.push_back(std::make_pair(subMerge, subBefore));
					}
				}
			}

			auto const putBack = [file, &puts]()
			{
				for (auto const &one : puts)
				{
					file->SetPayload(one.first, one.second);
				}
			};

			Image image;

			if (!file->BuildLevel(image))
			{
				putBack();
				return PolyMergeResult::CantBuild;
			}

			switch (file->ReplaceLevel(image))
			{
			case LevelOpenResult::Opened: return PolyMergeResult::Done;
			case LevelOpenResult::CantRead: putBack(); return PolyMergeResult::CantBuild;
			default: putBack(); return PolyMergeResult::LoaderRefused;
			}
		}

		NewChunk chunks[2] = { { "SHPMRGDT", &mergeBytes, true }, { "SHPPOLYS", polygonBytes } };

		switch (RebuildShape(file, model.number, chunks, withPolygons ? 2 : 1))
		{
		case Rebuilt::Done: return PolyMergeResult::Done;
		case Rebuilt::CantBuild: return PolyMergeResult::CantBuild;
		default: return PolyMergeResult::LoaderRefused;
		}
	}

	PolyMergeResult PolyEdit_Merge(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, size_t *joined, std::string *why)
	{
		if (!file || !id || !polygons || count == 0 || !file->Level())
		{
			return PolyMergeResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(file->Level(), id);

		if (!object)
		{
			return PolyMergeResult::NoObject;
		}

		Shape_Chunk *const shape = object->get_assoc_shape();
		model::ShapeModel model;

		if (!shape || ShapeEdit_Read(file->Level(), file, id, &model, nullptr, why) != ShapeEditResult::Done)
		{
			return PolyMergeResult::NoShape;
		}

		for (size_t i = 0; i < count; i++)
		{
			if (polygons[i] < 0 || (size_t)polygons[i] >= model.polygons.size())
			{
				return PolyMergeResult::NoPolygon;
			}
		}

		Chunk *const mergeChunk = shape->lookup_single_child("SHPMRGDT");
		std::vector<int32_t> merge(model.polygons.size(), -1);
		Payload bytes;

		if (mergeChunk)
		{
			if (!file->PayloadOf(mergeChunk, &bytes) || bytes.size() != model.polygons.size() * 4)
			{
				return PolyMergeResult::Unreadable;
			}

			if (!bytes.empty())
			{
				memcpy(merge.data(), bytes.data(), bytes.size());
			}
		}

		size_t const made = PolyEdit_MergeInModel(model, &merge, polygons, count, false);

		if (joined)
		{
			*joined = made;
		}

		if (made == 0)
		{
			return PolyMergeResult::Done;
		}

		return WriteMergeNumbers(file, shape, model, mergeChunk, bytes, merge, false);
	}

	PolyMergeResult PolyEdit_SetMergeData(OpenLevelFile *file, uint8_t const *id, std::vector<int32_t> const &numbers, std::string *why, size_t *parted)
	{
		if (parted)
		{
			*parted = 0;
		}

		if (!file || !id || !file->Level())
		{
			return PolyMergeResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(file->Level(), id);

		if (!object)
		{
			return PolyMergeResult::NoObject;
		}

		Shape_Chunk *const shape = object->get_assoc_shape();
		model::ShapeModel model;

		if (!shape || ShapeEdit_Read(file->Level(), file, id, &model, nullptr, why) != ShapeEditResult::Done)
		{
			return PolyMergeResult::NoShape;
		}

		if (numbers.size() != model.polygons.size())
		{
			return PolyMergeResult::Unreadable;
		}

		for (int32_t const n : numbers)
		{
			if (n < -1 || n >= (int32_t)numbers.size())
			{
				return PolyMergeResult::NoPolygon;   // a partner the shape does not have
			}
		}

		Chunk *const mergeChunk = shape->lookup_single_child("SHPMRGDT");
		Payload bytes;

		if (mergeChunk && (!file->PayloadOf(mergeChunk, &bytes) || bytes.size() != model.polygons.size() * 4))
		{
			return PolyMergeResult::Unreadable;
		}

		// FUN_0055d500 (0x55bb73), before the numbers are written
		std::vector<int32_t> checked = numbers;
		Payload polygonBytes;
		size_t broken = 0;

		if (!CheckPairsForTheSave(file, shape, model, &checked, &polygonBytes, &broken, why))
		{
			return PolyMergeResult::Unreadable;
		}

		PolyMergeResult const result = WriteMergeNumbers(file, shape, model, mergeChunk, bytes, checked, true, &polygonBytes);

		if (result == PolyMergeResult::Done && parted)
		{
			*parted = broken;
		}

		return result;
	}

	PolyMergeResult PolyEdit_CheckPairsNow(OpenLevelFile *file, uint8_t const *id, bool write, size_t *parted, bool *polygonsChanged, std::string *why)
	{
		size_t broken = 0;
		bool changed = false;

		if (parted)
		{
			*parted = 0;
		}

		if (polygonsChanged)
		{
			*polygonsChanged = false;
		}

		if (!file || !id || !file->Level())
		{
			return PolyMergeResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(file->Level(), id);
		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;
		model::ShapeModel model;

		if (!object)
		{
			return PolyMergeResult::NoObject;
		}

		if (!shape || ShapeEdit_Read(file->Level(), file, id, &model, nullptr, why) != ShapeEditResult::Done)
		{
			return PolyMergeResult::NoShape;
		}

		Chunk *const mergeChunk = shape->lookup_single_child("SHPMRGDT");
		Payload bytes;

		if (!mergeChunk)
		{
			return PolyMergeResult::Done;   // no pairing: nothing to check
		}

		if (!file->PayloadOf(mergeChunk, &bytes) || bytes.size() != model.polygons.size() * 4)
		{
			return PolyMergeResult::Unreadable;
		}

		std::vector<int32_t> numbers(model.polygons.size(), -1);

		if (!bytes.empty())
		{
			memcpy(numbers.data(), bytes.data(), bytes.size());
		}

		for (int32_t const n : numbers)
		{
			if (n < -1 || n >= (int32_t)numbers.size())
			{
				return PolyMergeResult::NoPolygon;   // hostile input: a partner the shape does not have; left as the file holds it
			}
		}

		std::vector<int32_t> checked = numbers;
		Payload polygonBytes;

		if (!CheckPairsForTheSave(file, shape, model, &checked, &polygonBytes, &broken, why))
		{
			return PolyMergeResult::Unreadable;
		}

		changed = !polygonBytes.empty();

		if (parted)
		{
			*parted = broken;
		}

		if (polygonsChanged)
		{
			*polygonsChanged = changed;
		}

		if (!write || (broken == 0 && !changed))
		{
			return PolyMergeResult::Done;
		}

		return WriteMergeNumbers(file, shape, model, mergeChunk, bytes, checked, false, &polygonBytes);   // the shape's own SHPMRGDT only (FUN_0055d500 changes the working array)
	}

	namespace
	{
		// A shape's list of the objects that use it, as shape_merge_edit.cpp reads and writes it (Frost, TB-C8b): SHPHEAD1's payload holds 68 bytes of numbers, the count at 64, then each name and
		// its zero, then zeros up to a whole number of 4 bytes (Shape_Header_Chunk::size_chunk). An object's OBJHEAD1 holds the number of its shape at 56.
		size_t const kUsersCountAt = 64;
		size_t const kUsersHeadBytes = 68;
		size_t const kObjectShapeNumberAt = 56;
		// the rest of the 68 bytes a copy's fresh header sets (CloneShapeChunk): flags at 0, the lock user's 16 bytes at 4, the version at 60
		size_t const kHeadFlagsAt = 0;
		size_t const kHeadLockUserAt = 4;
		size_t const kHeadLockUserBytes = 16;
		size_t const kHeadVersionAt = 60;
		int32_t const kGeneralFlagLocked = 0x1;   // chunk.hpp GENERAL_FLAG_LOCKED

		bool CloneShapeChunk(OpenLevelFile *file, int32_t oldNumber, NewChunk const *chunks, size_t chunkCount, Payload *header, int32_t knownHighest, int32_t *newNumber, Image *out,
			PointRemap const *remap = nullptr);   // below, with Split

		bool UsersInHead(Payload const &head, std::vector<std::string> *names)
		{
			names->clear();

			if (head.size() < kUsersHeadBytes)
			{
				return false;
			}

			int32_t count;
			size_t at = kUsersHeadBytes;

			memcpy(&count, head.data() + kUsersCountAt, 4);

			if (count < 0 || (size_t)count > head.size() - at)
			{
				return false;
			}

			for (int32_t i = 0; i < count; i++)
			{
				unsigned char const *const zero = at < head.size() ? (unsigned char const *)memchr(head.data() + at, 0, head.size() - at) : nullptr;

				if (!zero)
				{
					return false;
				}

				names->push_back(std::string((char const *)head.data() + at, (size_t)(zero - (head.data() + at))));
				at += names->back().size() + 1;
			}

			return true;
		}

		Payload HeadWithUsers(Payload const &head, std::vector<std::string> const &names)
		{
			Payload out(head.begin(), head.begin() + (std::ptrdiff_t)kUsersHeadBytes);
			int32_t const count = (int32_t)names.size();

			memcpy(out.data() + kUsersCountAt, &count, 4);

			for (std::string const &name : names)
			{
				out.insert(out.end(), name.begin(), name.end());
				out.push_back(0);
			}

			while ((kHeadingBytes + out.size()) % 4 != 0)
			{
				out.push_back(0);
			}

			return out;
		}
	}

	bool PolyEdit_ShapeUserNames(OpenLevelFile *file, uint8_t const *id, std::vector<std::string> *names)
	{
		names->clear();

		Object_Chunk *const object = file && id && file->Level() ? ObjectWithId(file->Level(), id) : nullptr;
		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;
		Shape_Header_Chunk *const header = shape ? shape->get_header() : nullptr;
		Payload head;

		return header && file->PayloadOf(header, &head) && UsersInHead(head, names);
	}

	PolyMergeResult PolyEdit_SetMergeDataOnCopy(OpenLevelFile *file, uint8_t const *id, std::vector<std::string> const &movers, std::vector<int32_t> const &numbers, std::string *why, size_t *parted)
	{
		if (parted)
		{
			*parted = 0;
		}

		if (!file || !id || !file->Level() || movers.empty())
		{
			return PolyMergeResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(file->Level(), id);
		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;
		Shape_Header_Chunk *const header = shape ? shape->get_header() : nullptr;
		model::ShapeModel model;
		Payload head;
		std::vector<std::string> users;

		if (!object)
		{
			return PolyMergeResult::NoObject;
		}

		if (!shape || !header || ShapeEdit_Read(file->Level(), file, id, &model, nullptr, why) != ShapeEditResult::Done || !file->PayloadOf(header, &head) || !UsersInHead(head, &users))
		{
			return PolyMergeResult::NoShape;
		}

		if (numbers.size() != model.polygons.size())
		{
			return PolyMergeResult::Unreadable;
		}

		for (int32_t const n : numbers)
		{
			if (n < -1 || n >= (int32_t)numbers.size())
			{
				return PolyMergeResult::NoPolygon;
			}
		}

		// the users that keep the shape as it was: every name of its list but the movers' (each once)
		std::vector<std::string> keepers = users;

		for (std::string const &mover : movers)
		{
			auto const at = std::find(keepers.begin(), keepers.end(), mover);

			if (at == keepers.end())
			{
				Say(why, ("the shape's list does not name " + mover).c_str());
				return PolyMergeResult::NoObject;
			}

			keepers.erase(at);
		}

		// THE COPY (FUN_0050e880): the shape's chunks with the movers as its users, the working pairing as its SHPMRGDT, numbered one past the level's highest (CloneShapeChunk puts the number in)
		// FUN_0055d500 (0x55bb73) on the copy's pairing and polygons: the old shape keeps its own
		std::vector<int32_t> checked = numbers;
		Payload polygonBytes;
		size_t broken = 0;

		if (!CheckPairsForTheSave(file, shape, model, &checked, &polygonBytes, &broken, why))
		{
			return PolyMergeResult::Unreadable;
		}

		Payload copyHead = HeadWithUsers(head, movers);
		Payload mergeBytes(checked.size() * 4);

		if (!mergeBytes.empty())
		{
			memcpy(mergeBytes.data(), checked.data(), mergeBytes.size());
		}

		NewChunk copyChunks[3] = { { "SHPHEAD1", &copyHead }, { "SHPMRGDT", &mergeBytes, true }, { "SHPPOLYS", &polygonBytes } };
		Image copyImage;
		int32_t newNumber = -1;

		if (!CloneShapeChunk(file, model.number, copyChunks, polygonBytes.empty() ? 2 : 3, &copyHead, -1, &newNumber, &copyImage))
		{
			return PolyMergeResult::CantBuild;
		}

		// the movers' headers name the copy (same size: put in place)
		std::vector<std::pair<Chunk *, Payload>> puts;
		auto const putBack = [file, &puts]()
		{
			for (auto const &one : puts)
			{
				file->SetPayload(one.first, one.second);
			}
		};
		List<Object_Chunk *> objects;

		file->Level()->list_objects(&objects);

		for (std::string const &mover : movers)
		{
			bool found = false;

			for (LIF<Object_Chunk *> each(&objects); !each.done() && !found; each.next())
			{
				Object_Chunk *const one = each();
				Object_Header_Chunk *const objectHeader = one ? one->get_header() : nullptr;
				Payload bytes;

				if (!one || std::string(one->object_data.o_name, strnlen(one->object_data.o_name, sizeof(one->object_data.o_name))) != mover || !objectHeader ||
					!file->PayloadOf(objectHeader, &bytes) || bytes.size() < kObjectShapeNumberAt + 4)
				{
					continue;
				}

				Payload changed = bytes;

				memcpy(changed.data() + kObjectShapeNumberAt, &newNumber, 4);

				if (!file->SetPayload(objectHeader, changed))
				{
					putBack();
					return PolyMergeResult::CantBuild;
				}

				puts.push_back(std::make_pair((Chunk *)objectHeader, bytes));
				found = true;
			}

			if (!found)
			{
				putBack();
				Say(why, ("no object of the level is named " + mover).c_str());
				return PolyMergeResult::NoObject;
			}
		}

		// the old shape less the movers' names, and the copy at the end of the file, in ONE image the loader reads
		Payload oldHead = HeadWithUsers(head, keepers);
		NewChunk oldChunks[1] = { { "SHPHEAD1", &oldHead } };

		switch (RebuildShape(file, model.number, oldChunks, 1, &copyImage))
		{
		case Rebuilt::Done:
			if (parted)
			{
				*parted = broken;
			}

			return PolyMergeResult::Done;
		case Rebuilt::CantBuild: putBack(); return PolyMergeResult::CantBuild;
		default: putBack(); return PolyMergeResult::LoaderRefused;
		}
	}

	// ---------------------------------------------------------------- split into a new placed object

	namespace
	{
		// A whole new REBSHAPE chunk made from the shape numbered oldNumber: its chunks named in `chunks` are made from their payloads (a null payload leaves the chunk out), the others are copied; the new shape's number
		// (one more than the highest the level has) is put in the header payload (the chunk named SHPHEAD1 in `chunks`), whose list of users is left empty and which is made a fresh header (version 0, no lock user, no lock flag: FUN_0050e880). *newNumber says it. remap (none by default): the
		// copy's points are a renumbering of the old shape's, so its bone attachments follow (TM-C H1; the copy has no user yet, so no vertex colours: FUN_00435c80 finds none on inteng's copy)
		bool CloneShapeChunk(OpenLevelFile *file, int32_t oldNumber, NewChunk const *chunks, size_t chunkCount, Payload *header, int32_t knownHighest, int32_t *newNumber, Image *out, PointRemap const *remap)
		{
			Image image;

			if (!file->BuildLevel(image) || image.size() < kHeadingBytes || !NameIs(image, 0, "REBINFF2"))
			{
				return false;
			}

			std::vector<Span> top;

			if (!Children(image, kHeadingBytes, image.size(), &top))
			{
				return false;
			}

			size_t at_Shape = top.size();
			int32_t highest = -1;

			for (size_t i = 0; i < top.size(); i++)
			{
				if (!NameIs(image, top[i].at, "REBSHAPE"))
				{
					continue;
				}

				std::vector<Span> inside;

				if (!Children(image, top[i].at + kHeadingBytes, top[i].at + top[i].size, &inside))
				{
					return false;
				}

				for (Span const &child : inside)
				{
					if (NameIs(image, child.at, "SHPHEAD1") && child.size >= kHeadingBytes + kShapeNumberAt + 4)
					{
						int32_t const number = IntAt(image, child.at + kHeadingBytes + kShapeNumberAt);

						highest = number > highest ? number : highest;

						if (number == oldNumber && at_Shape == top.size())
						{
							at_Shape = i;
						}
					}
				}
			}

			// the numbers the objects name too (an object's header holds its shape's number at 56): a level can name a shape that is not among its top-level shapes
			List<Object_Chunk *> objects;

			file->Level()->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Header_Chunk *const objectHeader = each() ? each()->get_header() : nullptr;
				Payload objectBytes;

				if (objectHeader && file->PayloadOf(objectHeader, &objectBytes) && objectBytes.size() >= 60)
				{
					int32_t number;

					memcpy(&number, &objectBytes[56], sizeof(number));
					highest = number > highest ? number : highest;
				}
			}

			// and the numbers the library gives every shape it holds (the game's file_id_num)
			List<Shape_Chunk *> shapes;

			file->Level()->list_shapes(&shapes);

			for (LIF<Shape_Chunk *> each(&shapes); !each.done(); each.next())
			{
				Shape_Header_Chunk *const shapeHeader = each() ? each()->get_header() : nullptr;

				Payload shapeBytes;

				if (shapeHeader && file->PayloadOf(shapeHeader, &shapeBytes) && shapeBytes.size() >= kShapeNumberAt + 4)
				{
					int32_t number;

					memcpy(&number, &shapeBytes[kShapeNumberAt], sizeof(number));
					highest = number > highest ? number : highest;
				}
			}

			highest = knownHighest > highest ? knownHighest : highest;

			if (at_Shape == top.size() || highest < 0 || highest >= 0x7ffffff0 || header->size() < 68)
			{
				return false;
			}

			*newNumber = highest + 1;
			memcpy(header->data() + kShapeNumberAt, newNumber, 4);
			memset(header->data() + 64, 0, 4);   // no users yet: the object made around the shape adds its name
			header->resize(68);

			// a FRESH header, as inteng's FUN_0050e880 makes for both copies (Merge on a shared shape, FUN_0055aa50; Split, FUN_00564300): version 0 (the save makes it 1) and no lock user.
			// Differences from inteng, both fixed: inteng's flags are 0, which loses the shape's own flags, so ours keeps them less the lock flag; and inteng never writes the lock
			// user, so ours clears it. The same rule as shape_merge_edit.cpp FreshHead (Merge Objects' copy). SHPHEAD1's payload: flags at 0, the lock user's 16 bytes at 4, the
			// version at 60 (avpx chunk_shape.cpp Shape_Header_Chunk::fill_data_block).
			int32_t flags = 0;
			int32_t const version = 0;

			memcpy(&flags, header->data() + kHeadFlagsAt, 4);
			flags &= ~kGeneralFlagLocked;
			memcpy(header->data() + kHeadFlagsAt, &flags, 4);
			memset(header->data() + kHeadLockUserAt, 0, kHeadLockUserBytes);
			memcpy(header->data() + kHeadVersionAt, &version, 4);

			std::vector<Span> inside;

			if (!Children(image, top[at_Shape].at + kHeadingBytes, top[at_Shape].at + top[at_Shape].size, &inside))
			{
				return false;
			}

			out->clear();
			AppendBytes(out, image, top[at_Shape].at, kHeadingBytes);

			bool done[12] = { false };

			for (Span const &child : inside)
			{
				size_t which = chunkCount;

				for (size_t n = 0; n < chunkCount; n++)
				{
					if (!done[n] && NameIs(image, child.at, chunks[n].name))
					{
						which = n;
						break;
					}
				}

				if (which == chunkCount && remap && NameIs(image, child.at, "SHPBONHD"))
				{
					if (!RemapBoneHeader(image, child, *remap, out))   // TM-C H1: Split's copy keeps its bones, through its own map (FUN_00564300 runs FUN_00435080 on the copy)
					{
						return false;
					}
				}
				else if (which == chunkCount)
				{
					AppendBytes(out, image, child.at, child.size);
				}
				else
				{
					done[which] = true;

					if (chunks[which].payload)
					{
						AppendChunk(out, chunks[which].name, *chunks[which].payload);
					}
				}
			}

			for (size_t n = 0; n < chunkCount; n++)
			{
				if (!done[n] && chunks[n].payload)
				{
					if (!chunks[n].addIfMissing)
					{
						return false;
					}

					AppendChunk(out, chunks[n].name, *chunks[n].payload);
				}
			}

			PutIntAt(out, 8, (int32_t)out->size());
			return true;
		}
	}

	char const *PolySplitWords(PolySplitResult result)
	{
		switch (result)
		{
		case PolySplitResult::Done: return "done";
		case PolySplitResult::BadCall: return "nothing to split off, or a polygon named twice";
		case PolySplitResult::NoObject: return "no object has that name";
		case PolySplitResult::NoShape: return "the object has no shape the editor can read";
		case PolySplitResult::NoPolygon: return "a polygon the shape does not have";
		case PolySplitResult::AllPolygons: return "Cannot remove all polygons from shape";
		case PolySplitResult::Unsupported: return "the shape has a chunk that polygons cannot be split out of yet";
		case PolySplitResult::Unreadable: return "a chunk of the shape could not be read or put together";
		case PolySplitResult::CantBuild: return "the level couldn't be put together from its bytes";
		case PolySplitResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		case PolySplitResult::OldNotCut: return "the new object was made, but the polygons could not be taken out of the old shape (they are in both)";
		case PolySplitResult::NotModule: return "the new object was made and the polygons taken out, but it could not be made a module (it stays a placed object)";
		case PolySplitResult::BadName: return "that name cannot be used (plain letters and digits up to 63, and no object of the level has it)";
		}

		return "unknown answer";
	}

	PolySplitResult PolyEdit_CopyShapeAsObject(OpenLevelFile *file, uint8_t const *id, char const *name, int32_t const *place, float const *turn, uint8_t *newId, std::string *newName, std::string *why)
	{
		if (!file || !id || !name || !*name || !place || !turn || !file->Level())
		{
			return PolySplitResult::BadCall;
		}

		File_Chunk *const level = file->Level();
		Object_Chunk *const object = ObjectWithId(level, id);
		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

		if (!object)
		{
			return PolySplitResult::NoObject;
		}

		model::ShapeModel model;

		if (!shape || ShapeEdit_Read(level, file, id, &model, nullptr, why) != ShapeEditResult::Done)
		{
			return PolySplitResult::NoShape;
		}

		// the copy's header: the shape's own, its list of users emptied (CreateObjectWithShapeChunk names the new object as its one user)
		Chunk *const headerChunk = shape->lookup_single_child("SHPHEAD1");
		Payload headerBytes;

		if (!headerChunk || !file->PayloadOf(headerChunk, &headerBytes) || headerBytes.size() < kUsersHeadBytes)
		{
			return PolySplitResult::Unreadable;
		}

		Payload copyHead = HeadWithUsers(headerBytes, std::vector<std::string>());

		// the highest shape number any object of the level reads (as Split: the library's own lists have missed some)
		int32_t knownHighest = -1;
		List<Object_Chunk *> everyObject;

		level->list_objects(&everyObject);

		for (LIF<Object_Chunk *> each(&everyObject); !each.done(); each.next())
		{
			model::ShapeModel probe;
			std::string skip;

			if (each() && each()->get_assoc_shape() && ShapeEdit_Read(level, file, (uint8_t const *)&each()->object_data.ID, &probe, nullptr, &skip) == ShapeEditResult::Done)
			{
				knownHighest = probe.number > knownHighest ? probe.number : knownHighest;
			}
		}

		// every other chunk of the shape (points, polygons, pictures, its external file's record) is copied as it is
		NewChunk const chunks[1] = { { "SHPHEAD1", &copyHead } };
		Image shapeChunk;
		int32_t newShapeNumber = 0;

		if (!CloneShapeChunk(file, model.number, chunks, 1, &copyHead, knownHighest, &newShapeNumber, &shapeChunk))
		{
			return PolySplitResult::CantBuild;
		}

		ObjectMakeResult const made = CreateObjectWithShapeChunk(file, shapeChunk, name, place, turn, newId, newName, true);

		// the maker's own words say what was wrong; only a name it can't use is a bad NAME
		switch (made)
		{
			case ObjectMakeResult::Done:
				return PolySplitResult::Done;
			case ObjectMakeResult::LoaderRefused:
				return PolySplitResult::LoaderRefused;
			case ObjectMakeResult::BadShapeName:
			case ObjectMakeResult::SubNameNotCopied:
			case ObjectMakeResult::NoNameFree:
				Say(why, ObjectMakeWords(made));
				return PolySplitResult::BadName;
			case ObjectMakeResult::PlaceOutOfRange:
				Say(why, ObjectMakeWords(made));
				return PolySplitResult::BadCall;
			default:
				Say(why, ObjectMakeWords(made));
				return PolySplitResult::CantBuild;
		}
	}

	PolySplitResult PolyEdit_SplitAsObject(OpenLevelFile *file, uint8_t const *id, int32_t const *polygons, size_t count, uint8_t *newId, std::string *newName, std::string *why, bool asModule, char const *name)
	{
		if (!file || !id || !polygons || count == 0 || !file->Level())
		{
			return PolySplitResult::BadCall;
		}

		File_Chunk *const level = file->Level();
		Object_Chunk *const object = ObjectWithId(level, id);

		if (!object)
		{
			return PolySplitResult::NoObject;
		}

		Shape_Chunk *const shape = object->get_assoc_shape();

		if (!shape)
		{
			return PolySplitResult::NoShape;
		}

		for (char const *const name : { "SHPMORPH", "ANIMSEQU", "SHPPCINF", "SHPFRAGS", "SHPFRGTP", "SHPEXTFL" })
		{
			if (shape->lookup_single_child(name))
			{
				Say(why, (std::string("the shape has a ") + name + " chunk").c_str());
				return PolySplitResult::Unsupported;
			}
		}

		model::ShapeModel model;

		size_t users = 0;

		if (ShapeEdit_Read(level, file, id, &model, &users, why) != ShapeEditResult::Done)
		{
			return PolySplitResult::NoShape;
		}

		if (users != 1)
		{
			Say(why, "another object uses the same shape");
			return PolySplitResult::Unsupported;
		}

		size_t const n = model.polygons.size();
		std::vector<char> inSplit(n, 0);

		for (size_t i = 0; i < count; i++)
		{
			if (polygons[i] < 0 || (size_t)polygons[i] >= n)
			{
				return PolySplitResult::NoPolygon;
			}

			if (inSplit[(size_t)polygons[i]])
			{
				return PolySplitResult::BadCall;
			}

			inSplit[(size_t)polygons[i]] = 1;
		}

		if (count >= n)
		{
			return PolySplitResult::AllPolygons;
		}

		std::vector<int32_t> rest;

		for (size_t i = 0; i < n; i++)
		{
			if (!inSplit[i])
			{
				rest.push_back((int32_t)i);
			}
		}

		// the chunks that follow the polygons, as PolyEdit_Remove reads them
		Chunk *const mergeChunk = shape->lookup_single_child("SHPMRGDT");
		Chunk *const uvChunk = shape->lookup_single_child("SHPUVCRD");
		Chunk *const centreChunk = shape->lookup_single_child("SHPCENTR");
		Chunk *const headerChunk = shape->lookup_single_child("SHPHEAD1");
		std::vector<int32_t> merge;
		std::vector<model::UvList> lists;
		Payload bytes, centreBytes, headerBytes;

		if (!headerChunk || !file->PayloadOf(headerChunk, &headerBytes))
		{
			return PolySplitResult::Unreadable;
		}

		if (mergeChunk)
		{
			if (!file->PayloadOf(mergeChunk, &bytes) || bytes.size() % 4 != 0 || bytes.size() / 4 != n)
			{
				Say(why, "the shape's merge data is not one number for each polygon");
				return PolySplitResult::Unreadable;
			}

			merge.resize(bytes.size() / 4);

			if (!bytes.empty())
			{
				memcpy(merge.data(), bytes.data(), bytes.size());
			}
		}

		if (uvChunk && (!file->PayloadOf(uvChunk, &bytes) || model::ReadUvLists(bytes.data(), bytes.size(), &lists) != model::UvModelResult::Ok))
		{
			Say(why, "the shape's list of texture coordinates could not be read");
			return PolySplitResult::Unreadable;
		}

		if (centreChunk && (!file->PayloadOf(centreChunk, &centreBytes) || centreBytes.size() != 16))
		{
			Say(why, "the shape's centre is not what the editor reads");
			return PolySplitResult::Unreadable;
		}

		int32_t animShape = 0;
		model::TextureAnim anim;

		if (AnimEdit_Read(file, id, &animShape, &anim) != AnimEditResult::Done)
		{
			return PolySplitResult::NoShape;
		}

		// the object's place and turn, so the new object lies where the polygons already are
		Object_Header_Chunk *const objectHeader = object->get_header();
		Payload objectBytes;

		if (!objectHeader || !file->PayloadOf(objectHeader, &objectBytes) || objectBytes.size() < 48)
		{
			return PolySplitResult::Unreadable;
		}

		int32_t place[3];
		float turn[4];

		memcpy(place, &objectBytes[20], sizeof(place));
		memcpy(turn, &objectBytes[32], sizeof(turn));

		// THE NEW SHAPE: the old one without the polygons that stay, its lists, merge data and animation following
		model::ShapeModel fresh = model;
		std::vector<int32_t> freshMerge = merge;
		PolyRemoveMap freshMap;
		std::set<int32_t> notWanted;

		for (int32_t const staying : rest)
		{
			if (model::EngineTypeIsTextured(model.polygons[(size_t)staying].engineType) && !(model.polygons[(size_t)staying].colour & 0x8000))
			{
				notWanted.insert(model::PolygonUvListNumber(model.polygons[(size_t)staying].colour));
			}
		}

		if (PolyEdit_RemoveInModel(&fresh, mergeChunk ? &freshMerge : nullptr, rest.data(), rest.size(), &freshMap) != PolyRemoveResult::Done)
		{
			return PolySplitResult::Unreadable;
		}

		std::vector<model::UvList> freshLists = lists;
		bool freshListsChanged = false;

		if (!DropFreedLists(&fresh, &freshLists, notWanted, &freshListsChanged, why))
		{
			return PolySplitResult::Unreadable;
		}

		model::TextureAnim freshAnim;

		for (model::AnimPolygon const &one : anim.polygons)
		{
			if (one.polygon >= 0 && (size_t)one.polygon < freshMap.polygon.size() && freshMap.polygon[(size_t)one.polygon] >= 0)
			{
				model::AnimPolygon moved = one;

				moved.polygon = freshMap.polygon[(size_t)one.polygon];
				freshAnim.polygons.push_back(moved);
			}
		}

		Payload fHeader = headerBytes, fPoints(fresh.points.size() * model::kShapePointBytes), fPolygons(fresh.polygons.size() * model::kShapePolygonBytes);
		Payload fPolyNormals(fresh.polygonNormals.size() * model::kShapeNormalBytes), fPointNormals(fresh.pointNormals.size() * model::kShapeNormalBytes);
		Payload fMerge(freshMerge.size() * 4), fUv, fAnim, fCentre = centreBytes;

		if (model::WriteShapePoints(fresh, &fPoints) != model::ShapeModelResult::Ok || model::WriteShapePolygons(fresh, &fPolygons) != model::ShapeModelResult::Ok ||
			model::WriteShapeNormals(fresh.polygonNormals, &fPolyNormals) != model::ShapeModelResult::Ok || model::WriteShapeNormals(fresh.pointNormals, &fPointNormals) != model::ShapeModelResult::Ok ||
			model::WriteShapeHeaderSettingCounts(fresh, &fHeader) != model::ShapeModelResult::Ok)
		{
			Say(why, "the new shape's numbers wouldn't go into chunks");
			return PolySplitResult::Unreadable;
		}

		if (!freshMerge.empty())
		{
			memcpy(fMerge.data(), freshMerge.data(), fMerge.size());
		}

		if (uvChunk)
		{
			fUv = UvBytes(freshLists);
		}

		if (!freshAnim.polygons.empty() && model::WriteTextureAnim(freshAnim, &fAnim) != model::TextureAnimResult::Ok)
		{
			Say(why, "the new shape's animation wouldn't go into its chunk");
			return PolySplitResult::Unreadable;
		}

		if (centreChunk)
		{
			int32_t const mid[3] =
			{
				(int32_t)(((int64_t)fresh.extents.maxX + (int64_t)fresh.extents.minX) / 2),
				(int32_t)(((int64_t)fresh.extents.maxY + (int64_t)fresh.extents.minY) / 2),
				(int32_t)(((int64_t)fresh.extents.maxZ + (int64_t)fresh.extents.minZ) / 2),
			};
			float const aboutCentre = model::ShapeRadiusAboutCentre(fresh);

			memcpy(fCentre.data(), mid, 12);
			memcpy(fCentre.data() + 12, &aboutCentre, 4);
		}

		NewChunk chunks[10];
		size_t used = 0;

		chunks[used++] = { "SHPHEAD1", &fHeader };
		chunks[used++] = { "SHPRAWVT", &fPoints };
		chunks[used++] = { "SHPPOLYS", &fPolygons };
		chunks[used++] = { "SHPPNORM", &fPolyNormals };

		if (!fresh.pointNormals.empty())
		{
			chunks[used++] = { "SHPVNORM", &fPointNormals };
		}

		if (mergeChunk)
		{
			chunks[used++] = { "SHPMRGDT", &fMerge };
		}

		if (uvChunk)
		{
			chunks[used++] = { "SHPUVCRD", &fUv };
		}

		if (shape->lookup_single_child("TEXTANIM"))
		{
			chunks[used++] = { "TEXTANIM", freshAnim.polygons.empty() ? nullptr : &fAnim };
		}

		if (centreChunk)
		{
			chunks[used++] = { "SHPCENTR", &fCentre };
		}

		Image shapeChunk;
		int32_t newShapeNumber = 0;

		// the highest shape number any object of the level reads (the library's own lists have missed some)
		int32_t knownHighest = -1;
		List<Object_Chunk *> everyObject;

		level->list_objects(&everyObject);

		for (LIF<Object_Chunk *> each(&everyObject); !each.done(); each.next())
		{
			model::ShapeModel probe;
			std::string skip;

			if (each() && each()->get_assoc_shape() && ShapeEdit_Read(level, file, (uint8_t const *)&each()->object_data.ID, &probe, nullptr, &skip) == ShapeEditResult::Done)
			{
				knownHighest = probe.number > knownHighest ? probe.number : knownHighest;
			}
		}

		PointRemap const freshRemap = { &freshMap.point, (int32_t)fresh.points.size() };   // TM-C H1: the new shape's bones, through its own map

		if (!CloneShapeChunk(file, model.number, chunks, used, &fHeader, knownHighest, &newShapeNumber, &shapeChunk, &freshRemap))
		{
			return PolySplitResult::CantBuild;
		}

		// the header payload was made before the number was known: the chunk was built from it after (it is passed by pointer), so the number in the chunk is the new one
		// THE NEW OBJECT, around that shape, at the old one's place and turn
		std::string made;
		uint8_t madeId[8] = { 0 };

		bool const named = name && *name;

		switch (CreateObjectWithShapeChunk(file, shapeChunk, named ? name : "Split", place, turn, madeId, &made, named))
		{
		case ObjectMakeResult::Done: break;
		case ObjectMakeResult::LoaderRefused: return PolySplitResult::LoaderRefused;
		case ObjectMakeResult::BadShapeName:
		case ObjectMakeResult::NoNameFree:
		case ObjectMakeResult::SubNameNotCopied: return PolySplitResult::BadName;
		default: return PolySplitResult::CantBuild;
		}

		if (newId)
		{
			memcpy(newId, madeId, 8);
		}

		if (newName)
		{
			*newName = made;
		}

		// THE OLD SHAPE loses the polygons
		std::string cutWhy;

		if (PolyEdit_Remove(file, id, polygons, count, nullptr, &cutWhy) != PolyRemoveResult::Done)
		{
			Say(why, cutWhy.c_str());
			return PolySplitResult::OldNotCut;
		}

		// AS MODULE (FUN_00564300: a split made As Placed gets the placed-object flag 0x400, the game modes 0x3800 and a placed object's strategy; As Module gets none of these): the new object is a module, which is
		// Change Object Type > Module's own effect (object_type_edit.h: the strategy goes, the placed flag goes, the module flag 0x200 is set). NAMED: inteng asks for the new object's name ("Spliting Objects - Please give
		// name for new object", Escape: the default); this uses the default name.
		if (asModule)
		{
			ObjectTypeCall call;
			ObjectTypeOutcome outcome = ObjectTypeOutcome::NoObject;

			memcpy(call.id, madeId, 8);
			call.deleteModuleData = false;

			if (ObjectType_Change(file, ObjectTypeWhat::Module, &call, 1, &outcome) != ObjectTypeResult::Done || outcome != ObjectTypeOutcome::Changed)
			{
				return PolySplitResult::NotModule;
			}
		}

		return PolySplitResult::Done;
	}

	std::string PolyEdit_NextFreeObjectName(OpenLevelFile *file, char const *base, std::vector<std::string> const &alsoTaken)
	{
		if (!file || !file->Level() || !base)
		{
			return std::string();
		}

		List<Object_Chunk *> objects;

		file->Level()->list_objects(&objects);

		for (int n = 0; n < 100000; n++)
		{
			std::string const candidate = std::string(base) + std::to_string(n);
			bool taken = std::find(alsoTaken.begin(), alsoTaken.end(), candidate) != alsoTaken.end();

			for (LIF<Object_Chunk *> each(&objects); !each.done() && !taken; each.next())
			{
				taken = each() && strncmp(each()->object_data.o_name, candidate.c_str(), sizeof(each()->object_data.o_name)) == 0;
			}

			if (!taken)
			{
				return candidate;
			}
		}

		return std::string();
	}
}
