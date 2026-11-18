// See uv_edit.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "uv_edit.h"

#include <string.h>

#include <vector>

#include "chunk_object.hpp"   // Object_Chunk, Shape_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk's children (Chunk_With_Children::lookup_single_child)
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: PayloadOf and SetPayload
#include "shape_edit.h"       // ShapeEdit_Read: the shape's polygons, checked by the shape model

namespace host
{
	namespace
	{
		typedef std::vector<unsigned char> Bytes;

		// The one game flag only the loader sets (chunk_shape.cpp:1051): a level with it is refused, so the editor never writes it
		int32_t const kFlagOnlyTheLoaderSets = 0x40000000;

		UvEditResult Fail(std::string *why, char const *words, UvEditResult result)
		{
			if (why)
			{
				*why = words;
			}

			return result;
		}

		// The object with this ID, the first in the level's own order, or null
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

		// Everything a read or a write of one polygon needs, all checked by the models
		struct Gathered
		{
			model::ShapeModel shape;
			std::vector<model::UvList> lists;
			std::vector<std::string> names;
			model::PolygonUv current;
			Chunk *polygonsChunk;
			Chunk *uvChunk;
			size_t users;     // polygons that use the current polygon's list
		};

		UvEditResult Gather(OpenLevelFile const *store, File_Chunk *level, uint8_t const *objectId, int32_t polygonNumber, bool onePolygon, Gathered *got, std::string *why)
		{
			if (!store || !level || !objectId || !got)
			{
				return UvEditResult::BadCall;
			}

			std::string shapeWhy;
			ShapeEditResult const read = ShapeEdit_Read(level, store, objectId, &got->shape, nullptr, &shapeWhy);

			switch (read)
			{
			case ShapeEditResult::Done: break;
			case ShapeEditResult::NoObject: return UvEditResult::NoObject;
			case ShapeEditResult::NoShape: return UvEditResult::NoShape;
			case ShapeEditResult::NoBytes: return UvEditResult::NoBytes;
			case ShapeEditResult::BadShape: return Fail(why, shapeWhy.empty() ? ShapeEditWords(read) : shapeWhy.c_str(), UvEditResult::BadShape);
			default: return UvEditResult::BadCall;
			}

			if (onePolygon && (polygonNumber < 0 || (size_t)polygonNumber >= got->shape.polygons.size()))
			{
				return UvEditResult::NoPolygon;
			}

			Object_Chunk *const object = ObjectWithId(level, objectId);
			Shape_Chunk *const owned = object ? object->get_assoc_shape() : nullptr;

			if (!owned)
			{
				return UvEditResult::NoShape;
			}

			got->polygonsChunk = owned->lookup_single_child("SHPPOLYS");
			got->uvChunk = owned->lookup_single_child("SHPUVCRD");

			Chunk *const namesChunk = owned->lookup_single_child("SHPTEXFN");

			if (!got->polygonsChunk)
			{
				return UvEditResult::NoShape;
			}

			// A shape with no SHPUVCRD has no lists: a textured polygon of it then fails below with "no such list"
			got->lists.clear();
			got->names.clear();

			if (got->uvChunk)
			{
				Bytes bytes;

				if (!store->PayloadOf(got->uvChunk, &bytes))
				{
					return UvEditResult::NoBytes;
				}

				model::UvModelResult const answer = model::ReadUvLists(bytes.empty() ? nullptr : bytes.data(), bytes.size(), &got->lists);

				if (answer != model::UvModelResult::Ok)
				{
					return Fail(why, model::UvModelWords(answer), UvEditResult::BadUv);
				}
			}

			if (namesChunk)
			{
				Bytes bytes;

				if (!store->PayloadOf(namesChunk, &bytes))
				{
					return UvEditResult::NoBytes;
				}

				model::UvModelResult const answer = model::ReadTextureNames(bytes.empty() ? nullptr : bytes.data(), bytes.size(), &got->names);

				if (answer != model::UvModelResult::Ok)
				{
					return Fail(why, model::UvModelWords(answer), UvEditResult::BadUv);
				}
			}

			if (!onePolygon)
			{
				return UvEditResult::Done;
			}

			model::UvModelResult const polygon = model::ReadPolygonUv(got->shape, got->lists, polygonNumber, &got->current);

			if (polygon != model::UvModelResult::Ok)
			{
				return Fail(why, model::UvModelWords(polygon), UvEditResult::BadUv);
			}

			got->users = 0;

			if (got->current.textured)
			{
				got->users = model::PolygonsUsingList(got->shape, got->current.uvListNumber).size();

				if (got->current.pictureNumber >= 0 && (size_t)got->current.pictureNumber < got->names.size())
				{
					got->current.pictureName = got->names[(size_t)got->current.pictureNumber];
				}
			}

			return UvEditResult::Done;
		}

		bool SameCorners(model::UvList const &a, model::UvList const &b)
		{
			return a.cornerCount == b.cornerCount && memcmp(a.corners, b.corners, (size_t)a.cornerCount * sizeof(model::UvCorner)) == 0;
		}
	}

	char const *UvEditWords(UvEditResult result)
	{
		switch (result)
		{
		case UvEditResult::Done: return "done";
		case UvEditResult::BadCall: return "nothing to read or change";
		case UvEditResult::NoObject: return "no object has that name";
		case UvEditResult::NoShape: return "the object has no shape the editor can read";
		case UvEditResult::NoBytes: return "the editor has no bytes for one of the shape's chunks";
		case UvEditResult::BadShape: return "the shape's numbers are not ones the editor takes";
		case UvEditResult::BadUv: return "the texture coordinates are not ones the editor takes";
		case UvEditResult::NoPolygon: return "the shape has no such polygon";
		case UvEditResult::Shared: return "other polygons use the same texture list, so it is not changed here";
		case UvEditResult::Refused: return "the change is not one the editor makes, or the bytes wouldn't take it";
		}

		return "unknown answer";
	}

	UvEditResult UvEdit_ReadPolygon(OpenLevelFile const *store, File_Chunk *level, uint8_t const *objectId, int32_t polygonNumber, model::PolygonUv *out,
		std::string *why, size_t *listUsers)
	{
		if (!out)
		{
			return UvEditResult::BadCall;
		}

		Gathered got = {};
		UvEditResult const answer = Gather(store, level, objectId, polygonNumber, true, &got, why);

		if (answer != UvEditResult::Done)
		{
			return answer;
		}

		*out = got.current;

		if (listUsers)
		{
			*listUsers = got.users;
		}

		return UvEditResult::Done;
	}

	UvEditResult UvEdit_ReadAllPolygons(OpenLevelFile const *store, File_Chunk *level, uint8_t const *objectId, std::vector<model::PolygonUv> *out, std::string *why)
	{
		if (!out)
		{
			return UvEditResult::BadCall;
		}

		out->clear();

		Gathered got = {};
		UvEditResult const answer = Gather(store, level, objectId, 0, false, &got, why);

		if (answer != UvEditResult::Done)
		{
			return answer;
		}

		out->resize(got.shape.polygons.size());

		for (size_t p = 0; p < got.shape.polygons.size(); p++)
		{
			model::PolygonUv &one = (*out)[p];

			if (model::ReadPolygonUv(got.shape, got.lists, (int32_t)p, &one) != model::UvModelResult::Ok)
			{
				// one the rules refuse is given as not textured, so nothing is written for it
				one = {};
				one.polygonNumber = (int32_t)p;
				one.pictureNumber = -1;
				one.uvListNumber = -1;
				one.textured = false;
				continue;
			}

			if (one.textured && one.pictureNumber >= 0 && (size_t)one.pictureNumber < got.names.size())
			{
				one.pictureName = got.names[(size_t)one.pictureNumber];
			}
		}

		return UvEditResult::Done;
	}

	UvEditResult UvEdit_WritePolygons(OpenLevelFile *store, File_Chunk *level, uint8_t const *objectId, std::vector<model::PolygonUv> const &polygons, std::string *why)
	{
		if (polygons.empty())
		{
			return UvEditResult::Done;
		}

		// The shape, its lists and its names are read once; each polygon is then checked against what the file holds NOW
		Gathered got = {};
		UvEditResult const answer = Gather(store, level, objectId, 0, false, &got, why);

		if (answer != UvEditResult::Done)
		{
			return answer;
		}

		std::vector<bool> named(got.shape.polygons.size(), false);
		bool uvChanged = false;
		bool flagsChanged = false;

		for (model::PolygonUv const &polygon : polygons)
		{
			if (polygon.polygonNumber < 0 || (size_t)polygon.polygonNumber >= got.shape.polygons.size())
			{
				return UvEditResult::NoPolygon;
			}

			if (named[(size_t)polygon.polygonNumber])
			{
				return Fail(why, "the same polygon is named twice in one write", UvEditResult::Refused);
			}

			named[(size_t)polygon.polygonNumber] = true;

			// What the file holds for this polygon now (the lists and shape are changed as the loop goes, so a later polygon sees the earlier ones)
			model::PolygonUv now;
			model::UvModelResult const read = model::ReadPolygonUv(got.shape, got.lists, polygon.polygonNumber, &now);

			if (read != model::UvModelResult::Ok)
			{
				return Fail(why, model::UvModelWords(read), UvEditResult::BadUv);
			}

			// What the file's polygon is stays: the editor changes its coordinates and its flags only
			if (!now.textured || !polygon.textured)
			{
				return Fail(why, "a polygon with no texture gets no coordinates here: giving it a texture is not built yet", UvEditResult::Refused);
			}

			if (polygon.engineType != now.engineType || polygon.cornerCount != now.cornerCount || polygon.uvListNumber != now.uvListNumber)
			{
				return Fail(why, "the polygon's type, corners and texture list are not changed here", UvEditResult::Refused);
			}

			if (polygon.pictureNumber != now.pictureNumber)
			{
				return Fail(why, "the polygon's picture is not changed here yet: inteng's set-texture command is not read", UvEditResult::Refused);
			}

			if (polygon.flags & kFlagOnlyTheLoaderSets)
			{
				return Fail(why, "the flag word has a bit only the game's loader sets", UvEditResult::Refused);
			}

			if (!SameCorners(polygon.uv, now.uv))
			{
				// A list that another polygon uses is only changed when every user of it is named in this same write (and then each names it the same way)
				for (int32_t user : model::PolygonsUsingList(got.shape, polygon.uvListNumber))
				{
					if (user != polygon.polygonNumber)
					{
						bool inBatch = false;

						for (model::PolygonUv const &other : polygons)
						{
							if (other.polygonNumber == user && other.textured && SameCorners(other.uv, polygon.uv))
							{
								inBatch = true;
							}
						}

						if (!inBatch)
						{
							return Fail(why, UvEditWords(UvEditResult::Shared), UvEditResult::Shared);
						}
					}
				}

				model::UvModelResult const put = model::PutPolygonUv(polygon, &got.lists);

				if (put != model::UvModelResult::Ok)
				{
					return Fail(why, model::UvModelWords(put), UvEditResult::BadUv);
				}

				uvChanged = true;
			}

			if (polygon.flags != now.flags)
			{
				got.shape.polygons[(size_t)polygon.polygonNumber].flags = polygon.flags;
				flagsChanged = true;
			}
		}

		if (!uvChanged && !flagsChanged)
		{
			return UvEditResult::Done;
		}

		// Everything is made and checked first; a refusal anywhere leaves the store as it was
		Bytes uvBytes, polygonBytes;

		if (uvChanged)
		{
			if (!store->PayloadOf(got.uvChunk, &uvBytes))
			{
				return UvEditResult::NoBytes;
			}

			model::UvModelResult const put = model::WriteUvListsSameShape(got.lists, &uvBytes);

			if (put != model::UvModelResult::Ok)
			{
				return Fail(why, model::UvModelWords(put), UvEditResult::BadUv);
			}
		}

		if (flagsChanged)
		{
			if (!store->PayloadOf(got.polygonsChunk, &polygonBytes))
			{
				return UvEditResult::NoBytes;
			}

			model::ShapeModelResult const put = model::WriteShapePolygons(got.shape, &polygonBytes);

			if (put != model::ShapeModelResult::Ok)
			{
				return Fail(why, model::ShapeModelWords(put), UvEditResult::Refused);
			}
		}

		if ((uvChanged && !store->SetPayload(got.uvChunk, uvBytes)) || (flagsChanged && !store->SetPayload(got.polygonsChunk, polygonBytes)))
		{
			return Fail(why, "the editor's bytes for the shape wouldn't take the change", UvEditResult::Refused);
		}

		return UvEditResult::Done;
	}

	UvEditResult UvEdit_WritePolygon(OpenLevelFile *store, File_Chunk *level, uint8_t const *objectId, model::PolygonUv const &polygon, std::string *why)
	{
		std::vector<model::PolygonUv> one(1, polygon);

		return UvEdit_WritePolygons(store, level, objectId, one, why);
	}
}
