// See level_chunks.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "level_chunks.h"

#include <string.h>

#include <exception>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk (list_assoc_objs), SHAPE_FLAG_EXTERNALFILE
#include "mishchnk.hpp"       // File_Chunk::list_objects

// An object's ID is the 8 bytes a pick compares (selection.h): the game's ObjectID is two ints
static_assert(sizeof(ObjectID) == 8, "an ObjectID is the 8 bytes a pick in the view compares");

namespace host
{
	namespace
	{
		// A chunk's name is its 8 letters (chunk.hpp compares 8, strncmp)
		size_t const ChunkNameBytes = 8;

		// True when name is a text of exactly 8 letters, so comparing 8 never reads past its end
		bool NameIsEightLetters(char const *name)
		{
			return name && strlen(name) == ChunkNameBytes;
		}

		// A chunk that can hold others: one the library built as a holder, not one it kept as plain bytes (too small for what its kind
		// reads). Gives it as a holder, or null.
		Chunk_With_Children const *AsHolder(Chunk *chunk)
		{
			if (!chunk || chunk->r_u_miscellaneous())
			{
				return nullptr;
			}

			return dynamic_cast<Chunk_With_Children const *>(chunk);
		}

		// The level's REBENVDT, the one the game uses: the loader refuses a file with none (mishchnk.cpp:295-300)
		Chunk_With_Children const *Environment(File_Chunk *level)
		{
			return AsHolder(level->lookup_single_child("REBENVDT"));
		}

		// Every child of holder with that name, in its order, into *out
		void Collect(Chunk_With_Children const *holder, char const *name, std::vector<Chunk *> *out)
		{
			List<Chunk *> found;

			holder->lookup_child(name, found);

			for (LIF<Chunk *> each(&found); !each.done(); each.next())
			{
				Chunk *const chunk = each();

				if (chunk)
				{
					out->push_back(chunk);
				}
			}
		}

		bool IdIsAllZero(uint8_t const *id)
		{
			for (size_t i = 0; i < ChunkNameBytes; i++)
			{
				if (id[i] != 0)
				{
					return false;
				}
			}

			return true;
		}

		LevelChunkFound FindOne(File_Chunk *level, char const *name, Chunk **out)
		{
			if (!level)
			{
				return LevelChunkFound::NoLevel;
			}

			Chunk_With_Children const *const environment = Environment(level);

			if (!environment)
			{
				return LevelChunkFound::NoEnvironment;
			}

			Chunk *const found = environment->lookup_single_child(name);

			if (!found)
			{
				return LevelChunkFound::NoneThere;
			}

			*out = found;
			return LevelChunkFound::Found;
		}

		LevelChunkFound FindMany(File_Chunk *level, char const *name, std::vector<Chunk *> *out)
		{
			if (!level)
			{
				return LevelChunkFound::NoLevel;
			}

			Chunk_With_Children const *const environment = Environment(level);

			if (!environment)
			{
				return LevelChunkFound::NoEnvironment;
			}

			Collect(environment, name, out);
			return out->empty() ? LevelChunkFound::NoneThere : LevelChunkFound::Found;
		}

		LevelChunkFound FindFlags(File_Chunk *level, uint8_t const *id, Chunk **out)
		{
			if (!level)
			{
				return LevelChunkFound::NoLevel;
			}

			if (IdIsAllZero(id))
			{
				return LevelChunkFound::NoName;
			}

			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (!object || memcmp(&object->object_data.ID, id, ChunkNameBytes) != 0)
				{
					continue;
				}

				// The first object with this ID is the one, as the strategy lookup and the object list have it
				Chunk_With_Children const *const module = AsHolder(object->lookup_single_child("MODULEDT"));

				if (!module)
				{
					return LevelChunkFound::NotAModule;
				}

				Chunk *const flags = module->lookup_single_child("MODFLAGS");

				if (!flags)
				{
					return LevelChunkFound::NoneThere;
				}

				*out = flags;
				return LevelChunkFound::Found;
			}

			return LevelChunkFound::NoSuchObject;
		}
	}

	char const *LevelChunkFoundWords(LevelChunkFound found)
	{
		switch (found)
		{
		case LevelChunkFound::Found: return "the chunk was found";
		case LevelChunkFound::BadCall: return "there was nothing to look for, or nowhere to put the answer";
		case LevelChunkFound::NoLevel: return "no level is open";
		case LevelChunkFound::NoEnvironment: return "the level has no environment chunk";
		case LevelChunkFound::NoneThere: return "the level has no such chunk";
		case LevelChunkFound::NotAHolder: return "that chunk holds no other chunks";
		case LevelChunkFound::NoName: return "the object has no name to look for";
		case LevelChunkFound::NoSuchObject: return "the level has no object with that name";
		case LevelChunkFound::NotAModule: return "the object is not a module";
		}

		return "the chunk couldn't be looked up";
	}

	LevelChunkFound FindEnvironmentChild(File_Chunk *level, char const *name, Chunk **out)
	{
		if (!out)
		{
			return LevelChunkFound::BadCall;
		}

		*out = nullptr;

		if (!NameIsEightLetters(name))
		{
			return LevelChunkFound::BadCall;
		}

		return FindOne(level, name, out);
	}

	bool LevelRiffName(File_Chunk *level, size_t most, std::string *name)
	{
		if (!name)
		{
			return false;
		}

		name->clear();

		Chunk *found = nullptr;

		if (FindEnvironmentChild(level, "RIFFNAME", &found) != LevelChunkFound::Found || !found)
		{
			return false;
		}

		RIF_Name_Chunk const *const riff = dynamic_cast<RIF_Name_Chunk const *>(found);

		if (!riff || !riff->rif_name)
		{
			return false;
		}

		for (char const *p = riff->rif_name; *p && name->size() < most; p++)
		{
			name->push_back(*p >= 0x20 && *p <= 0x7e ? *p : '_');
		}

		return !name->empty();
	}

	LevelChunkFound FindEnvironmentChildren(File_Chunk *level, char const *name, std::vector<Chunk *> *out)
	{
		if (!out)
		{
			return LevelChunkFound::BadCall;
		}

		out->clear();

		if (!NameIsEightLetters(name))
		{
			return LevelChunkFound::BadCall;
		}

		// Running out of memory is an empty answer, not a crash
		try
		{
			return FindMany(level, name, out);
		}
		catch (std::exception const &)
		{
			out->clear();
			return LevelChunkFound::NoneThere;
		}
	}

	LevelChunkFound FindChildren(Chunk *holder, char const *name, std::vector<Chunk *> *out)
	{
		if (!out)
		{
			return LevelChunkFound::BadCall;
		}

		out->clear();

		if (!holder || !NameIsEightLetters(name))
		{
			return LevelChunkFound::BadCall;
		}

		Chunk_With_Children const *const asHolder = AsHolder(holder);

		if (!asHolder)
		{
			return LevelChunkFound::NotAHolder;
		}

		try
		{
			Collect(asHolder, name, out);
		}
		catch (std::exception const &)
		{
			out->clear();
			return LevelChunkFound::NoneThere;
		}

		return out->empty() ? LevelChunkFound::NoneThere : LevelChunkFound::Found;
	}

	LevelChunkFound FindModuleFlags(File_Chunk *level, uint8_t const *id, Chunk **out)
	{
		if (!out)
		{
			return LevelChunkFound::BadCall;
		}

		*out = nullptr;

		if (!id)
		{
			return LevelChunkFound::BadCall;
		}

		return FindFlags(level, id, out);
	}

	namespace
	{
		bool IsModuleObject(Object_Chunk *object)
		{
			Object_Header_Chunk *const header = object ? object->get_header() : nullptr;

			// A placed object (OBJECT_FLAG_PLACED_OBJECT, 0x400) is not a module
			return object && !(header && (header->flags & 0x400));
		}
	}

	size_t ModuleIndexes(File_Chunk *level, std::set<int32_t> *out)
	{
		if (!out)
		{
			return 0;
		}

		out->clear();

		if (!level)
		{
			return 0;
		}

		try
		{
			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				if (IsModuleObject(each()))
				{
					out->insert((int32_t)each()->object_data.index_num);
				}
			}
		}
		catch (std::exception const &)
		{
		}

		return out->size();
	}

	bool ModuleIndexOfId(File_Chunk *level, uint8_t const *id, int32_t *index)
	{
		if (!level || !id || !index)
		{
			return false;
		}

		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (object && memcmp(&object->object_data.ID, id, ChunkNameBytes) == 0)
			{
				// The first object with this ID is the one
				if (!IsModuleObject(object))
				{
					return false;
				}

				*index = (int32_t)object->object_data.index_num;
				return true;
			}
		}

		return false;
	}

	bool ObjectIdOfIndex(File_Chunk *level, int32_t index, uint8_t *id)
	{
		if (!level || !id)
		{
			return false;
		}

		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			if (each() && (int32_t)each()->object_data.index_num == index)
			{
				memcpy(id, &each()->object_data.ID, ChunkNameBytes);
				return true;
			}
		}

		return false;
	}

	bool ModuleIdOfIndex(File_Chunk *level, int32_t index, uint8_t *id)
	{
		if (!level || !id)
		{
			return false;
		}

		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (IsModuleObject(object) && (int32_t)object->object_data.index_num == index)
			{
				memcpy(id, &object->object_data.ID, ChunkNameBytes);
				return true;
			}
		}

		return false;
	}

	bool ReadModuleLinks(File_Chunk *level, int32_t index, std::vector<LinkItem> *out)
	{
		if (!level || !out)
		{
			return false;
		}

		out->clear();

		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (!IsModuleObject(object) || (int32_t)object->object_data.index_num != index)
			{
				continue;
			}

			Chunk_With_Children const *const module = AsHolder(object->lookup_single_child("MODULEDT"));
			Chunk *const found = module ? module->lookup_single_child("VMDARRAY") : nullptr;

			if (!found || found->r_u_miscellaneous())
			{
				return false;
			}

			VModule_Array_Chunk *const links = (VModule_Array_Chunk *)found;

			for (int i = 0; i < links->num_array_items; i++)
			{
				LinkItem item;

				item.branchNo = links->vmod_array[i].branch_no;
				item.flags = links->vmod_array[i].flags;
				item.spare = links->vmod_array[i].spare;
				item.objectIndex = links->vmod_array[i].object_index;
				out->push_back(item);
			}

			return true;
		}

		return false;
	}

	size_t GameModuleNumbers(File_Chunk *level, std::map<int32_t, int32_t> *out)
	{
		if (!out)
		{
			return 0;
		}

		out->clear();

		if (!level)
		{
			return 0;
		}

		try
		{
			List<Shape_Chunk *> shapes;

			level->list_shapes(&shapes);

			// mod_pos counts from 0, but the game's module array has its "infinite module" in front (projload.cpp:2012-2019 moves the arrays on by one while
			// loading, 2694 / 2726 put them back with sm_marray[0] = the infinite module), so the module at mod_pos is sm_marray[mod_pos + 1] and PreprocessModuleArray
			// (module.cpp:744) gives it m_index = mod_pos + 1. (Until 2026-11-16 this counted from 0: every module tint, Hide and pick that used it was one module off.)
			int32_t next = 1;

			for (LIF<Shape_Chunk *> each(&shapes); !each.done(); each.next())
			{
				Shape_Chunk *const shape = each();

				if (!shape)
				{
					continue;
				}

				// Copied: the library hands back its own list
				List<Object_Chunk *> objects = shape->list_assoc_objs();
				Shape_Header_Chunk *const header = shape->get_header();

				// An imported shape (SHAPE_FLAG_EXTERNALFILE) is placed objects only
				if (header && (header->flags & SHAPE_FLAG_EXTERNALFILE))
				{
					continue;
				}

				for (LIF<Object_Chunk *> one(&objects); !one.done(); one.next())
				{
					Object_Chunk *const object = one();
					Object_Header_Chunk *const objectHeader = object ? object->get_header() : nullptr;

					if (object && objectHeader && !(objectHeader->flags & 0x400))
					{
						(*out)[(int32_t)object->object_data.index_num] = next++;
					}
				}
			}
		}
		catch (std::exception const &)
		{
		}

		return out->size();
	}

	size_t FindModulesWithoutAdjacency(File_Chunk *level, std::vector<ModuleWithoutAdjacency> *out)
	{
		if (!out)
		{
			return 0;
		}

		out->clear();

		if (!level)
		{
			return 0;
		}

		size_t modules = 0;

		try
		{
			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();
				Object_Header_Chunk *const header = object ? object->get_header() : nullptr;

				// A placed object (OBJECT_FLAG_PLACED_OBJECT, 0x400) is not a module
				if (!object || (header && (header->flags & 0x400)))
				{
					continue;
				}

				modules++;

				Chunk_With_Children const *const module = AsHolder(object->lookup_single_child("MODULEDT"));

				if (module && module->lookup_single_child("ADJMDLEP"))
				{
					continue;
				}

				ModuleWithoutAdjacency found;
				char const *const name = object->object_data.o_name;
				void const *const zero = memchr(name, 0, sizeof(object->object_data.o_name));

				found.name.assign(name, zero ? (size_t)((char const *)zero - name) : sizeof(object->object_data.o_name));
				memcpy(found.id, &object->object_data.ID, sizeof(found.id));
				out->push_back(found);
			}
		}
		catch (std::exception const &)
		{
			// Out of memory: what was found so far is given
		}

		return modules;
	}

	bool LevelHasHierarchies(File_Chunk *level)
	{
		return level && level->lookup_single_child("OBJCHIER") != nullptr;
	}
}
