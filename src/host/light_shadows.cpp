// See light_shadows.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "light_shadows.h"

#include <algorithm>

#include "light_maths.h"

namespace host
{
	namespace light
	{
		namespace
		{
			// the sum of three 16.16 products, as FUN_004fd990
			int32_t Dot(int32_t const a[3], int32_t const b[3])
			{
				return (int32_t)((uint32_t)MulFixed(a[0], b[0]) + (uint32_t)MulFixed(a[1], b[1]) + (uint32_t)MulFixed(a[2], b[2]));
			}

			// FUN_004fd570 / FUN_004fd5e0 / FUN_004fd600: (a x wa + b x wb) / sum in 64 bits, the division truncating
			int32_t Mix(int32_t a, int32_t wa, int32_t b, int32_t wb, int32_t sum)
			{
				if (sum == 0)
				{
					return a;
				}

				return (int32_t)((((int64_t)a * (int64_t)wa) + ((int64_t)b * (int64_t)wb)) / (int64_t)sum);
			}

			int32_t Abs(int32_t v)
			{
				return v < 0 ? (int32_t)(0u - (uint32_t)v) : v;
			}
		}

		bool PointInPolygon(int32_t const point[2], int32_t const *polygon, int count, int stride)
		{
			if (count < 3)
			{
				return false;
			}

			int32_t const x = point[0];
			int32_t const y = point[1];
			int32_t previousX = polygon[(count - 1) * stride];
			int32_t previousY = polygon[(count - 1) * stride + 1];
			int crossings = 0;
			int32_t const *current = polygon;

			for (int i = 0; i < count; i++, current += stride)
			{
				int32_t const currentX = current[0];
				int32_t const currentY = current[1];
				int direction = 0;
				int32_t span = 0;

				if (previousX < x && x <= currentX)
				{
					span = currentX - previousX;
					direction = 1;
				}

				if (currentX < x && x <= previousX)
				{
					direction = -1;
					span = previousX - currentX;
				}

				if (direction != 0)
				{
					if (y < previousY && y < currentY)
					{
						crossings++;
					}
					else if (y <= previousY || y <= currentY)
					{
						int32_t const rise = Abs(currentY - previousY);

						if (span < 0xb505 && rise < 0xb505)
						{
							int32_t const cross = (currentY - previousY) * (x - previousX) - (currentX - previousX) * (y - previousY);

							if ((cross < 0 && direction < 0) || (0 < cross && 0 < direction))
							{
								crossings++;
							}
						}
						else
						{
							int64_t const a = (int64_t)(currentY - previousY) * (int64_t)(x - previousX);
							int64_t const b = (int64_t)(currentX - previousX) * (int64_t)(y - previousY);
							int const compared = a == b ? 0 : (a > b ? 1 : -1);

							if (compared == direction)
							{
								crossings++;
							}
						}
					}
				}

				previousX = currentX;
				previousY = currentY;
			}

			return (crossings & 1) != 0;
		}

		void ShadowScene::Add(Shape const &shape)
		{
			shapes.push_back(shape);
		}

		bool ShadowScene::Visible(int32_t const a[3], int32_t const b[3]) const
		{
			int32_t low[3], high[3];

			for (int axis = 0; axis < 3; axis++)
			{
				low[axis] = (std::min)(a[axis], b[axis]);
				high[axis] = (std::max)(a[axis], b[axis]);
			}

			for (Shape const &shape : shapes)
			{
				// the shape's box (max x, min x, max y, min y, max z, min z) has to meet the segment's
				if (!(low[0] <= shape.box[0] && low[1] <= shape.box[2] && low[2] <= shape.box[4] && shape.box[1] <= high[0] && shape.box[3] <= high[1] && shape.box[5] <= high[2]))
				{
					continue;
				}

				for (size_t k = 0; k < shape.polygons.size(); k++)
				{
					std::vector<int32_t> const &corners = shape.polygons[k];

					if (corners.empty() || 3 * k + 2 >= shape.normals.size())
					{
						continue;
					}

					// the polygon's own box
					int32_t polyLow[3], polyHigh[3];
					bool bad = false;

					for (int axis = 0; axis < 3; axis++)
					{
						polyLow[axis] = polyHigh[axis] = 0;
					}

					for (size_t c = 0; c < corners.size(); c++)
					{
						if (corners[c] < 0 || (size_t)corners[c] * 3 + 2 >= shape.points.size())
						{
							bad = true;
							break;
						}

						for (int axis = 0; axis < 3; axis++)
						{
							int32_t const value = shape.points[(size_t)corners[c] * 3 + axis];

							if (c == 0)
							{
								polyLow[axis] = polyHigh[axis] = value;
							}
							else
							{
								polyLow[axis] = (std::min)(polyLow[axis], value);
								polyHigh[axis] = (std::max)(polyHigh[axis], value);
							}
						}
					}

					if (bad || !(low[0] <= polyHigh[0] && low[1] <= polyHigh[1] && low[2] <= polyHigh[2] && polyLow[0] <= high[0] && polyLow[1] <= high[1] && polyLow[2] <= high[2]))
					{
						continue;
					}

					int32_t const normal[3] = { shape.normals[3 * k], shape.normals[3 * k + 1], shape.normals[3 * k + 2] };
					int32_t const *first = &shape.points[(size_t)corners[0] * 3];
					int32_t const fromA[3] = { (int32_t)((uint32_t)a[0] - (uint32_t)first[0]), (int32_t)((uint32_t)a[1] - (uint32_t)first[1]), (int32_t)((uint32_t)a[2] - (uint32_t)first[2]) };
					int32_t const fromB[3] = { (int32_t)((uint32_t)b[0] - (uint32_t)first[0]), (int32_t)((uint32_t)b[1] - (uint32_t)first[1]), (int32_t)((uint32_t)b[2] - (uint32_t)first[2]) };
					int32_t const sideA = Dot(fromA, normal);
					int32_t const sideB = Dot(fromB, normal);

					// opposite sides only: (a < 0 and b >= 0) or (a >= 0 and b < 0)
					if ((sideA < 0) == (sideB < 0))
					{
						continue;
					}

					int32_t const weightA = Abs(sideA);
					int32_t const weightB = Abs(sideB);
					int32_t const sum = (int32_t)((uint32_t)weightA + (uint32_t)weightB);
					int32_t crossing[3];

					for (int axis = 0; axis < 3; axis++)
					{
						crossing[axis] = Mix(a[axis], weightB, b[axis], weightA, sum);
					}

					int32_t const ax = Abs(normal[0]), ay = Abs(normal[1]), az = Abs(normal[2]);
					int first2d, second2d;

					if (ay < ax && az < ax)
					{
						first2d = 1;
						second2d = 2;
					}
					else if (ax < ay && az < ay)
					{
						first2d = 0;
						second2d = 2;
					}
					else
					{
						first2d = 0;
						second2d = 1;
					}

					int32_t const point[2] = { crossing[first2d], crossing[second2d] };
					std::vector<int32_t> flat;

					for (size_t c = 0; c < corners.size(); c++)
					{
						flat.push_back(shape.points[(size_t)corners[c] * 3 + first2d]);
						flat.push_back(shape.points[(size_t)corners[c] * 3 + second2d]);
					}

					if (PointInPolygon(point, flat.data(), (int)corners.size(), 2))
					{
						return false;
					}
				}
			}

			return true;
		}
	}
}
