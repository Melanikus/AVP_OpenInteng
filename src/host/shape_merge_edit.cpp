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
#include "anim_edit.h"        // AnimEdit_Read (TB-C8d)
#include "shape_merge.h"      // model::MergeShapes
#include "shape_model.h"
#include "shape_maths.h"       // model::ShapeRadiusAboutCentre

namespace host
{
	namespace
	{
		typedef std::vector<char> Image;
		typedef std::vector<unsigned char> Payload;

		size_t const kHeadingBytes = 12;         // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const kPlaceAt = 20;              // in an object header's payload: three ints
		size_t const kTurnAt = 32;               // in an object header's payload: the turn, four floats x, y, z, w (shape_edit.cpp kHeaderTurnAt)
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

		// A shape's chunk is one of these (the subset's refusals): none may be present on either shape. SHPUVCRD and SHPTEXFN are merged (TB-C8a), SHPMRGDT too (TB-C8c, below),
		// and the first's first SHPPCINF goes (TB-C8g, below).
		// NAMED (Bishop's ruling 2026-11-17, TB-C8e / C8f): ANIMSEQU and CONSHAPE are refused, not merged: no placed object of six stock levels has either, inteng's ANIMSEQU merge
		// corrupts (FUN_004497a0 / 004495f0: the frames' points are not moved, an unmatched first sequence is deleted), and a CONSHAPE was copied through unmerged (wrong data).
		// SHPMORPH: inteng refuses it too, before anything is merged (TB-C8i).
		char const *const kRefusedChunks[] = { "ANIMSEQU", "CONSHAPE", "SHPMORPH" };   // TEXTANIM is merged (TB-C8d, below)
		char const *const kRefusedWhy[] = { "the merge would lose the shape's animation sequences", "the merge would lose the shape's construction shape",
			"Cannot merge objects with morphing data" };

		// A shape's merge data, SHPMRGDT's payload (chunk_shape.cpp: one int per polygon, no count): *entries; a shape with none gives none. *has: it had the chunk. False when the
		// payload is not a whole number of ints.
		bool MergeDataOf(OpenLevelFile *file, Shape_Chunk *shape, std::vector<int32_t> *entries, bool *has)
		{
			entries->clear();
			*has = false;

			Chunk *const chunk = shape->lookup_single_child("SHPMRGDT");

			if (!chunk)
			{
				return true;
			}

			Payload bytes;

			if (!file->PayloadOf(chunk, &bytes) || bytes.size() % 4 != 0)
			{
				return false;
			}

			entries->resize(bytes.size() / 4);

			if (!bytes.empty())
			{
				memcpy(entries->data(), bytes.data(), bytes.size());
			}

			*has = true;
			return true;
		}

		// A shape's UV lists, SHPUVCRD's payload (chunk_shape.cpp): an int count, then each list: an int number of corners (0 to 4) and 8 bytes a corner. *count and the bytes after
		// the count; a shape with no SHPUVCRD has none (count 0). False when the payload does not hold exactly what its count says (the game's loader refuses such a chunk too).
		bool UvListsOf(OpenLevelFile *file, Shape_Chunk *shape, int32_t *count, Payload *lists)
		{
			*count = 0;
			lists->clear();

			Chunk *const chunk = shape->lookup_single_child("SHPUVCRD");

			if (!chunk)
			{
				return true;
			}

			Payload bytes;

			if (!file->PayloadOf(chunk, &bytes) || bytes.size() < 4)
			{
				return false;
			}

			int32_t lists_in = 0;

			memcpy(&lists_in, bytes.data(), 4);

			if (lists_in < 0 || (size_t)lists_in > (bytes.size() - 4) / 4)
			{
				return false;
			}

			size_t at = 4;

			for (int32_t i = 0; i < lists_in; i++)
			{
				int32_t corners = 0;

				if (at + 4 > bytes.size())
				{
					return false;
				}

				memcpy(&corners, bytes.data() + at, 4);

				if (corners < 0 || corners > 4 || at + 4 + 8 * (size_t)corners > bytes.size())
				{
					return false;
				}

				at += 4 + 8 * (size_t)corners;
			}

			*count = lists_in;
			lists->assign(bytes.begin() + 4, bytes.begin() + (std::ptrdiff_t)at);
			return true;
		}

		bool HasRefusedChunk(Shape_Chunk *shape, std::string *why)
		{
			for (size_t at = 0; at < sizeof(kRefusedChunks) / sizeof(kRefusedChunks[0]); at++)
			{
				if (shape->lookup_single_child(kRefusedChunks[at]))
				{
					std::string words = "a shape has a ";

					words += kRefusedChunks[at];
					words += " chunk: ";
					words += kRefusedWhy[at];
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

		// ---- A shape's list of the objects that use it (TB-C8b), the way shape_replace_edit.cpp reads and writes it: SHPHEAD1's payload holds 68 bytes of numbers, the count at 64,
		// then each name and its zero, then zeros up to a whole number of 4 bytes (Shape_Header_Chunk::size_chunk)
		size_t const kShapeCountAt = 64;
		size_t const kShapeHeadBytes = 68;
		size_t const kObjectShapeAt = 56;        // in OBJHEAD1's payload: the number of the shape the object uses (shape_id_no)

		// The names in a SHPHEAD1 payload, in order; false when the count or a name doesn't fit
		bool NamesInHead(Payload const &head, std::vector<std::string> *names)
		{
			names->clear();

			if (head.size() < kShapeHeadBytes)
			{
				return false;
			}

			int32_t count;
			size_t at = kShapeHeadBytes;

			memcpy(&count, head.data() + kShapeCountAt, 4);

			if (count < 0 || (size_t)count > head.size() - at)
			{
				return false;
			}

			for (int32_t i = 0; i < count; i++)
			{
				unsigned char const *const zero = at < head.size() ? (unsigned char const *)memchr(head.data() + at, 0, head.size() - at) : nullptr;

				if (!zero)
				{
					return false;
				}

				names->push_back(std::string((char const *)head.data() + at, (size_t)(zero - (head.data() + at))));
				at += names->back().size() + 1;
			}

			return true;
		}

		// The SHPHEAD1 payload with its numbers as they are and another list of names (number: the shape's number, or -1 to keep it)
		Payload HeadWithNames(Payload const &head, std::vector<std::string> const &names, int32_t number)
		{
			Payload out(head.begin(), head.begin() + (std::ptrdiff_t)kShapeHeadBytes);
			int32_t const count = (int32_t)names.size();

			memcpy(out.data() + kShapeCountAt, &count, 4);

			if (number >= 0)
			{
				memcpy(out.data() + kShapeNumberAt, &number, 4);
			}

			for (std::string const &name : names)
			{
				out.insert(out.end(), name.begin(), name.end());
				out.push_back(0);
			}

			while ((kHeadingBytes + out.size()) % 4 != 0)
			{
				out.push_back(0);
			}

			return out;
		}

		// The names without the first one that is `name`
		std::vector<std::string> Without(std::vector<std::string> names, std::string const &name)
		{
			for (size_t at = 0; at < names.size(); at++)
			{
				if (names[at] == name)
				{
					names.erase(names.begin() + (std::ptrdiff_t)at);
					break;
				}
			}

			return names;
		}

		// A REBSHAPE copied with its SHPHEAD1's list of names less `leaving` (each name once); false when its SHPHEAD1 can't be read
		bool AppendShapeWithNames(Image *out, Image const &image, Span const &shape, std::vector<std::string> const &leaving)
		{
			std::vector<Span> inside;

			if (!Children(image, shape.at + kHeadingBytes, shape.at + shape.size, &inside))
			{
				return false;
			}

			size_t const start = out->size();
			bool headDone = false;

			AppendBytes(out, image, shape.at, kHeadingBytes);

			for (Span const &child : inside)
			{
				if (!headDone && NameIs(image, child.at, "SHPHEAD1"))
				{
					Payload head(image.begin() + (std::ptrdiff_t)(child.at + kHeadingBytes), image.begin() + (std::ptrdiff_t)(child.at + child.size));
					std::vector<std::string> names;

					if (!NamesInHead(head, &names))
					{
						return false;
					}

					headDone = true;
					for (std::string const &name : leaving)
					{
						names = Without(names, name);
					}

					AppendChunk(out, "SHPHEAD1", HeadWithNames(head, names, -1));
				}
				else
				{
					AppendBytes(out, image, child.at, child.size);
				}
			}

			PutIntAt(out, start + 8, (int32_t)(out->size() - start));
			return headDone;
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

		// Two objects that use ONE shape (an object and its copy; on stock levels most placed objects share their shape): as inteng (FUN_00449cf0), the first gets a copy
		// of its own (0x449dba-0x449ddb) into which the shape, now the second's, is merged; the shape goes at the end (0x44a830) when the second was its last user
		bool const sameShape = shapeA == shapeB;

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

		// TB-C8b: a first shape other objects use too is not refused any more. As inteng (FUN_00449cf0 0x449dba-0x449ddb: FUN_004d1090 takes the first object off its shape,
		// FUN_0050e880 copies the shape, FUN_004d0ff0 gives the object the copy), the first object gets a COPY of its own, numbered one past the level's highest, and the merge
		// goes into the copy; the other users keep the shape as it was, less the first object's name in its list
		bool const sharedA = usersA > 1;

		// The two places and turns, from the objects' headers
		model::ShapePoint places[2];
		float turns[2][4];
		Object_Chunk *const objects[2] = { objectA, objectB };

		for (int which = 0; which < 2; which++)
		{
			Object_Header_Chunk *const header = objects[which]->get_header();
			Payload bytes;

			if (!header || !file->PayloadOf(header, &bytes) || bytes.size() < kTurnAt + 16)
			{
				return ShapeMergeEditResult::NoShape;
			}

			int32_t place[3];

			memcpy(place, &bytes[kPlaceAt], sizeof(place));
			memcpy(turns[which], &bytes[kTurnAt], sizeof(turns[which]));
			places[which].x = place[0];
			places[which].y = place[1];
			places[which].z = place[2];
		}

		// TB-C8h, THE TURN BAKE: before the merge, inteng turns each shape by its object's turn (FUN_00449970 at 0x449f07 for the first, 0x449f0d for the second) when the
		// turn's x, y or z is not 0 (0x449ac4-0x449aef; w is never looked at; FCOMP and TEST AH,0x40 take a NaN as 0): every point, point normal and polygon normal turned,
		// and the object's turn becomes (0, 0, 0, 1) with its place kept (0x449c41-0x449c62). The shape has one user by then (a shared one was copied first, TB-C8b).
		bool baked[2] = { false, false };
		model::ShapeModel *const bakedModels[2] = { &modelA, &modelB };

		for (int which = 0; which < 2; which++)
		{
			auto zero = [](float f)
			{
				uint32_t bits;

				memcpy(&bits, &f, sizeof(bits));
				return (bits & 0x7FFFFFFFu) == 0 || ((bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0);   // 0 or -0, or a NaN (read by its bits: /fp:fast)
			};

			baked[which] = !(zero(turns[which][0]) && zero(turns[which][1]) && zero(turns[which][2]));

			if (baked[which] && model::TurnShapeByTurn(bakedModels[which], turns[which]) != model::ShapeMathsResult::Ok)
			{
				Say(why, "an object's turn would take its shape's points out of range (or isn't a number)");
				return ShapeMergeEditResult::ModelRefused;
			}
		}

		// The numbers of the merged shape
		model::ShapeMergeOutcome outcome;
		// THE UV LISTS (TB-C8a, construction-closeout-frost.md; FUN_00448cf0 0x448f57-0x449029): the merged shape's lists are the first's, then the second's; each of the second's
		// textured polygons takes its list number plus the first's count (the model does that); SHPTEXFN is the first's (FUN_0042fc00 copies the first's record)
		int32_t uvCountA = 0, uvCountB = 0;
		Payload uvListsA, uvListsB;

		if (!UvListsOf(file, shapeA, &uvCountA, &uvListsA) || !UvListsOf(file, shapeB, &uvCountB, &uvListsB))
		{
			Say(why, "a shape's UV lists (SHPUVCRD) don't hold what their count says");
			return ShapeMergeEditResult::Unsupported;
		}

		model::ShapeMergeResult const merged = model::MergeShapes(&modelA, places[0], modelB, places[1], (size_t)uvCountA, false, &outcome);

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
		// the merged SHPUVCRD's payload: the count of both, the first's lists, the second's (written only when either shape has lists)
		bool const uvMerged = uvCountA + (int64_t)uvCountB > 0;
		Payload uvBytes;

		if (uvMerged)
		{
			int32_t const total = uvCountA + uvCountB;

			uvBytes.resize(4);
			memcpy(uvBytes.data(), &total, 4);
			uvBytes.insert(uvBytes.end(), uvListsA.begin(), uvListsA.end());
			uvBytes.insert(uvBytes.end(), uvListsB.begin(), uvListsB.end());
		}

		// TB-C8k: SHPCENTR worked out again for the merged shape, as inteng's rebuild does (FUN_0050f570 at 0x44a780, 0x50fc36-0x50fcd4): the middle of the merged extents, each sum
		// halved toward zero, then the longest distance of a point from it (the way poly_edit.cpp remakes it). Before, the first's old centre was kept.
		Payload centreBytes(16);
		int32_t const mid[3] =
		{
			(int32_t)(((int64_t)modelA.extents.maxX + (int64_t)modelA.extents.minX) / 2),
			(int32_t)(((int64_t)modelA.extents.maxY + (int64_t)modelA.extents.minY) / 2),
			(int32_t)(((int64_t)modelA.extents.maxZ + (int64_t)modelA.extents.minZ) / 2),
		};
		float const aboutCentre = model::ShapeRadiusAboutCentre(modelA);

		memcpy(centreBytes.data(), mid, 12);
		memcpy(centreBytes.data() + 12, &aboutCentre, 4);
		// THE MERGE DATA (TB-C8c; FUN_00449cf0 0x44a466-0x44a4ba): the first's entries, then the second's, each -1 kept and any other plus the first's count; the polygons are the
		// first's then the second's (the model's order). INTENG BUG, FIXED: inteng takes both shapes' SHPMRGDT without looking (0x44a29f / 0x44a42f only log "Tried to get first
		// entry from a 0-length list" and go on with the list's sentinel); here a shape without one counts as all -1 (none of its polygons merged), and the chunk is written when
		// either shape has one.
		std::vector<int32_t> mergeA, mergeB;
		bool hasMergeA = false, hasMergeB = false;

		if (!MergeDataOf(file, shapeA, &mergeA, &hasMergeA) || !MergeDataOf(file, shapeB, &mergeB, &hasMergeB))
		{
			Say(why, "a shape's merge data (SHPMRGDT) is not a whole number of entries");
			return ShapeMergeEditResult::Unsupported;
		}

		bool const mergeMerged = hasMergeA || hasMergeB;
		Payload mergeBytes;

		if (mergeMerged)
		{
			if (!hasMergeA)
			{
				mergeA.assign(outcome.firstPolygons, -1);
			}

			if (!hasMergeB)
			{
				mergeB.assign(modelA.polygons.size() - outcome.firstPolygons, -1);
			}

			std::vector<int32_t> all = mergeA;
			int64_t const offset = (int64_t)mergeA.size();

			for (int32_t const entry : mergeB)
			{
				int64_t const moved = entry == -1 ? -1 : (int64_t)entry + offset;

				if (moved < -1 || moved > INT32_MAX)
				{
					Say(why, "a shape's merge data names a polygon out of range");
					return ShapeMergeEditResult::Unsupported;
				}

				all.push_back((int32_t)moved);
			}

			mergeBytes.resize(all.size() * 4);

			if (!all.empty())
			{
				memcpy(mergeBytes.data(), all.data(), mergeBytes.size());
			}
		}

		// THE ANIMATION (TB-C8d; FUN_004492f0(S1, S2, S1's polygon count before the merge), called at 0x44a14e): only when the second shape has records (0x449482), the first
		// gets a TEXTANIM when it has none (0x449490-0x4494a8), and each of the second's records is copied (FUN_00407240) with its polygon (+4) plus the first's polygon count
		// (0x449524-0x44952a), AFTER the first's (0x449509-0x449516). The group numbers are not changed. A second without records leaves the first's chunk as it is.
		model::TextureAnim animA, animB;
		int32_t animShape = 0;

		if (AnimEdit_Read(file, first, &animShape, &animA) != AnimEditResult::Done || AnimEdit_Read(file, second, &animShape, &animB) != AnimEditResult::Done)
		{
			Say(why, "a shape's animation (TEXTANIM) couldn't be read");
			return ShapeMergeEditResult::Unsupported;
		}

		bool const animMerged = !animB.polygons.empty();
		Payload animBytes;

		if (animMerged)
		{
			for (model::AnimPolygon polygon : animB.polygons)
			{
				int64_t const moved = (int64_t)polygon.polygon + (int64_t)outcome.firstPolygons;

				if (moved > INT32_MAX)
				{
					Say(why, "a shape's animation names a polygon out of range");
					return ShapeMergeEditResult::ModelRefused;
				}

				polygon.polygon = (int32_t)moved;
				animA.polygons.push_back(polygon);
			}

			if (model::WriteTextureAnim(animA, &animBytes) != model::TextureAnimResult::Ok)
			{
				Say(why, "the merged animation wouldn't go into its chunk (TEXTANIM)");
				return ShapeMergeEditResult::ModelRefused;
			}
		}

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
		bool foundA = false, foundB = false, foundShape = false, foundShapeB = false;
		size_t at_A = 0, at_B = 0, at_Shape = 0, at_ShapeB = 0;
		Span headerOfA = { 0, 0 };
		int64_t highest = -1;   // TB-C8b: the highest shape number the level names, in a shape's SHPHEAD1 or an object's OBJHEAD1 (poly_edit.cpp CloneShapeChunk's rule)

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

				if (header.size >= kHeadingBytes + kObjectShapeAt + 4)
				{
					int32_t const used = IntAt(image, header.at + kHeadingBytes + kObjectShapeAt);

					highest = used > highest ? used : highest;
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
			else if (NameIs(image, top[i].at, "REBSHAPE"))
			{
				std::vector<Span> inside;

				if (!Children(image, top[i].at + kHeadingBytes, top[i].at + top[i].size, &inside))
				{
					return ShapeMergeEditResult::CantBuild;
				}

				for (Span const &child : inside)
				{
					if (NameIs(image, child.at, "SHPHEAD1") && child.size >= kHeadingBytes + kShapeNumberAt + 4)
					{
						int32_t const number = IntAt(image, child.at + kHeadingBytes + kShapeNumberAt);

						highest = number > highest ? number : highest;

						if (!foundShape && number == modelA.number)
						{
							foundShape = true;
							at_Shape = i;
						}
						else if (!foundShapeB && number == modelB.number)
						{
							foundShapeB = true;
							at_ShapeB = i;
						}

						break;
					}
				}
			}
		}

		if (sameShape)
		{
			foundShapeB = foundShape;   // one shape: the second's is the first's
			at_ShapeB = at_Shape;
		}

		if (!foundA || !foundB || !foundShape || !foundShapeB)
		{
			return ShapeMergeEditResult::CantBuild;
		}

		// TB-C8s: the second object's shape goes with it when it was the second's alone, as inteng deletes it (FUN_00449cf0: taken at 0x449d35, deleted at 0x44a830-0x44a83c).
		// TB-C8b: a second shape others use too stays, less the second's name in its list (inteng gives the second a copy first, 0x449e9b-0x449ebc, and deletes the copy).
		// One shape for both: the first has its copy, so the shape goes when the two were its only users, else it stays less both names.
		bool const dropShapeB = sameShape ? usersA == 2 : usersB == 1;

		// TB-C8b: the first object's own copy of a shared shape is numbered one past the highest (the library: file_id_num -1 becomes ++max_id, chunk_shape.cpp prepare_for_output)
		int64_t const copyNumber = highest + 1;

		if (sharedA && (copyNumber < 1 || copyNumber > 65535))
		{
			Say(why, "the level's shape numbers leave no room for the first object's own copy of its shape");
			return ShapeMergeEditResult::ModelRefused;   // Shape_Header_Chunk::Max_File_Id_Num: the game's loader refuses a higher number
		}

		Payload const copyHead = sharedA ? HeadWithNames(headerBytes, std::vector<std::string>(1, nameA), (int32_t)copyNumber) : Payload();
		Image copyShape;   // the merged shape, when it is the first object's new copy: it goes after the last chunk, as inteng's copy is a new child of the file (FUN_0050e880)

		// The second object may not be named by any other object: the bytes of every other RBOBJECT are searched for its 8-byte ID
		for (size_t i = 0; i < top.size(); i++)
		{
			if (i != at_B && NameIs(image, top[i].at, "RBOBJECT") && HoldsId(image, top[i], second))
			{
				return ShapeMergeEditResult::Referenced;
			}
		}

		// The new image: every top-level chunk as it was, except the second object and its own shape (left out), the first object (its place moved) and
		// the first shape (its five chunks made again)
		Image out;

		AppendBytes(&out, image, 0, kHeadingBytes);

		for (size_t i = 0; i < top.size(); i++)
		{
			if (i == at_B || (dropShapeB && i == at_ShapeB && i != at_Shape))
			{
				continue;
			}

			size_t const start = out.size();

			if (i == at_ShapeB || (sharedA && i == at_Shape))
			{
				// a shared shape that stays for its other users: as it was, less the merged objects' names in its list (both, when the two used it). One the two used alone
				// is not written: the first's copy below is made from its bytes all the same.
				std::vector<std::string> leaving;

				if (sharedA && i == at_Shape)
				{
					leaving.push_back(nameA);
				}

				if (i == at_ShapeB)
				{
					leaving.push_back(nameB);
				}

				if (!(dropShapeB && i == at_ShapeB) && !AppendShapeWithNames(&out, image, top[i], leaving))
				{
					return ShapeMergeEditResult::CantBuild;
				}

				if (i != at_Shape)
				{
					continue;
				}
			}
			else
			{
				AppendBytes(&out, image, top[i].at, top[i].size);
			}

			if (i == at_A)
			{
				// OBJHEAD1's place: the chunk's own offset of the header, plus the heading and the place's offset in the payload
				size_t const placeAt = start + (headerOfA.at - top[i].at) + kHeadingBytes + kPlaceAt;

				for (int axis = 0; axis < 3; axis++)
				{
					PutIntAt(&out, placeAt + 4 * (size_t)axis, (int32_t)newPlace[axis]);
				}

				// TB-C8b: the first object uses its own copy now (FUN_004d0ff0)
				if (sharedA)
				{
					PutIntAt(&out, start + (headerOfA.at - top[i].at) + kHeadingBytes + kObjectShapeAt, (int32_t)copyNumber);
				}

				// TB-C8h: its turn is in the shape now, so the object's turn becomes (0, 0, 0, 1) (FUN_00449970 0x449c41-0x449c62)
				if (baked[0])
				{
					float const none[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

					memcpy(out.data() + start + (headerOfA.at - top[i].at) + kHeadingBytes + kTurnAt, none, sizeof(none));
				}
			}
			else if (i == at_Shape)
			{
				std::vector<Span> inside;

				if (!Children(image, top[i].at + kHeadingBytes, top[i].at + top[i].size, &inside))
				{
					return ShapeMergeEditResult::CantBuild;
				}

				Image *const into = sharedA ? &copyShape : &out;
				size_t const from = sharedA ? 0 : start;

				if (!sharedA)
				{
					out.resize(start);
				}

				AppendBytes(into, image, top[i].at, kHeadingBytes);   // the name and an old size, made right below

				bool done[5] = { false, false, false, false, false };
				char const *const names[5] = { "SHPHEAD1", "SHPRAWVT", "SHPPOLYS", "SHPPNORM", "SHPVNORM" };
				Payload const *const fresh[5] = { sharedA ? &copyHead : &headerBytes, &pointsBytes, &polygonBytes, &polyNormalBytes, &pointNormalBytes };
				bool uvDone = false;
				bool centreDone = false;
				bool mergeDone = false;
				bool pcinfDropped = false;
				bool animDone = false;

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

					if (which < 0 && uvMerged && !uvDone && NameIs(image, child.at, "SHPUVCRD"))
					{
						uvDone = true;
						AppendChunk(into, "SHPUVCRD", uvBytes);   // the first's lists, then the second's
					}
					else if (which < 0 && !centreDone && NameIs(image, child.at, "SHPCENTR"))
					{
						centreDone = true;
						AppendChunk(into, "SHPCENTR", centreBytes);   // the merged shape's own centre and radius (TB-C8k)
					}
					else if (which < 0 && mergeMerged && !mergeDone && NameIs(image, child.at, "SHPMRGDT"))
					{
						mergeDone = true;
						AppendChunk(into, "SHPMRGDT", mergeBytes);   // the first's merge data, then the second's moved up
					}
					else if (which < 0 && !pcinfDropped && NameIs(image, child.at, "SHPPCINF"))
					{
						// TB-C8g: the first shape's FIRST SHPPCINF is left out, as inteng deletes the first entry of its list (FUN_00449cf0: taken at 0x44a5f0, destroyed at
						// 0x44a769-0x44a774); its others stay, and the second's is read (0x44a5f3-0x44a601) and nothing is done with it
						pcinfDropped = true;
					}
					else if (which < 0 && animMerged && !animDone && NameIs(image, child.at, "TEXTANIM"))
					{
						animDone = true;
						AppendChunk(into, "TEXTANIM", animBytes);   // the first's records, then the second's with their polygon moved up (TB-C8d)
					}
					else if (which < 0)
					{
						AppendBytes(into, image, child.at, child.size);
					}
					else
					{
						done[which] = true;
						AppendChunk(into, names[which], *fresh[which]);
					}
				}

				if (uvMerged && !uvDone)
				{
					AppendChunk(into, "SHPUVCRD", uvBytes);   // the first shape had no lists: the second's come in a chunk of their own
				}

				if (animMerged && !animDone)
				{
					AppendChunk(into, "TEXTANIM", animBytes);   // the first shape had no animation: one is made for the second's records (0x449490-0x4494a8)
				}

				if (!centreDone)
				{
					AppendChunk(into, "SHPCENTR", centreBytes);   // the first shape had none: inteng's rebuild makes one (FUN_0050f570 0x50fce0-0x50fd05)
				}

				if (mergeMerged && !mergeDone)
				{
					AppendChunk(into, "SHPMRGDT", mergeBytes);   // the first shape had no merge data: one for the merged shape
				}

				for (int n = 0; n < 5; n++)
				{
					if (!done[n])
					{
						return ShapeMergeEditResult::CantBuild;   // the shape lacked one of the five (a missing normals chunk was refused above)
					}
				}

				PutIntAt(into, from + 8, (int32_t)(into->size() - from));
			}
		}

		// TB-C8b: the first object's own copy of the shape, merged, after the last chunk
		out.insert(out.end(), copyShape.begin(), copyShape.end());

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
