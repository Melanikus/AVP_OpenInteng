// See asc_export.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "asc_export.h"

#include <string.h>

namespace host
{
	namespace
	{
		// The 24 half-words FUN_00506f90 stores for the marker box (menu-reads-2.txt:11520-11545), each pair the low word then the high word of a
		// double: twelve numbers, three to a vertex
		uint32_t const kMarkerWords[24] =
		{
			0xc0010c6f, 0xbff487b1,   // vertex 0: x
			0, 0,                     //           y
			0xc0010c6f, 0x400487b1,   //           z
			0xc0010c6f, 0x3ff487b1,   // vertex 1: x
			0, 0,                     //           y
			0xc0010c6f, 0x400487b1,   //           z
			0xc0010c6f, 0x3ff487b1,   // vertex 2: x
			0, 0,                     //           y
			0xc0010c6f, 0xc00487b1,   //           z
			0xc0010c6f, 0xbff487b1,   // vertex 3: x
			0, 0,                     //           y
			0xc0010c6f, 0xc00487b1,   //           z
		};

		double MarkerNumber(size_t at)
		{
			uint64_t const bits = (uint64_t)kMarkerWords[2 * at] | ((uint64_t)kMarkerWords[2 * at + 1] << 32);
			double number = 0;

			memcpy(&number, &bits, sizeof(number));
			return number;
		}
	}

	AscOffset AscOffsetOf(std::vector<AscShape const *> const &shapes)
	{
		// The largest of each negated coordinate, never below 0 (the three start at 0)
		int64_t x = 0;
		int64_t y = 0;
		int64_t z = 0;

		for (AscShape const *const shape : shapes)
		{
			if (!shape)
			{
				continue;
			}

			for (AscPoint const &point : shape->points)
			{
				x = -(int64_t)point.x > x ? -(int64_t)point.x : x;
				y = -(int64_t)point.y > y ? -(int64_t)point.y : y;
				z = -(int64_t)point.z > z ? -(int64_t)point.z : z;
			}
		}

		// A coordinate is an int32, so its negation fits in 33 bits; the offset is held to int32 as inteng's is (a coordinate of -2^31 is not
		// one a level can reach: the game refuses places beyond 2^24)
		AscOffset const offset = { (int32_t)(x > INT32_MAX ? INT32_MAX : x), (int32_t)(y > INT32_MAX ? INT32_MAX : y), (int32_t)(z > INT32_MAX ? INT32_MAX : z) };

		return offset;
	}

	AscObject MakeAscObject(AscShape const &shape, int32_t const place[3], AscOffset const &offset, double scale)
	{
		AscObject object;

		// Each point: ((point + offset + place) + 0.5) * scale; Y is the z, Z is the negated y. The sum is made in 64 bits so a hostile
		// coordinate can't overflow it.
		for (AscPoint const &point : shape.points)
		{
			double const x = (double)((int64_t)point.x + offset.x + place[0]);
			double const y = (double)((int64_t)point.y + offset.y + place[1]);
			double const z = (double)((int64_t)point.z + offset.z + place[2]);
			AscVertex vertex;

			vertex.x = (x + 0.5) * scale;
			vertex.y = (z + 0.5) * scale;
			vertex.z = -((y + 0.5) * scale);
			object.vertices.push_back(vertex);
		}

		size_t const count = shape.polygons.size();

		for (size_t i = 0; i < count; i++)
		{
			AscPolygon const &polygon = shape.polygons[i];
			AscFace face;

			face.a = polygon.corner[0];
			face.b = polygon.corner[1];
			face.c = polygon.corner[2];
			face.ab = 1;
			face.bc = 1;
			face.ca = 1;

			int32_t const other = polygon.mergedWith;

			if (other >= 0 && (size_t)other < count)
			{
				AscPolygon const &joined = shape.polygons[(size_t)other];
				int found = -1;
				int k = 0;

				for (; k < 3; k++)
				{
					bool shared = false;

					for (int m = 0; m < 3; m++)
					{
						shared = shared || polygon.corner[k] == joined.corner[m];
					}

					if (!shared)
					{
						if (found != -1)
						{
							break;
						}

						found = k;
					}
				}

				// Exactly one corner that the other polygon lacks: the edge opposite it was joined away
				if (k == 3 && found != -1)
				{
					int32_t *const edges[3] = { &face.ab, &face.bc, &face.ca };

					*edges[(found + 1) % 3] = 0;
				}
			}

			object.faces.push_back(face);
		}

		return object;
	}

	AscMarker MakeAscMarker(AscOffset const &offset, double scale)
	{
		AscMarker marker;

		for (size_t v = 0; v < 4; v++)
		{
			// X = offset.x * scale + the vertex's x; Y = offset.z * scale + its y; Z = its z - offset.y * scale (FUN_00506f90, 005075ae-005075be)
			marker.vertices[v].x = (double)offset.x * scale + MarkerNumber(3 * v);
			marker.vertices[v].y = (double)offset.z * scale + MarkerNumber(3 * v + 1);
			marker.vertices[v].z = MarkerNumber(3 * v + 2) - (double)offset.y * scale;
		}

		return marker;
	}
}
