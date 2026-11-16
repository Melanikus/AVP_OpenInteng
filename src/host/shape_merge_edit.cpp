// See shape_merge_edit.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "shape_merge_edit.h"

#include <string.h>

#include <vector>

#include "chunk_object.hpp"   // Object_Chunk, Shape_Chunk, Object_Header_Chunk
#include "chunk_shape.hpp"    // Chunk_With_Children::lookup_single_child
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: PayloadOf, BuildLevel, ReplaceLevel

#include "shape_edit.h"       // ShapeEdit_Read
#include "shape_merge.h"      // model::MergeShapes
#include "shape_model.h"

namespace host
{
	namespace
	{
		typedef std::vector<char> Image;
		typedef std::vector<unsigned char> Payload;

		size_t const kHeadingBytes = 12;         // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const kPlaceAt = 20;              // in an object header's payload: three ints
		size_t const kNameAt = 60;               // in an object header's payload: the name, after 60 bytes of numbers
		size_t const kMostNameBytes = 63;
		size_t const kShapeNumberAt = 20;        // in SHPHEAD1's payload (shape_model.h)
		int64_t const kFarthestPlace = 1 << 24;  // chunk.hpp:492

		bool Say(std::string *why, char const *words)
		{
			if (why)
			{
				*why = words;
			}

			return false;
		}

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

		// The name an RBOBJECT chunk's header holds, or false
		bool NameOfObjectChunk(Image const &bytes, Span const &object, std::string *name, Span *header)
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
					size_t const payload = child.at + kHeadingBytes;
					size_t const length = child.size - kHeadingBytes;

					if (length < kNameAt + 1)
					{
						return false;
					}

					char const *const first = bytes.data() + payload + kNameAt;
					char const *const zero = (char const *)memchr(first, 0, length - kNameAt);

					if (!zero || (size_t)(zero - first) > kMostNameBytes)
					{
						return false;
					}

					name->assign(first, (size_t)(zero - first));
					*header = child;
					return true;
				}
			}

			return false;
		}

		bool HoldsId(Image const &bytes, Span const &object, uint8_t const *id)
		{
			std::vector<Span> inside;

			if (!Children(bytes, object.at + kHeadingBytes, object.at + object.size, &inside))
			{
				return true;   // a chunk that can't be walked is treated as holding it: the merge is refused
			}

			for (Span const &child : inside)
			{
				for (size_t at = child.at + kHeadingBytes; at + 8 <= child.at + child.size; at++)
				{
					if (memcmp(bytes.data() + at, id, 8) == 0)
					{
						return true;
					}
				}
			}

			return false;
		}

		// A shape's chunk is one of these (the subset's refusals): none may be present on either shape
		char const *const kRefusedChunks[] = { "ANIMSEQU", "TEXTANIM", "SHPMRGDT", "SHPMORPH", "SHPUVCRD", "SHPTEXFN", "SHPPCINF" };

		bool HasRefusedChunk(Shape_Chunk *shape, std::string *why)
		{
			for (char const *const name : kRefusedChunks)
			{
				if (shape->lookup_single_child(name))
				{
					std::string words = "a shape has a ";

					words += name;
					words += " chunk, which Merge Objects doesn't handle yet";
					Say(why, words.c_str());
					return true;
				}
			}

			return false;
		}

		void AppendChunk(Image *out, char const *name, Payload const &payload)
		{
			size_t const start = out->size();

			out->resize(start + kHeadingBytes + payload.size());
			memcpy(out->data() + start, name, 8);
			PutIntAt(out, start + 8, (int32_t)(kHeadingBytes + payload.size()));

			if (!payload.empty())
			{
				memcpy(out->data() + start + kHeadingBytes, payload.data(), payload.size());
			}
		}

		void AppendBytes(Image *out, Image const &from, size_t at, size_t size)
		{
			out->insert(out->end(), from.begin() + (std::ptrdiff_t)at, from.begin() + (std::ptrdiff_t)(at + size));
		}
	}

	char const *ShapeMergeEditWords(ShapeMergeEditResult result)
	{
		switch (result)
		{
		case ShapeMergeEditResult::Done: return "done";
		case ShapeMergeEditResult::BadCall: return "nothing to merge";
		case ShapeMergeEditResult::NoObject: return "no object has that name";
		case ShapeMergeEditResult::NoShape: return "an object has no shape the editor can read";
		case ShapeMergeEditResult::SameObject: return "the two objects are one object, or use one shape";
		case ShapeMergeEditResult::IsAModule: return "a module can't be merged (other chunks name modules)";
		case ShapeMergeEditResult::SharedShape: return "another object uses the first shape too, which Merge Objects doesn't handle yet";
		case ShapeMergeEditResult::Unsupported: return "a shape has a chunk Merge Objects doesn't handle yet";
		case ShapeMergeEditResult::ModelRefused: return "the shapes' numbers wouldn't merge";
		case ShapeMergeEditResult::Referenced: return "another object names the second object, so it can't be taken out (inteng would leave the name pointing at nothing)";
		case ShapeMergeEditResult::CantBuild: return "the level couldn't be put together from its bytes";
		case ShapeMergeEditResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	ShapeMergeEditResult ShapeMergeEdit_Objects(OpenLevelFile *file, uint8_t const *first, uint8_t const *second, std::string *why)
	{
		if (!file || !first || !second || !file->Level())
		{
			return ShapeMergeEditResult::BadCall;
		}

		File_Chunk *const level = file->Level();

		if (memcmp(first, second, 8) == 0)
		{
			return ShapeMergeEditResult::SameObject;
		}

		Object_Chunk *const objectA = ObjectWithId(level, first);
		Object_Chunk *const objectB = ObjectWithId(level, second);

		if (!objectA || !objectB)
		{
			return ShapeMergeEditResult::NoObject;
		}

		if (objectA->lookup_single_child("MODULEDT") || objectB->lookup_single_child("MODULEDT"))
		{
			return ShapeMergeEditResult::IsAModule;
		}

		Shape_Chunk *const shapeA = objectA->get_assoc_shape();
		Shape_Chunk *const shapeB = objectB->get_assoc_shape();

		if (!shapeA || !shapeB)
		{
			return ShapeMergeEditResult::NoShape;
		}

		if (shapeA == shapeB)
		{
			return ShapeMergeEditResult::SameObject;
		}

		if (HasRefusedChunk(shapeA, why) || HasRefusedChunk(shapeB, why))
		{
			return ShapeMergeEditResult::Unsupported;
		}

		// The shapes' numbers, from the editor's own bytes (an earlier edit is seen)
		model::ShapeModel modelA, modelB;
		size_t usersA = 0, usersB = 0;
		ShapeEditResult const readA = ShapeEdit_Read(level, file, first, &modelA, &usersA, why);
		ShapeEditResult const readB = ShapeEdit_Read(level, file, second, &modelB, &usersB, why);

		if (readA != ShapeEditResult::Done || readB != ShapeEditResult::Done)
		{
			return ShapeMergeEditResult::NoShape;
		}

		if (usersA != 1)
		{
			return ShapeMergeEditResult::SharedShape;
		}

		// The two places, from the objects' headers
		model::ShapePoint places[2];
		Object_Chunk *const objects[2] = { objectA, objectB };

		for (int which = 0; which < 2; which++)
		{
			Object_Header_Chunk *const header = objects[which]->get_header();
			Payload bytes;

			if (!header || !file->PayloadOf(header, &bytes) || bytes.size() < kPlaceAt + 12)
			{
				return ShapeMergeEditResult::NoShape;
			}

			int32_t place[3];

			memcpy(place, &bytes[kPlaceAt], sizeof(place));
			places[which].x = place[0];
			places[which].y = place[1];
			places[which].z = place[2];
		}

		// The numbers of the merged shape
		model::ShapeMergeOutcome outcome;
		model::ShapeMergeResult const merged = model::MergeShapes(&modelA, places[0], modelB, places[1], 0, false, &outcome);

		if (merged != model::ShapeMergeResult::Ok)
		{
			Say(why, model::ShapeMergeWords(merged));
			return ShapeMergeEditResult::ModelRefused;
		}

		int64_t const newPlace[3] = { (int64_t)places[0].x + outcome.centre.x, (int64_t)places[0].y + outcome.centre.y, (int64_t)places[0].z + outcome.centre.z };

		for (int axis = 0; axis < 3; axis++)
		{
			if (newPlace[axis] < -kFarthestPlace || newPlace[axis] > kFarthestPlace)
			{
				Say(why, "the first object would be put farther than 16777216 from the middle, which the game's loader refuses");
				return ShapeMergeEditResult::ModelRefused;
			}
		}

		// The new bytes of the shape's five chunks, each at its new size
		Payload pointsBytes(modelA.points.size() * model::kShapePointBytes);
		Payload polygonBytes(modelA.polygons.size() * model::kShapePolygonBytes);
		Payload polyNormalBytes(modelA.polygonNormals.size() * model::kShapeNormalBytes);
		Payload pointNormalBytes(modelA.pointNormals.size() * model::kShapeNormalBytes);
		Payload headerBytes;
		Shape_Chunk *const owned = shapeA;
		Chunk *const headerChunk = owned->lookup_single_child("SHPHEAD1");

		if (!headerChunk || !file->PayloadOf(headerChunk, &headerBytes))
		{
			return ShapeMergeEditResult::NoShape;
		}

		if (model::WriteShapePoints(modelA, &pointsBytes) != model::ShapeModelResult::Ok ||
			model::WriteShapePolygons(modelA, &polygonBytes) != model::ShapeModelResult::Ok ||
			model::WriteShapeNormals(modelA.polygonNormals, &polyNormalBytes) != model::ShapeModelResult::Ok ||
			model::WriteShapeNormals(modelA.pointNormals, &pointNormalBytes) != model::ShapeModelResult::Ok ||
			model::WriteShapeHeaderSettingCounts(modelA, &headerBytes) != model::ShapeModelResult::Ok)
		{
			Say(why, "the merged shape's numbers wouldn't go into chunks");
			return ShapeMergeEditResult::ModelRefused;
		}

		// The level as the next save would write it
		Image image;

		if (!file->BuildLevel(image) || image.size() < kHeadingBytes || !NameIs(image, 0, "REBINFF2"))
		{
			return ShapeMergeEditResult::CantBuild;
		}

		std::vector<Span> top;

		if (!Children(image, kHeadingBytes, image.size(), &top))
		{
			return ShapeMergeEditResult::CantBuild;
		}

		// Which top-level chunk is which: the two objects by name, the first shape by its number
		std::string nameA(objectA->object_data.o_name, strnlen(objectA->object_data.o_name, sizeof(objectA->object_data.o_name)));
		std::string nameB(objectB->object_data.o_name, strnlen(objectB->object_data.o_name, sizeof(objectB->object_data.o_name)));
		bool foundA = false, foundB = false, foundShape = false;
		size_t at_A = 0, at_B = 0, at_Shape = 0;
		Span headerOfA = { 0, 0 };

		for (size_t i = 0; i < top.size(); i++)
		{
			if (NameIs(image, top[i].at, "RBOBJECT"))
			{
				std::string name;
				Span header;

				if (!NameOfObjectChunk(image, top[i], &name, &header))
				{
					return ShapeMergeEditResult::CantBuild;
				}

				if (!foundA && name == nameA)
				{
					foundA = true;
					at_A = i;
					headerOfA = header;
				}
				else if (!foundB && name == nameB)
				{
					foundB = true;
					at_B = i;
				}
			}
			else if (NameIs(image, top[i].at, "REBSHAPE") && !foundShape)
			{
				std::vector<Span> inside;

				if (!Children(image, top[i].at + kHeadingBytes, top[i].at + top[i].size, &inside))
				{
					return ShapeMergeEditResult::CantBuild;
				}

				for (Span const &child : inside)
				{
					if (NameIs(image, child.at, "SHPHEAD1") && child.size >= kHeadingBytes + kShapeNumberAt + 4 &&
						IntAt(image, child.at + kHeadingBytes + kShapeNumberAt) == modelA.number)
					{
						foundShape = true;
						at_Shape = i;
						break;
					}
				}
			}
		}

		if (!foundA || !foundB || !foundShape)
		{
			return ShapeMergeEditResult::CantBuild;
		}

		// The second object may not be named by any other object: the bytes of every other RBOBJECT are searched for its 8-byte ID
		for (size_t i = 0; i < top.size(); i++)
		{
			if (i != at_B && NameIs(image, top[i].at, "RBOBJECT") && HoldsId(image, top[i], second))
			{
				return ShapeMergeEditResult::Referenced;
			}
		}

		// The new image: every top-level chunk as it was, except the second object (left out), the first object (its place moved) and the
		// first shape (its five chunks made again)
		Image out;

		AppendBytes(&out, image, 0, kHeadingBytes);

		for (size_t i = 0; i < top.size(); i++)
		{
			if (i == at_B)
			{
				continue;
			}

			size_t const start = out.size();

			AppendBytes(&out, image, top[i].at, top[i].size);

			if (i == at_A)
			{
				// OBJHEAD1's place: the chunk's own offset of the header, plus the heading and the place's offset in the payload
				size_t const placeAt = start + (headerOfA.at - top[i].at) + kHeadingBytes + kPlaceAt;

				for (int axis = 0; axis < 3; axis++)
				{
					PutIntAt(&out, placeAt + 4 * (size_t)axis, (int32_t)newPlace[axis]);
				}
			}
			else if (i == at_Shape)
			{
				std::vector<Span> inside;

				if (!Children(image, top[i].at + kHeadingBytes, top[i].at + top[i].size, &inside))
				{
					return ShapeMergeEditResult::CantBuild;
				}

				out.resize(start);
				AppendBytes(&out, image, top[i].at, kHeadingBytes);   // the name and an old size, made right below

				bool done[5] = { false, false, false, false, false };
				char const *const names[5] = { "SHPHEAD1", "SHPRAWVT", "SHPPOLYS", "SHPPNORM", "SHPVNORM" };
				Payload const *const fresh[5] = { &headerBytes, &pointsBytes, &polygonBytes, &polyNormalBytes, &pointNormalBytes };

				for (Span const &child : inside)
				{
					int which = -1;

					for (int n = 0; n < 5; n++)
					{
						if (!done[n] && NameIs(image, child.at, names[n]))
						{
							which = n;
							break;
						}
					}

					if (which < 0)
					{
						AppendBytes(&out, image, child.at, child.size);
					}
					else
					{
						done[which] = true;
						AppendChunk(&out, names[which], *fresh[which]);
					}
				}

				for (int n = 0; n < 5; n++)
				{
					if (!done[n])
					{
						return ShapeMergeEditResult::CantBuild;   // the shape lacked one of the five (a missing normals chunk was refused above)
					}
				}

				PutIntAt(&out, start + 8, (int32_t)(out.size() - start));
			}
		}

		PutIntAt(&out, 8, (int32_t)out.size());

		LevelOpenResult const result = file->ReplaceLevel(out);

		switch (result)
		{
		case LevelOpenResult::Opened: return ShapeMergeEditResult::Done;
		case LevelOpenResult::CantRead: return ShapeMergeEditResult::CantBuild;
		default: return ShapeMergeEditResult::LoaderRefused;
		}
	}
}
