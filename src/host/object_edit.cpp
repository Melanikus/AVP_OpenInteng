// See object_edit.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (S5). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "object_edit.h"

#include <string.h>

#include <algorithm>
#include <map>
#include <exception>
#include <set>
#include <string>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk (Max_Index_Num, object_data)
#include "mishchnk.hpp"       // File_Chunk::list_objects

#include "field_model.h"      // model::MakeNewStrategy: a new strategy of a kind the old editor offered
#include "level_file.h"

namespace host
{
	namespace
	{
		typedef std::vector<char> Bytes;

		size_t const HeadingBytes = 12;          // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const NameAt = 60;                // in an object header's payload: the name starts here, after 60 bytes of numbers
		size_t const FlagsAt = 0;
		size_t const LockUserAt = 4;             // 16 bytes
		size_t const PlaceAt = 20;               // three ints
		size_t const IndexAt = 48;
		size_t const MostNameBytes = 63;         // OBJECT_NAME_BYTES (64) with its ending zero
		int32_t const PlacedObjectFlag = 0x400;  // OBJECT_FLAG_PLACED_OBJECT (chunk_object.hpp)
		int32_t const MostPlace = 1 << 24;       // CheckPlace (chunk.hpp): a place farther than 2^24 millimetres from the middle is refused

		int32_t IntAt(Bytes const &bytes, size_t at)
		{
			int32_t value;
			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		void PutIntAt(Bytes &bytes, size_t at, int32_t value)
		{
			memcpy(bytes.data() + at, &value, sizeof(value));
		}

		// The size of the chunk at `at`, inside [at, end), or false when its heading isn't there or its size isn't a size that fits
		bool ChunkSizeAt(Bytes const &bytes, size_t at, size_t end, size_t *size)
		{
			if (end > bytes.size() || at > end || end - at < HeadingBytes)
			{
				return false;
			}

			int32_t const stored = IntAt(bytes, at + 8);

			if (stored < (int32_t)HeadingBytes || (size_t)stored > end - at)
			{
				return false;
			}

			*size = (size_t)stored;
			return true;
		}

		bool NameIs(Bytes const &bytes, size_t at, char const *name)
		{
			return memcmp(bytes.data() + at, name, 8) == 0;
		}

		struct Piece
		{
			size_t at;
			size_t size;
		};

		struct ObjectInfo
		{
			Piece whole;                 // the RBOBJECT chunk
			std::string name;
			int32_t index;
			int32_t flags;
			bool module;                 // it has a MODULEDT
			bool hasHeader;
			bool hasStrategy;
			Piece strategy;              // the AVPSTRAT chunk, when it has one (its payload is after the 12-byte heading)
			bool hasProject;             // it has an OBJPRJDT, the chunk a strategy is held in
			Piece moduleData;            // the MODULEDT chunk, when module is true
			bool hasLinks;               // its MODULEDT has a VMDARRAY (the first one is the one the game uses)
			Piece links;
			bool hasAdj = false;         // its MODULEDT has an ADJMDLEP (the first one)
			Piece adj = { 0, 0 };
			std::vector<Piece> moduleChildren;   // every chunk directly inside its (first) MODULEDT, in order
			Piece project;
			std::vector<Piece> children; // every chunk directly inside it, in order
		};

		// Reads the chunks inside one object. False for bytes that can't be walked (a size that doesn't fit, a name with no ending zero or too
		// long, a header too short).
		bool ReadObject(Bytes const &bytes, Piece whole, ObjectInfo *info)
		{
			info->whole = whole;
			info->index = 0;
			info->flags = 0;
			info->module = false;
			info->hasHeader = false;
			info->hasStrategy = false;
			info->hasProject = false;
			info->hasLinks = false;
			info->hasAdj = false;
			info->moduleChildren.clear();
			info->children.clear();

			for (size_t at = whole.at + HeadingBytes; at < whole.at + whole.size; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, whole.at + whole.size, &size))
				{
					return false;
				}

				Piece const child = { at, size };
				info->children.push_back(child);

				if (NameIs(bytes, at, "OBJHEAD1"))
				{
					size_t const payload = at + HeadingBytes;
					size_t const length = size - HeadingBytes;

					if (info->hasHeader || length < NameAt + 1)
					{
						return false;
					}

					char const *const first = bytes.data() + payload + NameAt;
					size_t const room = length - NameAt;
					char const *const zero = (char const *)memchr(first, 0, room);

					if (!zero || (size_t)(zero - first) > MostNameBytes)
					{
						return false;
					}

					info->name.assign(first, (size_t)(zero - first));
					info->flags = IntAt(bytes, payload + FlagsAt);
					info->index = IntAt(bytes, payload + IndexAt);
					info->hasHeader = true;
				}
				else if (NameIs(bytes, at, "MODULEDT"))
				{
					// The first MODULEDT is the one the game uses (lookup_single_child)
					if (!info->module)
					{
						info->module = true;
						info->moduleData = child;

						for (size_t inner = at + HeadingBytes; inner < at + size; )
						{
							size_t innerSize = 0;

							if (!ChunkSizeAt(bytes, inner, at + size, &innerSize))
							{
								return false;
							}

							if (NameIs(bytes, inner, "VMDARRAY") && !info->hasLinks)
							{
								info->hasLinks = true;
								info->links.at = inner;
								info->links.size = innerSize;
							}

							if (NameIs(bytes, inner, "ADJMDLEP") && !info->hasAdj)
							{
								info->hasAdj = true;
								info->adj.at = inner;
								info->adj.size = innerSize;
							}

							Piece const inside = { inner, innerSize };

							info->moduleChildren.push_back(inside);

							inner += innerSize;
						}
					}
				}
				else if (NameIs(bytes, at, "OBJPRJDT"))
				{
					// The first one is the one the game uses (lookup_single_child)
					if (!info->hasProject)
					{
						info->hasProject = true;
						info->project = child;
					}

					for (size_t inner = at + HeadingBytes; info->project.at == at && inner < at + size; )
					{
						size_t innerSize = 0;

						if (!ChunkSizeAt(bytes, inner, at + size, &innerSize))
						{
							return false;
						}

						if (NameIs(bytes, inner, "AVPSTRAT") && !info->hasStrategy)
						{
							info->strategy.at = inner;
							info->strategy.size = innerSize;
							info->hasStrategy = true;
						}

						inner += innerSize;
					}
				}

				at += size;
			}

			return info->hasHeader;
		}

		// Every object of the level, in the file's order. False when the level's own chunks can't be walked.
		bool ReadObjects(Bytes const &bytes, std::vector<ObjectInfo> *objects)
		{
			objects->clear();

			if (bytes.size() < HeadingBytes || !NameIs(bytes, 0, "REBINFF2"))
			{
				return false;
			}

			for (size_t at = HeadingBytes; at < bytes.size(); )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, bytes.size(), &size))
				{
					return false;
				}

				if (NameIs(bytes, at, "RBOBJECT"))
				{
					ObjectInfo info;
					Piece const whole = { at, size };

					if (!ReadObject(bytes, whole, &info))
					{
						return false;
					}

					objects->push_back(info);
				}

				at += size;
			}

			return true;
		}

		bool IdIsAllZero(uint8_t const *id)
		{
			for (size_t i = 0; i < 8; i++)
			{
				if (id[i] != 0)
				{
					return false;
				}
			}

			return true;
		}

		// The first object of the library's level with that ID, and its name, which is how it is found among the bytes
		bool NameOfObject(OpenLevelFile *file, uint8_t const *id, std::string *name)
		{
			File_Chunk *const level = file->Level();

			if (!level)
			{
				return false;
			}

			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && memcmp(&object->object_data.ID, id, 8) == 0)
				{
					size_t length = 0;

					while (length < sizeof(object->object_data.o_name) && object->object_data.o_name[length] != 0)
					{
						length++;
					}

					name->assign(object->object_data.o_name, length);
					return true;
				}
			}

			return false;
		}

		// The ID of the first object of the library's level with that name, found after the level was replaced
		bool IdOfName(OpenLevelFile *file, std::string const &name, uint8_t *id)
		{
			File_Chunk *const level = file->Level();

			if (!level)
			{
				return false;
			}

			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && strncmp(object->object_data.o_name, name.c_str(), sizeof(object->object_data.o_name)) == 0)
				{
					memcpy(id, &object->object_data.ID, 8);
					return true;
				}
			}

			return false;
		}

		bool NameIsTaken(std::vector<ObjectInfo> const &objects, std::string const &name)
		{
			for (ObjectInfo const &object : objects)
			{
				if (_stricmp(object.name.c_str(), name.c_str()) == 0)
				{
					return true;
				}
			}

			return false;
		}

		bool NameIsPlainText(char const *name)
		{
			size_t length = 0;

			while (name[length] != 0 && length <= MostNameBytes)
			{
				if ((unsigned char)name[length] < 32 || (unsigned char)name[length] > 126)
				{
					return false;
				}

				length++;
			}

			return length >= 1 && length <= MostNameBytes;
		}

		ObjectEditResult FromLoader(LevelOpenResult result)
		{
			switch (result)
			{
			case LevelOpenResult::Opened: return ObjectEditResult::Done;
			case LevelOpenResult::NothingOpen: return ObjectEditResult::NothingOpen;
			case LevelOpenResult::CantRead: return ObjectEditResult::CantBuild;
			default: return ObjectEditResult::LoaderRefused;
			}
		}

		// The open level as the next save would write it, its objects, and the first of them with this ID
		ObjectEditResult Prepare(OpenLevelFile *file, uint8_t const *id, Bytes *image, std::vector<ObjectInfo> *objects, size_t *found)
		{
			if (!file || !file->Level())
			{
				return ObjectEditResult::NothingOpen;
			}

			if (!id || IdIsAllZero(id))
			{
				return ObjectEditResult::NoName;
			}

			std::string name;

			if (!NameOfObject(file, id, &name))
			{
				return ObjectEditResult::NoSuchObject;
			}

			if (!file->BuildLevel(*image) || !ReadObjects(*image, objects))
			{
				return ObjectEditResult::CantBuild;
			}

			for (size_t i = 0; i < objects->size(); i++)
			{
				if ((*objects)[i].name == name)
				{
					*found = i;
					return ObjectEditResult::Done;
				}
			}

			return ObjectEditResult::NoSuchObject;
		}

		// Takes the object with this ID out (remove), or adds a copy of it
		ObjectEditResult Edit(OpenLevelFile *file, uint8_t const *id, char const *name, int32_t const *place, bool remove, uint8_t *newId)
		{
			Bytes image;
			std::vector<ObjectInfo> objects;
			size_t at = 0;
			ObjectEditResult const prepared = Prepare(file, id, &image, &objects, &at);

			if (prepared != ObjectEditResult::Done)
			{
				return prepared;
			}

			ObjectInfo const object = objects[at];

			if (object.module)
			{
				return ObjectEditResult::IsAModule;
			}

			if (remove)
			{
				// inteng's Delete (OP 0x16, the placed object's method +0x24 at 0x4e0480) never asks who names the object, so neither does this: a name another object holds
				// is left as it is, pointing at nothing (Melanikus's ruling 2026-11-15, relayed by Bishop: copy inteng). Merge Objects keeps its own refusal.
				image.erase(image.begin() + (std::ptrdiff_t)object.whole.at, image.begin() + (std::ptrdiff_t)(object.whole.at + object.whole.size));
				PutIntAt(image, 8, (int32_t)image.size());
				return FromLoader(file->ReplaceLevel(image));
			}

			// A copy
			if (!(object.flags & PlacedObjectFlag))
			{
				return ObjectEditResult::NotPlaced;
			}

			std::string made;

			if (name && name[0])
			{
				if (!NameIsPlainText(name))
				{
					return ObjectEditResult::BadName;
				}

				made = name;

				if (NameIsTaken(objects, made))
				{
					return ObjectEditResult::NameTaken;
				}
			}
			else
			{
				std::string const base = object.name.substr(0, MostNameBytes - 6);

				for (int n = 2; n < 100000 && made.empty(); n++)
				{
					std::string const candidate = base + "_" + std::to_string(n);

					if (!NameIsTaken(objects, candidate))
					{
						made = candidate;
					}
				}

				if (made.empty())
				{
					return ObjectEditResult::NameTaken;
				}
			}

			if (place)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					if (place[axis] > MostPlace || place[axis] < -MostPlace)
					{
						return ObjectEditResult::PlaceOutOfRange;
					}
				}
			}

			int32_t nextIndex = 0;

			for (ObjectInfo const &each : objects)
			{
				nextIndex = each.index >= nextIndex ? each.index + 1 : nextIndex;
			}

			if (nextIndex < 0 || nextIndex > Object_Chunk::Max_Index_Num)
			{
				return ObjectEditResult::TooManyObjects;
			}

			// The copy's bytes: the object's own, but for its header, which is made again with the new name, place, number and nobody's lock
			Bytes copy(HeadingBytes, 0);
			memcpy(copy.data(), "RBOBJECT", 8);

			for (Piece const &child : object.children)
			{
				if (!NameIs(image, child.at, "OBJHEAD1"))
				{
					copy.insert(copy.end(), image.begin() + (std::ptrdiff_t)child.at, image.begin() + (std::ptrdiff_t)(child.at + child.size));
					continue;
				}

				size_t length = HeadingBytes + NameAt + made.size() + 1;
				length += (4 - length % 4) % 4;

				size_t const start = copy.size();
				copy.resize(start + length, 0);
				memcpy(copy.data() + start, "OBJHEAD1", 8);
				PutIntAt(copy, start + 8, (int32_t)length);
				memcpy(copy.data() + start + HeadingBytes, image.data() + child.at + HeadingBytes, NameAt);
				memset(copy.data() + start + HeadingBytes + LockUserAt, 0, 16);
				PutIntAt(copy, start + HeadingBytes + IndexAt, nextIndex);

				if (place)
				{
					for (int axis = 0; axis < 3; axis++)
					{
						PutIntAt(copy, start + HeadingBytes + PlaceAt + 4 * (size_t)axis, place[axis]);
					}
				}

				memcpy(copy.data() + start + HeadingBytes + NameAt, made.c_str(), made.size());
			}

			PutIntAt(copy, 8, (int32_t)copy.size());

			image.insert(image.begin() + (std::ptrdiff_t)(object.whole.at + object.whole.size), copy.begin(), copy.end());
			PutIntAt(image, 8, (int32_t)image.size());

			ObjectEditResult const done = FromLoader(file->ReplaceLevel(image));

			if (done == ObjectEditResult::Done && newId)
			{
				memset(newId, 0, 8);
				IdOfName(file, made, newId);
			}

			return done;
		}

		// The index in objects of the first object with this ID (found by its name among the bytes, as Prepare does), or false
		bool IndexOfObject(OpenLevelFile *file, uint8_t const *id, std::vector<ObjectInfo> const &objects, size_t *index)
		{
			std::string name;

			if (!id || IdIsAllZero(id) || !NameOfObject(file, id, &name))
			{
				return false;
			}

			for (size_t i = 0; i < objects.size(); i++)
			{
				if (objects[i].name == name)
				{
					*index = i;
					return true;
				}
			}

			return false;
		}

		// Puts a whole AVPSTRAT chunk (its heading and payload) in place of the strategy of the object at `target`, in the image, or in when it
		// has none; with a null chunk, takes the strategy out. The sizes of the chunks around it are made again: its OBJPRJDT's, the object's,
		// and the level's own at +8. An OBJPRJDT that holds nothing else goes with it when the strategy is taken out, and one is made when the
		// object has none.
		void PutStrategy(Bytes &image, ObjectInfo const &object, Bytes const *chunk)
		{
			size_t const newSize = chunk ? chunk->size() : 0;

			if (object.hasStrategy)
			{
				bool const onlyChild = object.project.size == HeadingBytes + object.strategy.size;

				if (!chunk && onlyChild)
				{
					// The whole OBJPRJDT goes
					image.erase(image.begin() + (std::ptrdiff_t)object.project.at, image.begin() + (std::ptrdiff_t)(object.project.at + object.project.size));
					PutIntAt(image, object.whole.at + 8, (int32_t)(object.whole.size - object.project.size));
				}
				else
				{
					image.erase(image.begin() + (std::ptrdiff_t)object.strategy.at, image.begin() + (std::ptrdiff_t)(object.strategy.at + object.strategy.size));

					if (chunk)
					{
						image.insert(image.begin() + (std::ptrdiff_t)object.strategy.at, chunk->begin(), chunk->end());
					}

					PutIntAt(image, object.project.at + 8, (int32_t)(object.project.size - object.strategy.size + newSize));
					PutIntAt(image, object.whole.at + 8, (int32_t)(object.whole.size - object.strategy.size + newSize));
				}
			}
			else if (object.hasProject)
			{
				// Into the OBJPRJDT it has, at its end
				image.insert(image.begin() + (std::ptrdiff_t)(object.project.at + object.project.size), chunk->begin(), chunk->end());
				PutIntAt(image, object.project.at + 8, (int32_t)(object.project.size + newSize));
				PutIntAt(image, object.whole.at + 8, (int32_t)(object.whole.size + newSize));
			}
			else
			{
				// A new OBJPRJDT holding it, at the end of the object
				Bytes wrapper(HeadingBytes, 0);

				memcpy(wrapper.data(), "OBJPRJDT", 8);
				wrapper.insert(wrapper.end(), chunk->begin(), chunk->end());
				PutIntAt(wrapper, 8, (int32_t)wrapper.size());
				image.insert(image.begin() + (std::ptrdiff_t)(object.whole.at + object.whole.size), wrapper.begin(), wrapper.end());
				PutIntAt(image, object.whole.at + 8, (int32_t)(object.whole.size + wrapper.size()));
			}

			PutIntAt(image, 8, (int32_t)image.size());
		}

		// A whole AVPSTRAT chunk made of a payload
		Bytes StrategyChunk(uint8_t const *payload, size_t length)
		{
			Bytes chunk(HeadingBytes, 0);

			memcpy(chunk.data(), "AVPSTRAT", 8);
			chunk.insert(chunk.end(), (char const *)payload, (char const *)payload + length);
			PutIntAt(chunk, 8, (int32_t)chunk.size());
			return chunk;
		}

		enum class StrategyWhat
		{
			Delete,
			Assign,
			Copy,
			Replace   // the bytes given
		};

		// What the three strategy edits share: the object `id`, a strategy to put in it (Assign's kind, or Copy's from), or none to take out
		ObjectEditResult EditStrategy(OpenLevelFile *file, uint8_t const *id, StrategyWhat what, int32_t kind, uint8_t const *from, uint8_t const *given = 0, size_t givenLength = 0)
		{
			Bytes image;
			std::vector<ObjectInfo> objects;
			size_t at = 0;
			ObjectEditResult const prepared = Prepare(file, id, &image, &objects, &at);

			if (prepared != ObjectEditResult::Done)
			{
				return prepared;
			}

			ObjectInfo const object = objects[at];

			// A strategy sits on a PLACED object or on a MODULE: the stock levels hold 712 on modules (Doors 417, Lighting 293, Switch Doors 2), which inteng's Assign Strategy gives
			// to modules too (its class takes Door, Lift, Airlock, Switch Door and Lighting there); an object that is neither is refused
			if (!object.module && !(object.flags & PlacedObjectFlag))
			{
				return ObjectEditResult::NotPlaced;
			}

			Bytes chunk;
			Bytes const *put = 0;

			if (what == StrategyWhat::Delete)
			{
				if (!object.hasStrategy)
				{
					return ObjectEditResult::NoStrategy;
				}
			}
			else if (what == StrategyWhat::Replace)
			{
				if (!given || givenLength < 12 || givenLength > 65536)
				{
					return ObjectEditResult::BadStrategy;
				}

				chunk = StrategyChunk(given, givenLength);
				put = &chunk;
			}
			else if (what == StrategyWhat::Assign)
			{
				uint8_t payload[1024];
				size_t length = 0;

				if (!model::MakeNewStrategy(kind, payload, sizeof(payload), &length))
				{
					return ObjectEditResult::BadStrategy;
				}

				// a new Switch Door's associated door is the NULL ID (FUN_0052d020 stores DAT_00914930/34 at +0x14: a .bss pair nothing ever writes, so 0 0; the view shows "None"):
				// the model's creation value 0 (reassess/strategy-data-kinds.md S-K59-0)

				chunk = StrategyChunk(payload, length);
				put = &chunk;
			}
			else
			{
				size_t source = 0;

				if (!IndexOfObject(file, from, objects, &source))
				{
					return ObjectEditResult::NoSuchObject;
				}

				if (source == at)
				{
					return ObjectEditResult::SameObject;
				}

				if (!objects[source].hasStrategy)
				{
					return ObjectEditResult::NoStrategy;
				}

				Piece const piece = objects[source].strategy;

				chunk.assign(image.begin() + (std::ptrdiff_t)piece.at, image.begin() + (std::ptrdiff_t)(piece.at + piece.size));
				put = &chunk;
			}

			// No check of who names the object, as inteng's Copy / Assign / Delete (FUN_00528340 / FUN_00528830 / FUN_00528790) have none: the names stay, and the game follows each by name when a level
			// starts (avpx bh_types.cpp AssignAllSBNames), null-safe and checking the kind it finds (Bishop's ruling 11-16, strategy-data-kinds.md S-KC-15; tests/host_strategy_named_test.cpp)
			PutStrategy(image, object, put);
			return FromLoader(file->ReplaceLevel(image));
		}

		// Puts a whole VMDARRAY chunk in place of the module's, or in at the end of its MODULEDT when it has none, and makes the sizes around it again
		void PutLinks(Bytes &image, ObjectInfo const &object, Bytes const &chunk)
		{
			size_t const newSize = chunk.size();

			if (object.hasLinks)
			{
				image.erase(image.begin() + (std::ptrdiff_t)object.links.at, image.begin() + (std::ptrdiff_t)(object.links.at + object.links.size));
				image.insert(image.begin() + (std::ptrdiff_t)object.links.at, chunk.begin(), chunk.end());
				PutIntAt(image, object.moduleData.at + 8, (int32_t)(object.moduleData.size - object.links.size + newSize));
				PutIntAt(image, object.whole.at + 8, (int32_t)(object.whole.size - object.links.size + newSize));
			}
			else
			{
				image.insert(image.begin() + (std::ptrdiff_t)(object.moduleData.at + object.moduleData.size), chunk.begin(), chunk.end());
				PutIntAt(image, object.moduleData.at + 8, (int32_t)(object.moduleData.size + newSize));
				PutIntAt(image, object.whole.at + 8, (int32_t)(object.whole.size + newSize));
			}

			PutIntAt(image, 8, (int32_t)image.size());
		}

		ObjectEditResult EditLinks(OpenLevelFile *file, uint8_t const *id, ModuleLinkItem const *items, size_t count)
		{
			if (!items || count == 0 || count > 100000)
			{
				return ObjectEditResult::BadLinks;
			}

			Bytes image;
			std::vector<ObjectInfo> objects;
			size_t at = 0;
			ObjectEditResult const prepared = Prepare(file, id, &image, &objects, &at);

			if (prepared != ObjectEditResult::Done)
			{
				return prepared;
			}

			ObjectInfo const object = objects[at];

			if (!object.module)
			{
				return ObjectEditResult::NotAModule;
			}

			// VMDARRAY: its heading, the number of items, then each item as object index, branch number, flags, spare (chunk_object.cpp:661)
			Bytes chunk(HeadingBytes + 4 + 16 * count, 0);

			memcpy(chunk.data(), "VMDARRAY", 8);
			PutIntAt(chunk, 8, (int32_t)chunk.size());
			PutIntAt(chunk, 12, (int32_t)count);

			for (size_t i = 0; i < count; i++)
			{
				size_t const place = HeadingBytes + 4 + 16 * i;

				PutIntAt(chunk, place, items[i].objectIndex);
				PutIntAt(chunk, place + 4, items[i].branchNo);
				PutIntAt(chunk, place + 8, items[i].flags);
				PutIntAt(chunk, place + 12, items[i].spare);
			}

			PutLinks(image, object, chunk);
			return FromLoader(file->ReplaceLevel(image));
		}

		// Several modules' links in ONE new image: each VMDARRAY is put in from the module that comes LAST in the file to the first, so the places the
		// first read found stay right for the ones before. A module with no MODULEDT, or with no items, is skipped (*written false for it).
		ObjectEditResult EditLinksMany(OpenLevelFile *file, std::vector<ModuleLinksFor> const &edits, std::vector<bool> *written)
		{
			written->assign(edits.size(), false);

			if (!file || !file->Level())
			{
				return ObjectEditResult::NothingOpen;
			}

			Bytes image;
			std::vector<ObjectInfo> objects;

			if (!file->BuildLevel(image) || !ReadObjects(image, &objects))
			{
				return ObjectEditResult::CantBuild;
			}

			// which object each edit is for, and where it lies in the file
			struct Place { size_t edit; size_t object; };

			std::vector<Place> places;
			std::set<size_t> usedObjects;

			for (size_t e = 0; e < edits.size(); e++)
			{
				std::string name;

				if (edits[e].items.empty() || edits[e].items.size() > 100000 || IdIsAllZero(edits[e].id) || !NameOfObject(file, edits[e].id, &name))
				{
					continue;
				}

				for (size_t i = 0; i < objects.size(); i++)
				{
					if (objects[i].name == name)
					{
						if (objects[i].module && usedObjects.insert(i).second)
						{
							Place place = { e, i };

							places.push_back(place);
						}

						break;
					}
				}
			}

			if (places.empty())
			{
				return ObjectEditResult::Done;
			}

			std::sort(places.begin(), places.end(), [&objects](Place const &a, Place const &b) { return objects[a.object].whole.at > objects[b.object].whole.at; });

			for (Place const &place : places)
			{
				std::vector<ModuleLinkItem> const &items = edits[place.edit].items;
				Bytes chunk(HeadingBytes + 4 + 16 * items.size(), 0);

				memcpy(chunk.data(), "VMDARRAY", 8);
				PutIntAt(chunk, 8, (int32_t)chunk.size());
				PutIntAt(chunk, 12, (int32_t)items.size());

				for (size_t i = 0; i < items.size(); i++)
				{
					size_t const at = HeadingBytes + 4 + 16 * i;

					PutIntAt(chunk, at, items[i].objectIndex);
					PutIntAt(chunk, at + 4, items[i].branchNo);
					PutIntAt(chunk, at + 8, items[i].flags);
					PutIntAt(chunk, at + 12, items[i].spare);
				}

				PutLinks(image, objects[place.object], chunk);
			}

			ObjectEditResult const result = FromLoader(file->ReplaceLevel(image));

			if (result == ObjectEditResult::Done)
			{
				for (Place const &place : places)
				{
					(*written)[place.edit] = true;
				}
			}

			return result;
		}
	}

	ObjectEditResult SetModuleLinksMany(OpenLevelFile *file, std::vector<ModuleLinksFor> const &edits, std::vector<bool> *written)
	{
		std::vector<bool> none;

		try
		{
			return EditLinksMany(file, edits, written ? written : &none);
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	namespace
	{
		// The entries of an ADJMDLEP chunk: heading (12), the count, then 20 bytes each. False when the count does not fit the chunk (read as not there).
		bool ReadAdjacencyChunk(Bytes const &image, Piece const &chunk, std::vector<ModuleAdjacencyEntry> *out)
		{
			out->clear();

			if (chunk.size < HeadingBytes + 4)
			{
				return false;
			}

			int32_t const count = IntAt(image, chunk.at + HeadingBytes);

			if (count < 0 || (size_t)count > (chunk.size - HeadingBytes - 4) / 20)
			{
				return false;
			}

			for (int32_t i = 0; i < count; i++)
			{
				size_t const at = chunk.at + HeadingBytes + 4 + 20 * (size_t)i;
				ModuleAdjacencyEntry entry;

				entry.objectIndex = IntAt(image, at);
				entry.flags = IntAt(image, at + 4);
				entry.point[0] = IntAt(image, at + 8);
				entry.point[1] = IntAt(image, at + 12);
				entry.point[2] = IntAt(image, at + 16);
				out->push_back(entry);
			}

			return true;
		}

		bool ReadLinksChunk(Bytes const &image, Piece const &chunk, std::vector<ModuleLinkItem> *out)
		{
			out->clear();

			if (chunk.size < HeadingBytes + 4)
			{
				return false;
			}

			int32_t const count = IntAt(image, chunk.at + HeadingBytes);

			if (count < 0 || (size_t)count > (chunk.size - HeadingBytes - 4) / 16)
			{
				return false;
			}

			for (int32_t i = 0; i < count; i++)
			{
				size_t const at = chunk.at + HeadingBytes + 4 + 16 * (size_t)i;
				ModuleLinkItem item;

				item.objectIndex = IntAt(image, at);
				item.branchNo = IntAt(image, at + 4);
				item.flags = IntAt(image, at + 8);
				item.spare = IntAt(image, at + 12);
				out->push_back(item);
			}

			return true;
		}

		ObjectEditResult ReadSnapshots(OpenLevelFile *file, std::vector<ModuleSnapshot> *out)
		{
			out->clear();

			if (!file || !file->Level())
			{
				return ObjectEditResult::NothingOpen;
			}

			Bytes image;
			std::vector<ObjectInfo> objects;

			if (!file->BuildLevel(image) || !ReadObjects(image, &objects))
			{
				return ObjectEditResult::CantBuild;
			}

			for (ObjectInfo const &object : objects)
			{
				ModuleSnapshot snapshot;

				snapshot.index = object.index;
				snapshot.module = object.module;
				snapshot.hasAdjacency = object.hasAdj && ReadAdjacencyChunk(image, object.adj, &snapshot.adjacency);
				snapshot.hasLinks = object.hasLinks && ReadLinksChunk(image, object.links, &snapshot.links);
				snapshot.isMaster = false;
				snapshot.masterIndex = -1;
				snapshot.hasWaypoint = false;

				bool sawSlave = false;

				for (Piece const &child : object.moduleChildren)
				{
					if (NameIs(image, child.at, "AIMODMAS"))
					{
						snapshot.isMaster = true;
					}
					else if (NameIs(image, child.at, "AIMODSLA") && !sawSlave && child.size >= HeadingBytes + 4)
					{
						sawSlave = true;
						snapshot.masterIndex = IntAt(image, child.at + HeadingBytes);
					}
					else if (NameIs(image, child.at, "WAYPOINT") && !snapshot.hasWaypoint)
					{
						snapshot.hasWaypoint = true;
						snapshot.waypoint.assign(image.begin() + (std::ptrdiff_t)child.at, image.begin() + (std::ptrdiff_t)(child.at + child.size));
					}
				}

				out->push_back(snapshot);
			}

			return ObjectEditResult::Done;
		}

		// Each module's ADJMDLEP is taken out and (unless remove) a new one is put at the end of its MODULEDT, from the module that comes LAST in the file to the first
		ObjectEditResult EditAdjacencyMany(OpenLevelFile *file, std::vector<ModuleAdjacencyFor> const &edits, std::vector<bool> *written)
		{
			written->assign(edits.size(), false);

			if (!file || !file->Level())
			{
				return ObjectEditResult::NothingOpen;
			}

			Bytes image;
			std::vector<ObjectInfo> objects;

			if (!file->BuildLevel(image) || !ReadObjects(image, &objects))
			{
				return ObjectEditResult::CantBuild;
			}

			struct Place { size_t edit; size_t object; };

			std::vector<Place> places;
			std::set<size_t> usedObjects;

			for (size_t e = 0; e < edits.size(); e++)
			{
				if (!edits[e].remove && (edits[e].entries.empty() || edits[e].entries.size() > 100000))
				{
					continue;
				}

				for (size_t i = 0; i < objects.size(); i++)
				{
					if (objects[i].index == edits[e].objectIndex)
					{
						if (objects[i].module && usedObjects.insert(i).second)
						{
							Place place = { e, i };

							places.push_back(place);
						}

						break;
					}
				}
			}

			if (places.empty())
			{
				return ObjectEditResult::Done;
			}

			std::sort(places.begin(), places.end(), [&objects](Place const &a, Place const &b) { return objects[a.object].whole.at > objects[b.object].whole.at; });

			for (Place const &place : places)
			{
				ObjectInfo const &object = objects[place.object];
				ModuleAdjacencyFor const &edit = edits[place.edit];
				size_t oldSize = 0;

				if (object.hasAdj)
				{
					oldSize = object.adj.size;
					image.erase(image.begin() + (std::ptrdiff_t)object.adj.at, image.begin() + (std::ptrdiff_t)(object.adj.at + object.adj.size));
				}

				Bytes chunk;

				if (!edit.remove)
				{
					chunk.assign(HeadingBytes + 4 + 20 * edit.entries.size(), 0);
					memcpy(chunk.data(), "ADJMDLEP", 8);
					PutIntAt(chunk, 8, (int32_t)chunk.size());
					PutIntAt(chunk, 12, (int32_t)edit.entries.size());

					for (size_t i = 0; i < edit.entries.size(); i++)
					{
						size_t const at = HeadingBytes + 4 + 20 * i;

						PutIntAt(chunk, at, edit.entries[i].objectIndex);
						PutIntAt(chunk, at + 4, edit.entries[i].flags);
						PutIntAt(chunk, at + 8, edit.entries[i].point[0]);
						PutIntAt(chunk, at + 12, edit.entries[i].point[1]);
						PutIntAt(chunk, at + 16, edit.entries[i].point[2]);
					}

					size_t const end = object.moduleData.at + object.moduleData.size - oldSize;

					image.insert(image.begin() + (std::ptrdiff_t)end, chunk.begin(), chunk.end());
				}

				size_t const newModuleSize = object.moduleData.size - oldSize + chunk.size();
				size_t const newWholeSize = object.whole.size - oldSize + chunk.size();

				PutIntAt(image, object.moduleData.at + 8, (int32_t)newModuleSize);
				PutIntAt(image, object.whole.at + 8, (int32_t)newWholeSize);
				PutIntAt(image, 8, (int32_t)image.size());
			}

			ObjectEditResult const result = FromLoader(file->ReplaceLevel(image));

			if (result == ObjectEditResult::Done)
			{
				for (Place const &place : places)
				{
					(*written)[place.edit] = true;
				}
			}

			return result;
		}
	}

	namespace
	{
		// Module child edits: each object's MODULEDT is taken apart into its chunks, the edits are applied to that list in order, and it is put together again; from the object
		// that comes LAST in the file to the first
		ObjectEditResult EditChildrenMany(OpenLevelFile *file, std::vector<ModuleChildEdit> const &edits, std::vector<bool> *written)
		{
			written->assign(edits.size(), false);

			if (!file || !file->Level())
			{
				return ObjectEditResult::NothingOpen;
			}

			Bytes image;
			std::vector<ObjectInfo> objects;

			if (!file->BuildLevel(image) || !ReadObjects(image, &objects))
			{
				return ObjectEditResult::CantBuild;
			}

			// which object each edit is for (the first object with that number)
			std::map<size_t, std::vector<size_t> > editsOf;   // object position in `objects` -> its edits, in the order given

			for (size_t e = 0; e < edits.size(); e++)
			{
				if (!edits[e].remove && (edits[e].chunk.size() < HeadingBytes || edits[e].chunk.size() > (1u << 28)))
				{
					continue;
				}

				for (size_t i = 0; i < objects.size(); i++)
				{
					if (objects[i].index == edits[e].objectIndex)
					{
						if (objects[i].module)
						{
							editsOf[i].push_back(e);
						}

						break;
					}
				}
			}

			if (editsOf.empty())
			{
				return ObjectEditResult::Done;
			}

			for (std::map<size_t, std::vector<size_t> >::reverse_iterator it = editsOf.rbegin(); it != editsOf.rend(); ++it)
			{
				// the objects are in file order, but `editsOf` is by position in `objects`, which is the same order: the last object is first
				ObjectInfo const &object = objects[it->first];
				std::vector<Bytes> children;

				for (Piece const &child : object.moduleChildren)
				{
					children.push_back(Bytes(image.begin() + (std::ptrdiff_t)child.at, image.begin() + (std::ptrdiff_t)(child.at + child.size)));
				}

				for (size_t const e : it->second)
				{
					ModuleChildEdit const &edit = edits[e];

					for (size_t c = 0; c < children.size(); c++)
					{
						if (memcmp(children[c].data(), edit.name, 8) == 0)
						{
							children.erase(children.begin() + (std::ptrdiff_t)c);
							break;
						}
					}

					if (!edit.remove)
					{
						children.push_back(Bytes(edit.chunk.begin(), edit.chunk.end()));
					}
				}

				// the MODULEDT again: its heading as it was, then the chunks; a chunk before the first child (none: children start at the heading's end)
				size_t const inside = object.moduleData.at + HeadingBytes;
				size_t const oldEnd = object.moduleData.at + object.moduleData.size;
				Bytes body;

				for (Bytes const &child : children)
				{
					body.insert(body.end(), child.begin(), child.end());
				}

				// only the chunks that are children are replaced: the bytes between the heading and the end are exactly the children (ReadObject refuses anything else)
				image.erase(image.begin() + (std::ptrdiff_t)inside, image.begin() + (std::ptrdiff_t)oldEnd);
				image.insert(image.begin() + (std::ptrdiff_t)inside, body.begin(), body.end());

				size_t const newModuleSize = HeadingBytes + body.size();
				size_t const newWholeSize = object.whole.size - object.moduleData.size + newModuleSize;

				PutIntAt(image, object.moduleData.at + 8, (int32_t)newModuleSize);
				PutIntAt(image, object.whole.at + 8, (int32_t)newWholeSize);
				PutIntAt(image, 8, (int32_t)image.size());
			}

			ObjectEditResult const result = FromLoader(file->ReplaceLevel(image));

			if (result == ObjectEditResult::Done)
			{
				for (std::map<size_t, std::vector<size_t> >::const_iterator it = editsOf.begin(); it != editsOf.end(); ++it)
				{
					for (size_t const e : it->second)
					{
						(*written)[e] = true;
					}
				}
			}

			return result;
		}
	}

	namespace
	{
		ObjectEditResult EditObjectChildrenMany(OpenLevelFile *file, std::vector<ObjectChildEdit> const &edits, std::vector<bool> *written)
		{
			written->assign(edits.size(), false);

			if (!file || !file->Level())
			{
				return ObjectEditResult::NothingOpen;
			}

			Bytes image;
			std::vector<ObjectInfo> objects;

			if (!file->BuildLevel(image) || !ReadObjects(image, &objects))
			{
				return ObjectEditResult::CantBuild;
			}

			std::map<size_t, std::vector<size_t> > editsOf;   // object position in `objects` -> its edits, in the order given

			for (size_t e = 0; e < edits.size(); e++)
			{
				if (!edits[e].remove && (edits[e].chunk.size() < HeadingBytes || edits[e].chunk.size() > (1u << 28)))
				{
					continue;
				}

				for (size_t i = 0; i < objects.size(); i++)
				{
					if (objects[i].index == edits[e].objectIndex)
					{
						editsOf[i].push_back(e);
						break;
					}
				}
			}

			if (editsOf.empty())
			{
				return ObjectEditResult::Done;
			}

			for (std::map<size_t, std::vector<size_t> >::reverse_iterator it = editsOf.rbegin(); it != editsOf.rend(); ++it)
			{
				ObjectInfo const &object = objects[it->first];
				std::vector<Bytes> children;
				size_t childBytes = 0;

				for (Piece const &child : object.children)
				{
					children.push_back(Bytes(image.begin() + (std::ptrdiff_t)child.at, image.begin() + (std::ptrdiff_t)(child.at + child.size)));
					childBytes += child.size;
				}

				// the object's own bytes after its heading have to be exactly its children: otherwise this object is not touched
				if (childBytes + HeadingBytes != object.whole.size)
				{
					continue;
				}

				for (size_t const e : it->second)
				{
					ObjectChildEdit const &edit = edits[e];
					size_t const starts = strnlen(edit.payloadStarts, 8);

					for (size_t c = 0; c < children.size();)
					{
						Bytes const &child = children[c];

						if (child.size() >= HeadingBytes && memcmp(child.data(), edit.name, 8) == 0 &&
							(starts == 0 || (child.size() >= HeadingBytes + starts && memcmp(child.data() + HeadingBytes, edit.payloadStarts, starts) == 0)))
						{
							children.erase(children.begin() + (std::ptrdiff_t)c);
						}
						else
						{
							c++;
						}
					}

					if (!edit.remove)
					{
						children.push_back(Bytes(edit.chunk.begin(), edit.chunk.end()));
					}
				}

				Bytes body;

				for (Bytes const &child : children)
				{
					body.insert(body.end(), child.begin(), child.end());
				}

				size_t const inside = object.whole.at + HeadingBytes;
				size_t const oldEnd = object.whole.at + object.whole.size;

				image.erase(image.begin() + (std::ptrdiff_t)inside, image.begin() + (std::ptrdiff_t)oldEnd);
				image.insert(image.begin() + (std::ptrdiff_t)inside, body.begin(), body.end());
				PutIntAt(image, object.whole.at + 8, (int32_t)(HeadingBytes + body.size()));
				PutIntAt(image, 8, (int32_t)image.size());
			}

			ObjectEditResult const result = FromLoader(file->ReplaceLevel(image));

			if (result == ObjectEditResult::Done)
			{
				for (std::map<size_t, std::vector<size_t> >::const_iterator it = editsOf.begin(); it != editsOf.end(); ++it)
				{
					for (size_t const e : it->second)
					{
						(*written)[e] = true;
					}
				}
			}

			return result;
		}
	}

	ObjectEditResult SetObjectChildrenMany(OpenLevelFile *file, std::vector<ObjectChildEdit> const &edits, std::vector<bool> *written)
	{
		std::vector<bool> none;

		try
		{
			return EditObjectChildrenMany(file, edits, written ? written : &none);
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	ObjectEditResult SetModuleChildrenMany(OpenLevelFile *file, std::vector<ModuleChildEdit> const &edits, std::vector<bool> *written)
	{
		std::vector<bool> none;

		try
		{
			return EditChildrenMany(file, edits, written ? written : &none);
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	ObjectEditResult DropModuleLinks(OpenLevelFile *file, int32_t objectIndex, bool *dropped)
	{
		if (dropped)
		{
			*dropped = false;
		}

		try
		{
			std::vector<ModuleSnapshot> snapshots;
			ObjectEditResult const read = ReadSnapshots(file, &snapshots);

			if (read != ObjectEditResult::Done)
			{
				return read;
			}

			bool has = false;

			for (ModuleSnapshot const &snapshot : snapshots)
			{
				if (snapshot.module && snapshot.index == objectIndex)
				{
					has = snapshot.hasLinks;
					break;
				}
			}

			if (!has)
			{
				return ObjectEditResult::Done;
			}

			ModuleChildEdit edit;

			edit.objectIndex = objectIndex;
			memset(edit.name, 0, sizeof(edit.name));
			memcpy(edit.name, "VMDARRAY", 8);
			edit.remove = true;

			std::vector<ModuleChildEdit> edits;
			std::vector<bool> wrote;

			edits.push_back(edit);

			ObjectEditResult const result = EditChildrenMany(file, edits, &wrote);

			if (result == ObjectEditResult::Done && dropped)
			{
				*dropped = wrote.size() == 1 && wrote[0];
			}

			return result;
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	ObjectEditResult ReadModuleSnapshots(OpenLevelFile *file, std::vector<ModuleSnapshot> *out)
	{
		try
		{
			return ReadSnapshots(file, out);
		}
		catch (std::exception const &)
		{
			out->clear();
			return ObjectEditResult::CantBuild;
		}
	}

	ObjectEditResult SetModuleAdjacencyMany(OpenLevelFile *file, std::vector<ModuleAdjacencyFor> const &edits, std::vector<bool> *written)
	{
		std::vector<bool> none;

		try
		{
			return EditAdjacencyMany(file, edits, written ? written : &none);
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	char const *ObjectEditWords(ObjectEditResult result)
	{
		switch (result)
		{
		case ObjectEditResult::Done: return "done";
		case ObjectEditResult::NothingOpen: return "no level is open";
		case ObjectEditResult::NoName: return "the object has no name to look for";
		case ObjectEditResult::NoSuchObject: return "the level has no object with that name";
		case ObjectEditResult::IsAModule: return "a module can be neither taken out nor copied";
		case ObjectEditResult::NotPlaced: return "only a placed object can be copied";
		case ObjectEditResult::BadName: return "a name is 1 to 63 plain letters";
		case ObjectEditResult::NameTaken: return "another object has that name";
		case ObjectEditResult::PlaceOutOfRange: return "that place is too far from the middle of the level";
		case ObjectEditResult::TooManyObjects: return "the level has no object number left";
		case ObjectEditResult::CantBuild: return "the level could not be put together";
		case ObjectEditResult::LoaderRefused: return "the game would not take the level that way, so nothing was changed";
		case ObjectEditResult::NoStrategy: return "the object has no strategy";
		case ObjectEditResult::BadStrategy: return "the editor can't make a strategy of that kind";
		case ObjectEditResult::SameObject: return "that is the object's own strategy";
		case ObjectEditResult::NotAModule: return "the object is not a module";
		case ObjectEditResult::BadLinks: return "there are no links to write, or too many";
		}

		return "the object could not be changed";
	}

	// Running out of memory refuses the change, with the level as it was
	ObjectEditResult DeleteObject(OpenLevelFile *file, uint8_t const *id)
	{
		try
		{
			return Edit(file, id, 0, 0, true, 0);
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	ObjectEditResult CreateObjectLike(OpenLevelFile *file, uint8_t const *like, char const *name, int32_t const *place, uint8_t *newId)
	{
		try
		{
			return Edit(file, like, name, place, false, newId);
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	ObjectEditResult RenameObject(OpenLevelFile *file, uint8_t const *id, char const *name, uint8_t *newId)
	{
		try
		{
			if (!name || !name[0] || strlen(name) > MostNameBytes)
			{
				return ObjectEditResult::NoName;
			}

			Bytes image;
			std::vector<ObjectInfo> objects;
			size_t at = 0;
			ObjectEditResult const prepared = Prepare(file, id, &image, &objects, &at);

			if (prepared != ObjectEditResult::Done)
			{
				return prepared;
			}

			ObjectInfo const object = objects[at];
			std::string const made = name;

			// The object's bytes as they are, but for its header, made again with the new name at the size the library writes it for that name (Object_Header_Chunk::size_chunk:
			// 72 and the name with its zero, to a whole 4); every other byte of the header (the fixed 60 after its heading) as it was
			Bytes whole(HeadingBytes, 0);
			bool headerSeen = false;

			memcpy(whole.data(), image.data() + object.whole.at, HeadingBytes);

			for (Piece const &child : object.children)
			{
				if (!NameIs(image, child.at, "OBJHEAD1") || headerSeen || child.size < HeadingBytes + NameAt)
				{
					whole.insert(whole.end(), image.begin() + (std::ptrdiff_t)child.at, image.begin() + (std::ptrdiff_t)(child.at + child.size));
					continue;
				}

				headerSeen = true;

				size_t length = HeadingBytes + NameAt + made.size() + 1;
				length += (4 - length % 4) % 4;

				size_t const start = whole.size();
				whole.resize(start + length, 0);
				memcpy(whole.data() + start, "OBJHEAD1", 8);
				PutIntAt(whole, start + 8, (int32_t)length);
				memcpy(whole.data() + start + HeadingBytes, image.data() + child.at + HeadingBytes, NameAt);
				memcpy(whole.data() + start + HeadingBytes + NameAt, made.c_str(), made.size());
			}

			if (!headerSeen)
			{
				return ObjectEditResult::CantBuild;
			}

			PutIntAt(whole, 8, (int32_t)whole.size());
			image.erase(image.begin() + (std::ptrdiff_t)object.whole.at, image.begin() + (std::ptrdiff_t)(object.whole.at + object.whole.size));
			image.insert(image.begin() + (std::ptrdiff_t)object.whole.at, whole.begin(), whole.end());
			PutIntAt(image, 8, (int32_t)image.size());

			ObjectEditResult const done = FromLoader(file->ReplaceLevel(image));

			if (done == ObjectEditResult::Done && newId)
			{
				memset(newId, 0, 8);
				IdOfName(file, made, newId);
			}

			return done;
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	ObjectEditResult DeleteStrategy(OpenLevelFile *file, uint8_t const *id)
	{
		try
		{
			return EditStrategy(file, id, StrategyWhat::Delete, 0, 0);
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	ObjectEditResult AssignStrategy(OpenLevelFile *file, uint8_t const *id, int32_t kind)
	{
		try
		{
			return EditStrategy(file, id, StrategyWhat::Assign, kind, 0);
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	ObjectEditResult SetModuleLinks(OpenLevelFile *file, uint8_t const *id, ModuleLinkItem const *items, size_t count)
	{
		try
		{
			return EditLinks(file, id, items, count);
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	ObjectEditResult SetStrategyBytes(OpenLevelFile *file, uint8_t const *id, uint8_t const *payload, size_t length)
	{
		try
		{
			return EditStrategy(file, id, StrategyWhat::Replace, 0, 0, payload, length);
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}

	ObjectEditResult CopyStrategy(OpenLevelFile *file, uint8_t const *from, uint8_t const *to)
	{
		if (!from)
		{
			return ObjectEditResult::NoName;
		}

		try
		{
			return EditStrategy(file, to, StrategyWhat::Copy, 0, from);
		}
		catch (std::exception const &)
		{
			return ObjectEditResult::CantBuild;
		}
	}
}
