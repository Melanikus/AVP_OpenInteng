// See shape_pane_pack.h. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shape_pane_pack.h"

#include <float.h>
#include <math.h>
#include <string.h>

#include <new>

namespace host
{
	namespace
	{
		// Whether a float is a number: its exponent's eight bits all ones is infinity or NaN. Read from its bits, because the editor builds with /fp:fast
		// (FloatingPointModel Fast), under which a compiler may take every number to be finite and drop an isfinite test
		bool IsNumber(float f)
		{
			uint32_t bits = 0;
			memcpy(&bits, &f, sizeof(bits));
			return (bits & 0x7F800000u) != 0x7F800000u;
		}

		bool Refuse(ShapePanePacked *packed, std::string *why, std::string const &words)
		{
			*packed = ShapePanePacked();

			if (why)
			{
				*why = "the live pane: " + words;
			}

			return false;
		}

		// A face's unit normal: (b - a) x (c - a) made one long, in floats as the card works. 0, 0, 0 when any number on the way isn't one (a side so long its
		// sums pass a float), or the length is too small to divide by (no area, or an area below the smallest whole float): never a length of zero divided by
		void FaceNormal(ShapeThumbCorner const &a, ShapeThumbCorner const &b, ShapeThumbCorner const &c, float n[3])
		{
			float const ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
			float const vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
			float const nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
			float const lengthSquared = nx * nx + ny * ny + nz * nz;

			n[0] = n[1] = n[2] = 0.0f;

			if (!IsNumber(nx) || !IsNumber(ny) || !IsNumber(nz) || !IsNumber(lengthSquared) || !(lengthSquared >= FLT_MIN))
			{
				return;
			}

			float const inverse = 1.0f / sqrtf(lengthSquared);
			float const made[3] = { nx * inverse, ny * inverse, nz * inverse };

			if (IsNumber(made[0]) && IsNumber(made[1]) && IsNumber(made[2]))
			{
				n[0] = made[0];
				n[1] = made[1];
				n[2] = made[2];
			}
		}
	}

	bool ShapePane_Pack(std::vector<ShapeThumbBatch> const &batches, size_t pictureCount, ShapePanePacked *packed, std::string *why)
	{
		*packed = ShapePanePacked();

		// FIRST, EVERY NUMBER CHECKED, before anything is kept: whole triangles, the picture there, each place a number, the triangles under the most
		uint64_t triangles = 0;

		for (ShapeThumbBatch const &batch : batches)
		{
			size_t const corners = batch.corners.size();

			if (corners == 0)
			{
				continue;   // a batch with no triangle is left out
			}

			if (corners % 3 != 0)
			{
				return Refuse(packed, why, "a batch's corners aren't whole triangles (" + std::to_string((unsigned long long)corners) + " corners)");
			}

			if (batch.picture < -1 || (batch.picture >= 0 && (size_t)batch.picture >= pictureCount))
			{
				return Refuse(packed, why, "a batch names picture " + std::to_string(batch.picture) + ", and the shape names " +
					std::to_string((unsigned long long)pictureCount) + " (-1, or 0 to one less, is a picture)");
			}

			triangles += corners / 3;

			if (triangles > kShapePaneMostTriangles)
			{
				return Refuse(packed, why, "more than " + std::to_string((unsigned long long)kShapePaneMostTriangles) + " triangles (a frame could never draw it)");
			}

			for (ShapeThumbCorner const &c : batch.corners)
			{
				if (!IsNumber(c.x) || !IsNumber(c.y) || !IsNumber(c.z) || !IsNumber(c.u) || !IsNumber(c.v))
				{
					return Refuse(packed, why, "a corner's place, or its place in its picture, isn't a number");
				}
			}
		}

		if (triangles == 0)
		{
			return Refuse(packed, why, "no triangle to draw");
		}

		// THEN PACKED: three corners a triangle, in batch order, each with its face's normal; the indices our own numbers, so each is below the corners' count
		try
		{
			ShapePanePacked made;

			made.corners.reserve((size_t)triangles * 3);
			made.indices.reserve((size_t)triangles * 3);

			for (ShapeThumbBatch const &batch : batches)
			{
				if (batch.corners.empty())
				{
					continue;
				}

				ShapePanePackedBatch one;

				one.picture = batch.picture;
				one.firstIndex = (uint32_t)made.indices.size();
				one.triangles = (uint32_t)(batch.corners.size() / 3);
				one.minVertex = (uint32_t)made.corners.size();
				one.vertexCount = (uint32_t)batch.corners.size();

				for (size_t t = 0; t + 2 < batch.corners.size(); t += 3)
				{
					float normal[3];

					FaceNormal(batch.corners[t], batch.corners[t + 1], batch.corners[t + 2], normal);

					for (size_t k = 0; k < 3; k++)
					{
						ShapeThumbCorner const &c = batch.corners[t + k];
						ShapePaneCorner corner = { c.x, c.y, c.z, normal[0], normal[1], normal[2], c.u, c.v };

						made.indices.push_back((uint32_t)made.corners.size());
						made.corners.push_back(corner);
					}
				}

				made.batches.push_back(one);
			}

			made.triangles = (uint32_t)triangles;
			made.bufferBytes = (uint64_t)made.corners.size() * sizeof(ShapePaneCorner) + (uint64_t)made.indices.size() * sizeof(uint32_t);
			*packed = std::move(made);
		}
		catch (std::bad_alloc const &)
		{
			return Refuse(packed, why, "no room in memory to pack it");
		}

		return true;
	}

	bool ShapePane_RoomFor(uint64_t heldBytes, uint64_t addBytes, std::string *why)
	{
		uint64_t const most = kShapePaneStoreMostBytes;

		// by subtraction, so no sum wraps round: what is held must be under the most, and what is added under what is left
		if (heldBytes > most || addBytes > most - heldBytes)
		{
			if (why)
			{
				*why = "the picker's card memory is full (" + std::to_string((unsigned long long)(most / (1024 * 1024))) + " MB): " +
					std::to_string((unsigned long long)(heldBytes / 1024)) + " KB held, " + std::to_string((unsigned long long)(addBytes / 1024)) + " KB more asked";
			}

			return false;
		}

		return true;
	}
}
