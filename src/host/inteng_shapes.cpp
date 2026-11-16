// See inteng_shapes.h. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "inteng_shapes.h"

namespace host
{
	namespace
	{
		// FUN_0042c700 (0x42c711-0x42c7c0): slots 0..9 are read through these pointer words, slots 10..18 are these headers themselves
		uint32_t const kSlotPointerWord[10] = { 0x5cd958, 0x5cd95c, 0x5cd960, 0x5cd964, 0x5cd97c, 0x5cd980, 0x5cd96c, 0x5cd970, 0x5cd974, 0x5cd984 };
		uint32_t const kSlotHeader[9] = { 0x5df230, 0x5de270, 0x5d9b90, 0x5e0730, 0x5d84e0, 0x5dfcb0, 0x5dcce0, 0x5d31a0, 0x5e48c8 };

		bool Refuse(IntengShapesRefusal *why, IntengShapesRefusal what)
		{
			if (why)
			{
				*why = what;
			}

			return false;
		}

		// One SHAPEHEADER, its polygons fanned into triangles
		IntengShapesRefusal ReadShape(IntengExeImage const &image, uint32_t header, std::vector<IntengShapeTriangle> *out)
		{
			int32_t pointCount = 0, itemCount = 0;
			uint32_t pointsHolder = 0, pointsAt = 0, itemsAt = 0;

			if (!IntengExe_ReadInt32(image, header, &pointCount) || !IntengExe_ReadInt32(image, header + 4, &itemCount) || !IntengExe_ReadUint32(image, header + 12, &pointsHolder) ||
				!IntengExe_ReadUint32(image, header + 16, &itemsAt))
			{
				return IntengShapesRefusal::BadAddress;
			}

			if (pointCount <= 0 || pointCount > kIntengShapeMostPoints || itemCount <= 0 || itemCount > kIntengShapeMostItems)
			{
				return IntengShapesRefusal::BadCounts;
			}

			if (!IntengExe_ReadUint32(image, pointsHolder, &pointsAt))
			{
				return IntengShapesRefusal::BadAddress;
			}

			// the whole point list must be in the file: 3 ints a point
			std::vector<int32_t> points((size_t)pointCount * 3);

			for (size_t i = 0; i < points.size(); i++)
			{
				if (!IntengExe_ReadInt32(image, pointsAt + (uint32_t)(i * 4), &points[i]))
				{
					return IntengShapesRefusal::BadAddress;
				}
			}

			for (int32_t item = 0; item < itemCount; item++)
			{
				uint32_t polygon = 0, colour = 0;

				if (!IntengExe_ReadUint32(image, itemsAt + (uint32_t)item * 4, &polygon) || !IntengExe_ReadUint32(image, polygon + 12, &colour))
				{
					return IntengShapesRefusal::BadAddress;
				}

				int corners[kIntengShapeMostCorners];
				int cornerCount = 0;
				bool ended = false;

				// the corners, then the -1 (at most kIntengShapeMostCorners before it)
				for (int k = 0; k <= kIntengShapeMostCorners; k++)
				{
					int32_t offset = 0;

					if (!IntengExe_ReadInt32(image, polygon + 16 + (uint32_t)k * 4, &offset))
					{
						return IntengShapesRefusal::BadAddress;
					}

					if (offset == -1)
					{
						ended = true;
						break;
					}

					if (k == kIntengShapeMostCorners || offset < 0 || offset % 3 != 0 || offset / 3 >= pointCount)
					{
						return IntengShapesRefusal::BadCorner;
					}

					corners[cornerCount++] = offset / 3;
				}

				if (!ended || cornerCount < 3)
				{
					return IntengShapesRefusal::BadCorner;
				}

				for (int k = 1; k + 1 < cornerCount; k++)
				{
					IntengShapeTriangle t;
					int const which[3] = { corners[0], corners[k], corners[k + 1] };

					for (int c = 0; c < 3; c++)
					{
						for (int axis = 0; axis < 3; axis++)
						{
							t.p[c * 3 + axis] = points[(size_t)which[c] * 3 + axis];
						}
					}

					t.colour = colour & 0xffffffu;
					out->push_back(t);
				}
			}

			return IntengShapesRefusal::None;
		}
	}

	char const *IntengShapes_Why(IntengShapesRefusal why)
	{
		switch (why)
		{
		case IntengShapesRefusal::None: return "read";
		case IntengShapesRefusal::BadSlot: return "a built-in shape number outside 0..18 was asked for";
		case IntengShapesRefusal::BadAddress: return "a built-in shape points outside inteng.exe's bytes";
		case IntengShapesRefusal::BadCounts: return "a built-in shape has no points or polygons, or too many";
		case IntengShapesRefusal::BadCorner: return "a built-in shape has a polygon whose corners are not its points";
		}

		return "refused";
	}

	bool IntengShapes_HeaderOf(IntengExeImage const &image, int slot, uint32_t *header)
	{
		if (!header || slot < 0 || slot >= kIntengShapeSlots)
		{
			return false;
		}

		if (slot >= 10)
		{
			*header = kSlotHeader[slot - 10];
			return true;
		}

		return IntengExe_ReadUint32(image, kSlotPointerWord[slot], header);
	}

	bool IntengShapes_Read(IntengExeImage const &image, int const *wanted, size_t wantedCount, IntengShapes *out, IntengShapesRefusal *why)
	{
		if (!out || (wantedCount > 0 && !wanted))
		{
			return Refuse(why, IntengShapesRefusal::BadSlot);
		}

		*out = IntengShapes();

		IntengShapes made;

		for (size_t i = 0; i < wantedCount; i++)
		{
			int const slot = wanted[i];
			uint32_t header = 0;

			if (slot < 0 || slot >= kIntengShapeSlots)
			{
				return Refuse(why, IntengShapesRefusal::BadSlot);
			}

			if (!IntengShapes_HeaderOf(image, slot, &header))
			{
				return Refuse(why, IntengShapesRefusal::BadAddress);
			}

			std::vector<IntengShapeTriangle> triangles;
			IntengShapesRefusal const result = ReadShape(image, header, &triangles);

			if (result != IntengShapesRefusal::None)
			{
				return Refuse(why, result);
			}

			made.have[slot] = true;
			made.triangles[slot].swap(triangles);
		}

		*out = made;

		if (why)
		{
			*why = IntengShapesRefusal::None;
		}

		return true;
	}

	char const *IntengNames_Why(IntengNamesRefusal why)
	{
		switch (why)
		{
		case IntengNamesRefusal::None: return "read";
		case IntengNamesRefusal::BadAddress: return "inteng's shape-name table points outside inteng.exe's bytes";
		case IntengNamesRefusal::NoEnd: return "inteng's shape-name table has no end within its first rows";
		case IntengNamesRefusal::BadName: return "inteng's shape-name table holds a name that is not a plain short name";
		case IntengNamesRefusal::BadNumber: return "inteng's shape-name table holds a type or extra data out of range";
		}

		return "refused";
	}

	bool IntengShapeNames_Read(IntengExeImage const &image, std::vector<IntengShapeName> *out, IntengNamesRefusal *why)
	{
		auto refuse = [&](IntengNamesRefusal what)
		{
			if (out)
			{
				out->clear();
			}

			if (why)
			{
				*why = what;
			}

			return false;
		};

		if (!out)
		{
			return refuse(IntengNamesRefusal::BadAddress);
		}

		std::vector<IntengShapeName> rows;

		for (size_t row = 0; ; row++)
		{
			uint32_t const at = kIntengShapeNamesAt + (uint32_t)row * 12;
			uint32_t namePointer = 0;

			if (!IntengExe_ReadUint32(image, at, &namePointer))
			{
				return refuse(IntengNamesRefusal::BadAddress);
			}

			if (namePointer == 0)
			{
				break;   // FUN_004f6a20 0x4f6a70: the end
			}

			if (row >= kIntengShapeNamesMost)
			{
				return refuse(IntengNamesRefusal::NoEnd);
			}

			IntengShapeName one;

			if (!IntengExe_ReadInt32(image, at + 4, &one.type) || !IntengExe_ReadInt32(image, at + 8, &one.extra))
			{
				return refuse(IntengNamesRefusal::BadAddress);
			}

			if (one.type < 0 || one.type > 31 || one.extra < 0 || one.extra > 65535)
			{
				return refuse(IntengNamesRefusal::BadNumber);
			}

			for (size_t k = 0; ; k++)
			{
				uint8_t letter = 0;

				if (!IntengExe_ReadBytes(image, namePointer + (uint32_t)k, 1, &letter))
				{
					return refuse(IntengNamesRefusal::BadAddress);
				}

				if (letter == 0)
				{
					break;
				}

				if (k >= kIntengShapeNameMost || letter < '!' || letter > '~')
				{
					return refuse(IntengNamesRefusal::BadName);
				}

				one.name += (char)letter;
			}

			if (one.name.empty())
			{
				return refuse(IntengNamesRefusal::BadName);
			}

			rows.push_back(one);
		}

		out->swap(rows);

		if (why)
		{
			*why = IntengNamesRefusal::None;
		}

		return true;
	}
}
