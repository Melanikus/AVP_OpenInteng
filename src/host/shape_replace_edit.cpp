// See shape_replace_edit.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "shape_replace_edit.h"

#include <string.h>

#include <exception>
#include <vector>

#include "chunk_object.hpp"   // Object_Chunk, Object_Header_Chunk
#include "chunk_shape.hpp"    // Chunk_With_Children::lookup_single_child
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: BuildLevel, ReplaceLevel

namespace host
{
	namespace
	{
		typedef std::vector<char> Image;

		size_t const kHeadingBytes = 12;         // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const kObjectShapeAt = 56;        // in OBJHEAD1's payload: the shape's number, after flags, lock, place, turn, index and version
		size_t const kObjectNameAt = 60;         // in OBJHEAD1's payload: the name
		size_t const kShapeNumberAt = 20;        // in SHPHEAD1's payload: the shape's number
		size_t const kShapeCountAt = 64;         // in SHPHEAD1's payload: how many object names follow
		size_t const kShapeHeadBytes = 68;       // SHPHEAD1's numbers, before the names
		size_t const kMostNameBytes = 63;
		int32_t const kPlacedObjectFlag = 0x400; // OBJECT_FLAG_PLACED_OBJECT (chunk_object.hpp)

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

		// The size of the chunk at `at`, inside [at, end), or false when its heading isn't there or its size doesn't fit
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

		// The chunks directly inside [from, to), in order. False for bytes that can't be walked.
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

		bool SameLetters(std::string const &a, std::string const &b)
		{
			if (a.size() != b.size())
			{
				return false;
			}

			for (size_t i = 0; i < a.size(); i++)
			{
				char x = a[i];
				char y = b[i];

				x = (x >= 'A' && x <= 'Z') ? (char)(x + 32) : x;
				y = (y >= 'A' && y <= 'Z') ? (char)(y + 32) : y;

				if (x != y)
				{
					return false;
				}
			}

			return true;
		}

		// The name in an OBJHEAD1 chunk, or false
		bool ObjectNameIn(Image const &bytes, Span const &header, std::string *name)
		{
			size_t const length = header.size - kHeadingBytes;

			if (length < kObjectNameAt + 1)
			{
				return false;
			}

			char const *const first = bytes.data() + header.at + kHeadingBytes + kObjectNameAt;
			char const *const zero = (char const *)memchr(first, 0, length - kObjectNameAt);

			if (!zero || (size_t)(zero - first) > kMostNameBytes)
			{
				return false;
			}

			name->assign(first, (size_t)(zero - first));
			return true;
		}

		// The outside file's name, without its extension, of a SHPEXTFL (its SHPEXTFN holds a double, a number, then the name); "" when it has none
		bool OutsideNameOf(Image const &bytes, Span const &external, std::string *name)
		{
			std::vector<Span> inside;

			name->clear();

			if (!Children(bytes, external.at + kHeadingBytes, external.at + external.size, &inside))
			{
				return false;
			}

			for (Span const &child : inside)
			{
				if (NameIs(bytes, child.at, "SHPEXTFN") && child.size > kHeadingBytes + 12)
				{
					char const *const first = bytes.data() + child.at + kHeadingBytes + 12;
					size_t const room = child.size - kHeadingBytes - 12;
					char const *const zero = (char const *)memchr(first, 0, room);

					if (!zero)
					{
						return false;
					}

					std::string file(first, (size_t)(zero - first));
					size_t const slash = file.find_last_of("\\/");

					if (slash != std::string::npos)
					{
						file = file.substr(slash + 1);
					}

					size_t const dot = file.find_last_of('.');

					*name = dot == std::string::npos ? file : file.substr(0, dot);
					return true;
				}
			}

			return true;
		}

		// A shape of the level as the image holds it
		struct ShapeAt
		{
			Span chunk;           // the REBSHAPE
			Span head;            // its SHPHEAD1
			int32_t number;
			std::string outside;  // the outside file's name, "" when none
		};

		bool ReadShapes(Image const &image, std::vector<Span> const &top, std::vector<ShapeAt> *shapes)
		{
			shapes->clear();

			for (Span const &one : top)
			{
				if (!NameIs(image, one.at, "REBSHAPE"))
				{
					continue;
				}

				std::vector<Span> inside;

				if (!Children(image, one.at + kHeadingBytes, one.at + one.size, &inside))
				{
					return false;
				}

				ShapeAt shape;
				bool hasHead = false;

				shape.chunk = one;
				shape.number = 0;

				for (Span const &child : inside)
				{
					if (!hasHead && NameIs(image, child.at, "SHPHEAD1") && child.size >= kHeadingBytes + kShapeHeadBytes)
					{
						hasHead = true;
						shape.head = child;
						shape.number = IntAt(image, child.at + kHeadingBytes + kShapeNumberAt);
					}
					else if (NameIs(image, child.at, "SHPEXTFL"))
					{
						if (!OutsideNameOf(image, child, &shape.outside))
						{
							return false;
						}
					}
				}

				if (hasHead)
				{
					shapes->push_back(shape);
				}
			}

			return true;
		}

		// The object names in a SHPHEAD1 chunk, in order; false when the count or a name doesn't fit
		bool NamesIn(Image const &bytes, Span const &head, std::vector<std::string> *names)
		{
			names->clear();

			int32_t const count = IntAt(bytes, head.at + kHeadingBytes + kShapeCountAt);
			size_t at = head.at + kHeadingBytes + kShapeHeadBytes;
			size_t const end = head.at + head.size;

			if (count < 0 || (size_t)count > end - at)
			{
				return false;
			}

			for (int32_t i = 0; i < count; i++)
			{
				if (at >= end)
				{
					return false;
				}

				char const *const zero = (char const *)memchr(bytes.data() + at, 0, end - at);

				if (!zero)
				{
					return false;
				}

				names->push_back(std::string(bytes.data() + at, (size_t)(zero - (bytes.data() + at))));
				at += names->back().size() + 1;
			}

			return true;
		}

		// A SHPHEAD1 chunk made again with another list of names: its numbers as they were, the names, then zeros up to a whole number of 4 bytes
		// (Shape_Header_Chunk::size_chunk: 80 and a name and its zero each, rounded up to 4)
		void AppendHead(Image *out, Image const &image, Span const &head, std::vector<std::string> const &names)
		{
			size_t const start = out->size();

			out->insert(out->end(), image.begin() + head.at, image.begin() + head.at + kHeadingBytes + kShapeHeadBytes);
			PutIntAt(out, start + kHeadingBytes + kShapeCountAt, (int32_t)names.size());

			for (std::string const &name : names)
			{
				out->insert(out->end(), name.begin(), name.end());
				out->push_back(0);
			}

			while ((out->size() - start) % 4 != 0)
			{
				out->push_back(0);
			}

			PutIntAt(out, start + 8, (int32_t)(out->size() - start));
		}

		// A REBSHAPE chunk copied with its SHPHEAD1 replaced by one holding `names`
		bool AppendShapeWithNames(Image *out, Image const &image, ShapeAt const &shape, std::vector<std::string> const &names)
		{
			std::vector<Span> inside;

			if (!Children(image, shape.chunk.at + kHeadingBytes, shape.chunk.at + shape.chunk.size, &inside))
			{
				return false;
			}

			size_t const start = out->size();

			out->insert(out->end(), image.begin() + shape.chunk.at, image.begin() + shape.chunk.at + kHeadingBytes);

			for (Span const &child : inside)
			{
				if (child.at == shape.head.at)
				{
					AppendHead(out, image, child, names);
				}
				else
				{
					out->insert(out->end(), image.begin() + child.at, image.begin() + child.at + child.size);
				}
			}

			PutIntAt(out, start + 8, (int32_t)(out->size() - start));
			return true;
		}

		Object_Chunk *ObjectWithId(File_Chunk *level, uint8_t const *id)
		{
			List<Object_Chunk *> found;

			level->list_objects(&found);

			for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && memcmp(&object->object_data.ID, id, 8) == 0)
				{
					return object;
				}
			}

			return nullptr;
		}

		ShapeReplaceResult Replace(OpenLevelFile *file, uint8_t const *id, char const *shapeName, int32_t shapeNumber, int32_t *newNumber)
		{
			if (!file || !file->Level() || !id)
			{
				return ShapeReplaceResult::BadCall;
			}

			Object_Chunk *const object = ObjectWithId(file->Level(), id);

			if (!object)
			{
				return ShapeReplaceResult::NoObject;
			}

			Object_Header_Chunk *const header = object->get_header();

			if (!header || !(header->flags & kPlacedObjectFlag) || object->lookup_single_child("MODULEDT"))
			{
				return ShapeReplaceResult::NotPlaced;
			}

			Image image;

			if (!file->BuildLevel(image) || image.size() < kHeadingBytes || !NameIs(image, 0, "REBINFF2"))
			{
				return ShapeReplaceResult::CantBuild;
			}

			std::vector<Span> top;
			std::vector<ShapeAt> shapes;

			if (!Children(image, kHeadingBytes, image.size(), &top) || !ReadShapes(image, top, &shapes))
			{
				return ShapeReplaceResult::CantBuild;
			}

			// The object's chunk, by its name (the first with that name), and its header inside
			std::string const objectName(object->object_data.o_name, strnlen(object->object_data.o_name, sizeof(object->object_data.o_name)));
			size_t objectTop = top.size();
			Span objectHeader = { 0, 0 };

			for (size_t i = 0; i < top.size() && objectTop == top.size(); i++)
			{
				std::vector<Span> inside;

				if (!NameIs(image, top[i].at, "RBOBJECT") || !Children(image, top[i].at + kHeadingBytes, top[i].at + top[i].size, &inside))
				{
					continue;
				}

				for (Span const &child : inside)
				{
					std::string name;

					if (NameIs(image, child.at, "OBJHEAD1") && ObjectNameIn(image, child, &name) && name == objectName)
					{
						objectTop = i;
						objectHeader = child;
						break;
					}
				}
			}

			if (objectTop == top.size() || objectHeader.size < kHeadingBytes + kObjectShapeAt + 4)
			{
				return ShapeReplaceResult::CantBuild;
			}

			// The shapes: the object's now, and the chosen one (by name or by number)
			int32_t const oldNumber = IntAt(image, objectHeader.at + kHeadingBytes + kObjectShapeAt);
			size_t oldShape = shapes.size();
			size_t newShape = shapes.size();

			for (size_t i = 0; i < shapes.size(); i++)
			{
				if (oldShape == shapes.size() && shapes[i].number == oldNumber)
				{
					oldShape = i;
				}

				if (newShape == shapes.size())
				{
					bool const chosen = shapeName ? (!shapes[i].outside.empty() && SameLetters(shapes[i].outside, shapeName)) : shapes[i].number == shapeNumber;

					if (chosen)
					{
						newShape = i;
					}
				}
			}

			if (newShape == shapes.size())
			{
				return ShapeReplaceResult::NoSuchShape;
			}

			if (newNumber)
			{
				*newNumber = shapes[newShape].number;
			}

			if (shapes[newShape].number == oldNumber)
			{
				return ShapeReplaceResult::SameShape;
			}

			// The lists of names: the old shape's without the object (its first entry of that name), the new shape's with it at the end
			std::vector<std::string> oldNames;
			std::vector<std::string> newNames;
			bool const haveOld = oldShape != shapes.size();

			if ((haveOld && !NamesIn(image, shapes[oldShape].head, &oldNames)) || !NamesIn(image, shapes[newShape].head, &newNames))
			{
				return ShapeReplaceResult::CantBuild;
			}

			for (size_t i = 0; i < oldNames.size(); i++)
			{
				if (oldNames[i] == objectName)
				{
					oldNames.erase(oldNames.begin() + (std::ptrdiff_t)i);
					break;
				}
			}

			newNames.push_back(objectName);

			// The new image: every top-level chunk as it was, but the object's number, and the two shapes' lists
			Image out;

			out.insert(out.end(), image.begin(), image.begin() + kHeadingBytes);

			for (size_t i = 0; i < top.size(); i++)
			{
				size_t const start = out.size();

				if (haveOld && top[i].at == shapes[oldShape].chunk.at)
				{
					if (!AppendShapeWithNames(&out, image, shapes[oldShape], oldNames))
					{
						return ShapeReplaceResult::CantBuild;
					}
				}
				else if (top[i].at == shapes[newShape].chunk.at)
				{
					if (!AppendShapeWithNames(&out, image, shapes[newShape], newNames))
					{
						return ShapeReplaceResult::CantBuild;
					}
				}
				else
				{
					out.insert(out.end(), image.begin() + top[i].at, image.begin() + top[i].at + top[i].size);

					if (i == objectTop)
					{
						PutIntAt(&out, start + (objectHeader.at - top[i].at) + kHeadingBytes + kObjectShapeAt, shapes[newShape].number);
					}
				}
			}

			PutIntAt(&out, 8, (int32_t)out.size());

			switch (file->ReplaceLevel(out))
			{
			case LevelOpenResult::Opened: return ShapeReplaceResult::Done;
			case LevelOpenResult::CantRead: return ShapeReplaceResult::CantBuild;
			default: return ShapeReplaceResult::LoaderRefused;
			}
		}
	}

	char const *ShapeReplaceWords(ShapeReplaceResult result)
	{
		switch (result)
		{
		case ShapeReplaceResult::Done: return "done";
		case ShapeReplaceResult::BadCall: return "no level is open, or no object or shape was given";
		case ShapeReplaceResult::NoObject: return "the level has no such object";
		case ShapeReplaceResult::NotPlaced: return "only a placed object is given another shape";
		case ShapeReplaceResult::NoSuchShape: return "the level has no shape by that name";
		case ShapeReplaceResult::SameShape: return "the object already has that shape";
		case ShapeReplaceResult::CantBuild: return "the level could not be put together";
		case ShapeReplaceResult::LoaderRefused: return "the game would not take the level that way, so nothing was changed";
		}

		return "the shape could not be replaced";
	}

	ShapeReplaceResult ShapeReplace_ByName(OpenLevelFile *file, uint8_t const *id, char const *shapeName, int32_t *newShapeNumber)
	{
		if (!shapeName || !*shapeName)
		{
			return ShapeReplaceResult::BadCall;
		}

		try
		{
			return Replace(file, id, shapeName, 0, newShapeNumber);
		}
		catch (std::exception const &)
		{
			return ShapeReplaceResult::CantBuild;
		}
	}

	ShapeReplaceResult ShapeReplace_ByNumber(OpenLevelFile *file, uint8_t const *id, int32_t shapeNumber)
	{
		try
		{
			return Replace(file, id, nullptr, shapeNumber, nullptr);
		}
		catch (std::exception const &)
		{
			return ShapeReplaceResult::CantBuild;
		}
	}
}
