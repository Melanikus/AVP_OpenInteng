// See light_maths.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "light_maths.h"

#include <math.h>

#include "tables.h"

namespace host
{
	namespace light
	{
		namespace
		{
			// A float rounded to the nearest whole number, ties to even, as the x87's FIST does (FUN_004fd740)
			int32_t RoundToInt(float value)
			{
				double const rounded = nearbyint((double)value);

				if (!(rounded > -2147483648.0 && rounded < 2147483647.0))
				{
					return rounded < 0 ? INT32_MIN : INT32_MAX;
				}

				return (int32_t)rounded;
			}

			int32_t Dot(int32_t const a[3], int32_t const b[3])
			{
				return (int32_t)((uint32_t)MulFixed(a[0], b[0]) + (uint32_t)MulFixed(a[1], b[1]) + (uint32_t)MulFixed(a[2], b[2]));
			}

			void Scale(int32_t d[3], int32_t factor)
			{
				d[0] = MulFixed(d[0], factor);
				d[1] = MulFixed(d[1], factor);
				d[2] = MulFixed(d[2], factor);
			}

			// FUN_0057ff50: a x b / c in 64 bits, the division truncating
			int32_t MulDiv(int32_t a, int32_t b, int32_t c)
			{
				if (c == 0)
				{
					return 0;
				}

				return (int32_t)(((int64_t)a * (int64_t)b) / (int64_t)c);
			}

			int32_t CosineAt(int32_t index)
			{
				if (index < 0 || index >= kTableSize)
				{
					return 0;
				}

				return cosine[index];
			}

			// FUN_00580400: scales d (the direction to the vertex) by the light's attenuation, and says whether the vertex is lit at all
			bool Attenuate(Light const &light, int32_t distance, int32_t d[3])
			{
				bool lit = true;

				if ((light.flags & kOmni) == 0)
				{
					int32_t const away[3] = { -light.direction[0], -light.direction[1], -light.direction[2] };
					int32_t const dot = Dot(d, away);

					if (dot < light.cosineOfSpread)
					{
						lit = false;
					}
					else if ((light.flags & kSpreadAtten) != 0)
					{
						int32_t const angle = ArcCosOf(dot);
						int32_t const index = MulDiv(angle, 0x400, light.spread);
						Scale(d, CosineAt(index));
					}
				}

				if ((light.flags & kLinear) != 0)
				{
					// FUN_005803e0: ((range - distance) << 16) / range, in 64 bits
					int32_t const factor = light.range == 0 ? 0 : (int32_t)((((int64_t)(light.range - distance)) * 65536) / (int64_t)light.range);
					Scale(d, factor);
				}
				else if ((light.flags & kAtten) != 0)
				{
					Scale(d, CosineAt(MulDiv(distance, 0x400, light.range)));
				}

				if (light.brightness == 0)
				{
					return false;
				}

				if (light.brightness != 0x10000)
				{
					Scale(d, light.brightness);
				}

				return lit;
			}
		}

		int32_t MulFixed(int32_t a, int32_t b)
		{
			return (int32_t)(uint32_t)(((int64_t)a * (int64_t)b) >> 16);
		}

		int32_t IntLength(int32_t const v[3])
		{
			double const sum = (double)v[0] * (double)v[0] + (double)v[1] * (double)v[1] + (double)v[2] * (double)v[2];
			return RoundToInt((float)sqrt(sum));
		}

		void NormaliseTo65536(int32_t v[3])
		{
			float const x = (float)v[0];
			float const y = (float)v[1];
			float const z = (float)v[2];
			float const length = (float)sqrt((double)x * (double)x + (double)y * (double)y + (double)z * (double)z);

			v[0] = RoundToInt((float)((double)x * 65536.0 / (double)length));
			v[1] = RoundToInt((float)((double)y * 65536.0 / (double)length));
			v[2] = RoundToInt((float)((double)z * 65536.0 / (double)length));
		}

		int32_t Distance(int32_t const a[3], int32_t const b[3])
		{
			int32_t const difference[3] = { (int32_t)((uint32_t)a[0] - (uint32_t)b[0]), (int32_t)((uint32_t)a[1] - (uint32_t)b[1]), (int32_t)((uint32_t)a[2] - (uint32_t)b[2]) };
			return IntLength(difference);
		}

		int32_t ArcCosOf(int32_t c)
		{
			if (c < -65535)
			{
				c = -65535;
			}
			else if (c > 65535)
			{
				c = 65535;
			}

			return (int32_t)(uint16_t)ArcCosTable[(c >> 5) + 2048] & 0xfff;
		}

		void FlatVertexNormals(model::ShapeModel const &shape, std::vector<int32_t> *out)
		{
			out->assign(shape.points.size() * 3, 0);

			for (size_t p = 0; p < shape.polygons.size(); p++)
			{
				model::ShapePolygon const &polygon = shape.polygons[p];

				if (polygon.cornerCount < 3)
				{
					continue;
				}

				bool usable = true;

				for (int c = 0; c < polygon.cornerCount; c++)
				{
					usable = usable && polygon.corners[c] >= 0 && (size_t)polygon.corners[c] < shape.points.size();
				}

				if (!usable)
				{
					continue;
				}

				model::ShapePoint const &a = shape.points[(size_t)polygon.corners[0]];
				model::ShapePoint const &b = shape.points[(size_t)polygon.corners[1]];
				model::ShapePoint const &c = shape.points[(size_t)polygon.corners[2]];
				int32_t const u[3] = { (int32_t)((uint32_t)b.x - (uint32_t)a.x), (int32_t)((uint32_t)b.y - (uint32_t)a.y), (int32_t)((uint32_t)b.z - (uint32_t)a.z) };
				int32_t const w[3] = { (int32_t)((uint32_t)c.x - (uint32_t)a.x), (int32_t)((uint32_t)c.y - (uint32_t)a.y), (int32_t)((uint32_t)c.z - (uint32_t)a.z) };
				int32_t face[3] = { (int32_t)((uint32_t)MulFixed(u[1], w[2]) - (uint32_t)MulFixed(u[2], w[1])), (int32_t)((uint32_t)MulFixed(u[2], w[0]) - (uint32_t)MulFixed(u[0], w[2])),
					(int32_t)((uint32_t)MulFixed(u[0], w[1]) - (uint32_t)MulFixed(u[1], w[0])) };

				if (face[0] != 0 || face[1] != 0 || face[2] != 0)
				{
					NormaliseTo65536(face);
				}

				for (int k = 0; k < polygon.cornerCount; k++)
				{
					size_t const at = 3 * (size_t)polygon.corners[k];

					(*out)[at] += face[0];
					(*out)[at + 1] += face[1];
					(*out)[at + 2] += face[2];
				}
			}

			for (size_t v = 0; v < shape.points.size(); v++)
			{
				if ((*out)[3 * v] != 0 || (*out)[3 * v + 1] != 0 || (*out)[3 * v + 2] != 0)
				{
					NormaliseTo65536(&(*out)[3 * v]);
				}
			}
		}

		int32_t Intensity(int32_t const vertex[3], int32_t const normal[3], Light const &light)
		{
			if (!light.point)
			{
				// A directional light (field [1] zero): the brightness, or the normal's dot with the direction scaled by the brightness
				if ((light.flags & kConstant) != 0)
				{
					return light.brightness;
				}

				if (light.brightness == 0)
				{
					return 0;
				}

				int32_t scaled[3] = { light.direction[0], light.direction[1], light.direction[2] };

				if (light.brightness != 0x10000)
				{
					Scale(scaled, light.brightness);
				}

				return Dot(normal, scaled);
			}

			// every axis difference strictly less than the range (FUN_004b31f0)
			for (int axis = 0; axis < 3; axis++)
			{
				int64_t const difference = (int64_t)vertex[axis] - (int64_t)light.place[axis];

				if ((difference < 0 ? -difference : difference) >= (int64_t)light.range)
				{
					return 0;
				}
			}

			int32_t const distance = Distance(vertex, light.place);

			if (distance > light.range)
			{
				return 0;
			}

			int32_t d[3] = { (int32_t)((uint32_t)light.place[0] - (uint32_t)vertex[0]), (int32_t)((uint32_t)light.place[1] - (uint32_t)vertex[1]), (int32_t)((uint32_t)light.place[2] - (uint32_t)vertex[2]) };
			NormaliseTo65536(d);

			if (!Attenuate(light, distance, d))
			{
				return 0;
			}

			if ((light.flags & kConstant) != 0)
			{
				return IntLength(d);
			}

			int32_t intensity = Dot(normal, d);

			if ((light.flags & kSoft) != 0)
			{
				int32_t const length = IntLength(d);

				if (intensity < 1)
				{
					return (length + ((length >> 31) & 7)) >> 3;
				}

				intensity = (length + intensity) / 2;
			}

			return intensity;
		}

		uint16_t Contribution(int32_t const vertex[3], int32_t const normal[3], Light const &light)
		{
			int32_t const value = Intensity(vertex, normal, light);

			if (value < 0)
			{
				return 0;
			}

			return value > 0xffff ? (uint16_t)0xffff : (uint16_t)value;
		}

		int32_t SavedPart(int32_t sum, int32_t ambience)
		{
			int32_t value = sum;

			if (value <= ambience)
			{
				value = ambience;
			}

			return ((uint32_t)value & 0xffffff00u) < 0xff00u ? (value >> 8) : 255;
		}

		void AddLight(int32_t sum[3], uint16_t contribution, uint32_t colour)
		{
			sum[0] += MulFixed((int32_t)contribution, (int32_t)(((colour >> 16) & 0xffffu) << 8));
			sum[1] += MulFixed((int32_t)contribution, (int32_t)(colour & 0xff00u));
			sum[2] += MulFixed((int32_t)contribution, (int32_t)((colour & 0xffu) << 8));
		}

		uint32_t SavedValue(int32_t const sum[3], int32_t ambience)
		{
			return ((uint32_t)SavedPart(sum[0], ambience) << 16) | ((uint32_t)SavedPart(sum[1], ambience) << 8) | (uint32_t)SavedPart(sum[2], ambience);
		}
	}
}
