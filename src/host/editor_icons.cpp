// See editor_icons.h. Written 2026-11-16 by Vasquez (Sonnet 5.5) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "editor_icons.h"

#include <math.h>

#include <string>

namespace host
{
	namespace
	{
		struct EditorIconTriangle
		{
			int32_t p[9];
			uint32_t colour;
		};

		struct Shape
		{
			int index;
			std::vector<EditorIconTriangle> triangles;
		};

		int const kKnown[] = { 2, 3, 4, 7, 8, 9, 13 };

		// OUR OWN plain marker (Bishop's ruling 11-16), for a shape not read from inteng.exe: a box 500 mm across (inteng's units, 100 to a millimetre), centred on the place
		int32_t const kBoxHalf = 25000;

		uint32_t BoxColour(int shape)
		{
			switch (shape)
			{
			case 3: return 0x30d030;    // generator
			case 7: return 0xe03030;    // bad guy
			case 9: return 0xffffff;    // player start
			case 8: return 0x30d0e0;    // sound
			case 2: return 0xf0a030;    // particle generator (and the light marker)
			case 4: return 0xd030d0;    // placed hierarchy
			case 13: return 0x3080f0;   // virtual object
			}

			return 0xc0c0c0;
		}

		std::vector<EditorIconTriangle> Box(uint32_t colour)
		{
			// the 8 corners (bit 0: x, bit 1: y, bit 2: z) and the 6 faces as two triangles each
			static int const faces[6][4] = { { 0, 1, 3, 2 }, { 4, 6, 7, 5 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 5, 7, 3 } };
			std::vector<EditorIconTriangle> out;

			for (auto const &face : faces)
			{
				for (int half = 0; half < 2; half++)
				{
					int const corner[3] = { face[0], face[half + 1], face[half + 2] };
					EditorIconTriangle t;

					for (int c = 0; c < 3; c++)
					{
						for (int axis = 0; axis < 3; axis++)
						{
							t.p[c * 3 + axis] = (corner[c] >> axis) & 1 ? kBoxHalf : -kBoxHalf;
						}
					}

					t.colour = colour;
					out.push_back(t);
				}
			}

			return out;
		}

		std::vector<Shape> &Shapes()
		{
			static std::vector<Shape> shapes;

			if (shapes.empty())
			{
				for (int index : kKnown)
				{
					shapes.push_back(Shape{ index, Box(BoxColour(index)) });
				}
			}

			return shapes;
		}

		std::string &FromWords()
		{
			static std::string words = "our own plain boxes (the shapes were not read from inteng.exe)";

			return words;
		}

		Shape const *Find(int shape)
		{
			for (Shape const &one : Shapes())
			{
				if (one.index == shape)
				{
					return &one;
				}
			}

			return nullptr;
		}
	}

	char const *EditorIcon_From()
	{
		return FromWords().c_str();
	}

	void EditorIcon_UseShapes(IntengShapes const &shapes, std::string const &from)
	{
		int read = 0;

		for (Shape &one : Shapes())
		{
			if (one.index < 0 || one.index >= kIntengShapeSlots || !shapes.have[one.index] || shapes.triangles[one.index].empty())
			{
				continue;
			}

			one.triangles.clear();

			for (IntengShapeTriangle const &t : shapes.triangles[one.index])
			{
				EditorIconTriangle made;

				for (int i = 0; i < 9; i++)
				{
					made.p[i] = t.p[i];
				}

				made.colour = t.colour & 0xffffffu;
				one.triangles.push_back(made);
			}

			read++;
		}

		FromWords() = read == (int)(sizeof(kKnown) / sizeof(kKnown[0])) ? "inteng's own shapes, read from " + from : "inteng's own shapes for some objects, our plain boxes for the rest (" + from + ")";
	}

	void EditorIcon_NotRead(std::string const &why)
	{
		FromWords() = "our own plain boxes: " + why;
	}

	bool EditorIcon_Known(int shape)
	{
		return Find(shape) != nullptr;
	}

	size_t EditorIcon_TriangleCount(int shape)
	{
		Shape const *const found = Find(shape);

		return found ? found->triangles.size() : 0;
	}

	double EditorIcon_Radius(int shape)
	{
		Shape const *const found = Find(shape);
		double farthest = 0.0;

		for (size_t i = 0; found && i < found->triangles.size(); i++)
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

		for (size_t t = 0; t < found->triangles.size(); t++)
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
