// See hub_text.h. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "hub_text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <exception>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_edit.h"

namespace host
{
	namespace
	{
		size_t const kHeaderFlagsAt = 0;     // the header payload's flags word
		size_t const kHeaderPlaceAt = 20;    // its place, three int32s (as shape_edit.cpp reads it)

		Object_Chunk *ObjectWithId(File_Chunk *level, uint8_t const *id)
		{
			List<Object_Chunk *> all;

			level->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
			{
				if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
				{
					return each();
				}
			}

			return nullptr;
		}

		// inteng's int of the file's number times the scale (the loader's (int)(x * local_scale)), kept inside the int range
		int32_t Scaled(double value, double scale)
		{
			double const scaled = value * scale;

			if (!(scaled > -2147483648.0 && scaled < 2147483647.0))
			{
				return 0;
			}

			return (int32_t)scaled;
		}
	}

	void HubText_Lines(File_Chunk *level, OpenLevelFile const *store, int64_t shapes, int64_t placed, std::vector<std::array<uint8_t, 8>> const &selected, double scale,
		int32_t fontHeight, std::vector<HubTextLine> *lines)
	{
		if (!lines)
		{
			return;
		}

		lines->clear();

		if (!level || !store)
		{
			return;
		}

		char text[160];

		_snprintf_s(text, sizeof(text), _TRUNCATE, "Polygon count : %d / %d", (int)shapes, (int)placed);
		lines->push_back({ 0x32, text });

		int32_t row = 2 * fontHeight;

		try
		{
			for (size_t at = 0; at < selected.size() && at < kHubTextMostObjects; at++)
			{
				uint8_t const *const id = selected[at].data();
				Object_Chunk *const object = ObjectWithId(level, id);

				if (!object)
				{
					continue;
				}

				Chunk *const header = object->get_header();
				std::vector<unsigned char> bytes;
				uint32_t flags = 0;
				int32_t place[3] = { 0, 0, 0 };

				if (header && store->PayloadOf(header, &bytes) && bytes.size() >= kHeaderPlaceAt + 12)
				{
					memcpy(&flags, &bytes[kHeaderFlagsAt], 4);
					memcpy(place, &bytes[kHeaderPlaceAt], sizeof(place));
				}

				// the name, up to its first 0 (the level's own: only printable letters kept)
				std::string name;

				for (size_t k = 0; k < sizeof(object->object_data.o_name) && object->object_data.o_name[k]; k++)
				{
					char const letter = object->object_data.o_name[k];

					name += letter >= 0x20 && letter <= 0x7e ? letter : '?';
				}

				// the shape's radius as the loaded header holds it (the file's radius times the scale, cut), shown x 1e-05
				model::ShapeModel shape;
				std::string why;
				int32_t radius = 0;

				if (ShapeEdit_Read(level, store, id, &shape, nullptr, &why) == ShapeEditResult::Done && isfinite(shape.radius))
				{
					radius = Scaled(shape.radius, scale);
				}

				_snprintf_s(text, sizeof(text), _TRUNCATE, "%s radius %-6.4f", name.c_str(), (double)radius * 1e-05);

				std::string line = text;

				if (flags & 0x400)
				{
					line += " (placed)";
				}

				if (flags & 0x100)
				{
					line += " (base)";
				}

				if (flags & 0x4000)
				{
					if (!(flags & 0x10000))
					{
						line += " (not psx)";
					}

					if (!(flags & 0x20000))
					{
						line += " (not saturn)";
					}
				}

				lines->push_back({ 0x32 + row, line });

				// the place: the loaded record's (the file's times the scale, cut), times the float 0.01 and cut toward 0
				double const kHundredth = (double)0.01f;
				int32_t shown[3];

				for (int axis = 0; axis < 3; axis++)
				{
					shown[axis] = (int32_t)((double)Scaled((double)place[axis], scale) * kHundredth);
				}

				_snprintf_s(text, sizeof(text), _TRUNCATE, "Location %d %d %d", (int)shown[0], (int)shown[1], (int)shown[2]);
				lines->push_back({ 0x32 + row + fontHeight, text });

				row += 2 * fontHeight;
			}
		}
		catch (std::exception const &)
		{
			lines->resize(1);
		}
	}
}
