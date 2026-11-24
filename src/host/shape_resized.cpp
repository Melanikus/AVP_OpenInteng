// See shape_resized.h. Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "shape_resized.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace host
{
	namespace
	{
		std::string Lower(std::string s)
		{
			for (char &c : s)
			{
				c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
			}

			return s;
		}

		std::string Count(size_t n)
		{
			return std::to_string((unsigned long long)n);
		}

		// Whether a float is a number: its exponent's eight bits all ones is infinity or NaN. Read from its bits, because the editor builds with /fp:fast, under
		// which a compiler may take every number to be finite and drop an isfinite test, and a NaN makes every comparison false, so "off by more than the slack"
		// would never be true and a NaN would count as the same (Frost's SHOULD 1 on a844d4dc; as shape_pane_pack.cpp's IsNumber)
		bool IsNumber(float f)
		{
			uint32_t bits = 0;

			memcpy(&bits, &f, sizeof(bits));
			return (bits & 0x7F800000u) != 0x7F800000u;
		}

		// The texture name a polygon is drawn with, in small letters, or "" when it is drawn shaded (its picture -1, or a number past the list: never read past it)
		std::string PictureName(ShapeThumbModel const &model, int picture)
		{
			if (picture < 0 || (size_t)picture >= model.pictures.size())
			{
				return std::string();
			}

			return Lower(model.pictures[(size_t)picture].name);
		}

		// Polygon p of both, the same in every part the scale doesn't touch; *why says which part differs
		bool SamePolygon(ShapeThumbModel const &copy, ShapeThumbModel const &stock, size_t p, std::string *why)
		{
			ShapeThumbPolygon const &a = copy.polygons[p];
			ShapeThumbPolygon const &b = stock.polygons[p];
			std::string const at = "polygon " + Count(p);

			if (a.cornerCount != b.cornerCount || a.cornerCount < 3 || a.cornerCount > 5)
			{
				*why = at + " has another number of corners";
				return false;
			}

			for (int c = 0; c < a.cornerCount; c++)
			{
				if (a.corners[c] != b.corners[c])
				{
					*why = at + " joins other points";
					return false;
				}

				if (!IsNumber(a.texels[c][0]) || !IsNumber(a.texels[c][1]) || !IsNumber(b.texels[c][0]) || !IsNumber(b.texels[c][1]))
				{
					*why = at + " has a place in its picture that isn't a number";
					return false;
				}

				if (!(fabs((double)a.texels[c][0] - b.texels[c][0]) <= kShapeResizedTexelSlack && fabs((double)a.texels[c][1] - b.texels[c][1]) <= kShapeResizedTexelSlack))
				{
					*why = at + " takes other places in its picture";
					return false;
				}
			}

			if (a.engineType != b.engineType || a.flags != b.flags)
			{
				*why = at + " has another item type or other flags";
				return false;
			}

			if ((a.picture < 0) != (b.picture < 0) || PictureName(copy, a.picture) != PictureName(stock, b.picture))
			{
				*why = at + " is drawn with another picture";
				return false;
			}

			return true;
		}
	}

	ShapeResizedAnswer ShapeResized_Compare(ShapeThumbModel const &copy, ShapeThumbModel const &stock)
	{
		ShapeResizedAnswer answer;

		// THE COUNTS FIRST: nothing below reads past either shape, and the work is bounded before it starts
		if (copy.parts != 1 || stock.parts != 1)
		{
			answer.why = "a file of more than one shape is never compared";
			return answer;
		}

		if ((copy.shapeFlags & ~kShapeResizedOutsideMark) != (stock.shapeFlags & ~kShapeResizedOutsideMark))   // (the mark of where it came from left out)
		{
			char flags[64];

			snprintf(flags, sizeof flags, " (0x%X and 0x%X)", (unsigned)copy.shapeFlags, (unsigned)stock.shapeFlags);
			answer.why = std::string("other shape flags") + flags;
			return answer;
		}

		if (copy.points.size() % 3 != 0 || stock.points.size() % 3 != 0 || copy.points.empty() || stock.points.empty())
		{
			answer.why = "a shape with no whole points";
			return answer;
		}

		size_t const points = stock.points.size() / 3;

		if (copy.points.size() != stock.points.size())
		{
			answer.why = "another number of points: " + Count(copy.points.size() / 3) + " and " + Count(points);
			return answer;
		}

		if (points > kShapeResizedMostPoints)
		{
			answer.why = "more than " + Count(kShapeResizedMostPoints) + " points";
			return answer;
		}

		if (copy.polygons.size() != stock.polygons.size())
		{
			answer.why = "another number of polygons: " + Count(copy.polygons.size()) + " and " + Count(stock.polygons.size());
			return answer;
		}

		if (stock.polygons.empty() || stock.polygons.size() > kShapeResizedMostPolygons)
		{
			answer.why = stock.polygons.empty() ? "a shape with no polygon" : "more than " + Count(kShapeResizedMostPolygons) + " polygons";
			return answer;
		}

		// THE POLYGONS: the same, in the same order (a corner's point number is checked against the count, so it names a point both shapes have)
		for (size_t p = 0; p < stock.polygons.size(); p++)
		{
			if (!SamePolygon(copy, stock, p, &answer.why))
			{
				return answer;
			}

			for (int c = 0; c < stock.polygons[p].cornerCount; c++)
			{
				if (stock.polygons[p].corners[c] < 0 || (size_t)stock.polygons[p].corners[c] >= points)
				{
					answer.why = "polygon " + Count(p) + " names a point the shape doesn't have";
					return answer;
				}
			}
		}

		// THE SCALE: the least-squares one through the origin, over every coordinate (doubles: 16,384 points of a few metres sum well inside their range)
		double copyTimesStock = 0, stockTimesStock = 0, farthest = 0;

		for (size_t k = 0; k < stock.points.size(); k++)
		{
			double const a = copy.points[k], b = stock.points[k];

			if (!IsNumber(copy.points[k]) || !IsNumber(stock.points[k]))   // (by its bits: /fp:fast, above)
			{
				answer.why = "a point that isn't a number";
				return answer;
			}

			copyTimesStock += a * b;
			stockTimesStock += b * b;
		}

		// the stock shape's size: the half-diagonal of its points' own box (worked out here, from the points alone)
		double low[3] = { stock.points[0], stock.points[1], stock.points[2] }, high[3] = { low[0], low[1], low[2] };

		for (size_t q = 1; q < points; q++)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				double const v = stock.points[q * 3 + axis];

				low[axis] = v < low[axis] ? v : low[axis];
				high[axis] = v > high[axis] ? v : high[axis];
			}
		}

		for (int axis = 0; axis < 3; axis++)
		{
			farthest += (high[axis] - low[axis]) * (high[axis] - low[axis]) / 4;
		}

		if (sqrt(farthest) < kShapeResizedLeastRadiusMm || stockTimesStock <= 0)
		{
			answer.why = "the stock shape is too small to tell a scale by";
			return answer;
		}

		double const scale = copyTimesStock / stockTimesStock;

		answer.scale = scale;

		if (!(scale >= kShapeResizedLeastScale && scale <= kShapeResizedMostScale))
		{
			answer.why = scale <= 0 ? "no scale greater than 0 fits (a mirrored or turned copy)" : "a scale past what is compared (1/64 to 64)";
			return answer;
		}

		// AT THE STOCK SHAPE'S OWN SIZE nothing was rescaled, so nothing was rounded: such a copy is the stock shape only when EVERY POINT IS EXACTLY WHERE THE
		// STOCK ONE IS (0 mm, at scale exactly 1), whatever its other bytes (Bishop's (a), the owner's dupes, 2026-11-24: the old tools' package's Pulse.RIF is
		// Steam's pulse.rif point for point, its table row "anchored", but other bytes). An EDITED shape at that size is never taken for it: measured, medbox01
		// with one point moved 1 mm fit scale 1.000 within the rounding's 1.01 mm, so the rounding's allowance is never given at the stock size
		answer.sameSize = fabs(scale - 1.0) < kShapeResizedLeastChange;

		double const fitted = answer.sameSize ? 1.0 : scale;

		answer.scale = fitted;

		// THE POINTS: each coordinate of the copy within the tolerance of the stock one times that one scale (none at the stock size)
		answer.allowedMm = answer.sameSize ? 0.0 : kShapeResizedRoundingMm * (1.0 + scale) + kShapeResizedSlackMm;

		for (size_t k = 0; k < stock.points.size(); k++)
		{
			double const off = fabs((double)copy.points[k] - fitted * stock.points[k]);

			answer.worstMm = off > answer.worstMm ? off : answer.worstMm;
		}

		if (!(answer.worstMm <= answer.allowedMm))
		{
			answer.why = answer.sameSize ? "at the stock shape's own size every point must be exactly where its is, and one is off (an edited shape is never taken for it)" :
				"no one scale takes every point there (a stretched, mirrored, moved or other shape)";
			return answer;
		}

		answer.same = true;
		return answer;
	}
}
