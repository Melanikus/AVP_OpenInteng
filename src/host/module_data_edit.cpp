// See module_data_edit.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "module_data_edit.h"

#include <string.h>

#include <string>
#include <vector>

#include "chunk_object.hpp"   // Object_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: BuildLevel, ReplaceLevel

namespace host
{
	namespace
	{
		typedef std::vector<char> Image;

		size_t const kHeadingBytes = 12;         // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const kNameAt = 60;               // in an object header's payload: the name, after 60 bytes of numbers
		size_t const kMostNameBytes = 63;

		int32_t IntAt(Image const &bytes, size_t at)
		{
			int32_t value;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		void PutIntAt(Image *bytes, size_t at, int32_t value)
		{
			memcpy(bytes->data() + at, &value, sizeof(value));
		}

		struct Span
		{
			size_t at;
			size_t size;
		};

		bool ChunkSizeAt(Image const &bytes, size_t at, size_t end, size_t *size)
		{
			if (end > bytes.size() || at > end || end - at < kHeadingBytes)
			{
				return false;
			}

			int32_t const stored = IntAt(bytes, at + 8);

			if (stored < (int32_t)kHeadingBytes || (size_t)stored > end - at)
			{
				return false;
			}

			*size = (size_t)stored;
			return true;
		}

		bool NameIs(Image const &bytes, size_t at, char const *name)
		{
			return memcmp(bytes.data() + at, name, 8) == 0;
		}

		bool Children(Image const &bytes, size_t from, size_t to, std::vector<Span> *out)
		{
			out->clear();

			for (size_t at = from; at < to; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, to, &size))
				{
					return false;
				}

				Span const span = { at, size };

				out->push_back(span);
				at += size;
			}

			return true;
		}

		bool NameOfObject(File_Chunk *level, uint8_t const *id, std::string *name)
		{
			List<Object_Chunk *> found;

			level->list_objects(&found);

			for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && memcmp(&object->object_data.ID, id, 8) == 0)
				{
					name->assign(object->object_data.o_name, strnlen(object->object_data.o_name, sizeof(object->object_data.o_name)));
					return true;
				}
			}

			return false;
		}

		// The name an RBOBJECT chunk's OBJHEAD1 holds, or false
		bool NameOfObjectChunk(Image const &bytes, Span const &object, std::string *name)
		{
			std::vector<Span> inside;

			if (!Children(bytes, object.at + kHeadingBytes, object.at + object.size, &inside))
			{
				return false;
			}

			for (Span const &child : inside)
			{
				if (NameIs(bytes, child.at, "OBJHEAD1"))
				{
					size_t const length = child.size - kHeadingBytes;

					if (length < kNameAt + 1)
					{
						return false;
					}

					char const *const first = bytes.data() + child.at + kHeadingBytes + kNameAt;
					char const *const zero = (char const *)memchr(first, 0, length - kNameAt);

					if (!zero || (size_t)(zero - first) > kMostNameBytes)
					{
						return false;
					}

					name->assign(first, (size_t)(zero - first));
					return true;
				}
			}

			return false;
		}
	}

	char const *ModuleDataWords(ModuleDataResult result)
	{
		switch (result)
		{
		case ModuleDataResult::Done: return "done";
		case ModuleDataResult::BadCall: return "nothing to change";
		case ModuleDataResult::NoObject: return "no object has that name";
		case ModuleDataResult::NothingToDelete: return "the object has no module data to delete";
		case ModuleDataResult::CantBuild: return "the level couldn't be put together from its bytes";
		case ModuleDataResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	ModuleDataResult ModuleData_DeleteLinks(OpenLevelFile *file, uint8_t const *id, size_t *removed)
	{
		if (removed)
		{
			*removed = 0;
		}

		if (!file || !id || !file->Level())
		{
			return ModuleDataResult::BadCall;
		}

		std::string name;

		if (!NameOfObject(file->Level(), id, &name))
		{
			return ModuleDataResult::NoObject;
		}

		Image image;

		if (!file->BuildLevel(image) || image.size() < kHeadingBytes || !NameIs(image, 0, "REBINFF2"))
		{
			return ModuleDataResult::CantBuild;
		}

		std::vector<Span> top;

		if (!Children(image, kHeadingBytes, image.size(), &top))
		{
			return ModuleDataResult::CantBuild;
		}

		// The object's chunk, then the first MODULEDT inside it (the one the game uses), then the VMDARRAYs inside that
		size_t objectAt = top.size();

		for (size_t i = 0; i < top.size() && objectAt == top.size(); i++)
		{
			std::string found;

			if (NameIs(image, top[i].at, "RBOBJECT") && NameOfObjectChunk(image, top[i], &found) && found == name)
			{
				objectAt = i;
			}
		}

		if (objectAt == top.size())
		{
			return ModuleDataResult::NoObject;
		}

		std::vector<Span> inside;

		if (!Children(image, top[objectAt].at + kHeadingBytes, top[objectAt].at + top[objectAt].size, &inside))
		{
			return ModuleDataResult::CantBuild;
		}

		size_t moduleAt = inside.size();

		for (size_t i = 0; i < inside.size() && moduleAt == inside.size(); i++)
		{
			if (NameIs(image, inside[i].at, "MODULEDT"))
			{
				moduleAt = i;
			}
		}

		if (moduleAt == inside.size())
		{
			return ModuleDataResult::NothingToDelete;
		}

		std::vector<Span> parts;

		if (!Children(image, inside[moduleAt].at + kHeadingBytes, inside[moduleAt].at + inside[moduleAt].size, &parts))
		{
			return ModuleDataResult::CantBuild;
		}

		size_t count = 0;

		for (Span const &part : parts)
		{
			if (NameIs(image, part.at, "VMDARRAY"))
			{
				count++;
			}
		}

		if (count == 0)
		{
			return ModuleDataResult::NothingToDelete;
		}

		// The new image: everything as it was, the object's chunk made again without the VMDARRAYs, the sizes of MODULEDT, RBOBJECT and the level
		// made right
		Image out(image.begin(), image.begin() + (std::ptrdiff_t)top[objectAt].at);
		size_t const objectStart = out.size();

		out.insert(out.end(), image.begin() + (std::ptrdiff_t)top[objectAt].at, image.begin() + (std::ptrdiff_t)(top[objectAt].at + kHeadingBytes));

		for (size_t i = 0; i < inside.size(); i++)
		{
			if (i != moduleAt)
			{
				out.insert(out.end(), image.begin() + (std::ptrdiff_t)inside[i].at, image.begin() + (std::ptrdiff_t)(inside[i].at + inside[i].size));
				continue;
			}

			size_t const moduleStart = out.size();

			out.insert(out.end(), image.begin() + (std::ptrdiff_t)inside[i].at, image.begin() + (std::ptrdiff_t)(inside[i].at + kHeadingBytes));

			for (Span const &part : parts)
			{
				if (!NameIs(image, part.at, "VMDARRAY"))
				{
					out.insert(out.end(), image.begin() + (std::ptrdiff_t)part.at, image.begin() + (std::ptrdiff_t)(part.at + part.size));
				}
			}

			PutIntAt(&out, moduleStart + 8, (int32_t)(out.size() - moduleStart));
		}

		PutIntAt(&out, objectStart + 8, (int32_t)(out.size() - objectStart));
		out.insert(out.end(), image.begin() + (std::ptrdiff_t)(top[objectAt].at + top[objectAt].size), image.end());
		PutIntAt(&out, 8, (int32_t)out.size());

		LevelOpenResult const result = file->ReplaceLevel(out);

		if (result == LevelOpenResult::Opened)
		{
			if (removed)
			{
				*removed = count;
			}

			return ModuleDataResult::Done;
		}

		return result == LevelOpenResult::CantRead ? ModuleDataResult::CantBuild : ModuleDataResult::LoaderRefused;
	}
}
