// See file_pick.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "file_pick.h"

#include <math.h>
#include <string.h>

#include <vector>

#include "chunk_object.hpp"   // Object_Chunk, Shape_Chunk
#include "chunk_shape.hpp"
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: PayloadOf
#include "shape_edit.h"       // ShapeEdit_Read
#include "shape_maths.h"      // RotateByTurn

namespace host
{
	namespace
	{
		size_t const kHeaderPlaceAt = 20;
		size_t const kHeaderTurnAt = 32;

		bool Finite(double value)
		{
			uint64_t bits;

			memcpy(&bits, &value, sizeof(bits));
			return (bits & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;   // by its bits: the build is /fp:fast
		}

		// The point is inside the polygon's outline, both on the polygon's plane: dropped onto the two widest axes of the normal, then the crossing count
		bool Inside(double const corners[][3], int count, double const normal[3], double const point[3])
		{
			double const ax = fabs(normal[0]), ay = fabs(normal[1]), az = fabs(normal[2]);
			int const drop = (ax >= ay && ax >= az) ? 0 : (ay >= az ? 1 : 2);
			int const u = drop == 0 ? 1 : 0;
			int const v = drop == 2 ? 1 : 2;
			bool inside = false;

			for (int i = 0, j = count - 1; i < count; j = i++)
			{
				double const ui = corners[i][u], vi = corners[i][v], uj = corners[j][u], vj = corners[j][v];

				if ((vi > point[v]) != (vj > point[v]) && point[u] < (uj - ui) * (point[v] - vi) / (vj - vi) + ui)
				{
					inside = !inside;
				}
			}

			return inside;
		}

		int32_t Truncated(double value)
		{
			return value >= 2147483647.0 ? INT32_MAX : (value <= -2147483648.0 ? INT32_MIN : (int32_t)value);
		}
	}

	bool FilePick_Shape(model::ShapeModel const &shape, int32_t const place[3], float const turn[4], FilePickRay const &ray, FilePickHit *hit)
	{
		if (!hit)
		{
			return false;
		}

		memset(hit, 0, sizeof(*hit));

		for (int axis = 0; axis < 3; axis++)
		{
			if (!Finite(ray.from[axis]) || !Finite(ray.direction[axis]))
			{
				return false;
			}
		}

		if (ray.direction[0] == 0.0 && ray.direction[1] == 0.0 && ray.direction[2] == 0.0)
		{
			return false;
		}

		// every point of the shape, in the world: turned by the object's turn, then its place added (as inteng's FUN_004b3860 / FUN_004b3800 do)
		std::vector<double> world(shape.points.size() * 3);

		for (size_t p = 0; p < shape.points.size(); p++)
		{
			model::ShapePoint turned;

			if (model::RotateByTurn(turn, shape.points[p], &turned) != model::ShapeMathsResult::Ok)
			{
				return false;
			}

			world[3 * p] = (double)turned.x + place[0];
			world[3 * p + 1] = (double)turned.y + place[1];
			world[3 * p + 2] = (double)turned.z + place[2];
		}

		double bestT = 0.0;

		for (size_t i = 0; i < shape.polygons.size(); i++)
		{
			model::ShapePolygon const &polygon = shape.polygons[i];
			double corners[5][3];

			if (polygon.cornerCount < 3 || polygon.cornerCount > 5)
			{
				continue;
			}

			bool good = true;

			for (int c = 0; c < polygon.cornerCount && good; c++)
			{
				int32_t const number = polygon.corners[c];

				if (number < 0 || (size_t)number >= shape.points.size())
				{
					good = false;
					break;
				}

				memcpy(corners[c], &world[3 * (size_t)number], sizeof(corners[c]));
			}

			if (!good)
			{
				continue;
			}

			// the polygon's normal in the world: the shape's own, turned (scaled to 65536ths by a C cast, cut toward 0, as the game loader does it, chunk_load.cpp:2511; inteng conversion not read: ASSUMED); from the corners when it has none
			double normal[3] = { 0, 0, 0 };

			if (polygon.normalIndex >= 0 && (size_t)polygon.normalIndex < shape.polygonNormals.size())
			{
				model::ShapeNormal const &own = shape.polygonNormals[(size_t)polygon.normalIndex];
				model::ShapePoint scaled = { Truncated(own.x * 65536.0), Truncated(own.y * 65536.0), Truncated(own.z * 65536.0) };
				model::ShapePoint turned;

				if (model::RotateByTurn(turn, scaled, &turned) == model::ShapeMathsResult::Ok)
				{
					normal[0] = turned.x / 65536.0;
					normal[1] = turned.y / 65536.0;
					normal[2] = turned.z / 65536.0;
				}
			}

			double length = sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);

			if (!(length > 0.5))
			{
				// the corners' own: the cross product of the first two edges
				double const a[3] = { corners[1][0] - corners[0][0], corners[1][1] - corners[0][1], corners[1][2] - corners[0][2] };
				double const b[3] = { corners[2][0] - corners[0][0], corners[2][1] - corners[0][1], corners[2][2] - corners[0][2] };

				normal[0] = a[1] * b[2] - a[2] * b[1];
				normal[1] = a[2] * b[0] - a[0] * b[2];
				normal[2] = a[0] * b[1] - a[1] * b[0];
				length = sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);

				if (!(length > 0.0))
				{
					continue;   // a polygon with no area has no plane
				}
			}

			for (int axis = 0; axis < 3; axis++)
			{
				normal[axis] /= length;
			}

			// FUN_00473680: t = (plane . point) / (plane . ray); no hit when the denominator is exactly 0.0
			double const denominator = normal[0] * ray.direction[0] + normal[1] * ray.direction[1] + normal[2] * ray.direction[2];

			if (denominator == 0.0)
			{
				continue;
			}

			double const planeDot = normal[0] * (corners[0][0] - ray.from[0]) + normal[1] * (corners[0][1] - ray.from[1]) + normal[2] * (corners[0][2] - ray.from[2]);
			double const t = planeDot / denominator;

			if (!(t > 0.0) || !Finite(t) || (hit->found && !(t < bestT)))
			{
				continue;
			}

			double const at[3] = { ray.from[0] + t * ray.direction[0], ray.from[1] + t * ray.direction[1], ray.from[2] + t * ray.direction[2] };

			if (!Inside(corners, polygon.cornerCount, normal, at))
			{
				continue;
			}

			bestT = t;
			hit->found = true;
			hit->polygon = (int32_t)i;
			hit->distance = t;
			hit->cornerCount = polygon.cornerCount;

			for (int axis = 0; axis < 3; axis++)
			{
				hit->point[axis] = Truncated(at[axis]);
				hit->planePoint[axis] = Truncated(corners[0][axis]);
				hit->normal[axis] = normal[axis];
			}
		}

		return true;
	}

	FilePickResult FilePick_Object(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, FilePickRay const &ray, FilePickHit *hit)
	{
		if (!store || !level || !id || !hit)
		{
			return FilePickResult::BadCall;
		}

		memset(hit, 0, sizeof(*hit));

		Object_Chunk *object = nullptr;
		List<Object_Chunk *> found;

		level->list_objects(&found);

		for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
		{
			if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
			{
				object = each();
				break;
			}
		}

		if (!object)
		{
			return FilePickResult::NoObject;
		}

		model::ShapeModel shape;
		std::string why;

		if (ShapeEdit_Read(level, store, id, &shape, nullptr, &why) != ShapeEditResult::Done)
		{
			return FilePickResult::NoShape;
		}

		Object_Header_Chunk *const header = object->get_header();
		std::vector<unsigned char> bytes;

		if (!header || !store->PayloadOf(header, &bytes) || bytes.size() < kHeaderTurnAt + 16)
		{
			return FilePickResult::BadHeader;
		}

		int32_t place[3];
		float turn[4];

		memcpy(place, bytes.data() + kHeaderPlaceAt, sizeof(place));
		memcpy(turn, bytes.data() + kHeaderTurnAt, sizeof(turn));

		FilePick_Shape(shape, place, turn, ray, hit);
		return FilePickResult::Done;
	}

	FilePickResult FilePick_World(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, model::ShapeModel *shape, std::vector<int32_t> *worldPoints, std::vector<int32_t> *worldNormals)
	{
		if (!store || !level || !id || !shape || !worldPoints || !worldNormals)
		{
			return FilePickResult::BadCall;
		}

		Object_Chunk *object = nullptr;
		List<Object_Chunk *> found;

		level->list_objects(&found);

		for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
		{
			if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
			{
				object = each();
				break;
			}
		}

		std::string why;

		if (!object)
		{
			return FilePickResult::NoObject;
		}

		if (ShapeEdit_Read(level, store, id, shape, nullptr, &why) != ShapeEditResult::Done)
		{
			return FilePickResult::NoShape;
		}

		Object_Header_Chunk *const header = object->get_header();
		std::vector<unsigned char> bytes;

		if (!header || !store->PayloadOf(header, &bytes) || bytes.size() < kHeaderTurnAt + 16)
		{
			return FilePickResult::BadHeader;
		}

		int32_t place[3];
		float turn[4];

		memcpy(place, bytes.data() + kHeaderPlaceAt, sizeof(place));
		memcpy(turn, bytes.data() + kHeaderTurnAt, sizeof(turn));

		worldPoints->assign(shape->points.size() * 3, 0);
		worldNormals->assign(shape->polygons.size() * 3, 0);

		for (size_t p = 0; p < shape->points.size(); p++)
		{
			model::ShapePoint turned;

			if (model::RotateByTurn(turn, shape->points[p], &turned) != model::ShapeMathsResult::Ok)
			{
				return FilePickResult::BadHeader;
			}

			(*worldPoints)[3 * p] = turned.x + place[0];
			(*worldPoints)[3 * p + 1] = turned.y + place[1];
			(*worldPoints)[3 * p + 2] = turned.z + place[2];
		}

		for (size_t i = 0; i < shape->polygons.size(); i++)
		{
			int32_t const number = shape->polygons[i].normalIndex;

			if (number >= 0 && (size_t)number < shape->polygonNormals.size())
			{
				model::ShapeNormal const &own = shape->polygonNormals[(size_t)number];
				model::ShapePoint scaled = { Truncated(own.x * 65536.0), Truncated(own.y * 65536.0), Truncated(own.z * 65536.0) };
				model::ShapePoint turned;

				if (model::RotateByTurn(turn, scaled, &turned) == model::ShapeMathsResult::Ok)
				{
					(*worldNormals)[3 * i] = turned.x;
					(*worldNormals)[3 * i + 1] = turned.y;
					(*worldNormals)[3 * i + 2] = turned.z;
				}
			}
		}

		return FilePickResult::Done;
	}
}
