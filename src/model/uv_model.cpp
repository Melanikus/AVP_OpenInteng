// See uv_model.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "uv_model.h"

#include <string.h>

#include "real_numbers.h"   // IsRealNumber, by the bits: the editor is built with /fp:fast

namespace model
{
	namespace
	{
		int32_t Int(uint8_t const *at)
		{
			int32_t value;

			memcpy(&value, at, sizeof(value));
			return value;
		}

		float Float(uint8_t const *at)
		{
			float value;

			memcpy(&value, at, sizeof(value));
			return value;
		}

		void PutInt(uint8_t *at, int32_t value)
		{
			memcpy(at, &value, sizeof(value));
		}

		void PutFloat(uint8_t *at, float value)
		{
			memcpy(at, &value, sizeof(value));
		}

		bool Whole(uint8_t const *bytes, size_t size)
		{
			return bytes != nullptr || size == 0;
		}

		size_t const kCountBytes = 4;
		size_t const kCornerBytes = 8;   // a ChunkUV: u and v, two floats
		int32_t const kTexturedMark = 0x8000;   // a textured polygon's colour with this bit is refused by the game's loader (chunk_shape.cpp:319)

		bool TexelFits(float value)
		{
			// a real number by its bits first, because under /fp:fast a NaN compares true or false at will; the range only once it is a real number
			return IsRealNumber(value) && value >= -kMostTexel && value <= kMostTexel;
		}

		// Walks the lists of a SHPUVCRD payload: checks every rule, and when `lists` is not null fills it. Shared by the reader and the writer's check.
		UvModelResult Walk(uint8_t const *payload, size_t size, std::vector<UvList> *lists, std::vector<size_t> *corner_places)
		{
			if (!Whole(payload, size))
			{
				return UvModelResult::BadCall;
			}

			if (size < kCountBytes)
			{
				return UvModelResult::TooShort;
			}

			int32_t const count = Int(payload);

			if (count < 0 || (size_t)count > (size - kCountBytes) / kCountBytes)
			{
				return UvModelResult::CountDoesNotFit;
			}

			if ((size_t)count > kMostUvLists)
			{
				return UvModelResult::TooBig;
			}

			size_t at = kCountBytes;

			for (int32_t i = 0; i < count; i++)
			{
				if (size - at < kCountBytes)
				{
					return UvModelResult::TooShort;
				}

				int32_t const corners = Int(payload + at);

				at += kCountBytes;

				if (corners < 0 || corners > kMostUvCorners || (size - at) / kCornerBytes < (size_t)corners)
				{
					return UvModelResult::BadCornerCount;
				}

				UvList list = {};

				list.cornerCount = corners;

				for (int32_t j = 0; j < corners; j++)
				{
					float const u = Float(payload + at);
					float const v = Float(payload + at + 4);

					if (!TexelFits(u) || !TexelFits(v))
					{
						return UvModelResult::BadTexel;
					}

					list.corners[j].u = u;
					list.corners[j].v = v;
					at += kCornerBytes;
				}

				if (lists)
				{
					lists->push_back(list);
				}

				if (corner_places)
				{
					corner_places->push_back(at - (size_t)corners * kCornerBytes);
				}
			}

			return UvModelResult::Ok;
		}
	}

	char const *UvModelWords(UvModelResult result)
	{
		switch (result)
		{
		case UvModelResult::Ok: return "done";
		case UvModelResult::BadCall: return "nothing to read or change";
		case UvModelResult::TooShort: return "the texture coordinates end too soon";
		case UvModelResult::TooBig: return "the shape has more texture lists than the editor reads";
		case UvModelResult::CountDoesNotFit: return "the texture coordinates say more lists or names than their bytes can hold";
		case UvModelResult::BadCornerCount: return "a texture list has a count of corners the bytes or the game can't hold";
		case UvModelResult::BadTexel: return "a texture coordinate is not a real number or lies too far from the picture";
		case UvModelResult::WrongShape: return "the texture lists differ in number or corners from the bytes they would replace";
		case UvModelResult::NotTextured: return "the polygon is not a textured one";
		case UvModelResult::BadColour: return "the textured polygon's colour has a bit the game refuses";
		case UvModelResult::NoList: return "the polygon's texture list is not one the shape has";
		case UvModelResult::ListTooShort: return "the polygon's texture list has fewer corners than the polygon";
		case UvModelResult::BadName: return "a picture name has no end or is too long";
		case UvModelResult::NoPolygon: return "the shape has no such polygon";
		case UvModelResult::BadFlag: return "the flag change would set a bit only the game's loader sets";
		case UvModelResult::BadWindow: return "the texture window is too small for a turn";
		case UvModelResult::BadPlane: return "the chosen polygon has an edge of no length";
		}

		return "unknown answer";
	}

	UvModelResult ReadUvLists(uint8_t const *payload, size_t size, std::vector<UvList> *lists)
	{
		if (!lists)
		{
			return UvModelResult::BadCall;
		}

		lists->clear();

		std::vector<UvList> read;
		UvModelResult const answer = Walk(payload, size, &read, nullptr);

		if (answer == UvModelResult::Ok)
		{
			lists->swap(read);
		}

		return answer;
	}

	UvModelResult WriteUvListsSameShape(std::vector<UvList> const &lists, std::vector<uint8_t> *payload)
	{
		if (!payload)
		{
			return UvModelResult::BadCall;
		}

		// The bytes as they are say how many lists there are and where each one's corners begin; the lists handed in must match that exactly
		std::vector<UvList> now;
		std::vector<size_t> places;
		UvModelResult const answer = Walk(payload->data(), payload->size(), &now, &places);

		if (answer != UvModelResult::Ok)
		{
			return answer;
		}

		if (now.size() != lists.size())
		{
			return UvModelResult::WrongShape;
		}

		for (size_t i = 0; i < lists.size(); i++)
		{
			if (lists[i].cornerCount != now[i].cornerCount)
			{
				return UvModelResult::WrongShape;
			}

			for (int j = 0; j < lists[i].cornerCount; j++)
			{
				if (!TexelFits(lists[i].corners[j].u) || !TexelFits(lists[i].corners[j].v))
				{
					return UvModelResult::BadTexel;
				}
			}
		}

		// Everything is checked: now the numbers go in, each corner at the place the walk found. The counts and every other byte stay as they were.
		for (size_t i = 0; i < lists.size(); i++)
		{
			for (int j = 0; j < lists[i].cornerCount; j++)
			{
				uint8_t *const at = payload->data() + places[i] + (size_t)j * kCornerBytes;

				PutFloat(at, lists[i].corners[j].u);
				PutFloat(at + 4, lists[i].corners[j].v);
			}
		}

		return UvModelResult::Ok;
	}

	UvModelResult ReadTextureNames(uint8_t const *payload, size_t size, std::vector<std::string> *names)
	{
		if (!names)
		{
			return UvModelResult::BadCall;
		}

		names->clear();

		if (!Whole(payload, size))
		{
			return UvModelResult::BadCall;
		}

		if (size < kCountBytes)
		{
			return UvModelResult::TooShort;
		}

		int32_t const count = Int(payload);

		// each name takes at least its end byte, as the game's CountIfItFits(count, 1, bytes left) measures it
		if (count < 0 || (size_t)count > size - kCountBytes)
		{
			return UvModelResult::CountDoesNotFit;
		}

		if ((size_t)count > kMostTextureNames)
		{
			return UvModelResult::TooBig;
		}

		std::vector<std::string> read;
		size_t at = kCountBytes;

		for (int32_t i = 0; i < count; i++)
		{
			size_t length = 0;

			while (at + length < size && payload[at + length] != 0)
			{
				length++;
			}

			if (at + length >= size || length > kMostTextureNameBytes)
			{
				return UvModelResult::BadName;
			}

			read.push_back(std::string((char const *)payload + at, length));
			at += length + 1;
		}

		names->swap(read);
		return UvModelResult::Ok;
	}

	bool EngineTypeIsTextured(int32_t engineType)
	{
		// shape.h's ShapeItems: I_2dTexturedPolygon 5, I_Gouraud2dTexturedPolygon 6, I_3dTexturedPolygon 7, I_ZB_2dTexturedPolygon 21,
		// I_ZB_Gouraud2dTexturedPolygon 22, I_ZB_3dTexturedPolygon 23, I_Gouraud3dTexturedPolygon 24, I_ZB_Gouraud3dTexturedPolygon 25
		return (engineType >= 5 && engineType <= 7) || (engineType >= 21 && engineType <= 25);
	}

	int32_t PolygonPictureNumber(int32_t colour)
	{
		return colour & 0x7fff;
	}

	int32_t PolygonUvListNumber(int32_t colour)
	{
		return (int32_t)((uint32_t)colour >> 16);
	}

	int32_t MakePolygonColour(int32_t pictureNumber, int32_t uvListNumber)
	{
		return (int32_t)(((uint32_t)uvListNumber << 16) | ((uint32_t)pictureNumber & 0x7fff));
	}

	UvModelResult ReadPolygonUv(ShapeModel const &shape, std::vector<UvList> const &lists, int32_t polygonNumber, PolygonUv *out)
	{
		if (!out)
		{
			return UvModelResult::BadCall;
		}

		if (polygonNumber < 0 || (size_t)polygonNumber >= shape.polygons.size())
		{
			return UvModelResult::NoPolygon;
		}

		ShapePolygon const &polygon = shape.polygons[(size_t)polygonNumber];
		PolygonUv result = {};

		result.polygonNumber = polygonNumber;
		result.engineType = polygon.engineType;
		result.flags = polygon.flags;
		result.colour = polygon.colour;
		result.cornerCount = polygon.cornerCount;
		result.pictureNumber = -1;
		result.uvListNumber = -1;
		result.textured = EngineTypeIsTextured(polygon.engineType);

		if (result.textured)
		{
			if (polygon.colour & kTexturedMark)
			{
				return UvModelResult::BadColour;
			}

			int32_t const list = PolygonUvListNumber(polygon.colour);

			if (list < 0 || (size_t)list >= lists.size())
			{
				return UvModelResult::NoList;
			}

			UvList const &held = lists[(size_t)list];

			if (held.cornerCount != 0 && held.cornerCount < polygon.cornerCount)
			{
				return UvModelResult::ListTooShort;
			}

			result.pictureNumber = PolygonPictureNumber(polygon.colour);
			result.uvListNumber = list;
			result.uv = held;
		}

		*out = result;
		return UvModelResult::Ok;
	}

	std::vector<int32_t> PolygonsUsingList(ShapeModel const &shape, int32_t uvListNumber)
	{
		std::vector<int32_t> users;

		for (size_t i = 0; i < shape.polygons.size(); i++)
		{
			ShapePolygon const &polygon = shape.polygons[i];

			if (EngineTypeIsTextured(polygon.engineType) && !(polygon.colour & kTexturedMark) && PolygonUvListNumber(polygon.colour) == uvListNumber)
			{
				users.push_back((int32_t)i);
			}
		}

		return users;
	}

	UvModelResult PutPolygonUv(PolygonUv const &polygon, std::vector<UvList> *lists)
	{
		if (!lists)
		{
			return UvModelResult::BadCall;
		}

		if (!polygon.textured)
		{
			return UvModelResult::NotTextured;
		}

		if (polygon.uvListNumber < 0 || (size_t)polygon.uvListNumber >= lists->size())
		{
			return UvModelResult::NoList;
		}

		UvList &held = (*lists)[(size_t)polygon.uvListNumber];

		if (polygon.uv.cornerCount != held.cornerCount)
		{
			return UvModelResult::WrongShape;
		}

		for (int j = 0; j < polygon.uv.cornerCount; j++)
		{
			if (!TexelFits(polygon.uv.corners[j].u) || !TexelFits(polygon.uv.corners[j].v))
			{
				return UvModelResult::BadTexel;
			}
		}

		held = polygon.uv;
		return UvModelResult::Ok;
	}

	namespace
	{
		// inteng's __ftol: a double to a whole number, toward zero
		int32_t Trunc(double x)
		{
			return (int32_t)x;
		}

		// The corners of the polygon's list as whole numbers, the move applied to each (u, v) -> (u', v') through `move`, every result checked, then written. The new
		// list is made first; a failing corner leaves the polygon as it was.
		template <typename Move>
		UvModelResult MoveCorners(PolygonUv *polygon, Move move)
		{
			if (!polygon)
			{
				return UvModelResult::BadCall;
			}

			if (!polygon->textured)
			{
				return UvModelResult::NotTextured;
			}

			UvList made = polygon->uv;

			for (int j = 0; j < made.cornerCount; j++)
			{
				int32_t const u = Trunc((double)made.corners[j].u);
				int32_t const v = Trunc((double)made.corners[j].v);
				int32_t u2 = 0;
				int32_t v2 = 0;

				move(u, v, &u2, &v2);

				float const fu = (float)u2;
				float const fv = (float)v2;

				if (!TexelFits(fu) || !TexelFits(fv))
				{
					return UvModelResult::BadTexel;
				}

				made.corners[j].u = fu;
				made.corners[j].v = fv;
			}

			polygon->uv = made;
			return UvModelResult::Ok;
		}

		bool TurnsAllowed(TextureWindow const &w)
		{
			return w.sizeX > 1 && w.sizeY > 1;
		}
	}

	// FUN_00570f40 0x57101e: u' = state[+8] - u + (state[+0x10] - 1); the first corner point's value is not changed in v
	UvModelResult FlipU(PolygonUv *polygon, TextureWindow const &window)
	{
		int64_t const sum = (int64_t)window.left + (int64_t)(window.right - 1);

		return MoveCorners(polygon, [&](int32_t u, int32_t v, int32_t *u2, int32_t *v2)
		{
			*u2 = (int32_t)(sum - u);
			*v2 = v;
		});
	}

	// 0x571030: v' = state[+0xc] - v + (state[+0x14] - 1)
	UvModelResult FlipV(PolygonUv *polygon, TextureWindow const &window)
	{
		int64_t const sum = (int64_t)window.top + (int64_t)(window.bottom - 1);

		return MoveCorners(polygon, [&](int32_t u, int32_t v, int32_t *u2, int32_t *v2)
		{
			*u2 = u;
			*v2 = (int32_t)(sum - v);
		});
	}

	// 0x571084 (command 0x19): the first result is the extended-precision one, the second one has been stored as a 32-bit float
	UvModelResult TurnA(PolygonUv *polygon, TextureWindow const &window)
	{
		if (!TurnsAllowed(window))
		{
			return UvModelResult::BadWindow;
		}

		double const a = (double)(window.sizeX - 1);
		double const b = (double)(window.sizeY - 1);
		double const right = (double)(window.right - 1);

		return MoveCorners(polygon, [&](int32_t u, int32_t v, int32_t *u2, int32_t *v2)
		{
			double const s = ((double)(v - window.top) / b) * a + (double)window.left;
			float const t = (float)((((right - (double)u) / a) * b) + (double)window.top);

			*u2 = Trunc(s);
			*v2 = Trunc((double)t);
		});
	}

	// 0x571045 (command 0x1b)
	UvModelResult TurnB(PolygonUv *polygon, TextureWindow const &window)
	{
		if (!TurnsAllowed(window))
		{
			return UvModelResult::BadWindow;
		}

		double const a = (double)(window.sizeX - 1);
		double const b = (double)(window.sizeY - 1);
		double const bottom = (double)(window.bottom - 1);

		return MoveCorners(polygon, [&](int32_t u, int32_t v, int32_t *u2, int32_t *v2)
		{
			double const s = ((bottom - (double)v) / b) * a + (double)window.left;
			float const t = (float)((((double)(u - window.left) / a) * b) + (double)window.top);

			*u2 = Trunc(s);
			*v2 = Trunc((double)t);
		});
	}

	// FUN_00570ef0 (listing-all-4.txt, 0x570ef0): n = trunc(x); when |x - n| is NOT above 0.5 the answer is n; else x + 0.5 (x above 0) or x - 0.5 (otherwise), truncated
	int32_t RoundHalfTowardZero(double x)
	{
		int32_t const n = Trunc(x);
		double const fraction = x - (double)n;
		double const away = fraction < 0 ? -fraction : fraction;

		if (away <= 0.5)
		{
			return n;
		}

		return Trunc(x > 0.0 ? x + 0.5 : x - 0.5);
	}
}
