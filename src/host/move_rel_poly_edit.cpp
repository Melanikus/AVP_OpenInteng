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
#include "shape_maths.h"      // RotateByTurn
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

	MoveRelPolyEditResult MoveRelPolyEdit_Pose(OpenLevelFile *store, File_Chunk *level, uint8_t const *targetId, int32_t polygon, int32_t distance, int32_t const *hitFile, double scale,
		int32_t place[3], int32_t matrix[9], std::string *why)
	{
		if (!store || !level || !targetId || !place || !matrix || NotANumber(scale) || !(scale > 0.0))
		{
			return MoveRelPolyEditResult::BadCall;
		}

		Object_Chunk *const target = ObjectWithId(level, targetId);

		if (!target)
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

		Header targetHeader;

		if (!ReadHeader(store, target, &targetHeader))
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

		memcpy(place, result.place, sizeof(result.place));
		memcpy(matrix, result.matrix, sizeof(result.matrix));
		return MoveRelPolyEditResult::Done;
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

		Header movingHeader;

		if (!ReadHeader(store, moving, &movingHeader))
		{
			return MoveRelPolyEditResult::NoShape;
		}

		double const unit = 100.0 * scale;   // file units to inteng's
		MoveRelPolyResult result;
		MoveRelPolyEditResult const made = MoveRelPolyEdit_Pose(store, level, targetId, polygon, distance, hitFile, scale, result.place, result.matrix, why);

		if (made != MoveRelPolyEditResult::Done)
		{
			return made;
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

	namespace
	{
		// ((v < 1) - 1 & 0x20000) - 0x10000 (FUN_0053d1e0): +65536 for a part of 1 or more, else -65536 (0 too)
		int32_t AxisSign(int32_t v)
		{
			return v < 1 ? -65536 : 65536;
		}

		// the code's absolute value: v < 1 then -v
		int64_t Size(int32_t v)
		{
			return v < 1 ? -(int64_t)v : (int64_t)v;
		}

		// FUN_004b5650: out = a x b, each product >> 16 (FUN_00499b30), the differences wrapping as the code's
		void CrossFixed(int32_t const a[3], int32_t const b[3], int32_t out[3])
		{
			out[0] = (int32_t)((uint32_t)MoveRelPoly_Multiply(a[1], b[2]) - (uint32_t)MoveRelPoly_Multiply(a[2], b[1]));
			out[1] = (int32_t)((uint32_t)MoveRelPoly_Multiply(a[2], b[0]) - (uint32_t)MoveRelPoly_Multiply(a[0], b[2]));
			out[2] = (int32_t)((uint32_t)MoveRelPoly_Multiply(a[0], b[1]) - (uint32_t)MoveRelPoly_Multiply(a[1], b[0]));
		}

		// FUN_004fd750: an int vector scaled to length 65536 (65536.0 x part / length), each rounded to nearest, ties to even (the x87's mode), as move_rel_poly.cpp's
		// ScaleTo65536; only ever called here on a vector that is not 0
		void ScaleTo65536(int32_t v[3])
		{
			double const length = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);

			for (int axis = 0; axis < 3; axis++)
			{
				v[axis] = (int32_t)nearbyint(65536.0 * v[axis] / length);
			}
		}

		bool AllZero(int32_t const v[3])
		{
			return v[0] == 0 && v[1] == 0 && v[2] == 0;
		}

		// FUN_004b34b0(first, second, out): out = second x first, row by row, through its own sums (the call may write over either)
		void TimesFixed(int32_t const first[9], int32_t const second[9], int32_t out[9])
		{
			int32_t sum[9];

			for (int r = 0; r < 3; r++)
			{
				for (int c = 0; c < 3; c++)
				{
					sum[r * 3 + c] = Mul(second[r * 3], first[c]) + Mul(second[r * 3 + 1], first[3 + c]) + Mul(second[r * 3 + 2], first[6 + c]);
				}
			}

			memcpy(out, sum, sizeof(sum));
		}

		// FUN_004b3710
		void TransposeFixed(int32_t m[9])
		{
			int32_t t;

			t = m[1]; m[1] = m[3]; m[3] = t;
			t = m[2]; m[2] = m[6]; m[6] = t;
			t = m[5]; m[5] = m[7]; m[7] = t;
		}
	}

	void SnapVertex_NormalMatrix(int32_t const n[3], int32_t m[9])
	{
		int32_t axis[3] = { 0, 0, 0 };
		int64_t const x = Size(n[0]), y = Size(n[1]), z = Size(n[2]);

		if (x < y && x < z)
		{
			axis[0] = 65536;
		}
		else if (y < z)
		{
			axis[1] = 65536;
		}
		else
		{
			axis[2] = 65536;
		}

		int32_t third[3], first[3];

		CrossFixed(axis, n, third);          // FUN_004b5650(&axis, &n, &local_30)

		if (!AllZero(third))
		{
			ScaleTo65536(third);             // FUN_004fd750 (a zero normal gives zeros here: inteng's would divide by 0)
		}

		CrossFixed(n, third, first);         // FUN_004b5650(&n, &local_30, &axis)

		if (!AllZero(first))
		{
			ScaleTo65536(first);
		}

		int32_t const out[9] = { first[0], first[1], first[2], n[0], n[1], n[2], third[0], third[1], third[2] };

		memcpy(m, out, sizeof(out));
	}

	void SnapVertex_SquareUp(int32_t m[9])
	{
		// row 0 onto its largest part, then row 1 onto its larger of the two left (0x53d1ea-0x53d336)
		if (!(Size(m[0]) < Size(m[1])) && !(Size(m[0]) < Size(m[2])))
		{
			int32_t const r1y = m[4], r1z = m[5];

			m[0] = AxisSign(m[0]);
			m[1] = 0;
			m[2] = 0;
			m[3] = 0;

			if (Size(r1y) < Size(r1z))
			{
				m[4] = 0;
				m[5] = AxisSign(r1z);
			}
			else
			{
				m[5] = 0;
				m[4] = AxisSign(r1y);
			}
		}
		else if (Size(m[1]) < Size(m[2]))
		{
			int32_t const r1x = m[3], r1y = m[4];

			m[2] = AxisSign(m[2]);
			m[0] = 0;
			m[1] = 0;
			m[5] = 0;

			if (Size(r1x) < Size(r1y))
			{
				m[3] = 0;
				m[4] = AxisSign(r1y);
			}
			else
			{
				m[4] = 0;
				m[3] = AxisSign(r1x);
			}
		}
		else
		{
			int32_t const r1x = m[3], r1z = m[5];

			m[1] = AxisSign(m[1]);
			m[0] = 0;
			m[2] = 0;

			if (Size(r1x) < Size(r1z))
			{
				m[4] = 0;
				m[3] = 0;
				m[5] = AxisSign(r1z);
			}
			else
			{
				m[5] = 0;
				m[4] = 0;
				m[3] = AxisSign(r1x);
			}
		}

		// row 2 = row 0 x row 1 made length 65536 (FUN_004fd620): the two rows are axes, so the product is one too
		int32_t third[3];

		CrossFixed(m, m + 3, third);

		if (!AllZero(third))
		{
			ScaleTo65536(third);
		}

		m[6] = third[0];
		m[7] = third[1];
		m[8] = third[2];
	}

	void SnapVertex_Align(int32_t const n[3], int32_t m[9])
	{
		if (AllZero(n))
		{
			return;
		}

		int32_t frame[9];

		SnapVertex_NormalMatrix(n, frame);
		TransposeFixed(frame);               // 0x4f71a6
		TimesFixed(frame, m, m);             // 0x4f71c7: m = m x Nt
		TransposeFixed(frame);               // 0x4f71d3
		SnapVertex_SquareUp(m);              // 0x4f71de
		TimesFixed(frame, m, m);             // 0x4f71e4: m = m x N
	}

	MoveRelPolyEditResult SnapToVertex_Pose(OpenLevelFile *store, File_Chunk *level, uint8_t const *movingId, uint8_t const *targetId, double const point[3], int32_t polygon, bool align,
		int32_t place[3], float turn[4], int32_t const *fromPlace, float const *fromTurn)
	{
		if (!store || !level || !movingId || !targetId || !point || !place || !turn || memcmp(movingId, targetId, 8) == 0)
		{
			return MoveRelPolyEditResult::BadCall;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			if (NotANumber(point[axis]))
			{
				return MoveRelPolyEditResult::BadCall;
			}
		}

		// T: the target's world vertex nearest the point under the pointer (0x4f70a1-0x4f7124)
		model::ShapeModel targetShape;
		std::vector<int32_t> targetPoints, targetNormals;
		FilePickResult const readTarget = FilePick_World(store, level, targetId, &targetShape, &targetPoints, &targetNormals);
		int32_t vertex[3];

		if (readTarget == FilePickResult::NoObject)
		{
			return MoveRelPolyEditResult::NoObject;
		}

		if (readTarget != FilePickResult::Done || !NearestWorldPoint(targetPoints, point, vertex))
		{
			return MoveRelPolyEditResult::NoShape;
		}

		Object_Chunk *const moving = ObjectWithId(level, movingId);
		Header header;
		model::ShapeModel movingShape;
		std::vector<int32_t> movingPoints, movingNormals;

		if (!moving)
		{
			return MoveRelPolyEditResult::NoObject;
		}

		if (!ReadHeader(store, moving, &header) || FilePick_World(store, level, movingId, &movingShape, &movingPoints, &movingNormals) != FilePickResult::Done)
		{
			return MoveRelPolyEditResult::NoShape;
		}

		// the pose snapped from: the store's, or the one given (both or neither)
		bool const given = fromPlace && fromTurn;

		if ((fromPlace != nullptr) != (fromTurn != nullptr))
		{
			return MoveRelPolyEditResult::BadCall;
		}

		if (given)
		{
			memcpy(header.place, fromPlace, sizeof(header.place));
			memcpy(header.turn, fromTurn, sizeof(header.turn));
		}

		float newTurn[4];

		memcpy(newTurn, header.turn, sizeof(newTurn));

		// the align (0x4f7139-0x4f71e4): the hit polygon's normal in the world, the moving matrix turned to it and squared up; the moving points turned by the new turn
		if (align && polygon >= 0 && (size_t)polygon * 3 + 2 < targetNormals.size())
		{
			int32_t const normal[3] = { targetNormals[(size_t)polygon * 3], targetNormals[(size_t)polygon * 3 + 1], targetNormals[(size_t)polygon * 3 + 2] };
			int32_t matrix[9];

			MoveRelPolyEdit_TurnToMatrix(header.turn, matrix);
			SnapVertex_Align(normal, matrix);
			MoveRelPolyEdit_MatrixToTurn(matrix, newTurn);
		}

		// the moving points at the pose snapped from (FilePick_World put them at the store's): turned by the new turn and moved by the place
		if ((align && polygon >= 0 && (size_t)polygon * 3 + 2 < targetNormals.size()) || given)
		{
			movingPoints.clear();

			for (model::ShapePoint const &local : movingShape.points)
			{
				model::ShapePoint turned;

				if (model::RotateByTurn(newTurn, local, &turned) != model::ShapeMathsResult::Ok)
				{
					return MoveRelPolyEditResult::NoShape;
				}

				int32_t const parts[3] = { turned.x, turned.y, turned.z };

				// as FilePick_World puts a point in the world: the turned point plus the place
				for (int axis = 0; axis < 3; axis++)
				{
					double const world = (double)parts[axis] + (double)header.place[axis];

					if (fabs(world) >= (double)kFarthestPlace)
					{
						return MoveRelPolyEditResult::TooFar;
					}

					movingPoints.push_back((int32_t)world);
				}
			}
		}

		// V: the moving object's own world vertex nearest T; the place moves by T - V (0x4f71f2-0x4f72b0)
		double const target[3] = { (double)vertex[0], (double)vertex[1], (double)vertex[2] };
		int32_t own[3];

		if (!NearestWorldPoint(movingPoints, target, own))
		{
			return MoveRelPolyEditResult::NoShape;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			double const moved = (double)header.place[axis] + ((double)vertex[axis] - (double)own[axis]);

			if (NotANumber(moved) || fabs(moved) >= (double)kFarthestPlace)
			{
				return MoveRelPolyEditResult::TooFar;
			}

			place[axis] = (int32_t)moved;
		}

		memcpy(turn, newTurn, sizeof(newTurn));
		return MoveRelPolyEditResult::Done;
	}

	MoveRelPolyEditResult ObjectPose_Get(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, int32_t place[3], float turn[4])
	{
		if (!store || !level || !id || !place || !turn)
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

		memcpy(place, header.place, sizeof(header.place));
		memcpy(turn, header.turn, sizeof(header.turn));
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
