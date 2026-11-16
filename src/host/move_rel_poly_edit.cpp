// See move_rel_poly_edit.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "move_rel_poly_edit.h"

#include <math.h>
#include <string.h>

#include <vector>

#include "chunk_object.hpp"   // Object_Chunk, Shape_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: PayloadOf and SetPayload
#include "file_pick.h"
#include "move_rel_poly.h"
#include "shape_edit.h"       // ShapeEdit_Read
#include "shape_model.h"

namespace host
{
	namespace
	{
		size_t const kHeaderPlaceAt = 20;
		size_t const kHeaderTurnAt = 32;
		int64_t const kFarthestPlace = 1 << 24;   // chunk.hpp:492, the game's loader's limit

		bool NotANumber(double value)
		{
			uint64_t bits;

			memcpy(&bits, &value, sizeof(bits));
			return (bits & 0x7FF0000000000000ull) == 0x7FF0000000000000ull;   // a NaN or an infinity, by its bits: the build is /fp:fast
		}

		// The float to int of FISTP: to nearest, ties to even
		bool RoundToInt(double value, int32_t *out)
		{
			if (NotANumber(value))
			{
				return false;
			}

			double const rounded = nearbyint(value);

			if (rounded > 2147483647.0 || rounded < -2147483648.0)
			{
				return false;
			}

			*out = (int32_t)rounded;
			return true;
		}

		Object_Chunk *ObjectWithId(File_Chunk *level, uint8_t const *id)
		{
			List<Object_Chunk *> found;

			level->list_objects(&found);

			for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
			{
				if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
				{
					return each();
				}
			}

			return nullptr;
		}

		// An object's header bytes, with its place and turn
		struct Header
		{
			Chunk *chunk;
			std::vector<unsigned char> bytes;
			int32_t place[3];
			float turn[4];
		};

		bool ReadHeader(OpenLevelFile const *store, Object_Chunk *object, Header *header)
		{
			header->chunk = object->get_header();

			if (!header->chunk || !store->PayloadOf(header->chunk, &header->bytes) || header->bytes.size() < kHeaderTurnAt + 16)
			{
				return false;
			}

			memcpy(header->place, header->bytes.data() + kHeaderPlaceAt, sizeof(header->place));
			memcpy(header->turn, header->bytes.data() + kHeaderTurnAt, sizeof(header->turn));

			for (int i = 0; i < 4; i++)
			{
				if (NotANumber(header->turn[i]))
				{
					return false;
				}
			}

			return true;
		}

		int32_t Mul(int32_t a, int32_t b)
		{
			return MoveRelPoly_Multiply(a, b);
		}
	}

	void MoveRelPolyEdit_TurnToMatrix(float const turn[4], int32_t m[9])
	{
		// the int quaternion (w, x, y, z) of the conjugate of the file's (x, y, z, w): see the header
		double const parts[4] = { turn[3], -(double)turn[0], -(double)turn[1], -(double)turn[2] };
		int32_t q[4] = { 0, 0, 0, 0 };

		for (int i = 0; i < 4; i++)
		{
			if (!RoundToInt(parts[i] * 65536.0, &q[i]))
			{
				q[i] = 0;
			}
		}

		int32_t const w = q[0], x = q[1], y = q[2], z = q[3];

		// FUN_004b3c80 (menu-reads-33.txt, p11 section 46): each product is the 16.16 multiply of the DOUBLED first factor and the second
		m[0] = (0x10000 - Mul(2 * z, z)) - Mul(2 * y, y);
		m[1] = Mul(2 * x, y) - Mul(2 * z, w);
		m[2] = Mul(2 * x, z) + Mul(2 * y, w);
		m[3] = Mul(2 * x, y) + Mul(2 * z, w);
		m[4] = (0x10000 - Mul(2 * z, z)) - Mul(2 * x, x);
		m[5] = Mul(2 * y, z) - Mul(2 * x, w);
		m[6] = Mul(2 * x, z) - Mul(2 * y, w);
		m[7] = Mul(2 * y, z) + Mul(2 * x, w);
		m[8] = (0x10000 - Mul(2 * y, y)) - Mul(2 * x, x);
	}

	void MoveRelPolyEdit_MatrixToTurn(int32_t const m[9], float turn[4])
	{
		// FUN_004de070 (menu-reads-12.txt:2592-2680): the nine ints times 1/65536 into doubles
		double a[3][3];

		for (int r = 0; r < 3; r++)
		{
			for (int c = 0; c < 3; c++)
			{
				a[r][c] = (double)m[r * 3 + c] * (1.0 / 65536.0);
			}
		}

		double q[4];
		double const trace = a[2][2] + a[1][1] + a[0][0];

		if (trace <= 0.0)
		{
			int const next[3] = { 1, 2, 0 };
			int i = a[0][0] < a[1][1] ? 1 : 0;

			if (a[i][i] < a[2][2])
			{
				i = 2;
			}

			int const j = next[i];
			int const k = next[j];
			double const s = sqrt((a[i][i] - (a[k][k] + a[j][j])) + 1.0);

			q[i] = s * 0.5;

			double const f = 0.5 / s;

			q[3] = (a[j][k] - a[k][j]) * f;
			q[j] = (a[j][i] + a[i][j]) * f;
			q[k] = (a[k][i] + a[i][k]) * f;
		}
		else
		{
			double const s = sqrt(trace + 1.0);

			q[3] = s * 0.5;

			double const f = 0.5 / s;

			q[0] = (a[1][2] - a[2][1]) * f;
			q[1] = (a[2][0] - a[0][2]) * f;
			q[2] = (a[0][1] - a[1][0]) * f;
		}

		for (int i = 0; i < 4; i++)
		{
			turn[i] = (float)q[i];
		}
	}

	char const *MoveRelPolyEdit_Words(MoveRelPolyEditResult result)
	{
		switch (result)
		{
		case MoveRelPolyEditResult::Done:
			return "moved";
		case MoveRelPolyEditResult::BadCall:
			return "nothing to move or no polygon given";
		case MoveRelPolyEditResult::SameObject:
			return "that is the object's own polygon";
		case MoveRelPolyEditResult::NoObject:
			return "an object isn't in the level";
		case MoveRelPolyEditResult::NoShape:
			return "the polygon's object has no shape to read";
		case MoveRelPolyEditResult::BadPolygon:
			return "that polygon isn't in the shape";
		case MoveRelPolyEditResult::TooFar:
			return "the place would be too far out";
		case MoveRelPolyEditResult::Refused:
			return "the level wouldn't take the new place";
		}

		return "unknown answer";
	}

	bool MoveRelPolyEdit_ObjectNamed(File_Chunk *level, char const *name, uint8_t id[8])
	{
		if (!level || !name || !name[0] || !id)
		{
			return false;
		}

		List<Object_Chunk *> found;

		level->list_objects(&found);

		for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			// A module of the level is an object that is not a PLACED one (projload.cpp: the loader makes a module of every object without OBJECT_FLAG_PLACED_OBJECT), and a placed
			// object can carry the same name (a door and its room), so the placed ones are never taken for the module
			if (object && object->object_data.o_name && object->get_header() && !(object->get_header()->flags & OBJECT_FLAG_PLACED_OBJECT) && _stricmp(object->object_data.o_name, name) == 0)
			{
				memcpy(id, &object->object_data.ID, 8);
				return true;
			}
		}

		return false;
	}

	MoveRelPolyEditResult MoveRelPolyEdit_Do(OpenLevelFile *store, File_Chunk *level, uint8_t const *movingId, uint8_t const *targetId, int32_t polygon, int32_t distance,
		int32_t const *hitFile, double scale, std::string *why)
	{
		if (!store || !level || !movingId || !targetId || NotANumber(scale) || !(scale > 0.0))
		{
			return MoveRelPolyEditResult::BadCall;
		}

		if (memcmp(movingId, targetId, 8) == 0)
		{
			return MoveRelPolyEditResult::SameObject;
		}

		Object_Chunk *const moving = ObjectWithId(level, movingId);
		Object_Chunk *const target = ObjectWithId(level, targetId);

		if (!moving || !target)
		{
			return MoveRelPolyEditResult::NoObject;
		}

		model::ShapeModel shape;
		std::string unused;

		if (ShapeEdit_Read(level, store, targetId, &shape, nullptr, why ? why : &unused) != ShapeEditResult::Done)
		{
			return MoveRelPolyEditResult::NoShape;
		}

		if (polygon < 0 || (size_t)polygon >= shape.polygons.size())
		{
			return MoveRelPolyEditResult::BadPolygon;
		}

		Header movingHeader, targetHeader;

		if (!ReadHeader(store, moving, &movingHeader) || !ReadHeader(store, target, &targetHeader))
		{
			return MoveRelPolyEditResult::NoShape;
		}

		double const unit = 100.0 * scale;   // file units to inteng's
		model::ShapePolygon const &poly = shape.polygons[(size_t)polygon];

		MoveRelPolyTarget sums;

		memset(&sums, 0, sizeof(sums));

		// the corners: the polygon's own, without repeats (a file polygon may name a point twice)
		for (int c = 0; c < poly.cornerCount; c++)
		{
			int32_t const number = poly.corners[c];
			bool again = false;

			if (number < 0 || (size_t)number >= shape.points.size())
			{
				return MoveRelPolyEditResult::BadPolygon;
			}

			for (int p = 0; p < c; p++)
			{
				again = again || poly.corners[p] == number;
			}

			if (again || sums.cornerCount >= kMoveRelPolyMostCorners)
			{
				continue;
			}

			model::ShapePoint const point = shape.points[(size_t)number];
			double const file[3] = { (double)point.x, (double)point.y, (double)point.z };

			for (int axis = 0; axis < 3; axis++)
			{
				if (!RoundToInt(file[axis] * unit, &sums.corners[sums.cornerCount][axis]))
				{
					return MoveRelPolyEditResult::TooFar;
				}
			}

			sums.cornerCount++;
		}

		if (sums.cornerCount < 1)
		{
			return MoveRelPolyEditResult::BadPolygon;
		}

		// the polygon's normal as inteng holds it: the shape's float normal times 65536, cut toward 0
		if (poly.normalIndex < 0 || (size_t)poly.normalIndex >= shape.polygonNormals.size())
		{
			return MoveRelPolyEditResult::BadPolygon;
		}

		model::ShapeNormal const &own = shape.polygonNormals[(size_t)poly.normalIndex];
		double const parts[3] = { (double)own.x * 65536.0, (double)own.y * 65536.0, (double)own.z * 65536.0 };

		for (int axis = 0; axis < 3; axis++)
		{
			if (NotANumber(parts[axis]) || fabs(parts[axis]) > 2147483647.0)
			{
				return MoveRelPolyEditResult::BadPolygon;
			}

			sums.normal[axis] = (int32_t)parts[axis];
		}

		MoveRelPolyEdit_TurnToMatrix(targetHeader.turn, sums.matrix);

		for (int axis = 0; axis < 3; axis++)
		{
			if (!RoundToInt((double)targetHeader.place[axis] * unit, &sums.place[axis]))
			{
				return MoveRelPolyEditResult::TooFar;
			}
		}

		int32_t hit[3] = { 0, 0, 0 };

		if (hitFile)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				if (!RoundToInt((double)hitFile[axis] * unit, &hit[axis]))
				{
					return MoveRelPolyEditResult::TooFar;
				}
			}
		}

		MoveRelPolyResult result;

		if (!MoveRelPoly_Make(sums, distance, hitFile ? hit : nullptr, &result))
		{
			return MoveRelPolyEditResult::BadPolygon;
		}

		// back to the file: the place cut toward 0, the matrix to four floats
		int32_t newPlace[3];

		for (int axis = 0; axis < 3; axis++)
		{
			double const file = (double)result.place[axis] / unit;

			if (NotANumber(file) || fabs(file) >= (double)kFarthestPlace)
			{
				return MoveRelPolyEditResult::TooFar;
			}

			newPlace[axis] = (int32_t)file;
		}

		float newTurn[4];

		MoveRelPolyEdit_MatrixToTurn(result.matrix, newTurn);

		for (int i = 0; i < 4; i++)
		{
			if (NotANumber(newTurn[i]))
			{
				return MoveRelPolyEditResult::TooFar;
			}
		}

		memcpy(movingHeader.bytes.data() + kHeaderPlaceAt, newPlace, sizeof(newPlace));
		memcpy(movingHeader.bytes.data() + kHeaderTurnAt, newTurn, sizeof(newTurn));

		if (!store->SetPayload(movingHeader.chunk, movingHeader.bytes))
		{
			return MoveRelPolyEditResult::Refused;
		}

		return MoveRelPolyEditResult::Done;
	}

	namespace
	{
		// the world point of the shape nearest to p (file units, squared distance in doubles); false with no points
		bool NearestWorldPoint(std::vector<int32_t> const &points, double const p[3], int32_t out[3])
		{
			double best = 0.0;
			bool any = false;

			for (size_t i = 0; i + 2 < points.size(); i += 3)
			{
				double const dx = (double)points[i] - p[0], dy = (double)points[i + 1] - p[1], dz = (double)points[i + 2] - p[2];
				double const d = dx * dx + dy * dy + dz * dz;

				if (!any || d < best)
				{
					any = true;
					best = d;
					out[0] = points[i];
					out[1] = points[i + 1];
					out[2] = points[i + 2];
				}
			}

			return any;
		}
	}

	MoveRelPolyEditResult SnapToVertex_Target(OpenLevelFile *store, File_Chunk *level, uint8_t const *targetId, double const point[3], int32_t vertex[3])
	{
		if (!store || !level || !targetId || !point || !vertex)
		{
			return MoveRelPolyEditResult::BadCall;
		}

		model::ShapeModel shape;
		std::vector<int32_t> points, normals;
		FilePickResult const read = FilePick_World(store, level, targetId, &shape, &points, &normals);

		if (read == FilePickResult::NoObject)
		{
			return MoveRelPolyEditResult::NoObject;
		}

		if (read != FilePickResult::Done || !NearestWorldPoint(points, point, vertex))
		{
			return MoveRelPolyEditResult::NoShape;
		}

		return MoveRelPolyEditResult::Done;
	}

	MoveRelPolyEditResult SnapToVertex_MoveObject(OpenLevelFile *store, File_Chunk *level, uint8_t const *movingId, int32_t const vertex[3])
	{
		if (!store || !level || !movingId || !vertex)
		{
			return MoveRelPolyEditResult::BadCall;
		}

		Object_Chunk *const moving = ObjectWithId(level, movingId);

		if (!moving)
		{
			return MoveRelPolyEditResult::NoObject;
		}

		model::ShapeModel shape;
		std::vector<int32_t> points, normals;

		if (FilePick_World(store, level, movingId, &shape, &points, &normals) != FilePickResult::Done)
		{
			return MoveRelPolyEditResult::NoShape;
		}

		double const target[3] = { (double)vertex[0], (double)vertex[1], (double)vertex[2] };
		int32_t own[3];

		if (!NearestWorldPoint(points, target, own))
		{
			return MoveRelPolyEditResult::NoShape;
		}

		Header header;

		if (!ReadHeader(store, moving, &header))
		{
			return MoveRelPolyEditResult::NoShape;
		}

		int32_t place[3];

		memcpy(place, header.bytes.data() + kHeaderPlaceAt, sizeof(place));

		for (int axis = 0; axis < 3; axis++)
		{
			double const moved = (double)place[axis] + ((double)vertex[axis] - (double)own[axis]);

			if (NotANumber(moved) || fabs(moved) >= (double)kFarthestPlace)
			{
				return MoveRelPolyEditResult::TooFar;
			}

			place[axis] = (int32_t)moved;
		}

		memcpy(header.bytes.data() + kHeaderPlaceAt, place, sizeof(place));

		if (!store->SetPayload(header.chunk, header.bytes))
		{
			return MoveRelPolyEditResult::Refused;
		}

		return MoveRelPolyEditResult::Done;
	}

	MoveRelPolyEditResult ObjectPlace_Get(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, int32_t place[3])
	{
		if (!store || !level || !id || !place)
		{
			return MoveRelPolyEditResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(level, id);
		Header header;

		if (!object)
		{
			return MoveRelPolyEditResult::NoObject;
		}

		if (!ReadHeader(store, object, &header))
		{
			return MoveRelPolyEditResult::NoShape;
		}

		memcpy(place, header.bytes.data() + kHeaderPlaceAt, 3 * sizeof(int32_t));
		return MoveRelPolyEditResult::Done;
	}

	MoveRelPolyEditResult ObjectPlace_Set(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, int32_t const place[3])
	{
		if (!store || !level || !id || !place)
		{
			return MoveRelPolyEditResult::BadCall;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			if (place[axis] <= -kFarthestPlace || place[axis] >= kFarthestPlace)
			{
				return MoveRelPolyEditResult::TooFar;
			}
		}

		Object_Chunk *const object = ObjectWithId(level, id);
		Header header;

		if (!object)
		{
			return MoveRelPolyEditResult::NoObject;
		}

		if (!ReadHeader(store, object, &header))
		{
			return MoveRelPolyEditResult::NoShape;
		}

		memcpy(header.bytes.data() + kHeaderPlaceAt, place, 3 * sizeof(int32_t));
		return store->SetPayload(header.chunk, header.bytes) ? MoveRelPolyEditResult::Done : MoveRelPolyEditResult::Refused;
	}
}
