// See shape_edit.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "shape_edit.h"

#include <string.h>

#include <vector>

#include "chunk_object.hpp"   // Object_Chunk, Shape_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk's children (Chunk_With_Children::lookup_single_child)
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: PayloadOf and SetPayload
#include "shape_maths.h"      // RecentreTarget, TranslateShape, RotateByTurn (Recentre)

namespace host
{
	namespace
	{
		typedef std::vector<unsigned char> Bytes;

		bool Say(std::string *why, char const *words)
		{
			if (why)
			{
				*why = words;
			}

			return false;
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

		// The shape's chunks, the first of each name as the game takes it. Null where there is none.
		struct ShapeChunks
		{
			Chunk *header;
			Chunk *points;
			Chunk *polygons;
			Chunk *polyNormals;
			Chunk *pointNormals;
		};

		ShapeChunks ChunksOf(Shape_Chunk *shape)
		{
			ShapeChunks chunks;

			chunks.header = shape->lookup_single_child("SHPHEAD1");
			chunks.points = shape->lookup_single_child("SHPRAWVT");
			chunks.polygons = shape->lookup_single_child("SHPPOLYS");
			chunks.polyNormals = shape->lookup_single_child("SHPPNORM");
			chunks.pointNormals = shape->lookup_single_child("SHPVNORM");
			return chunks;
		}

		// The payload of a chunk that may be absent: empty for none, false when the chunk is there but the store has no bytes for it
		bool PayloadOrNone(OpenLevelFile const *store, Chunk *chunk, Bytes *bytes)
		{
			bytes->clear();
			return !chunk || store->PayloadOf(chunk, bytes);
		}

		void Point(Bytes const &bytes, uint8_t const **at, size_t *size)
		{
			*at = bytes.empty() ? nullptr : bytes.data();
			*size = bytes.size();
		}
	}

	char const *ShapeEditWords(ShapeEditResult result)
	{
		switch (result)
		{
		case ShapeEditResult::Done: return "done";
		case ShapeEditResult::BadCall: return "nothing to read or change";
		case ShapeEditResult::NoObject: return "no object has that name";
		case ShapeEditResult::NoShape: return "the object has no shape the editor can read";
		case ShapeEditResult::NoBytes: return "the editor has no bytes for one of the shape's chunks";
		case ShapeEditResult::BadShape: return "the shape's numbers are not ones the editor takes";
		case ShapeEditResult::Refused: return "the shape's numbers wouldn't take the change";
		}

		return "unknown answer";
	}

	ShapeEditResult ShapeEdit_Read(File_Chunk *level, OpenLevelFile const *store, uint8_t const *id, model::ShapeModel *shape, size_t *users,
		std::string *why)
	{
		if (!level || !store || !id || !shape)
		{
			return ShapeEditResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(level, id);

		if (!object)
		{
			return ShapeEditResult::NoObject;
		}

		Shape_Chunk *const owned = object->get_assoc_shape();

		if (!owned)
		{
			return ShapeEditResult::NoShape;
		}

		ShapeChunks const chunks = ChunksOf(owned);

		if (!chunks.header || !chunks.points || !chunks.polygons)
		{
			return ShapeEditResult::NoShape;
		}

		Bytes header, points, polygons, polyNormals, pointNormals;

		if (!store->PayloadOf(chunks.header, &header) || !store->PayloadOf(chunks.points, &points) || !store->PayloadOf(chunks.polygons, &polygons) ||
			!PayloadOrNone(store, chunks.polyNormals, &polyNormals) || !PayloadOrNone(store, chunks.pointNormals, &pointNormals))
		{
			return ShapeEditResult::NoBytes;
		}

		model::ShapeBytes bytes = {};

		Point(header, &bytes.header, &bytes.headerSize);
		Point(points, &bytes.points, &bytes.pointsSize);
		Point(polygons, &bytes.polygons, &bytes.polygonsSize);
		Point(polyNormals, &bytes.polyNormals, &bytes.polyNormalsSize);
		Point(pointNormals, &bytes.ptNormals, &bytes.ptNormalsSize);

		model::ShapeModelResult const read = model::ReadShapeModel(bytes, shape);

		if (read != model::ShapeModelResult::Ok)
		{
			Say(why, model::ShapeModelWords(read));
			return ShapeEditResult::BadShape;
		}

		if (users)
		{
			*users = 0;

			List<Object_Chunk *> all;

			level->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
			{
				if (each() && each()->get_assoc_shape() == owned)
				{
					(*users)++;
				}
			}
		}

		return ShapeEditResult::Done;
	}

	ShapeEditResult ShapeEdit_WriteSameCounts(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, model::ShapeModel const &shape,
		std::string *why)
	{
		if (!store || !level || !id)
		{
			return ShapeEditResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(level, id);

		if (!object)
		{
			return ShapeEditResult::NoObject;
		}

		Shape_Chunk *const owned = object->get_assoc_shape();

		if (!owned)
		{
			return ShapeEditResult::NoShape;
		}

		ShapeChunks const chunks = ChunksOf(owned);

		if (!chunks.header || !chunks.points || !chunks.polygons)
		{
			return ShapeEditResult::NoShape;
		}

		Bytes header, points, polygons, polyNormals, pointNormals;

		if (!store->PayloadOf(chunks.header, &header) || !store->PayloadOf(chunks.points, &points) || !store->PayloadOf(chunks.polygons, &polygons) ||
			!PayloadOrNone(store, chunks.polyNormals, &polyNormals) || !PayloadOrNone(store, chunks.pointNormals, &pointNormals))
		{
			return ShapeEditResult::NoBytes;
		}

		// Every chunk's new bytes are made first; a refusal anywhere leaves the store as it was
		model::ShapeModelResult answer = model::WriteShapePoints(shape, &points);

		if (answer == model::ShapeModelResult::Ok)
		{
			answer = model::WriteShapePolygons(shape, &polygons);
		}

		if (answer == model::ShapeModelResult::Ok)
		{
			answer = model::WriteShapeHeader(shape, &header);
		}

		// A normals chunk the file has is written from the model's normals, which have to be as many as the chunk holds; one it hasn't is left
		if (answer == model::ShapeModelResult::Ok && chunks.polyNormals)
		{
			answer = model::WriteShapeNormals(shape.polygonNormals, &polyNormals);
		}

		if (answer == model::ShapeModelResult::Ok && chunks.pointNormals)
		{
			answer = model::WriteShapeNormals(shape.pointNormals, &pointNormals);
		}

		if (answer != model::ShapeModelResult::Ok)
		{
			Say(why, model::ShapeModelWords(answer));
			return ShapeEditResult::Refused;
		}

		if (!store->SetPayload(chunks.points, points) || !store->SetPayload(chunks.polygons, polygons) || !store->SetPayload(chunks.header, header) ||
			(chunks.polyNormals && !store->SetPayload(chunks.polyNormals, polyNormals)) ||
			(chunks.pointNormals && !store->SetPayload(chunks.pointNormals, pointNormals)))
		{
			Say(why, "the editor's bytes for the shape wouldn't take the change");
			return ShapeEditResult::Refused;
		}

		return ShapeEditResult::Done;
	}

	namespace
	{
		// Where things are in an OBJHEAD1 chunk's bytes after its heading (object_place_edit.h): the place, then the turn's four floats
		size_t const kHeaderPlaceAt = 20;
		size_t const kHeaderTurnAt = 32;
		int64_t const kFarthestPlace = 1 << 24;   // chunk.hpp:492, the game's loader's limit

		// One object whose place has to move: its header chunk and the new header bytes
		struct MovedUser
		{
			Chunk *header;
			Bytes bytes;
		};
	}

	bool ShapeEdit_HasMorph(File_Chunk *level, uint8_t const *id)
	{
		if (!level || !id)
		{
			return false;
		}

		Object_Chunk *const object = ObjectWithId(level, id);
		Shape_Chunk *const owned = object ? object->get_assoc_shape() : nullptr;

		return owned && owned->lookup_single_child("SHPMORPH");
	}

	// polygon < 0: the extents' top or bottom (command 0x14a / 0x14b); polygon >= 0: the centre of that file polygon (Texture Map, Recentre Shape 0xd6, FUN_00565bd0)
	static ShapeEditResult RecentreImpl(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, bool top, int32_t polygon, size_t *users, std::string *why)
	{
		if (!store || !level || !id)
		{
			return ShapeEditResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(level, id);

		if (!object)
		{
			return ShapeEditResult::NoObject;
		}

		Shape_Chunk *const owned = object->get_assoc_shape();

		if (!owned)
		{
			return ShapeEditResult::NoShape;
		}

		if (owned->lookup_single_child("SHPMORPH"))
		{
			Say(why, "a shape with a morph can't be re-centred (inteng refuses it too)");
			return ShapeEditResult::Refused;
		}

		model::ShapeModel shape;
		ShapeEditResult const read = ShapeEdit_Read(level, store, id, &shape, nullptr, why);

		if (read != ShapeEditResult::Done)
		{
			return read;
		}

		model::ShapePoint offset;

		model::ShapePoint target;

		if (polygon < 0)
		{
			target = model::RecentreTarget(shape.extents, top);
		}
		else
		{
			// FUN_00565bd0: the polygon's distinct corner points are added up and Q = ftol(sum / -n) (FUN_0042f9b0 divides, truncating toward 0): minus the average, so the polygon's centre goes to the origin.
			// n is the count of distinct corners (inteng's is 3, or 4 for a polygon merged from two: the file polygon of a merged quad holds the 4)
			if ((size_t)polygon >= shape.polygons.size())
			{
				Say(why, "there is no such polygon in the shape");
				return ShapeEditResult::Refused;
			}

			model::ShapePolygon const &poly = shape.polygons[(size_t)polygon];
			int distinct[8];
			int n = 0;
			double sum[3] = { 0, 0, 0 };

			for (int k = 0; k < poly.cornerCount && k < 8; k++)
			{
				int const number = poly.corners[k];
				bool again = false;

				for (int j = 0; j < n; j++)
				{
					again = again || distinct[j] == number;
				}

				if (again)
				{
					continue;
				}

				if (number < 0 || (size_t)number >= shape.points.size())
				{
					Say(why, "the polygon names a point the shape does not have");
					return ShapeEditResult::Refused;
				}

				distinct[n++] = number;
				sum[0] += shape.points[(size_t)number].x;
				sum[1] += shape.points[(size_t)number].y;
				sum[2] += shape.points[(size_t)number].z;
			}

			if (n < 1)
			{
				Say(why, "the polygon has no corners");
				return ShapeEditResult::Refused;
			}

			// target = the average (cut toward 0), so that RecentreOffsetFor's negation gives -average
			target.x = (int32_t)(sum[0] / n);
			target.y = (int32_t)(sum[1] / n);
			target.z = (int32_t)(sum[2] / n);
		}

		if (model::RecentreOffsetFor(target, &offset) != model::ShapeMathsResult::Ok ||
			model::TranslateShape(&shape, offset) != model::ShapeMathsResult::Ok)
		{
			Say(why, "the shape's numbers wouldn't take the move: one would leave the range the file can hold");
			return ShapeEditResult::Refused;
		}

		// Every object that uses this shape: its new place is made now, so a refusal anywhere leaves everything as it was
		std::vector<MovedUser> moved;
		List<Object_Chunk *> all;

		level->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			Object_Chunk *const user = each();

			if (!user || user->get_assoc_shape() != owned)
			{
				continue;
			}

			Object_Header_Chunk *const header = user->get_header();
			MovedUser entry;

			if (!header || !store->PayloadOf(header, &entry.bytes) || entry.bytes.size() < kHeaderTurnAt + 16)
			{
				return ShapeEditResult::NoBytes;
			}

			entry.header = header;

			float turn[4];
			int32_t place[3];

			memcpy(turn, &entry.bytes[kHeaderTurnAt], sizeof(turn));
			memcpy(place, &entry.bytes[kHeaderPlaceAt], sizeof(place));

			model::ShapePoint turned;

			if (model::RotateByTurn(turn, offset, &turned) != model::ShapeMathsResult::Ok)
			{
				Say(why, "an object's turn isn't a number the editor can use");
				return ShapeEditResult::Refused;
			}

			int64_t const now[3] = { (int64_t)place[0] - turned.x, (int64_t)place[1] - turned.y, (int64_t)place[2] - turned.z };

			for (int axis = 0; axis < 3; axis++)
			{
				if (now[axis] < -kFarthestPlace || now[axis] > kFarthestPlace)
				{
					Say(why, "an object would be put farther than 16777216 from the middle, which the game's loader refuses");
					return ShapeEditResult::Refused;
				}

				int32_t const value = (int32_t)now[axis];

				memcpy(&entry.bytes[kHeaderPlaceAt + 4 * (size_t)axis], &value, sizeof(value));
			}

			moved.push_back(entry);
		}

		// The shape's bytes first (its own checks are the model's), then each user's header
		ShapeEditResult const wrote = ShapeEdit_WriteSameCounts(store, level, id, shape, why);

		if (wrote != ShapeEditResult::Done)
		{
			return wrote;
		}

		for (size_t at = 0; at < moved.size(); at++)
		{
			if (!store->SetPayload(moved[at].header, moved[at].bytes))
			{
				Say(why, "the editor's bytes for an object's header wouldn't take the change");
				return ShapeEditResult::Refused;
			}
		}

		if (users)
		{
			*users = moved.size();
		}

		return ShapeEditResult::Done;
	}

	ShapeEditResult ShapeEdit_Recentre(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, bool top, size_t *users, std::string *why)
	{
		return RecentreImpl(store, level, id, top, -1, users, why);
	}

	ShapeEditResult ShapeEdit_TurnQuarter(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, std::string *why)
	{
		if (!store || !level || !id)
		{
			return ShapeEditResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(level, id);

		if (!object)
		{
			return ShapeEditResult::NoObject;
		}

		Shape_Chunk *const owned = object->get_assoc_shape();

		if (!owned)
		{
			return ShapeEditResult::NoShape;
		}

		if (owned->lookup_single_child("SHPMORPH"))
		{
			Say(why, "a shape with a morph isn't turned (its morph's frames would not turn with it)");
			return ShapeEditResult::Refused;
		}

		model::ShapeModel shape;
		ShapeEditResult const read = ShapeEdit_Read(level, store, id, &shape, nullptr, why);

		if (read != ShapeEditResult::Done)
		{
			return read;
		}

		// -z must be a number the file holds: only INT32_MIN has no negative (the file's own numbers are hostile)
		auto const negatable = [](int32_t value) { return value != INT32_MIN; };

		for (model::ShapePoint const &point : shape.points)
		{
			if (!negatable(point.z))
			{
				Say(why, "a point of the shape is too far out to turn");
				return ShapeEditResult::Refused;
			}
		}

		if (!negatable(shape.extents.minZ) || !negatable(shape.extents.maxZ))
		{
			Say(why, "the shape's extents are too far out to turn");
			return ShapeEditResult::Refused;
		}

		// (x, y, z) -> (-z, y, x): a quarter turn about y (0x447341-0x44738d for the points and point normals, 0x4473e4-0x44740d for the polygon normals)
		for (model::ShapePoint &point : shape.points)
		{
			int32_t const x = point.x;

			point.x = -point.z;
			point.z = x;
		}

		for (std::vector<model::ShapeNormal> *normals : { &shape.pointNormals, &shape.polygonNormals })
		{
			for (model::ShapeNormal &normal : *normals)
			{
				float const x = normal.x;

				normal.x = -normal.z;
				normal.z = x;
			}
		}

		model::ShapeExtents const was = shape.extents;

		shape.extents.maxX = -was.minZ;
		shape.extents.minX = -was.maxZ;
		shape.extents.maxZ = was.maxX;
		shape.extents.minZ = was.minX;

		return ShapeEdit_WriteSameCounts(store, level, id, shape, why);
	}

	ShapeEditResult ShapeEdit_RecentreOnPolygon(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, int32_t polygon, size_t *users, std::string *why)
	{
		return RecentreImpl(store, level, id, false, polygon < 0 ? INT32_MAX : polygon, users, why);
	}

	ShapeEditResult ShapeEdit_Recalculate(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, size_t *users, std::string *why)
	{
		if (!store || !level || !id)
		{
			return ShapeEditResult::BadCall;
		}

		model::ShapeModel shape;
		ShapeEditResult const read = ShapeEdit_Read(level, store, id, &shape, users, why);

		if (read != ShapeEditResult::Done)
		{
			return read;
		}

		model::ShapeMathsResult const done = model::RecalculateNormalsAndExtents(&shape);

		if (done != model::ShapeMathsResult::Ok)
		{
			Say(why, model::ShapeMathsWords(done));
			return ShapeEditResult::Refused;
		}

		shape.radius = model::ShapeRadiusOf(shape.points);   // the header's radius is the origin one
		return ShapeEdit_WriteSameCounts(store, level, id, shape, why);
	}

	ShapeEditResult ShapeEdit_FixTears(OpenLevelFile *store, File_Chunk *level, uint8_t const *movedId, uint8_t const *referenceId, int32_t tolerance,
		size_t *welded, std::string *why)
	{
		if (welded)
		{
			*welded = 0;
		}

		if (!store || !level || !movedId || !referenceId)
		{
			return ShapeEditResult::BadCall;
		}

		Object_Chunk *const movedObject = ObjectWithId(level, movedId);
		Object_Chunk *const referenceObject = ObjectWithId(level, referenceId);

		if (!movedObject || !referenceObject)
		{
			return ShapeEditResult::NoObject;
		}

		Shape_Chunk *const movedShape = movedObject->get_assoc_shape();
		Shape_Chunk *const referenceShape = referenceObject->get_assoc_shape();

		if (!movedShape || !referenceShape)
		{
			return ShapeEditResult::NoShape;
		}

		if (movedShape == referenceShape)
		{
			Say(why, "the two objects use the same shape, so there is nothing to join");
			return ShapeEditResult::Refused;
		}

		if (movedShape->lookup_single_child("SHPMORPH") || referenceShape->lookup_single_child("SHPMORPH"))
		{
			Say(why, "a shape with a morph can't be joined yet (its sub-shapes are not built)");
			return ShapeEditResult::Refused;
		}

		model::ShapeModel moved;
		model::ShapeModel reference;
		size_t movedUsers = 0;
		ShapeEditResult answer = ShapeEdit_Read(level, store, movedId, &moved, &movedUsers, why);

		if (answer != ShapeEditResult::Done)
		{
			return answer;
		}

		answer = ShapeEdit_Read(level, store, referenceId, &reference, nullptr, why);

		if (answer != ShapeEditResult::Done)
		{
			return answer;
		}

		// inteng gives a shape that other objects share its own copy before it welds; this does not make copies, so it refuses instead
		if (movedUsers != 1)
		{
			Say(why, "the shape to be moved is used by other objects too; inteng would copy it first, and the editor doesn't yet");
			return ShapeEditResult::Refused;
		}

		// Each object's place, the three ints after the header's heading (a weld goes by world places, translation only)
		model::ShapePoint places[2];
		Object_Chunk *const objects[2] = { movedObject, referenceObject };

		for (int which = 0; which < 2; which++)
		{
			Object_Header_Chunk *const header = objects[which]->get_header();
			Bytes bytes;

			if (!header || !store->PayloadOf(header, &bytes) || bytes.size() < kHeaderPlaceAt + 12)
			{
				return ShapeEditResult::NoBytes;
			}

			int32_t place[3];

			memcpy(place, &bytes[kHeaderPlaceAt], sizeof(place));
			places[which].x = place[0];
			places[which].y = place[1];
			places[which].z = place[2];
		}

		size_t count = 0;
		model::ShapeMathsResult done = model::WeldShapePoints(&moved, places[0], reference, places[1], tolerance, &count);

		// inteng rebuilds the moved shape after the weld (FUN_0050f570) and then runs Recalculate (FUN_0044baa0): extents, normals, radius
		if (done == model::ShapeMathsResult::Ok)
		{
			done = model::RecalculateNormalsAndExtents(&moved);
		}

		if (done != model::ShapeMathsResult::Ok)
		{
			Say(why, model::ShapeMathsWords(done));
			return ShapeEditResult::Refused;
		}

		moved.radius = model::ShapeRadiusOf(moved.points);
		answer = ShapeEdit_WriteSameCounts(store, level, movedId, moved, why);

		if (answer == ShapeEditResult::Done && welded)
		{
			*welded = count;
		}

		return answer;
	}
}
