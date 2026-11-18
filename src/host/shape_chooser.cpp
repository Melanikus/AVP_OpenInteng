// See shape_chooser.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "shape_chooser.h"

#include <string.h>

#include <exception>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk, Shape_Chunk, Object_Header_Chunk
#include "chunk_shape.hpp"    // Chunk_With_Children::lookup_single_child
#include "mishchnk.hpp"       // File_Chunk::list_shapes, list_objects

#include "level_file.h"

namespace host
{
	namespace
	{
		size_t const kHeaderFixedBytes = 68;     // SHPHEAD1: the flags to the count of names (shape_model.cpp)
		size_t const kHeaderNumberAt = 20;
		size_t const kHeaderPointsAt = 24;
		size_t const kHeaderPolygonsAt = 28;
		int32_t const kExternalFileFlag = 0x1000;   // SHAPE_FLAG_EXTERNALFILE
		size_t const kMostName = 63;

		int32_t IntAt(std::vector<unsigned char> const &bytes, size_t at)
		{
			int32_t value;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		// FUN_004d4750 0x4d4823-0x4d4855: the extension cut at the LAST '.', then everything up to the last '\' (a '/' too: the editor's paths)
		std::string NameOf(char const *text, size_t length)
		{
			std::string name(text, length);
			size_t const dot = name.find_last_of('.');

			if (dot != std::string::npos)
			{
				name.resize(dot);
			}

			size_t const slash = name.find_last_of("\\/");

			if (slash != std::string::npos)
			{
				name.erase(0, slash + 1);
			}

			if (name.size() > kMostName)
			{
				name.resize(kMostName);
			}

			return name;
		}

		size_t const kMostGameName = 255;   // longer is no name the game's list holds for us to find (its names come from the same chunks, but a hostile one may be any length)

		// The name the game's loader keeps an outside shape by (chunk_load.cpp copy_to_mainshapelist: Shape_External_File_Chunk::get_shape_name, EXTOBJNM's "object@file", else
		// RIFFNAME's), empty for none or one longer than kMostGameName. The chunk library read both names with their lengths (CopyNameFromChunk).
		std::string GameName(Shape_Chunk *shape)
		{
			Chunk *const file = shape->lookup_single_child("SHPEXTFL");
			Shape_External_File_Chunk *const outside = file && !file->r_u_miscellaneous() ? dynamic_cast<Shape_External_File_Chunk *>(file) : nullptr;
			char const *const name = outside ? outside->get_shape_name() : nullptr;
			size_t const length = name ? strnlen(name, kMostGameName + 1) : 0;

			return length > kMostGameName ? std::string() : std::string(name ? name : "", length);
		}

		// The first SHPEXTFN inside the first SHPEXTFL of a shape chunk: its name (the bytes after the rescale double and the version number)
		bool ExternalName(Shape_Chunk *shape, OpenLevelFile const *store, std::string *name)
		{
			Chunk *const file = shape->lookup_single_child("SHPEXTFL");
			Chunk_With_Children *const holder = file && !file->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(file) : nullptr;

			if (!holder)
			{
				return false;
			}

			Chunk *const named = holder->lookup_single_child("SHPEXTFN");
			std::vector<unsigned char> bytes;

			if (!named || named->r_u_miscellaneous() || !store->PayloadOf(named, &bytes) || bytes.size() <= 12)
			{
				return false;
			}

			size_t length = 0;

			while (12 + length < bytes.size() && bytes[12 + length] != 0)
			{
				length++;
			}

			if (length == 0)
			{
				return false;
			}

			*name = NameOf((char const *)bytes.data() + 12, length);
			return !name->empty();
		}

		Object_Chunk *FindObject(File_Chunk *level, uint8_t const *id)
		{
			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
				{
					return each();
				}
			}

			return nullptr;
		}
	}

	size_t ShapeChooser_List(File_Chunk *level, OpenLevelFile const *store, std::vector<ShapeChoice> *out)
	{
		if (!out)
		{
			return 0;
		}

		out->clear();

		if (!level || !store)
		{
			return 0;
		}

		try
		{
			List<Shape_Chunk *> shapes;

			level->list_shapes(&shapes);

			for (LIF<Shape_Chunk *> each(&shapes); !each.done(); each.next())
			{
				Shape_Chunk *const shape = each();
				Chunk *const header = shape ? shape->lookup_single_child("SHPHEAD1") : nullptr;
				std::vector<unsigned char> bytes;
				std::string name;

				if (!header || !store->PayloadOf(header, &bytes) || bytes.size() < kHeaderFixedBytes)
				{
					continue;
				}

				if (!(IntAt(bytes, 0) & kExternalFileFlag) || !ExternalName(shape, store, &name))
				{
					continue;
				}

				ShapeChoice choice;

				choice.number = IntAt(bytes, kHeaderNumberAt);
				choice.points = IntAt(bytes, kHeaderPointsAt);
				choice.polygons = IntAt(bytes, kHeaderPolygonsAt);
				choice.name = name;
				choice.gameName = GameName(shape);
				out->push_back(choice);
			}
		}
		catch (std::exception const &)
		{
			out->clear();
		}

		return out->size();
	}

	bool ShapeChooser_ObjectIsPlaced(File_Chunk *level, OpenLevelFile const *store, uint8_t const *id)
	{
		if (!level || !store || !id)
		{
			return false;
		}

		try
		{
			Object_Chunk *const object = FindObject(level, id);
			Chunk *const header = object ? object->get_header() : nullptr;
			std::vector<unsigned char> bytes;

			return header && store->PayloadOf(header, &bytes) && bytes.size() >= 4 && (IntAt(bytes, 0) & 0x400) != 0;
		}
		catch (std::exception const &)
		{
			return false;
		}
	}

	bool ShapeChooser_ShapeOfObject(File_Chunk *level, OpenLevelFile const *store, uint8_t const *id, int32_t *number)
	{
		if (!level || !store || !id || !number)
		{
			return false;
		}

		try
		{
			Object_Chunk *const object = FindObject(level, id);
			Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;
			Chunk *const header = shape ? shape->lookup_single_child("SHPHEAD1") : nullptr;
			std::vector<unsigned char> bytes;

			if (!header || !store->PayloadOf(header, &bytes) || bytes.size() < kHeaderFixedBytes)
			{
				return false;
			}

			*number = IntAt(bytes, kHeaderNumberAt);
			return true;
		}
		catch (std::exception const &)
		{
			return false;
		}
	}

	bool ObjectNameInUse(File_Chunk *level, char const *name)
	{
		if (!level || !name)
		{
			return false;
		}

		try
		{
			List<Object_Chunk *> objects;
			size_t const length = strlen(name);

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && length < sizeof(object->object_data.o_name) && strncmp(object->object_data.o_name, name, sizeof(object->object_data.o_name)) == 0)
				{
					return true;
				}
			}
		}
		catch (std::exception const &)
		{
		}

		return false;
	}
}
