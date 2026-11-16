// level_image.h: the walking of a level's bytes that the edits which rebuild an object's chunks share: the level as the next save writes it (BuildLevel), its
// top chunks, an RBOBJECT found by the name its OBJHEAD1 holds, and the children of a chunk by name. Written 2026-11-15 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// object_type_edit.h and strategy_mode.h use it. (module_data_edit.cpp, object_edit.cpp and the other edits that came before keep their own copies of
// these few functions: nothing changes for them.)
//
// What the bytes hold is hostile: every size is checked as the chunks are walked (a heading is 12 bytes: the 8-letter name, then the size, which counts
// the heading too), and what can't be walked answers false.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "chunk_object.hpp"   // Object_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: BuildLevel

namespace host
{
	namespace image
	{
		typedef std::vector<char> Bytes;

		size_t const kHeadingBytes = 12;
		size_t const kNameAt = 60;               // in an object header's payload: the name, after 60 bytes of numbers
		size_t const kMostNameBytes = 63;
		size_t const kLargest = 0x7fffffff;      // the most a chunk's size field holds

		struct Span
		{
			size_t at;
			size_t size;
		};

		inline int32_t IntAt(Bytes const &bytes, size_t at)
		{
			int32_t value;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		inline void PutIntAt(Bytes *bytes, size_t at, int32_t value)
		{
			memcpy(bytes->data() + at, &value, sizeof(value));
		}

		inline void AddInt(Bytes *bytes, int32_t value)
		{
			char raw[sizeof(value)];

			memcpy(raw, &value, sizeof(value));
			bytes->insert(bytes->end(), raw, raw + sizeof(raw));
		}

		inline bool ChunkSizeAt(Bytes const &bytes, size_t at, size_t end, size_t *size)
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

		inline bool NameIs(Bytes const &bytes, size_t at, char const *name)
		{
			return memcmp(bytes.data() + at, name, 8) == 0;
		}

		inline bool Children(Bytes const &bytes, size_t from, size_t to, std::vector<Span> *out)
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

		// The first child of that name, or inside.size()
		inline size_t FirstNamed(Bytes const &bytes, std::vector<Span> const &inside, char const *name)
		{
			for (size_t i = 0; i < inside.size(); i++)
			{
				if (NameIs(bytes, inside[i].at, name))
				{
					return i;
				}
			}

			return inside.size();
		}

		inline void AddCopy(Bytes *out, Bytes const &from, Span const &span)
		{
			out->insert(out->end(), from.begin() + (std::ptrdiff_t)span.at, from.begin() + (std::ptrdiff_t)(span.at + span.size));
		}

		// A whole chunk of that name holding `payload`
		inline Bytes MakeChunk(char const *name, Bytes const &payload)
		{
			Bytes chunk(kHeadingBytes, 0);

			memcpy(chunk.data(), name, 8);
			chunk.insert(chunk.end(), payload.begin(), payload.end());
			PutIntAt(&chunk, 8, (int32_t)chunk.size());
			return chunk;
		}

		// The name of the object the level holds under that ID (the library's own), or false
		inline bool NameOfObject(File_Chunk *level, uint8_t const *id, std::string *name)
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
		inline bool NameOfObjectChunk(Bytes const &bytes, Span const &object, std::string *name)
		{
			std::vector<Span> inside;

			if (!Children(bytes, object.at + kHeadingBytes, object.at + object.size, &inside))
			{
				return false;
			}

			size_t const head = FirstNamed(bytes, inside, "OBJHEAD1");

			if (head == inside.size())
			{
				return false;
			}

			size_t const length = inside[head].size - kHeadingBytes;

			if (length < kNameAt + 1)
			{
				return false;
			}

			char const *const first = bytes.data() + inside[head].at + kHeadingBytes + kNameAt;
			char const *const zero = (char const *)memchr(first, 0, length - kNameAt);

			if (!zero || (size_t)(zero - first) > kMostNameBytes)
			{
				return false;
			}

			name->assign(first, (size_t)(zero - first));
			return true;
		}

		// The level as the next save writes it, and its top chunks. False when it can't be walked.
		inline bool Walk(OpenLevelFile *file, Bytes *image, std::vector<Span> *top)
		{
			if (!file->BuildLevel(*image) || image->size() < kHeadingBytes || !NameIs(*image, 0, "REBINFF2"))
			{
				return false;
			}

			return Children(*image, kHeadingBytes, image->size(), top);
		}

		// The RBOBJECT chunks by the name their header holds (the first of a name is the one the game uses)
		inline void ObjectsByName(Bytes const &image, std::vector<Span> const &top, std::map<std::string, size_t> *byName)
		{
			for (size_t i = 0; i < top.size(); i++)
			{
				std::string name;

				if (NameIs(image, top[i].at, "RBOBJECT") && NameOfObjectChunk(image, top[i], &name))
				{
					byName->insert(std::make_pair(name, i));
				}
			}
		}
	}
}
