// light_shadows.h: inteng's shadow test for the lighting calculation (Light Sourcing): is the straight line from a vertex (moved a little along its normal) to a light's place free of every polygon of the
// level's shapes. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The facts: AvP-docs\engine\inteng\drake-lighting-facts.md.
//
// WHAT inteng DOES (FUN_004fdd40, FUN_004fda00, FUN_004b4c00, "C:\Inteng\3dc\Prelight.c"): for a light with shadows on (the light's local flag 0x2, and not the engine flag 0x10) each vertex is moved by its normal shifted
// right 8 (FUN_004b3800), and the vertex counts as lit only if the move is nearer than the light's range and the segment from it to the light's place crosses no polygon of any shape that the light reaches. A polygon
// is tried only when its box meets the segment's box; the segment crosses its plane when the two ends are on opposite sides (a whole-number dot product with the polygon's normal; an end ON the plane counts as the
// positive side); the crossing point is (A x |dB| + B x |dA|) / (|dA| + |dB|) in 64 bits; the point and the polygon are flattened along the normal's strongest axis (x when |y| < |x| and |z| < |x|, else y when |x| < |y|
// and |z| < |y|, else z) and the point is tested inside the polygon by the even-odd crossing count FUN_004b4c00 (a 32-bit cross product when both spans are below 0xb505, a 64-bit one otherwise).
//
// NAMED: the shapes tried are every module's (inteng tries the shapes in the light's reach, and a shape out of reach cannot meet a segment that ends inside the range); the polygons' corners are the file's, moved by the
// module's place (a module's own matrix is the identity).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace host
{
	namespace light
	{
		class ShadowScene
		{
		public:
			// One shape in the world: its points (x, y, z each, in memory units, already moved by the place), its polygons (each a list of point numbers) with the polygon's normal (16.16 whole numbers) and the
			// shape's box (max x, min x, max y, min y, max z, min z, already moved by the place)
			struct Shape
			{
				std::vector<int32_t> points;
				std::vector<std::vector<int32_t> > polygons;
				std::vector<int32_t> normals;   // 3 per polygon
				int32_t box[6];
			};

			void Add(Shape const &shape);

			// True when nothing stands in the way from a to b
			bool Visible(int32_t const a[3], int32_t const b[3]) const;

			size_t ShapeCount() const { return shapes.size(); }

		private:
			std::vector<Shape> shapes;
		};

		// FUN_004b4c00: is the 2-D point inside the polygon (count points, `stride` numbers each)
		bool PointInPolygon(int32_t const point[2], int32_t const *polygon, int count, int stride);
	}
}
