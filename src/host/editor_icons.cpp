// See editor_icons.h. Written 2026-11-16 by Vasquez (Sonnet 5.5) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "editor_icons.h"

#include <math.h>

namespace host
{
	namespace
	{
		struct EditorIconTriangle
		{
			int32_t p[9];
			uint32_t colour;
		};

#include "editor_icon_shapes.inc"

		struct Shape
		{
			int index;
			EditorIconTriangle const *triangles;
			size_t count;
		};

		template <size_t N>
		Shape Make(int index, EditorIconTriangle const (&list)[N])
		{
			return Shape{ index, list, N };
		}

		Shape const *Find(int shape)
		{
			static Shape const shapes[] = { Make(2, shape2), Make(3, shape3), Make(4, shape4), Make(7, shape7), Make(8, shape8), Make(9, shape9), Make(13, shape13) };

			for (Shape const &one : shapes)
			{
				if (one.index == shape)
				{
					return &one;
				}
			}

			return nullptr;
		}
	}

	bool EditorIcon_Known(int shape)
	{
		return Find(shape) != nullptr;
	}

	size_t EditorIcon_TriangleCount(int shape)
	{
		Shape const *const found = Find(shape);

		return found ? found->count : 0;
	}

	double EditorIcon_Radius(int shape)
	{
		Shape const *const found = Find(shape);
		double farthest = 0.0;

		for (size_t i = 0; found && i < found->count; i++)
		{
			for (int corner = 0; corner < 3; corner++)
			{
				int32_t const *const p = &found->triangles[i].p[corner * 3];
				double const length = sqrt((double)p[0] * p[0] + (double)p[1] * p[1] + (double)p[2] * p[2]) / 100.0;

				farthest = length > farthest ? length : farthest;
			}
		}

		return farthest;
	}

	void EditorIcon_Add(int shape, double const place[3], double const *axes, uint32_t flash, std::vector<IconWorldTriangle> *out)
	{
		Shape const *const found = Find(shape);

		if (!found || !out || !place)
		{
			return;
		}

		static double const identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
		double const *const m = axes ? axes : identity;

		for (size_t t = 0; t < found->count; t++)
		{
			EditorIconTriangle const &source = found->triangles[t];
			IconWorldTriangle made;
			int32_t *const corners[3] = { made.a, made.b, made.c };

			for (int c = 0; c < 3; c++)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					// the shape's x, y and z go along the rows of the turn
					double sum = 0.0;

					for (int k = 0; k < 3; k++)
					{
						sum += (double)source.p[c * 3 + k] / 100.0 * m[k * 3 + axis];
					}

					corners[c][axis] = (int32_t)(place[axis] + sum);
				}
			}

			made.colour = flash ? flash : (0xFF000000u | source.colour);
			out->push_back(made);
		}
	}
}
