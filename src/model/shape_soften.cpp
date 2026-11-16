// See shape_soften.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "shape_soften.h"

#include <math.h>
#include <string.h>

#include <vector>

namespace model
{
	namespace
	{
		struct Corner
		{
			double position[3];
			double normal[3];
			size_t point;
		};

		struct Group
		{
			double position[3];
			std::vector<size_t> members;   // numbers in the corner list, in the order they joined
		};

		long double Dot(double const *a, double const *b)
		{
			return (long double)a[0] * (long double)b[0] + (long double)a[1] * (long double)b[1] + (long double)a[2] * (long double)b[2];
		}

		double FromBits(uint64_t bits)
		{
			double value;

			memcpy(&value, &bits, sizeof(value));
			return value;
		}
	}

	double SoftenThreshold(int angleDegrees)
	{
		// inteng's two constants: _DAT_005ab4a0 = 0x3f66c16c16c16c17 (1/360) and _DAT_005adba0 = 0x401921fb542fe938 (6.28318...), multiplied in that order in extended precision
		long double const radians = (long double)angleDegrees * (long double)FromBits(0x3f66c16c16c16c17ull) * (long double)FromBits(0x401921fb542fe938ull);

		return (double)cosl(radians);
	}

	bool SoftenShape(ShapeModel *shape, int angleDegrees, double levelScale, size_t *changed)
	{
		if (!shape || angleDegrees < 0 || angleDegrees > (int)kSoftenMostAngle || !(levelScale > 0.0) || levelScale > 1.0e9)
		{
			return false;
		}

		if (shape->pointNormals.size() != shape->points.size() || shape->polygonNormals.size() != shape->polygons.size() || shape->points.empty())
		{
			return false;
		}

		double const threshold = SoftenThreshold(angleDegrees);
		double const scale = 100.0 * levelScale;

		std::vector<Corner> corners;

		for (size_t p = 0; p < shape->polygons.size(); p++)
		{
			ShapePolygon const &polygon = shape->polygons[p];

			// a polygon holds four corners at most in the game's own shape record (ChunkPoly::vert_ind[4])
			for (int j = 0; j < polygon.cornerCount && j < 4; j++)
			{
				int32_t const pointNumber = polygon.corners[j];

				if (pointNumber < 0 || (size_t)pointNumber >= shape->points.size())
				{
					return false;
				}

				Corner corner;
				ShapePoint const &point = shape->points[(size_t)pointNumber];
				ShapeNormal const &normal = shape->polygonNormals[p];

				corner.position[0] = (double)point.x;
				corner.position[1] = (double)point.y;
				corner.position[2] = (double)point.z;
				corner.normal[0] = (double)normal.x;
				corner.normal[1] = (double)normal.y;
				corner.normal[2] = (double)normal.z;
				corner.point = (size_t)pointNumber;
				corners.push_back(corner);
			}
		}

		std::vector<Group> groups;

		for (size_t c = 0; c < corners.size(); c++)
		{
			bool taken = false;

			for (Group &group : groups)
			{
				// FUN_004b0020: close enough in position
				double const difference[3] = { group.position[0] - corners[c].position[0], group.position[1] - corners[c].position[1], group.position[2] - corners[c].position[2] };

				if ((long double)kSoftenTolerance < sqrtl(Dot(difference, difference)) * (long double)scale)
				{
					continue;
				}

				bool samePoint = false;

				for (size_t const member : group.members)
				{
					if (corners[member].point == corners[c].point)
					{
						samePoint = true;
						break;
					}
				}

				bool within = true;

				if (!samePoint)
				{
					for (size_t const member : group.members)
					{
						if (Dot(corners[member].normal, corners[c].normal) < (long double)threshold)
						{
							within = false;
							break;
						}
					}
				}

				if (!within)
				{
					continue;
				}

				// it joins: the group's position is the average of the members' and the new one's
				long double sum[3] = { 0, 0, 0 };

				for (size_t const member : group.members)
				{
					for (int axis = 0; axis < 3; axis++)
					{
						sum[axis] += corners[member].position[axis];
					}
				}

				for (int axis = 0; axis < 3; axis++)
				{
					sum[axis] += corners[c].position[axis];
				}

				group.members.push_back(c);

				for (int axis = 0; axis < 3; axis++)
				{
					group.position[axis] = (double)(sum[axis] / (long double)group.members.size());
				}

				taken = true;
				break;
			}

			if (!taken)
			{
				Group group;

				memcpy(group.position, corners[c].position, sizeof(group.position));
				group.members.push_back(c);
				groups.push_back(group);
			}
		}

		std::vector<ShapeNormal> result(shape->pointNormals);

		for (Group const &group : groups)
		{
			long double sum[3] = { 0, 0, 0 };

			for (size_t const member : group.members)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					sum[axis] += corners[member].normal[axis];
				}
			}

			double unit[3] = { (double)sum[0], (double)sum[1], (double)sum[2] };
			long double const length = sqrtl(Dot(unit, unit));

			if (length != 0.0L)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					unit[axis] = (double)((long double)unit[axis] / length);
				}
			}

			for (size_t const member : group.members)
			{
				ShapeNormal &out = result[corners[member].point];

				out.x = (float)unit[0];
				out.y = (float)unit[1];
				out.z = (float)unit[2];
			}
		}

		size_t different = 0;

		for (size_t v = 0; v < result.size(); v++)
		{
			different += (result[v].x != shape->pointNormals[v].x || result[v].y != shape->pointNormals[v].y || result[v].z != shape->pointNormals[v].z) ? 1 : 0;
		}

		shape->pointNormals.swap(result);

		if (changed)
		{
			*changed = different;
		}

		return true;
	}
}
