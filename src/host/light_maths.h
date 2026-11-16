// light_maths.h: inteng's lighting calculation (Light Sourcing) as functions: what one light gives one vertex, and the values a vertex's colour is saved as. Written 2026-11-15 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The facts: AvP-docs\engine\inteng\drake-lighting-facts.md.
//
// WHAT inteng DOES (the decompile, "C:\Inteng\3dc\Prelight.c"): every light has a place, a direction (the third row of its 3 x 3 matrix, 16.16 whole numbers), a brightness (65536 = 1.0), a spread, a range
// and a colour. For a vertex V (the shape's point, moved by the module's place, in inteng's memory units) with the vertex normal N (16.16 whole numbers): the light's CONTRIBUTION is the intensity
// FUN_004fde60, cut to 0..0xffff. The intensity of a point light: the vertex has to be nearer than the range on every axis and no farther than the range (FUN_004b31b0: the length of the difference as a float, rounded to
// nearest), D = (place - V) turned into a vector of length 65536 (FUN_004fd750: each part x 65536 / the float length, rounded to nearest), D is then scaled by the light's attenuation (FUN_00580400: the spread cone
// for a light that is not Omni (flag 4), faded by cosine[ArcCos(dot) x 1024 / spread] with flag 2; the range with flag 0x100 (linear) or flag 1 (cosine[d x 1024 / range]); the brightness), and the intensity is the
// dot product of N and the scaled D (a sum of 16.16 multiplies: each product shifted right 16 on its own). A vertex's colour is the sum over the lights that are on of contribution x the light's colour (FUN_004fe890),
// and what is saved is, for each of red, green and blue: the sum cut up to the level's ambience, shifted right 8, at most 255 (FUN_004aef90), as red << 16 | green << 8 | blue.
//
// NAMED: float and rounding behaviour is copied from the compiled code as far as it was read (a float is stored where the compiled code stores one, and every conversion to a whole number rounds to nearest, ties
// to even, as the x87's FIST does); a whole-number overflow wraps as a 32-bit int does. The shadow test (FUN_004fdd40) is not here (light_shadows.h).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "shape_model.h"

namespace host
{
	namespace light
	{
		// The engine light flags (include/prototyp.h:680-699, and FUN_004fe480's fields)
		uint32_t const kAtten = 0x1;          // range attenuation by the cosine table
		uint32_t const kSpreadAtten = 0x2;    // the spread cone is faded
		uint32_t const kOmni = 0x4;           // no spread cone
		uint32_t const kConstant = 0x80;      // the light's brightness is not the dot product's
		uint32_t const kLinear = 0x100;       // linear range attenuation
		uint32_t const kSoft = 0x200;         // soft edge
		uint32_t const kNoShadows = 0x10;

		// One light as inteng's engine holds it (FUN_004fe480's 0x74 bytes), in memory units
		struct Light
		{
			Light() : flags(0), point(true), brightness(65536), spread(1000), cosineOfSpread(0), range(0), colour(0), on(true), noPreLight(false), shadows(false)
			{
				for (int i = 0; i < 3; i++)
				{
					place[i] = 0;
					direction[i] = 0;
				}
			}

			uint32_t flags;          // the engine flags
			bool point;              // a point light (field [1] of the engine's light is not 0)
			int32_t place[3];
			int32_t direction[3];    // the third row of the light's matrix, 16.16 whole numbers
			int32_t brightness;
			int32_t spread;
			int32_t cosineOfSpread;  // cosine[spread]
			int32_t range;
			uint32_t colour;         // 0x00RRGGBB
			bool on;
			bool noPreLight;         // LOFlag_NoPreLight: switched off for the game's save
			bool shadows;            // LOFlag_ShadowData: the light casts shadows (FUN_004fe5e0's second argument)
		};

		// FUN_004fdfd0 / FUN_00499b30 / FUN_00580240: the 16.16 multiply
		int32_t MulFixed(int32_t a, int32_t b);

		// FUN_004fd830: the length of a whole-number vector, as a float rounded to nearest
		int32_t IntLength(int32_t const v[3]);

		// FUN_004fd750: the vector scaled to the length 65536, each part rounded to nearest
		void NormaliseTo65536(int32_t v[3]);

		// FUN_004b31b0: the distance between two places
		int32_t Distance(int32_t const a[3], int32_t const b[3]);

		// ArcCos as the game's table gives it (avpx maths.cpp ArcCos): 0..4095
		int32_t ArcCosOf(int32_t c);

		// FUN_004b15e0 (after Hierarchy / Sequence): the vertex normals of a shape made again from its polygons. Each polygon's normal is the cross product of (corner 1 - corner 0) and (corner 2 - corner 0) in 16.16 products
		// (FUN_004b5650: each product (a x b) >> 16), made a vector of length 65536 (FUN_004fd750); it is added to every corner's point; each point's sum, when it is not zero, is made a vector of length 65536 again. Three whole
		// numbers a point, in *out. The shape's points are the file's whole numbers, as the shape record holds them.
		void FlatVertexNormals(model::ShapeModel const &shape, std::vector<int32_t> *out);

		// FUN_004fde60: what the light gives the vertex (before it is cut to 0..0xffff)
		int32_t Intensity(int32_t const vertex[3], int32_t const normal[3], Light const &light);

		// FUN_004fe730's cut: 0..0xffff
		uint16_t Contribution(int32_t const vertex[3], int32_t const normal[3], Light const &light);

		// What FUN_004aef90 saves for one colour part from the sum FUN_004fe890 made (value = max(sum, ambience); the part is value >> 8, or 255 from 0xff00 up)
		int32_t SavedPart(int32_t sum, int32_t ambience);

		// The three parts a vertex's lights add up to (16.8 numbers: contribution x the colour part x 256 >> 16, as FUN_004fe890 adds them), with every light that is on (and, for the game's save, not
		// "No PreLight") counted
		void AddLight(int32_t sum[3], uint16_t contribution, uint32_t colour);

		// A vertex's saved value, red << 16 | green << 8 | blue
		uint32_t SavedValue(int32_t const sum[3], int32_t ambience);
	}
}
