// See shape_merge.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "shape_merge.h"

#include <math.h>

namespace model
{
	namespace
	{
		bool FitsInt32(int64_t value)
		{
			return value >= INT32_MIN && value <= INT32_MAX;
		}
	}

	char const *ShapeMergeWords(ShapeMergeResult result)
	{
		switch (result)
		{
		case ShapeMergeResult::Ok: return "done";
		case ShapeMergeResult::TooBig: return "the merged shape would have more points or polygons than the editor takes";
		case ShapeMergeResult::Overflow: return "a number of the merged shape would leave the range the file can hold";
		case ShapeMergeResult::BadShape: return "a polygon of one of the shapes names a point it doesn't have";
		}

		return "unknown answer";
	}

	bool PolygonTypeHasUvList(int32_t engineType)
	{
		return engineType == 5 || engineType == 6 || engineType == 7 || engineType == 0x15 || engineType == 0x16 || engineType == 0x17 ||
			engineType == 0x18 || engineType == 0x19;
	}

	ShapeMergeResult MergeShapes(ShapeModel *first, ShapePoint placeFirst, ShapeModel const &second, ShapePoint placeSecond, size_t uvListsInFirst,
		bool firstHasAnimation, ShapeMergeOutcome *outcome)
	{
		if (!first || !outcome || first == &second)
		{
			return ShapeMergeResult::BadShape;
		}

		if (first->polygonNormals.size() != first->polygons.size() || second.polygonNormals.size() != second.polygons.size())
		{
			return ShapeMergeResult::BadShape;
		}

		for (size_t at = 0; at < first->polygons.size(); at++)
		{
			for (int corner = 0; corner < first->polygons[at].cornerCount; corner++)
			{
				if (first->polygons[at].corners[corner] < 0 || (size_t)first->polygons[at].corners[corner] >= first->points.size())
				{
					return ShapeMergeResult::BadShape;
				}
			}
		}

		for (size_t at = 0; at < second.polygons.size(); at++)
		{
			for (int corner = 0; corner < second.polygons[at].cornerCount; corner++)
			{
				if (second.polygons[at].corners[corner] < 0 || (size_t)second.polygons[at].corners[corner] >= second.points.size())
				{
					return ShapeMergeResult::BadShape;
				}
			}
		}

		if (first->points.size() + second.points.size() > kMostShapePoints || first->polygons.size() + second.polygons.size() > kMostShapePolygons)
		{
			return ShapeMergeResult::TooBig;
		}

		// 1. The point map and the merged points (FUN_00449cf0 0x449fe5-0x44a0cd, FUN_00448cf0 0x448d62-0x448e0e)
		std::vector<ShapePoint> merged = first->points;
		std::vector<int32_t> map(second.points.size());
		size_t next = first->points.size();

		for (size_t at = 0; at < second.points.size(); at++)
		{
			int64_t const x = (int64_t)second.points[at].x + placeSecond.x - placeFirst.x;
			int64_t const y = (int64_t)second.points[at].y + placeSecond.y - placeFirst.y;
			int64_t const z = (int64_t)second.points[at].z + placeSecond.z - placeFirst.z;

			if (!FitsInt32(x) || !FitsInt32(y) || !FitsInt32(z))
			{
				return ShapeMergeResult::Overflow;
			}

			size_t found = first->points.size();

			// Equal means equal: inteng's test is "|difference| < 1" on whole numbers. With an animation on the first shape nothing is looked for.
			if (!firstHasAnimation)
			{
				for (size_t other = 0; other < first->points.size(); other++)
				{
					if (first->points[other].x == x && first->points[other].y == y && first->points[other].z == z)
					{
						found = other;
						break;
					}
				}
			}

			if (found < first->points.size())
			{
				map[at] = (int32_t)found;
			}
			else
			{
				map[at] = (int32_t)next;
				next++;

				ShapePoint added = { (int32_t)x, (int32_t)y, (int32_t)z };

				merged.push_back(added);
			}
		}

		// 2. The polygons (FUN_00448cf0 0x448e7a-0x448fab): the first shape's as they are, the second's with their numbers moved on
		std::vector<ShapePolygon> polygons = first->polygons;
		std::vector<ShapeNormal> polygonNormals = first->polygonNormals;
		size_t const firstPolygons = first->polygons.size();

		for (size_t at = 0; at < second.polygons.size(); at++)
		{
			ShapePolygon polygon = second.polygons[at];

			polygon.normalIndex = (int32_t)(firstPolygons + at);

			if (PolygonTypeHasUvList(polygon.engineType))
			{
				int64_t const list = (((int64_t)(uint32_t)polygon.colour) >> 16) + (int64_t)uvListsInFirst;

				if (list > 0xFFFF)
				{
					return ShapeMergeResult::Overflow;
				}

				polygon.colour = (int32_t)(((uint32_t)list << 16) | ((uint32_t)polygon.colour & 0xFFFFu));
			}

			for (int corner = 0; corner < polygon.cornerCount; corner++)
			{
				polygon.corners[corner] = map[(size_t)polygon.corners[corner]];
			}

			polygons.push_back(polygon);
			polygonNormals.push_back(second.polygonNormals[at]);
		}

		// 3. The point normals, from the polygon normals (0x449042-0x44912d): zeroed, summed, made unit; a length of 0 becomes (1, 0, 0)
		std::vector<ShapeNormal> pointNormals(merged.size());

		for (size_t at = 0; at < pointNormals.size(); at++)
		{
			pointNormals[at].x = 0.0f;
			pointNormals[at].y = 0.0f;
			pointNormals[at].z = 0.0f;
		}

		for (size_t at = 0; at < polygons.size(); at++)
		{
			for (int corner = 0; corner < polygons[at].cornerCount; corner++)
			{
				ShapeNormal &sum = pointNormals[(size_t)polygons[at].corners[corner]];

				sum.x += polygonNormals[at].x;
				sum.y += polygonNormals[at].y;
				sum.z += polygonNormals[at].z;
			}
		}

		for (size_t at = 0; at < pointNormals.size(); at++)
		{
			ShapeNormal &normal = pointNormals[at];
			double const length = sqrt((double)normal.x * normal.x + (double)normal.y * normal.y + (double)normal.z * normal.z);

			if (length != 0.0)
			{
				normal.x = (float)((double)normal.x / length);
				normal.y = (float)((double)normal.y / length);
				normal.z = (float)((double)normal.z / length);
			}
			else
			{
				normal.x = 1.0f;
				normal.y = 0.0f;
				normal.z = 0.0f;
			}
		}

		// 4. The extents from inteng's own starting values +-10000000, the centre, and every point moved back by it (0x449134-0x4492d6)
		int32_t minX = 10000000, minY = 10000000, minZ = 10000000;
		int32_t maxX = -10000000, maxY = -10000000, maxZ = -10000000;

		for (size_t at = 0; at < merged.size(); at++)
		{
			if (merged[at].x <= minX) minX = merged[at].x;
			if (maxX <= merged[at].x) maxX = merged[at].x;
			if (merged[at].y <= minY) minY = merged[at].y;
			if (maxY <= merged[at].y) maxY = merged[at].y;
			if (merged[at].z <= minZ) minZ = merged[at].z;
			if (maxZ <= merged[at].z) maxZ = merged[at].z;
		}

		ShapePoint const centre = { (int32_t)(((int64_t)minX + maxX) / 2), (int32_t)(((int64_t)minY + maxY) / 2), (int32_t)(((int64_t)minZ + maxZ) / 2) };

		for (size_t at = 0; at < merged.size(); at++)
		{
			int64_t const x = (int64_t)merged[at].x - centre.x;
			int64_t const y = (int64_t)merged[at].y - centre.y;
			int64_t const z = (int64_t)merged[at].z - centre.z;

			if (!FitsInt32(x) || !FitsInt32(y) || !FitsInt32(z))
			{
				return ShapeMergeResult::Overflow;
			}

			merged[at].x = (int32_t)x;
			merged[at].y = (int32_t)y;
			merged[at].z = (int32_t)z;
		}

		int64_t const extentsAfter[6] = { (int64_t)maxX - centre.x, (int64_t)minX - centre.x, (int64_t)maxY - centre.y, (int64_t)minY - centre.y,
			(int64_t)maxZ - centre.z, (int64_t)minZ - centre.z };

		for (int at = 0; at < 6; at++)
		{
			if (!FitsInt32(extentsAfter[at]))
			{
				return ShapeMergeResult::Overflow;
			}
		}

		ShapeExtents extents = { (int32_t)extentsAfter[0], (int32_t)extentsAfter[1], (int32_t)extentsAfter[2], (int32_t)extentsAfter[3],
			(int32_t)extentsAfter[4], (int32_t)extentsAfter[5] };

		// Nothing is changed before this point: the first shape takes the result all at once
		first->radius = ShapeRadiusOf(merged);
		first->extents = extents;
		first->points.swap(merged);
		first->polygons.swap(polygons);
		first->polygonNormals.swap(polygonNormals);
		first->pointNormals.swap(pointNormals);

		outcome->pointMap.swap(map);
		outcome->centre = centre;
		outcome->firstPolygons = firstPolygons;
		return ShapeMergeResult::Ok;
	}
}
