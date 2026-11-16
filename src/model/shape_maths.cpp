// See shape_maths.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "shape_maths.h"

#include <math.h>

namespace model
{
	namespace
	{
		bool FitsInt32(int64_t value)
		{
			return value >= INT32_MIN && value <= INT32_MAX;
		}

		// The sum of squares of a point, exact in 64 bits (each square is below 2^62, the sum below 3 * 2^62 which is under 2^64 unsigned)
		uint64_t SquaresOf(ShapePoint const &point)
		{
			uint64_t const x = (uint64_t)((int64_t)point.x * (int64_t)point.x);
			uint64_t const y = (uint64_t)((int64_t)point.y * (int64_t)point.y);
			uint64_t const z = (uint64_t)((int64_t)point.z * (int64_t)point.z);

			return x + y + z;
		}
	}

	char const *ShapeMathsWords(ShapeMathsResult result)
	{
		switch (result)
		{
		case ShapeMathsResult::Ok: return "done";
		case ShapeMathsResult::NoPoints: return "the shape has no points";
		case ShapeMathsResult::Overflow: return "a number of the shape would leave the range the file can hold";
		case ShapeMathsResult::SameShape: return "the two objects use the same shape, which can't be welded to itself";
		case ShapeMathsResult::BadShape: return "a polygon of the shape names a point it doesn't have";
		}

		return "unknown answer";
	}

	float ShapeRadiusOf(std::vector<ShapePoint> const &points)
	{
		// The longest wins, compared on the exact squares so no rounding picks the wrong one; only the winner is square-rooted
		uint64_t longest = 0;

		for (size_t at = 0; at < points.size(); at++)
		{
			uint64_t const squares = SquaresOf(points[at]);

			if (squares > longest)
			{
				longest = squares;
			}
		}

		return (float)sqrt((double)longest);
	}

	ShapeMathsResult ShapeExtentsOf(std::vector<ShapePoint> const &points, ShapeExtents *out)
	{
		if (points.empty() || !out)
		{
			return ShapeMathsResult::NoPoints;
		}

		ShapeExtents found = { points[0].x, points[0].x, points[0].y, points[0].y, points[0].z, points[0].z };

		for (size_t at = 1; at < points.size(); at++)
		{
			ShapePoint const &point = points[at];

			if (point.x > found.maxX) found.maxX = point.x;
			if (point.x < found.minX) found.minX = point.x;
			if (point.y > found.maxY) found.maxY = point.y;
			if (point.y < found.minY) found.minY = point.y;
			if (point.z > found.maxZ) found.maxZ = point.z;
			if (point.z < found.minZ) found.minZ = point.z;
		}

		*out = found;
		return ShapeMathsResult::Ok;
	}

	ShapePoint RecentreTarget(ShapeExtents const &extents, bool top)
	{
		// C++ integer division truncates toward zero, as the __ftol of inteng does; the sums are 64-bit so they can't wrap
		ShapePoint target;

		target.x = (int32_t)(((int64_t)extents.minX + (int64_t)extents.maxX) / 2);
		target.y = top ? extents.maxY : extents.minY;
		target.z = (int32_t)(((int64_t)extents.minZ + (int64_t)extents.maxZ) / 2);
		return target;
	}

	ShapeMathsResult RecentreOffsetFor(ShapePoint target, ShapePoint *offset)
	{
		if (!offset)
		{
			return ShapeMathsResult::NoPoints;
		}

		if (target.x == INT32_MIN || target.y == INT32_MIN || target.z == INT32_MIN)
		{
			return ShapeMathsResult::Overflow;
		}

		offset->x = -target.x;
		offset->y = -target.y;
		offset->z = -target.z;
		return ShapeMathsResult::Ok;
	}

	ShapeMathsResult TranslateShape(ShapeModel *shape, ShapePoint q)
	{
		if (!shape)
		{
			return ShapeMathsResult::NoPoints;
		}

		// Everything is made in copies first, so a number that would overflow leaves the shape untouched
		std::vector<ShapePoint> moved = shape->points;

		for (size_t at = 0; at < moved.size(); at++)
		{
			int64_t const x = (int64_t)moved[at].x + (int64_t)q.x;
			int64_t const y = (int64_t)moved[at].y + (int64_t)q.y;
			int64_t const z = (int64_t)moved[at].z + (int64_t)q.z;

			if (!FitsInt32(x) || !FitsInt32(y) || !FitsInt32(z))
			{
				return ShapeMathsResult::Overflow;
			}

			moved[at].x = (int32_t)x;
			moved[at].y = (int32_t)y;
			moved[at].z = (int32_t)z;
		}

		int64_t const extentsAfter[6] =
		{
			(int64_t)shape->extents.maxX + q.x, (int64_t)shape->extents.minX + q.x,
			(int64_t)shape->extents.maxY + q.y, (int64_t)shape->extents.minY + q.y,
			(int64_t)shape->extents.maxZ + q.z, (int64_t)shape->extents.minZ + q.z,
		};

		for (int at = 0; at < 6; at++)
		{
			if (!FitsInt32(extentsAfter[at]))
			{
				return ShapeMathsResult::Overflow;
			}
		}

		shape->points.swap(moved);
		shape->extents.maxX = (int32_t)extentsAfter[0];
		shape->extents.minX = (int32_t)extentsAfter[1];
		shape->extents.maxY = (int32_t)extentsAfter[2];
		shape->extents.minY = (int32_t)extentsAfter[3];
		shape->extents.maxZ = (int32_t)extentsAfter[4];
		shape->extents.minZ = (int32_t)extentsAfter[5];
		shape->radius = ShapeRadiusOf(shape->points);
		return ShapeMathsResult::Ok;
	}

	float ShapeRadiusAboutCentre(ShapeModel const &shape)
	{
		ShapePoint const centre = { (int32_t)(((int64_t)shape.extents.maxX + shape.extents.minX) / 2),
			(int32_t)(((int64_t)shape.extents.maxY + shape.extents.minY) / 2), (int32_t)(((int64_t)shape.extents.maxZ + shape.extents.minZ) / 2) };
		double longest = 0.0;

		for (size_t at = 0; at < shape.points.size(); at++)
		{
			// A difference of two 32-bit numbers is exact in a double, and so is its square up to 2^53: far beyond any level's size; the
			// squares of the three axes are added in double (inteng adds them in x87 extended: the last bit of the float can differ)
			double const dx = (double)((int64_t)shape.points[at].x - centre.x);
			double const dy = (double)((int64_t)shape.points[at].y - centre.y);
			double const dz = (double)((int64_t)shape.points[at].z - centre.z);
			double const squares = dx * dx + dy * dy + dz * dz;

			if (squares > longest)
			{
				longest = squares;
			}
		}

		return (float)sqrt(longest);
	}

	ShapeMathsResult RecalculateNormalsAndExtents(ShapeModel *shape)
	{
		if (!shape)
		{
			return ShapeMathsResult::NoPoints;
		}

		int64_t const limit = (int64_t)1 << 30;
		std::vector<ShapePoint> const &points = shape->points;
		std::vector<ShapeNormal> polygonNormals(shape->polygons.size());
		std::vector<ShapeNormal> pointNormals(points.size());

		for (size_t at = 0; at < pointNormals.size(); at++)
		{
			pointNormals[at].x = 0.0f;
			pointNormals[at].y = 0.0f;
			pointNormals[at].z = 0.0f;
		}

		// 1. The normal of each polygon, from its first three corners (FUN_00567610)
		for (size_t at = 0; at < shape->polygons.size(); at++)
		{
			ShapePolygon const &polygon = shape->polygons[at];

			if (polygon.cornerCount < 3 || polygon.cornerCount > 5)
			{
				return ShapeMathsResult::BadShape;
			}

			for (int corner = 0; corner < polygon.cornerCount; corner++)
			{
				if (polygon.corners[corner] < 0 || (size_t)polygon.corners[corner] >= points.size())
				{
					return ShapeMathsResult::BadShape;
				}
			}

			ShapePoint const &p0 = points[(size_t)polygon.corners[0]];
			ShapePoint const &p1 = points[(size_t)polygon.corners[1]];
			ShapePoint const &p2 = points[(size_t)polygon.corners[2]];
			int64_t const ax = (int64_t)p1.x - p0.x, ay = (int64_t)p1.y - p0.y, az = (int64_t)p1.z - p0.z;
			int64_t const bx = (int64_t)p2.x - p0.x, by = (int64_t)p2.y - p0.y, bz = (int64_t)p2.z - p0.z;

			if (ax > limit || ax < -limit || ay > limit || ay < -limit || az > limit || az < -limit || bx > limit || bx < -limit ||
				by > limit || by < -limit || bz > limit || bz < -limit)
			{
				return ShapeMathsResult::Overflow;
			}

			// a x b with a = p1 - p0 and b = p2 - p0 (FUN_0042f7a0), exact in 64 bits and rounded to double once, as x87 extended does
			double cross[3] = { (double)(ay * bz - az * by), (double)(az * bx - ax * bz), (double)(ax * by - ay * bx) };
			double const length = sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);

			if (length != 0.0)   // FUN_0042f820: a length of 0 leaves the vector as it is
			{
				cross[0] /= length;
				cross[1] /= length;
				cross[2] /= length;
			}

			polygonNormals[at].x = (float)cross[0];   // FUN_0042f720: the narrowing to float
			polygonNormals[at].y = (float)cross[1];
			polygonNormals[at].z = (float)cross[2];
		}

		// 2. Point normals: every polygon, in order, adds its normal to each corner (FUN_0042fa80), then each is made unit (FUN_0042fb00)
		for (size_t at = 0; at < shape->polygons.size(); at++)
		{
			ShapePolygon const &polygon = shape->polygons[at];

			for (int corner = 0; corner < polygon.cornerCount; corner++)
			{
				ShapeNormal &sum = pointNormals[(size_t)polygon.corners[corner]];

				sum.x += polygonNormals[at].x;
				sum.y += polygonNormals[at].y;
				sum.z += polygonNormals[at].z;
			}
		}

		for (size_t at = 0; at < pointNormals.size(); at++)
		{
			ShapeNormal &normal = pointNormals[at];
			double const length = sqrt((double)normal.x * normal.x + (double)normal.y * normal.y + (double)normal.z * normal.z);

			if (length != 0.0)   // a point normal of length 0 stays 0 here
			{
				normal.x = (float)((double)normal.x / length);
				normal.y = (float)((double)normal.y / length);
				normal.z = (float)((double)normal.z / length);
			}
		}

		// 3. The extents, from inteng's own starting values (FUN_0044baa0 0x44bca2-0x44bcbd)
		ShapeExtents extents = { -1000000000, 1000000000, -1000000000, 1000000000, -1000000000, 1000000000 };

		for (size_t at = 0; at < points.size(); at++)
		{
			if (extents.maxX <= points[at].x) extents.maxX = points[at].x;
			if (extents.maxY <= points[at].y) extents.maxY = points[at].y;
			if (extents.maxZ <= points[at].z) extents.maxZ = points[at].z;
			if (points[at].x <= extents.minX) extents.minX = points[at].x;
			if (points[at].y <= extents.minY) extents.minY = points[at].y;
			if (points[at].z <= extents.minZ) extents.minZ = points[at].z;
		}

		shape->polygonNormals.swap(polygonNormals);
		shape->pointNormals.swap(pointNormals);
		shape->extents = extents;
		return ShapeMathsResult::Ok;
	}

	ShapeMathsResult RotateByTurn(float const turn[4], ShapePoint v, ShapePoint *out)
	{
		if (!turn || !out)
		{
			return ShapeMathsResult::NoPoints;
		}

		for (int at = 0; at < 4; at++)
		{
			if (!(turn[at] == turn[at]) || turn[at] > 3.0e38f || turn[at] < -3.0e38f)
			{
				return ShapeMathsResult::Overflow;
			}
		}

		// FUN_00430d70, line by line: x and w stay floats where inteng keeps them floats, y and z are doubles
		float const x = turn[0];
		double const y = (double)turn[1];
		double const z = (double)turn[2];
		float const w = turn[3];
		double const twoY = y + y;
		double const twoZ = z + z;
		double const twoXw = (double)((x + x) * w);
		float const twoXx = (x + x) * x;
		double m[9];

		m[0] = 1.0 - (twoZ * z + twoY * y);
		m[3] = (double)((float)twoY * x) + (double)((float)twoZ * w);
		m[6] = (double)(x * (float)twoZ - (float)twoY * w);
		m[1] = (double)((float)twoY * x) - (double)((float)twoZ * w);
		m[4] = 1.0 - (twoZ * z + (double)twoXx);
		m[7] = twoZ * y + twoXw;
		m[2] = (double)(x * (float)twoZ + (float)twoY * w);
		m[5] = twoZ * y - twoXw;
		m[8] = 1.0 - (twoY * y + (double)twoXx);

		// FUN_00430e80: out = M x v, the sum of three products per row, then truncation toward zero
		double const vx = (double)v.x, vy = (double)v.y, vz = (double)v.z;
		double const result[3] = { vx * m[0] + vy * m[1] + vz * m[2], vx * m[3] + vy * m[4] + vz * m[5], vx * m[6] + vy * m[7] + vz * m[8] };

		for (int at = 0; at < 3; at++)
		{
			if (!(result[at] > -2147483648.0) || !(result[at] < 2147483648.0))
			{
				return ShapeMathsResult::Overflow;
			}
		}

		out->x = (int32_t)result[0];   // the conversion truncates toward zero, as __ftol does
		out->y = (int32_t)result[1];
		out->z = (int32_t)result[2];
		return ShapeMathsResult::Ok;
	}

	ShapeMathsResult WeldShapePoints(ShapeModel *b, ShapePoint placeB, ShapeModel const &a, ShapePoint placeA, int32_t tolerance, size_t *welded)
	{
		if (!b)
		{
			return ShapeMathsResult::NoPoints;
		}

		if (b == &a)
		{
			return ShapeMathsResult::SameShape;
		}

		std::vector<ShapePoint> result = b->points;
		size_t matches = 0;
		int64_t const most = tolerance;

		if (tolerance >= 0)
		{
			for (size_t at = 0; at < result.size(); at++)
			{
				int64_t const worldBX = (int64_t)result[at].x + placeB.x;
				int64_t const worldBY = (int64_t)result[at].y + placeB.y;
				int64_t const worldBZ = (int64_t)result[at].z + placeB.z;

				for (size_t other = 0; other < a.points.size(); other++)
				{
					int64_t const worldAX = (int64_t)a.points[other].x + placeA.x;
					int64_t const worldAY = (int64_t)a.points[other].y + placeA.y;
					int64_t const worldAZ = (int64_t)a.points[other].z + placeA.z;
					int64_t const dx = worldAX - worldBX;
					int64_t const dy = worldAY - worldBY;
					int64_t const dz = worldAZ - worldBZ;

					if (dx >= -most && dx <= most && dy >= -most && dy <= most && dz >= -most && dz <= most)
					{
						int64_t const x = worldAX - placeB.x;
						int64_t const y = worldAY - placeB.y;
						int64_t const z = worldAZ - placeB.z;

						if (!FitsInt32(x) || !FitsInt32(y) || !FitsInt32(z))
						{
							return ShapeMathsResult::Overflow;
						}

						result[at].x = (int32_t)x;
						result[at].y = (int32_t)y;
						result[at].z = (int32_t)z;
						matches++;
						break;
					}
				}
			}
		}

		b->points.swap(result);

		if (welded)
		{
			*welded = matches;
		}

		return ShapeMathsResult::Ok;
	}

	ShapeMathsResult RescaleShape(ShapeModel *shape, double factor)
	{
		if (!shape)
		{
			return ShapeMathsResult::NoPoints;
		}

		if (!(factor > 0.0) || !isfinite(factor))
		{
			return ShapeMathsResult::Overflow;
		}

		// FUN_00430210: each number is ftol((double)old x factor): truncation toward zero. Everything is made in copies first, so a number that
		// would leave the 32-bit range leaves the shape untouched.
		auto scaled = [factor](int32_t old, int32_t *out) -> bool
		{
			double const product = (double)old * factor;

			if (!(product > -2147483649.0 && product < 2147483648.0))
			{
				return false;
			}

			*out = (int32_t)product;   // truncation toward zero, as __ftol
			return true;
		};

		std::vector<ShapePoint> points = shape->points;

		for (size_t at = 0; at < points.size(); at++)
		{
			if (!scaled(points[at].x, &points[at].x) || !scaled(points[at].y, &points[at].y) || !scaled(points[at].z, &points[at].z))
			{
				return ShapeMathsResult::Overflow;
			}
		}

		ShapeExtents extents = shape->extents;
		int32_t *const numbers[6] = { &extents.maxX, &extents.minX, &extents.maxY, &extents.minY, &extents.maxZ, &extents.minZ };

		for (int at = 0; at < 6; at++)
		{
			if (!scaled(*numbers[at], numbers[at]))
			{
				return ShapeMathsResult::Overflow;
			}
		}

		double const radius = (double)shape->radius * factor;

		if (!isfinite(radius) || radius > 3.0e38 || radius < -3.0e38)
		{
			return ShapeMathsResult::Overflow;
		}

		shape->points.swap(points);
		shape->extents = extents;
		shape->radius = (float)radius;
		return ShapeMathsResult::Ok;
	}
}
